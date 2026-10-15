#include <gccore.h>
#include <wiiuse/wpad.h>
#include <stdint.h>
#include <string.h>

#include "psx_wii_input.h"
#include "psx_wii_video.h"
#include "psx_wii_gp0.h"
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

/* GP0 RGB888 command color packs R in bits 0-7, G in 8-15, B in 16-23. */
#define GP0_RGB(r,g,b) (((uint32_t)(r) & 255u) | (((uint32_t)(g) & 255u) << 8) | (((uint32_t)(b) & 255u) << 16))
#define GP0_XY(x,y) (((uint32_t)(x) & 0x7ffu) | (((uint32_t)(y) & 0x7ffu) << 16))

static void gp0_rect(uint32_t rgb, int x, int y, int w, int h)
{
    psx_wii_gp0_write(0x60000000u | (rgb & 0x00ffffffu));
    psx_wii_gp0_write(GP0_XY(x, y));
    psx_wii_gp0_write(((uint32_t)w & 0x3ffu) | (((uint32_t)h & 0x1ffu) << 16));
}

static void gp0_triangle(uint32_t rgb,
                         int x0, int y0, int x1, int y1, int x2, int y2)
{
    psx_wii_gp0_write(0x20000000u | (rgb & 0x00ffffffu));
    psx_wii_gp0_write(GP0_XY(x0, y0));
    psx_wii_gp0_write(GP0_XY(x1, y1));
    psx_wii_gp0_write(GP0_XY(x2, y2));
}

static void gp0_line(uint32_t rgb, int x0, int y0, int x1, int y1)
{
    psx_wii_gp0_write(0x40000000u | (rgb & 0x00ffffffu));
    psx_wii_gp0_write(GP0_XY(x0, y0));
    psx_wii_gp0_write(GP0_XY(x1, y1));
}

static void draw_scene(const PsxWiiInput *input)
{
    int i;
    const uint32_t sky_top = GP0_RGB(55, 90, 153);
    const uint32_t sky_mid = GP0_RGB(94, 135, 180);
    const uint32_t grass = GP0_RGB(36, 135, 61);
    const uint32_t grass_light = GP0_RGB(65, 167, 77);
    const uint32_t dirt = GP0_RGB(113, 76, 45);
    const uint32_t platform = GP0_RGB(132, 123, 96);
    const uint32_t platform_edge = GP0_RGB(195, 174, 121);
    const uint32_t player_blue = GP0_RGB(54, 94, 217);
    const uint32_t player_skin = GP0_RGB(214, 153, 111);
    const uint32_t white = GP0_RGB(255, 255, 255);
    const uint32_t net_color = (input->buttons & PSX_WII_CROSS)
        ? GP0_RGB(248, 69, 69) : GP0_RGB(242, 235, 206);

    /* Reset GP0 draw state then send GPU command words; no direct primitive calls here. */
    psx_wii_gp0_reset();
    gp0_rect(sky_top, 0, 0, VIEW_WIDTH, VIEW_HEIGHT);
    gp0_rect(sky_mid, 0, 0, VIEW_WIDTH, 74);

    /* Stylized sun and distant polygon hills, submitted as GP0 flat primitives. */
    gp0_rect(GP0_RGB(250, 207, 100), 245, 26, 18, 18);
    gp0_triangle(GP0_RGB(45, 75, 111), -20, 132, 60, 70, 140, 132);
    gp0_triangle(GP0_RGB(56, 91, 127), 70, 132, 170, 60, 260, 132);
    gp0_triangle(GP0_RGB(40, 68, 98), 210, 132, 274, 83, 350, 132);

    /* Ground, soil stripe, and floating platforms. */
    gp0_rect(grass, 0, 176, VIEW_WIDTH, 64);
    gp0_rect(dirt, 0, 201, VIEW_WIDTH, 39);
    gp0_line(grass_light, 0, 176, VIEW_WIDTH - 1, 176);
    gp0_rect(platform, 22, 153, 62, 8);
    gp0_line(platform_edge, 22, 153, 83, 153);
    gp0_rect(platform, 111, 135, 56, 8);
    gp0_line(platform_edge, 111, 135, 166, 135);
    gp0_rect(platform, 220, 151, 72, 8);
    gp0_line(platform_edge, 220, 151, 291, 151);

    /* Generic placeholder avatar, assembled through GP0 rectangle packets. */
    gp0_rect(GP0_RGB(14, 15, 20), s_player_x - 4, s_player_y + 12, 13, 5);
    gp0_rect(player_blue, s_player_x, s_player_y, 8, 15);
    gp0_rect(player_skin, s_player_x + 1, s_player_y - 6, 7, 7);
    gp0_rect(white, s_player_x + 2, s_player_y - 4, 2, 2);
    gp0_rect(white, s_player_x + 6, s_player_y - 4, 2, 2);

    /* A pointer-controlled net line and crosshair, also encoded as GP0 line packets. */
    gp0_line(net_color, s_player_x + 4, s_player_y + 2, s_aim_x, s_aim_y);
    gp0_line(white, s_aim_x - 4, s_aim_y, s_aim_x + 4, s_aim_y);
    gp0_line(white, s_aim_x, s_aim_y - 4, s_aim_x, s_aim_y + 4);

    /* RGB primary swatches exercise GP0 color-word conversion end to end. */
    for (i = 0; i < 5; ++i) {
        static const uint32_t swatches[5] = {
            GP0_RGB(255, 0, 0), GP0_RGB(0, 255, 0), GP0_RGB(0, 0, 255),
            GP0_RGB(255, 255, 255), GP0_RGB(128, 128, 128)
        };
        gp0_rect(swatches[i], 12 + i * 17, 12, 13, 5);
    }
}

int main(void)
{
    memset(s_vram, 0, sizeof(s_vram));
    psx_wii_gp0_init(s_vram);

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
    }

    psx_wii_video_shutdown();
    return 0;
}
