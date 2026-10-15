/*
 * Wii-only compatibility hooks for optional desktop/game-mod facilities.
 *
 * This file intentionally supplies only behavior that is safe for the current
 * native-width experimental Wii target:
 *   - no desktop debugging/tracing side effects;
 *   - no user mod plugins;
 *   - no widescreen expansion (the supported mode is native 4:3);
 *   - no native execution of mutable/self-modifying guest text until the Wii
 *     runtime has the matching dirty-code/reference-image guard.
 *
 * These are NOT replacements for the PS1 memory, GTE, interrupt, syscall, CD,
 * SPU, or GPU runtime. They allow those optional hooks to be isolated while the
 * actual hardware services are ported separately.
 */
#include <stdint.h>
#include "cpu_state.h"

/* Optional diagnostics: the Wii build has no desktop debug server. */
uint32_t g_debug_last_store_pc = 0;

void debug_server_log_call_entry(uint32_t func_addr)
{
    (void)func_addr;
}

void debug_server_cyc_observe(uint32_t block_leader_phys)
{
    (void)block_leader_phys;
}

/* No desktop CPS/aspect-ratio override is enabled in the native Wii build. */
int g_psx_cps_mode = 0;

/* Mods are opt-in desktop functionality and are disabled on Wii for now. */
int psx_mod_function_entry(CPUState *cpu, uint32_t address)
{
    (void)cpu;
    (void)address;
    return 0;
}

void psx_mod_instruction(CPUState *cpu, uint32_t address, uint32_t instruction)
{
    (void)cpu;
    (void)address;
    (void)instruction;
}

/* Never execute guest text through an unvalidated native fast path. */
int dirty_ram_text_native_ok_ranges(const uint32_t *lo_len_pairs, uint32_t count)
{
    (void)lo_len_pairs;
    (void)count;
    return 0;
}

int dirty_ram_text_native_ok_ranges_from(const uint32_t *lo_len_pairs,
                                         uint32_t count,
                                         uint32_t exec_pc)
{
    (void)lo_len_pairs;
    (void)count;
    (void)exec_pc;
    return 0;
}

/* At margin zero, these are the framework's vanilla 4:3 cull comparisons. */
int psx_ws_x_margin(void)
{
    return 0;
}

int psx_ws_cull_sltiu(uint32_t sx, uint32_t imm)
{
    return (uint32_t)((int32_t)(int16_t)(uint16_t)sx) < imm;
}

int psx_ws_cull_slti(uint32_t sx, uint32_t imm)
{
    return (int32_t)sx < (int32_t)imm;
}

int psx_ws_cull_slti_lower(uint32_t sx, uint32_t imm)
{
    return (int32_t)sx < (int32_t)(int16_t)(uint16_t)imm;
}

int psx_ws_cull_bltz(uint32_t value)
{
    return (int32_t)value < 0;
}

uint32_t psx_ws_cull_keep_result(uint32_t vanilla, uint32_t forced)
{
    (void)forced;
    return vanilla;
}
