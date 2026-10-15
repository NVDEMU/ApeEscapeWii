#include <gccore.h>
#include <wiiuse/wpad.h>
#include <stdint.h>
#include <string.h>

#include "psx_wii_input.h"
#include "psx_wii_video.h"
#include "gpu_sw_renderer.h"

#define VRAM_WIDTH  1024
#define VRAM_HEIGHT 512
#define VIEW_WIDTH  320
#define VIEW_HEIGHT 240
#define FRAME_PIXELS (VIEW_WIDTH * VIEW_HEIGHT)

static uint16_t s_vram[VRAM_WIDTH * VRAM_HEIGHT];
static uint32_t s_frame[FRAME_PIXELS];
static int s_player_x = 64;
static int s_player_y = 174;
static int s_aim_x = 160;
static int s_aim_y = 120;

#define RGB555(r,g,b) ((uint16_t)(((r) & 31) | (((g) & 31) << 5) | (((b) & 31) << 10)))

static void draw_scene(const PsxWiiInput *input)
{
    int i;
    const uint16_t sky_top = RGB555(8, 14, 24);
    const uint16_t sky_mid = RGB555(14, 21, 30);
    const uint16_t grass = RGB555(5, 17, 8);
    const uint16_t grass_light = RGB555(10, 25, 11);
    const uint16_t dirt = RGB555(18, 12, 7);
    const uint16_t platform = RGB555(20, 20, 17);
    const uint16_t platform_edge = RGB555(28, 26, 17);
    const uint16_t player_blue = RGB555(7, 18, 31);
    const uint16_t player_skin = RGB555(28, 19, 12);
    const uint16_t white = RGB555(31, 31, 31);
    const uint16_t net_color = (input->buttons & PSX_WII_CROSS) ? RGB555(31, 8, 8) : RGB555(28, 28, 19);

    /* PS1 VRAM is the source of truth: all scene primitives use the existing rasterizer. */
    sw_fill_rect(0, 0, VIEW_WIDTH, VIEW_HEIGHT, sky_top);
    sw_fill_rect(0, 0, VIEW_WIDTH, 74, sky_mid);

    /* Stylized sun and distant polygon hills. */
    sw_fill_rect(245, 26, 18, 18, RGB555(31, 23, 11));
    sw_fill_rect(249, 22, 10, 26, RGB555(31, 23, 11));
    sw_draw_flat_triangle(-20, 132, 60, 70, 140, 132, RGB555(7, 12, 20));
    sw_draw_flat_triangle(70, 132, 170, 60, 260, 132, RGB555(9, 15, 22));
    sw_draw_flat_triangle(210, 132, 274, 83, 350, 132, RGB555(6, 12, 18));

    /* Ground, soil stripe and several platforms. */
    sw_fill_rect(0, 176, VIEW_WIDTH, 64, grass);
    sw_fill_rect(0, 201, VIEW_WIDTH, 39, dirt);
    sw_draw_line(0, 176, VIEW_WIDTH - 1, 176, grass_light);
    sw_draw_flat_rect(22, 153, 62, 8, platform);
    sw_draw_line(22, 153, 83, 153, platform_edge);
    sw_draw_flat_rect(111, 135, 56, 8, platform);
    sw_draw_line(111, 135, 166, 135, platform_edge);
    sw_draw_flat_rect(220, 151, 72, 8, platform);
    sw_draw_line(220, 151, 291, 151, platform_edge);

    /* A generic test avatar made from PS1 primitives (not original game art). */
    sw_fill_rect(s_player_x - 4, s_player_y + 12, 13, 5, RGB555(1, 2, 3));
    sw_draw_flat_rect(s_player_x, s_player_y, 8, 15, player_blue);
    sw_draw_flat_rect(s_player_x + 1, s_player_y - 6, 7, 7, player_skin);
    sw_fill_rect(s_player_x + 2, s_player_y - 4, 2, 2, white);
    sw_fill_rect(s_player_x + 6, s_player_y - 4, 2, 2, white);

    /* Pointer aim is represented as a net line and crosshair. */
    sw_draw_line(s_player_x + 4, s_player_y + 2, s_aim_x, s_aim_y, net_color);
    sw_draw_line(s_aim_x - 4, s_aim_y, s_aim_x + 4, s_aim_y, white);
    sw_draw_line(s_aim_x, s_aim_y - 4, s_aim_x, s_aim_y + 4, white);

    /* Color strip makes it easy to verify RGB555 -> ARGB conversion. */
    for (i = 0; i < 5; ++i) {
        static const uint16_t swatches[5] = {
            RGB555(31, 0, 0), RGB555(0, 31, 0), RGB555(0, 0, 31),
            RGB555(31, 31, 31), RGB555(15, 15, 15)
        };
        sw_fill_rect(12 + i * 17, 12, 13, 5, swatches[i]);
    }
}

int main(void)
{
    uint32_t frame_number = 0;

    memset(s_vram, 0, sizeof(s_vram));
    sw_renderer_init(s_vram);
    sw_renderer_set_scale(1);

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

        /* Nunchuk walks the test avatar; the Wii Remote moves aim over the viewport. */
        s_player_x += input.move_x / 42;
        s_player_y += input.move_y / 48;
        if (s_player_x < 8) s_player_x = 8;
        if (s_player_x > VIEW_WIDTH - 12) s_player_x = VIEW_WIDTH - 12;
        if (s_player_y < 32) s_player_y = 32;
        if (s_player_y > VIEW_HEIGHT - 24) s_player_y = VIEW_HEIGHT - 24;
        s_aim_x = VIEW_WIDTH / 2 + ((int)input.aim_x * (VIEW_WIDTH / 2 - 8)) / 127;
        s_aim_y = VIEW_HEIGHT / 2 + ((int)input.aim_y * (VIEW_HEIGHT / 2 - 8)) / 127;

        draw_scene(&input);
        sw_render_display(s_frame, VIEW_WIDTH * (int)sizeof(uint32_t),
                         0, 0, VIEW_WIDTH, VIEW_HEIGHT);
        if (psx_wii_video_present_argb(
                s_frame, VIEW_WIDTH, VIEW_HEIGHT,
                VIEW_WIDTH * (uint32_t)sizeof(uint32_t)) != 0) {
            break;
        }

        ++frame_number;
        (void)frame_number;
    }

    psx_wii_video_shutdown();
    return 0;
}
