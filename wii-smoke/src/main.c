#include <gccore.h>
#include <wiiuse/wpad.h>
#include <stdio.h>

#include "ape_wii_input.h"

static void *xfb;
static GXRModeObj *rmode;

int main(void)
{
    unsigned int frame = 0;

    VIDEO_Init();
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_0, 640, 480);

    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
                 rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) {
        VIDEO_WaitVSync();
    }

    printf("ApeEscapeWii - controller bridge test\n");
    printf("-------------------------------------\n");
    printf("Nunchuk stick -> movement axes\n");
    printf("Wii Remote IR pointer -> aim axes\n");
    printf("A/B/1/2 -> Cross/Circle/Square/Triangle\n");
    printf("C/Z -> L1/R1; +/- -> Select/Start\n");
    printf("Connect the sensor bar and Nunchuk for full input.\n");
    printf("Press HOME to exit.\n");

    for (;;) {
        const WPADData *data;
        ApeWiiInput input;
        const u32 pressed;

        WPAD_ScanPads();
        data = WPAD_Data(WPAD_CHAN_0);
        pressed = WPAD_ButtonsDown(WPAD_CHAN_0);
        ape_wii_read_input(data, WPAD_ButtonsHeld(WPAD_CHAN_0), &input);

        if (pressed & WPAD_BUTTON_HOME) {
            break;
        }

        /* Refresh the console at 15 Hz to keep live axis feedback readable. */
        if ((frame++ % 4u) == 0u) {
            consoleClear();
            printf("ApeEscapeWii - controller bridge test\n");
            printf("-------------------------------------\n");
            printf("Nunchuk detected: %s\n", input.has_nunchuk ? "yes" : "no");
            printf("IR pointer:       %s\n", input.has_ir ? "yes" : "no");
            printf("Move X/Y:         %4d / %4d\n",
                   (int)input.move_x, (int)input.move_y);
            printf("Aim X/Y:          %4d / %4d\n",
                   (int)input.aim_x, (int)input.aim_y);
            printf("PS1 button mask:  0x%04X\n", (unsigned)input.psx_buttons);
            printf("\n");
            printf("Nunchuk stick -> movement axes\n");
            printf("Wii Remote IR pointer -> aim axes\n");
            printf("A/B/1/2 -> Cross/Circle/Square/Triangle\n");
            printf("C/Z -> L1/R1; +/- -> Select/Start\n");
            printf("Sensor bar needed for IR aiming. HOME exits.\n");
        }

        VIDEO_WaitVSync();
    }

    return 0;
}
