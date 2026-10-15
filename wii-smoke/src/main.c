#include <gccore.h>
#include <wiiuse/wpad.h>
#include <stdio.h>

static void *xfb;
static GXRModeObj *rmode;

int main(void)
{
    VIDEO_Init();
    WPAD_Init();

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

    printf("ApeEscapeWii - Wii DOL smoke test\n");
    printf("---------------------------------\n");
    printf("This is a toolchain test, not the game port yet.\n");
    printf("Press A or B to test Wii Remote input.\n");
    printf("Press HOME to exit.\n");

    for (;;) {
        WPAD_ScanPads();
        const u32 pressed = WPAD_ButtonsDown(0);

        if (pressed & WPAD_BUTTON_A) {
            printf("Wii Remote A detected.\n");
        }
        if (pressed & WPAD_BUTTON_B) {
            printf("Wii Remote B detected.\n");
        }
        if (pressed & WPAD_BUTTON_HOME) {
            break;
        }

        VIDEO_WaitVSync();
    }

    return 0;
}
