#ifndef APE_WII_INPUT_H
#define APE_WII_INPUT_H

#include <gccore.h>
#include <wiiuse/wpad.h>

/*
 * Experimental input adapter for the native port.
 *
 * Axes are signed -127..127. Positive X means right; positive Y means down.
 * Aim is a screen-center-relative first pass using the Wii Remote IR pointer.
 * The button mask uses conventional PS1 DualShock digital-button bit positions.
 */
typedef struct ApeWiiInput {
    s8 move_x;
    s8 move_y;
    s8 aim_x;
    s8 aim_y;
    u16 psx_buttons;
    bool has_nunchuk;
    bool has_ir;
} ApeWiiInput;

enum {
    APE_PSX_SELECT   = 0x0001,
    APE_PSX_START    = 0x0008,
    APE_PSX_UP       = 0x0010,
    APE_PSX_RIGHT    = 0x0020,
    APE_PSX_DOWN     = 0x0040,
    APE_PSX_LEFT     = 0x0080,
    APE_PSX_L1       = 0x0400,
    APE_PSX_R1       = 0x0800,
    APE_PSX_TRIANGLE = 0x1000,
    APE_PSX_CIRCLE   = 0x2000,
    APE_PSX_CROSS    = 0x4000,
    APE_PSX_SQUARE   = 0x8000
};

/* Convert a sampled Wii Remote/Nunchuk state into a portable pad snapshot. */
void ape_wii_read_input(const WPADData *data, u32 buttons_held,
                        ApeWiiInput *out);

#endif
