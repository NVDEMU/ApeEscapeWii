/*
 * Wii-only fallback for renderer features that are owned by the desktop
 * widescreen/backdrop runtime. Native-width Wii bring-up keeps that feature
 * disabled; these definitions satisfy the reusable rasterizer without linking
 * SDL/desktop settings code. Replace with real runtime state if Wii-wide mode
 * is implemented.
 */
int g_ws_bd_stretch_on = 0;
int g_ws_bd_stretch_pct = 0;

int psx_ws_prim_in_backdrop(void)
{
    return 0;
}
