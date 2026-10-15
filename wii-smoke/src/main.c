#include <gccore.h>
#include <wiiuse/wpad.h>
#include <stdio.h>

#include "psx_wii_input.h"

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

    printf("ApeEscapeWii - framework controller bridge test\n");
    printf("------------------------------------------------\n");
    printf("Nunchuk stick -> movement axes\n");
    printf("Wii Remote IR pointer -> aim axes\n");
    printf("A/B/1/2 -> Cross/Circle/Square/Triangle\n");
    printf("C/Z -> L1/R1; +/- -> Select/Start\n");
    printf("HOME exits; sensor bar needed for IR aiming.\n");

    for (;;) {
        const WPADData *data;
        PsxWiiInput input;
        u32 pressed;

        WPAD_ScanPads();
        data = WPAD_Data(WPAD_CHAN_0);
        pressed = WPAD_ButtonsDown(WPAD_CHAN_0);
        psx_wii_read_input(data, WPAD_ButtonsHeld(WPAD_CHAN_0), &input);

        if (pressed & WPAD_BUTTON_HOME) {
            break;
        }

        if ((frame++ % 4u) == 0u) {
            consoleClear();
            printf("ApeEscapeWii - framework controller bridge\n");
            printf("-------------------------------------------\n");
            printf("Nunchuk: %s  IR: %s\n",
                   input.has_nunchuk ? "yes" : "no",
                   input.has_ir ? "yes" : "no");
            printf("Move X/Y: %4d / %4d\n",
                   (int)input.move_x, (int)input.move_y);
            printf("Aim X/Y:  %4d / %4d\n",
                   (int)input.aim_x, (int)input.aim_y);
            printf("Buttons:  0x%04X\n", (unsigned)input.buttons);
            printf("\nMove the Nunchuk stick and point the remote.\n");
            printf("This is input diagnostics, not the game. HOME exits.\n");
        }
        VIDEO_WaitVSync();
    }

    return 0;
}
