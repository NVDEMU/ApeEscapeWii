/*
 * Private Wii runtime entry for the statically recompiled Ape Escape EXE.
 *
 * This path keeps the original executable's RAM image and BIOS/device behavior
 * separate from the generated native C. The generated game code is staged only
 * by the private workflow; this file itself contains no game bytes.
 */
#include <gccore.h>
#include <fat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "cpu_state.h"
#include "psx_memory.h"
#include "psx_interpreter.h"
#include "gpu.h"
#include "gpu_sw_renderer.h"
#include "interrupts.h"
#include "dma.h"
#include "timers.h"
#include "cdrom.h"
#include "mdec.h"
#include "sio.h"
#include "spu.h"
#include "memcard.h"
#include "psx_cycles.h"
#include "psx_wii_input.h"
#include "psx_wii_video.h"

#define GAME_EXE_PATH "sd:/apps/ApeEscapeWii/SCUS_944.23"
#define BIOS_PATH     "sd:/apps/ApeEscapeWii/SCPH1001.BIN"
#define DISC_CUE_PATH "sd:/apps/ApeEscapeWii/Ape Escape (USA).cue"
#define EXE_PAYLOAD_OFFSET 0x800u
#define RAM_BYTES 0x00800000u
#define FRAME_MAX_W 1024u
#define FRAME_MAX_H 576u
#define FRAME_PRESENT_CYCLES 250000ull

extern void memory_init(const char *bios_path);
extern uint8_t *memory_get_ram_ptr(void);
extern void dirty_ram_register_text_image(uint32_t phys_lo,
                                          const uint8_t *bytes,
                                          uint32_t len);
extern uint32_t psx_read_word(uint32_t addr);
extern uint16_t psx_read_half(uint32_t addr);
extern uint8_t psx_read_byte(uint32_t addr);
extern void psx_write_word(uint32_t addr, uint32_t value);
extern void psx_write_half(uint32_t addr, uint16_t value);
extern void psx_write_byte(uint32_t addr, uint8_t value);
extern void dirty_ram_reset_for_boot(void);
extern int psx_dispatch_game_compiled(CPUState *cpu, uint32_t addr);
extern uint64_t psx_cycle_count;

static uint8_t *s_exe_image;
static uint32_t s_exe_bytes;
static uint32_t s_loaded_addr;
static uint32_t s_loaded_bytes;
static FILE *s_boot_log;

static void boot_log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    if (s_boot_log) {
        vfprintf(s_boot_log, format, args);
        fflush(s_boot_log);
    }
    va_end(args);
}

static uint32_t s_frame[FRAME_MAX_W * FRAME_MAX_H];
static uint64_t s_last_present_cycle;
uint64_t s_frame_count = 0;
static uint32_t s_dispatches_since_present;
static int s_running;

static uint32_t rd32le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int read_file(const char *path, uint8_t **bytes_out, uint32_t *size_out)
{
    FILE *f = fopen(path, "rb");
    long length;
    uint8_t *bytes;
    if (!f) {
        printf("Cannot open %s\n", path);
        boot_log("ERROR: cannot open %s\n", path);
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (length = ftell(f)) < 0 ||
        fseek(f, 0, SEEK_SET) != 0 || (unsigned long)length > 0xFFFFFFFFul) {
        fclose(f);
        printf("Invalid file length: %s\n", path);
        boot_log("ERROR: invalid file length: %s\n", path);
        return 0;
    }
    bytes = (uint8_t *)malloc((size_t)length);
    if (!bytes || fread(bytes, 1, (size_t)length, f) != (size_t)length) {
        free(bytes);
        fclose(f);
        printf("Cannot read %s\n", path);
        boot_log("ERROR: cannot read %s\n", path);
        return 0;
    }
    fclose(f);
    *bytes_out = bytes;
    *size_out = (uint32_t)length;
    return 1;
}

/* Load the PS-X EXE's boot image into real PS1 guest RAM. The retained byte
 * image also becomes the dirty-code guard's immutable reference. */
static int load_boot_exe(CPUState *cpu)
{
    uint32_t pc, gp, load_addr, load_bytes, stack, phys;
    uint32_t available;
    uint8_t *ram;

    boot_log("Loading PS-X EXE: %s\n", GAME_EXE_PATH);
    if (!read_file(GAME_EXE_PATH, &s_exe_image, &s_exe_bytes))
        return 0;
    if (s_exe_bytes < EXE_PAYLOAD_OFFSET ||
        memcmp(s_exe_image, "PS-X EXE", 8) != 0) {
        printf("Not a valid PS-X EXE: %s\n", GAME_EXE_PATH);
        boot_log("ERROR: invalid PS-X EXE header\n");
        return 0;
    }

    pc = rd32le(s_exe_image + 0x10);
    gp = rd32le(s_exe_image + 0x14);
    load_addr = rd32le(s_exe_image + 0x18);
    load_bytes = rd32le(s_exe_image + 0x1C);
    stack = rd32le(s_exe_image + 0x30);
    phys = load_addr & 0x1FFFFFFFu;
    available = s_exe_bytes - EXE_PAYLOAD_OFFSET;

    if (load_bytes == 0 || load_bytes > available ||
        phys >= RAM_BYTES || load_bytes > RAM_BYTES - phys) {
        printf("Invalid EXE load range: addr=%08lx size=%08lx\n",
               (unsigned long)load_addr, (unsigned long)load_bytes);
        boot_log("ERROR: invalid EXE load range addr=%08lx size=%08lx\n",
                 (unsigned long)load_addr, (unsigned long)load_bytes);
        return 0;
    }

    ram = memory_get_ram_ptr();
    memcpy(ram + phys, s_exe_image + EXE_PAYLOAD_OFFSET, load_bytes);
    s_loaded_addr = phys;
    s_loaded_bytes = load_bytes;

    /* The PS-X EXE boot image is static text/data. Registers its original bytes
     * so the generated dispatcher can validate native entries and reject code
     * that a later runtime write changes. */
    dirty_ram_register_text_image(phys,
                                  s_exe_image + EXE_PAYLOAD_OFFSET,
                                  load_bytes);

    memset(cpu, 0, sizeof(*cpu));
    cpu->read_word = psx_read_word;
    cpu->write_word = psx_write_word;
    cpu->read_half = psx_read_half;
    cpu->write_half = psx_write_half;
    cpu->read_byte = psx_read_byte;
    cpu->write_byte = psx_write_byte;
    cpu->pc = pc ? pc : 0x800A3660u;
    cpu->gpr[28] = gp;
    cpu->gpr[29] = stack ? stack : 0x801FFFF0u;
    cpu->gpr[0] = 0;
    boot_log("EXE loaded: file=%lu pc=%08lx gp=%08lx load=%08lx bytes=%lu sp=%08lx\n",
             (unsigned long)s_exe_bytes, (unsigned long)pc, (unsigned long)gp,
             (unsigned long)load_addr, (unsigned long)load_bytes,
             (unsigned long)cpu->gpr[29]);
    return 1;
}

static void present_ps1_frame(void)
{
    GpuDisplayInfo info;
    ++s_frame_count;
    uint32_t width, height;
    gpu_get_display_info(&info);
    width = info.width;
    height = info.height;
    if (width == 0 || width > FRAME_MAX_W) width = 320;
    if (height == 0 || height > FRAME_MAX_H) height = 240;
    sw_render_display(s_frame, FRAME_MAX_W * (uint32_t)sizeof(uint32_t),
                      (int)info.display_x, (int)info.display_y,
                      (int)width, (int)height);
    (void)psx_wii_video_present_argb(s_frame, width, height,
                                     FRAME_MAX_W * (uint32_t)sizeof(uint32_t));
}

static void update_pad(void)
{
    /*
     * Temporary bring-up isolation: do not initialize or poll WPAD while we
     * diagnose a Dolphin invalid-write popup in libogc's USBV5_CancelEndpoint.
     * The PS1 pad remains present but neutral. Restore WPAD after the native
     * guest loop is stable; this deliberately avoids libogc USB/Bluetooth IO.
     */
    sio_set_pad_state(0xFFFFu);
    sio_set_pad_sticks(0, 128, 128, 128, 128);
}

int main(void)
{
    CPUState cpu;
    uint64_t last_present = 0;
    uint32_t loop_count = 0;

    if (!fatInitDefault()) {
        printf("SD card initialization failed.\n");
        return 1;
    }
    s_boot_log = fopen("sd:/apps/ApeEscapeWii/boot.log", "wb");
    boot_log("ApeEscapeWii native boot diagnostic\n");
    boot_log("BIOS path: %s\nDisc cue: %s\n", BIOS_PATH, DISC_CUE_PATH);
    if (psx_wii_video_init() != 0) {
        printf("GX video initialization failed.\n");
        boot_log("ERROR: GX video initialization failed\n");
        if (s_boot_log) fclose(s_boot_log);
        return 1;
    }
    boot_log("Wii video initialized; WPAD temporarily disabled for USB fault isolation\n");
    memset(s_frame, 0, sizeof(s_frame));

    /* The user-provided retail BIOS and original disc remain on the SD card;
     * neither is copied into the public source repository. */
    boot_log("Initializing PS1 memory/BIOS\n");
    memory_init(BIOS_PATH);
    boot_log("PS1 memory/BIOS initialization returned\n");
    sio_init();
    timers_init();
    dma_init();
    mdec_init();
    spu_init();
    gpu_init();
    sw_renderer_init((uint16_t *)gpu_get_vram());
    interrupts_init();
    psx_cycles_reset_for_boot();
    cdrom_init(DISC_CUE_PATH);
    boot_log("PS1 devices initialized; loading EXE\n");

    if (!load_boot_exe(&cpu)) {
        boot_log("ERROR: executable load failed; waiting for HOME\n");
        present_ps1_frame();
        for (;;) VIDEO_WaitVSync();
        psx_wii_video_shutdown();
        if (s_boot_log) fclose(s_boot_log);
        return 1;
    }

    sio_connect_pad(0);
    sio_set_pad_analog(0, 1, 128, 128, 128, 128);
    interp_init(&cpu);
    s_running = 1;
    boot_log("Entering guest execution loop at PC=%08lx\n", (unsigned long)cpu.pc);
    present_ps1_frame();

    while (s_running) {
        update_pad();
        /* Native AOT dispatch for compiled EXE functions; use the real R3000A
         * interpreter for BIOS ROM, runtime-written RAM code and any address
         * that is not in this executable's generated entry table. */
        if (s_dispatches_since_present < 128u) {
            boot_log("STEP %lu pc=%08lx\n", (unsigned long)s_dispatches_since_present,
                     (unsigned long)cpu.pc);
            ++s_dispatches_since_present;
        }
        if (!psx_dispatch_game_compiled(&cpu, cpu.pc))
            (void)interp_step(&cpu, 1);

        cpu.gpr[0] = 0;
        if (cpu.pc == 0) {
            /* A guest exit/fatal dispatch. Keep the last frame visible so this
             * first boot attempt is inspectable rather than returning to HBC. */
            for (;;) VIDEO_WaitVSync();
            break;
        }

        ++loop_count;
        if ((psx_cycle_count - last_present) >= FRAME_PRESENT_CYCLES ||
            (loop_count % 6000u) == 0) {
            present_ps1_frame();
            boot_log("FRAME %lu pc=%08lx cycles=%llu\n", (unsigned long)s_frame_count,
                     (unsigned long)cpu.pc, (unsigned long long)psx_cycle_count);
            last_present = psx_cycle_count;
        }
    }

    boot_log("Guest execution stopped at PC=%08lx cycles=%llu\n",
             (unsigned long)cpu.pc, (unsigned long long)psx_cycle_count);
    psx_wii_video_shutdown();
    if (s_boot_log) fclose(s_boot_log);
    return 0;
}
