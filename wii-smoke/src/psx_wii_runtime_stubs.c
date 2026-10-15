/*
 * Wii-only implementation of optional desktop/debug hooks.
 *
 * The PS1 hardware paths (memory, GPU, GTE, interrupts, CD-ROM, SIO, timers,
 * DMA, MDEC and SPU) are supplied by the real PSXRecomp runtime sources.
 * This file only turns off desktop observability, mod/plugin, netplay and
 * OpenGL-only enhancement hooks for the initial native-width Wii boot.
 */
#include <gccore.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "cpu_state.h"
#include "debug_server.h"
#include "event_ring.h"
#include "starvation_ring.h"
#include "audio_trace.h"
#include "card_read_summary.h"
#include "card_data_writes.h"
#include "spu_shadow.h"
#include "psx_netplay.h"
#include "nd_intro_ot.h"
#include "gpu_hd_textures.h"
#include "gpu_gl_renderer.h"
#include "mod_runtime.h"
#include "mod_texture_banks.h"
#include "text_xlate.h"
#include "ws_ui_group.h"
#include "render_pass.h"
#include "render_pass_plan.h"
#include "overlay_capture.h"
#include "overlay_loader.h"
#include "data_shards.h"
#include "parity_trace.h"
#include "crash_trace.h"

/* Desktop debugger state: inactive, but stable for hot-path read sites. */
uint32_t g_debug_current_func_addr = 0;
CPUState *debug_cpu_ptr = NULL;
int g_ram_read_watch_active = 0;
volatile int g_ds_recording = 0;
uint32_t g_call_unit_depth = 0;
void (*g_overlay_flush_pending_cycles)(void) = NULL;

/* There is no remote debugger on the Wii target. */
void debug_server_poll(void) {}
int debug_server_fmv_quiet(void) { return 0; }
int debug_server_is_connected(void) { return 0; }
void debug_server_trace_write_check(uint32_t phys, uint32_t old_val,
                                    uint32_t new_val, uint8_t width)
{ (void)phys; (void)old_val; (void)new_val; (void)width; }
void debug_server_trace_mmio_write(uint32_t addr, uint32_t val, uint8_t width)
{ (void)addr; (void)val; (void)width; }
void debug_server_trace_mmio_read(uint32_t addr, uint32_t val, uint8_t width)
{ (void)addr; (void)val; (void)width; }
void debug_server_trace_ram_read_watch(uint32_t phys, uint32_t val)
{ (void)phys; (void)val; }
void debug_server_log_sio_write(uint32_t addr, uint32_t value, uint8_t width)
{ (void)addr; (void)value; (void)width; }
void debug_server_log_restore_event(uint32_t kind, uint32_t target_pc, uint32_t jmp_val)
{ (void)kind; (void)target_pc; (void)jmp_val; }
void debug_server_log_thread_event(uint32_t kind, CPUState *cpu,
                                   uint32_t current_tcb, uint32_t target_tcb,
                                   uint32_t target_pc)
{ (void)kind; (void)cpu; (void)current_tcb; (void)target_tcb; (void)target_pc; }
void debug_server_log_thread_event_ex(uint32_t kind, CPUState *cpu,
                                      uint32_t current_tcb, uint32_t target_tcb,
                                      uint32_t target_pc, uintptr_t current_fiber,
                                      uintptr_t target_fiber)
{ (void)kind; (void)cpu; (void)current_tcb; (void)target_tcb; (void)target_pc;
  (void)current_fiber; (void)target_fiber; }
void debug_server_trace_dispatch(uint32_t func_addr) { (void)func_addr; }
void debug_server_trace_dispatch_return(uint32_t func_addr, CPUState *cpu)
{ (void)func_addr; (void)cpu; }
void debug_server_log_call_entry_cpu(uint32_t func_addr, CPUState *cpu)
{ (void)func_addr; (void)cpu; }
void debug_server_log_call_entry_fn(uint32_t func_addr) { (void)func_addr; }
void debug_server_log_probe(uint32_t pc, CPUState *cpu) { (void)pc; (void)cpu; }
int debug_server_dirty_break_maybe_pause(uint32_t target, CPUState *cpu)
{ (void)target; (void)cpu; return 0; }

/* Observability rings do not affect guest-visible device state. */
void event_ring_record_aux(uint16_t kind, uint8_t detail, uint32_t aux)
{ (void)kind; (void)detail; (void)aux; }
void event_ring_record(uint16_t kind, uint8_t detail)
{ (void)kind; (void)detail; }
void starvation_ring_record(uint8_t kind, uint8_t tx, uint8_t rx,
                            uint16_t ctrl, uint16_t stat,
                            int shift_active, int shift_remaining,
                            int tx_buffered, int pending_ack,
                            int ack_remaining,
                            uint8_t bus_owner, uint32_t bus_byte_index,
                            uint8_t active_device, uint8_t mc_state,
                            uint8_t pad_state, uint8_t selected_slot,
                            int g_sio_timing_active)
{
    (void)kind; (void)tx; (void)rx; (void)ctrl; (void)stat;
    (void)shift_active; (void)shift_remaining; (void)tx_buffered;
    (void)pending_ack; (void)ack_remaining; (void)bus_owner;
    (void)bus_byte_index; (void)active_device; (void)mc_state;
    (void)pad_state; (void)selected_slot; (void)g_sio_timing_active;
}
void starvation_watchdog_heartbeat(void) {}
void starvation_watchdog_check(void) {}
void starvation_ring_pc_sample(void) {}
void audio_trace_pcm(int tap, const int16_t *stereo, int frames)
{ (void)tap; (void)stereo; (void)frames; }
void audio_trace_event(uint16_t kind, uint32_t a, uint32_t b)
{ (void)kind; (void)a; (void)b; }
void card_read_summary_record(uint8_t slot, uint8_t cmd, uint16_t sector,
                              uint8_t checksum, uint8_t data_idx,
                              const uint8_t *data128)
{ (void)slot; (void)cmd; (void)sector; (void)checksum; (void)data_idx; (void)data128; }
void card_data_writes_arm(uint8_t value, uint16_t mc_state,
                          uint8_t mc_data_idx, uint8_t slot)
{ (void)value; (void)mc_state; (void)mc_data_idx; (void)slot; }

/* Built-in enhancement/shadow/render-pass features stay disabled. */
bool spu_shadow_enabled(void) { return false; }
void spu_shadow_reset(void) {}
void spu_shadow_process(int16_t *canon, int frames) { (void)canon; (void)frames; }
int psx_netplay_active(void) { return 0; }
uint32_t psx_netplay_sim_tick(void) { return 0; }
void psx_netplay_poll_snap(CPUState *cpu, uint32_t resume_pc)
{ (void)cpu; (void)resume_pc; }
int psx_netplay_is_resimulating(void) { return 0; }
int psx_netplay_cd_bisect_active(void) { return 0; }
uint32_t psx_netplay_rb_sticky_bb_pc(void) { return 0; }
int psx_selfcheck_enabled(void) { return 0; }
void psx_selfcheck_poll(void) {}
void psx_rewind_poll(void) {}
void savestate_poll(void) {}
void device_trace_note(uint32_t kind, ...) { (void)kind; }

void psx_nd_note_wood_batch_ot(uint32_t base) { (void)base; }
void psx_nd_note_wood_batch_ot_tagged(uint32_t base, uint32_t tag)
{ (void)base; (void)tag; }
uint32_t psx_nd_wood_batch_ot(void) { return 0; }
void psx_nd_note_sibling_ot_hint(uint32_t base) { (void)base; }
uint32_t psx_nd_sibling_ot_hint(void) { return 0; }

int gpu_hd_textures_dump_enabled(void) { return 0; }
int gpu_hd_textures_active(void) { return 0; }
int gpu_hd_textures_replacements_enabled(void) { return 0; }
void gpu_hd_textures_observe_draw(uint16_t texpage, uint16_t clut_x,
                                  uint16_t clut_y, const int limits[4],
                                  uint32_t texture_window, int semitransparent)
{ (void)texpage; (void)clut_x; (void)clut_y; (void)limits;
  (void)texture_window; (void)semitransparent; }
void gpu_hd_textures_set_vram(const uint16_t *vram) { (void)vram; }
void gpu_hd_textures_bind_vram(const uint16_t *vram) { (void)vram; }
void gpu_hd_textures_suppress_dumps(int on) { (void)on; }
void gpu_hd_textures_track_upload(int x, int y, int width_words, int height,
                                  const uint16_t *words)
{ (void)x; (void)y; (void)width_words; (void)height; (void)words; }
void gpu_hd_textures_begin_upload(int x, int y, int width_words, int height)
{ (void)x; (void)y; (void)width_words; (void)height; }
void gpu_hd_textures_begin_copy(int sx, int sy, int dx, int dy,
                                int width_words, int height)
{ (void)sx; (void)sy; (void)dx; (void)dy; (void)width_words; (void)height; }
void gpu_hd_textures_end_copy(void) {}
void gpu_hd_textures_invalidate(int x, int y, int width_words, int height)
{ (void)x; (void)y; (void)width_words; (void)height; }
void gpu_hd_textures_note_applied(void) {}
void gpu_hd_textures_note_gl_cache(int eviction) { (void)eviction; }

int gl_renderer_select_texture_bank(uint16_t id) { (void)id; return 0; }
int gl_renderer_select_texture_bank_live_clut(uint16_t id) { (void)id; return 0; }
int gl_renderer_texture_banks_supported(void) { return 0; }
int gl_renderer_pgxp_render_wanted(void) { return 0; }
int gl_renderer_frame_generation(void) { return 0; }
int gl_renderer_frame_generation_method(void) { return 0; }
void gl_renderer_fg_source(const uint32_t id[3], const int32_t pc[9],
                           const int32_t h[3], const int32_t x[3],
                           const int32_t y[3])
{ (void)id; (void)pc; (void)h; (void)x; (void)y; }
void gl_renderer_note_wide_triangle_recovery(int enabled) { (void)enabled; }
int gl_renderer_projective_supported(void) { return 0; }
void gl_renderer_draw_projected_triangle(const PSXProjectedVertex vertices[3],
    uint16_t texpage, uint16_t clut_x, uint16_t clut_y, int raw, int semi,
    int perspective)
{ (void)vertices; (void)texpage; (void)clut_x; (void)clut_y; (void)raw;
  (void)semi; (void)perspective; }

uint16_t mod_texture_packet_bank(uint32_t source, const uint32_t *words, uint32_t count)
{ (void)source; (void)words; (void)count; return 0; }
int mod_texture_packet_precision(uint32_t source, float q[3], float xy[6])
{ (void)source; (void)q; (void)xy; return 0; }
void ws_ui_group_assign(WsUiGroupItem *items, size_t count,
                        int32_t display_width, int dense_menu, int in_place)
{ (void)items; (void)count; (void)display_width; (void)dense_menu; (void)in_place; }
void text_xlate_vram_upload(int x, int y, int w, int h)
{ (void)x; (void)y; (void)w; (void)h; }

/* Mod/plugin integration is deliberately offline on the Wii build. */
void mod_runtime_on_dispatch(uint32_t target) { (void)target; }
void mod_runtime_on_vblank(void) {}
void mod_runtime_on_savestate_loaded(void) {}
void mod_runtime_activate_plugins(void) {}
void mod_runtime_function_entry_context_save(ModFunctionEntryContext *out)
{ if (out) memset(out, 0, sizeof(*out)); }
void mod_runtime_function_entry_context_restore(const ModFunctionEntryContext *in)
{ (void)in; }
void mod_runtime_patch_disc_sector(uint32_t lba, int raw, uint8_t *bytes, uint32_t size)
{ (void)lba; (void)raw; (void)bytes; (void)size; }
int mod_runtime_read_disc_extent(uint32_t lba, int raw, uint8_t *bytes, uint32_t size)
{ (void)lba; (void)raw; (void)bytes; (void)size; return 0; }
uint32_t mod_runtime_disc_extent_start(void) { return 0; }
uint32_t mod_runtime_disc_sector_count(uint32_t base_count) { return base_count; }
int mod_runtime_cdda_track_count(void) { return 0; }
uint32_t mod_runtime_cdda_track_start(int track) { (void)track; return 0; }

void overlay_loader_note_code_write(void) {}
void overlay_loader_active_write_check(uint32_t phys, uint32_t size)
{ (void)phys; (void)size; }
void overlay_loader_shadow_escape_fixup(uint64_t target_epoch) { (void)target_epoch; }
uint32_t overlay_loader_get_inprogress(void) { return 0; }
void overlay_capture_before_dma(uint32_t load_addr, uint32_t size)
{ (void)load_addr; (void)size; }
void overlay_capture_on_dma(uint32_t load_addr, uint32_t size, const uint8_t *bytes)
{ (void)load_addr; (void)size; (void)bytes; }
int overlay_loader_active_write_check_dummy(void) { return 0; }

void ds_note_read(uint32_t addr, uint32_t size) { (void)addr; (void)size; }
void ds_note_write(uint32_t addr, uint32_t size) { (void)addr; (void)size; }
void ds_note_mmio(uint32_t addr, int is_read) { (void)addr; (void)is_read; }
void ds_note_dma_write(void) {}
void ds_note_thread_switch(uint32_t from_tcb, uint32_t to_tcb)
{ (void)from_tcb; (void)to_tcb; }

void parity_trace_note_write(uint32_t addr, uint32_t width, uint32_t writer_pc)
{ (void)addr; (void)width; (void)writer_pc; }
void parity_trace_note_read(uint32_t addr, uint32_t value, uint32_t reader_pc)
{ (void)addr; (void)value; (void)reader_pc; }
int parity_trace_is_armed(void) { return 0; }
int parity_trace_is_frozen(void) { return 0; }
void parity_trace_record(parity_kind_t kind, uint32_t pc, uint32_t ra,
                         uint32_t sp, uint32_t target,
                         parity_read_word_fn read_word, void *ctx)
{ (void)kind; (void)pc; (void)ra; (void)sp; (void)target; (void)read_word; (void)ctx; }
void psx_crash_trace_dump(const char *reason, void *seh_info)
{ (void)reason; (void)seh_info; }
void psx_unknown_dispatch_record(uint32_t addr, uint32_t phys,
                                 uint32_t ra, uint32_t a0, uint32_t a1)
{ (void)addr; (void)phys; (void)ra; (void)a0; (void)a1; }
uint64_t crash_trace_dispatch_seq_get(void) { return 0; }
uint32_t crash_trace_dispatch_ring_get(int idx) { (void)idx; return 0; }

int render_pass_mmio_read_allowed(uint32_t phys, uint32_t width)
{ (void)phys; (void)width; return 1; }
int render_pass_store_to(const RenderPassStoreTarget *t, uint32_t addr,
                         uint32_t val, uint32_t width)
{ (void)t; (void)addr; (void)val; (void)width; return -1; }

/* CD-ROM and save-state support use a wall-clock sample only for non-guest
 * diagnostics/pacing. Libogc timer ticks provide a monotonic implementation. */
int timespec_get(struct timespec *ts, int base)
{
    if (!ts || base != TIME_UTC) return 0;
    uint64_t ticks = gettime();
    ts->tv_sec = (time_t)(ticks / TB_TIMER_CLOCK);
    ts->tv_nsec = (long)(((ticks % TB_TIMER_CLOCK) * 1000000000ull) / TB_TIMER_CLOCK);
    return base;
}
