#include "ape_wii_input.h"

#include <string.h>

#define APE_WII_IR_WIDTH  640.0f
#define APE_WII_IR_HEIGHT 480.0f
#define APE_WII_AXIS_MAX  127.0f
#define APE_WII_DEADZONE  9

static s8 scale_axis(int value, int minimum, int center, int maximum,
                     int invert)
{
    int span;
    int delta;
    int scaled;

    if (value >= center) {
        span = maximum - center;
        delta = value - center;
    } else {
        span = center - minimum;
        delta = value - center;
    }

    if (span <= 0) return 0;
    if (delta > 0) scaled = (delta * 127) / span;
    else scaled = (delta * 127) / span;

    if (scaled > 127) scaled = 127;
    if (scaled < -127) scaled = -127;
    if (scaled > -APE_WII_DEADZONE && scaled < APE_WII_DEADZONE) scaled = 0;
    if (invert) scaled = -scaled;
    return (s8)scaled;
}

static s8 scale_pointer(float coordinate, float extent)
{
    float normalized;
    int scaled;

    if (extent <= 0.0f) return 0;
    normalized = (coordinate - (extent * 0.5f)) / (extent * 0.5f);
    if (normalized > 1.0f) normalized = 1.0f;
    if (normalized < -1.0f) normalized = -1.0f;
    scaled = (int)(normalized * APE_WII_AXIS_MAX);
    if (scaled > -APE_WII_DEADZONE && scaled < APE_WII_DEADZONE) scaled = 0;
    return (s8)scaled;
}

void ape_wii_read_input(const WPADData *data, u32 held, ApeWiiInput *out)
{
    const struct ir_t *ir = NULL;

    if (!out) return;
    memset(out, 0, sizeof(*out));

    if (held & WPAD_BUTTON_MINUS) out->psx_buttons |= APE_PSX_SELECT;
    if (held & WPAD_BUTTON_PLUS) out->psx_buttons |= APE_PSX_START;
    if (held & WPAD_BUTTON_UP) out->psx_buttons |= APE_PSX_UP;
    if (held & WPAD_BUTTON_RIGHT) out->psx_buttons |= APE_PSX_RIGHT;
    if (held & WPAD_BUTTON_DOWN) out->psx_buttons |= APE_PSX_DOWN;
    if (held & WPAD_BUTTON_LEFT) out->psx_buttons |= APE_PSX_LEFT;

    /* Simple, editable defaults; tune after testing with Ape Escape gameplay. */
    if (held & WPAD_BUTTON_A) out->psx_buttons |= APE_PSX_CROSS;
    if (held & WPAD_BUTTON_B) out->psx_buttons |= APE_PSX_CIRCLE;
    if (held & WPAD_BUTTON_1) out->psx_buttons |= APE_PSX_SQUARE;
    if (held & WPAD_BUTTON_2) out->psx_buttons |= APE_PSX_TRIANGLE;
    if (held & WPAD_NUNCHUK_BUTTON_C) out->psx_buttons |= APE_PSX_L1;
    if (held & WPAD_NUNCHUK_BUTTON_Z) out->psx_buttons |= APE_PSX_R1;

    if (data && data->err == WPAD_ERR_NONE) {
        if ((data->data_present & WPAD_DATA_EXPANSION) &&
            data->exp.type == WPAD_EXP_NUNCHUK) {
            const joystick_t *js = &data->exp.nunchuk.js;
            out->has_nunchuk = true;
            out->move_x = scale_axis(js->pos.x, js->min.x, js->center.x,
                                     js->max.x, 0);
            /* The game-facing axes use +Y = down; the Nunchuk angle convention
             * uses +Y = up, so invert that axis. */
            out->move_y = scale_axis(js->pos.y, js->min.y, js->center.y,
                                     js->max.y, 1);
        }

        ir = &data->ir;
        if ((data->data_present & WPAD_DATA_IR) && ir->valid) {
            out->has_ir = true;
            out->aim_x = scale_pointer(ir->x, APE_WII_IR_WIDTH);
            out->aim_y = scale_pointer(ir->y, APE_WII_IR_HEIGHT);
        }
    }
}
