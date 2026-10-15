#include <gccore.h>
#include <wiiuse/wpad.h>
#include <stdint.h>

#include "psx_wii_input.h"
#include "psx_wii_video.h"

#define FRAME_WIDTH  640u
#define FRAME_HEIGHT 480u
#define FRAME_PIXELS (FRAME_WIDTH * FRAME_HEIGHT)

static uint32_t s_frame[FRAME_PIXELS];

static void fill_rect(int x, int y, int w, int h, uint32_t color)
{
    int yy, xx;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)FRAME_WIDTH) w = (int)FRAME_WIDTH - x;
    if (y + h > (int)FRAME_HEIGHT) h = (int)FRAME_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    for (yy = y; yy < y + h; ++yy) {
        uint32_t *row = &s_frame[(uint32_t)yy * FRAME_WIDTH];
        for (xx = x; xx < x + w; ++xx) row[xx] = color;
    }
}

static void draw_axis(int y, s8 value, uint32_t color)
{
    const int left = 32;
    const int right = (int)FRAME_WIDTH - 32;
    const int width = right - left;
    const int center = left + width / 2;
    const int position = left + ((int)value + 127) * width / 254;

    fill_rect(left, y, width, 12, 0xff303744u);
    fill_rect(center, y - 3, 2, 18, 0xff858b98u);
    fill_rect(position - 5, y - 6, 10, 24, color);
}

static void draw_button(int x, int y, int w, u16 mask,
                        const PsxWiiInput *input, uint32_t color)
{
    const uint32_t inactive = 0xff303744u;
    fill_rect(x, y, w, 28, (input->buttons & mask) ? color : inactive);
}

static void draw_diagnostic(const PsxWiiInput *input)
{
    unsigned int i;
    static const struct {
        u16 mask;
        uint32_t color;
    } button_indicators[] = {
        { PSX_WII_CROSS,    0xff42c5f5u },
        { PSX_WII_CIRCLE,   0xfff45b69u },
        { PSX_WII_SQUARE,   0xffc18bffu },
        { PSX_WII_TRIANGLE, 0xff68df9bu },
        { PSX_WII_L1,       0xffffbd59u },
        { PSX_WII_R1,       0xffffe56bu },
        { PSX_WII_START,    0xff58d6c7u },
        { PSX_WII_SELECT,   0xff9ba7b4u }
    };

    for (i = 0; i < FRAME_PIXELS; ++i) s_frame[i] = 0xff111722u;

    /* Device-state blocks: green means Nunchuk, blue means valid IR data. */
    fill_rect(32, 24, 70, 20,
              input->has_nunchuk ? 0xff68df9bu : 0xff303744u);
    fill_rect(116, 24, 70, 20,
              input->has_ir ? 0xff42c5f5u : 0xff303744u);

    draw_axis(74, input->move_x, 0xff68df9bu);
    draw_axis(126, input->move_y, 0xff68df9bu);
    draw_axis(178, input->aim_x, 0xff42c5f5u);
    draw_axis(230, input->aim_y, 0xffc18bffu);

    for (i = 0; i < sizeof(button_indicators) / sizeof(button_indicators[0]); ++i) {
        draw_button(32 + (int)i * 72, 294, 56,
                    button_indicators[i].mask, input, button_indicators[i].color);
    }

    /* D-pad indicators. */
    draw_button(32, 350, 44, PSX_WII_UP, input, 0xfff0f3f8u);
    draw_button(84, 350, 44, PSX_WII_RIGHT, input, 0xfff0f3f8u);
    draw_button(136, 350, 44, PSX_WII_DOWN, input, 0xfff0f3f8u);
    draw_button(188, 350, 44, PSX_WII_LEFT, input, 0xfff0f3f8u);
}

int main(void)
{
    if (psx_wii_video_init() != 0) {
        return 1;
    }

    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_0, 640, 480);

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

        draw_diagnostic(&input);
        (void)psx_wii_video_present_argb(
            s_frame, FRAME_WIDTH, FRAME_HEIGHT,
            FRAME_WIDTH * (uint32_t)sizeof(uint32_t));
    }

    psx_wii_video_shutdown();
    return 0;
}
