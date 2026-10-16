/* renders sample frames to PNG for visual checks: shot_tool <outdir> */
#include "../src/gfx.h"
#include <stdio.h>
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char p[512];
    Game g; Controls c = {0};
    gfx_init();
    game_init(&g, 7);
    for (int f = 0; f < 900; f++) {
        game_ai(&g, &c);
        game_step(&g, &c);
        for (int i = 0; i < g.nev; i++) gfx_fx_event(&g.ev[i], &g);
        gfx_fx_step(&g);
        if (f == 120 || f == 500 || f == 880) {
            gfx_draw_game(&g, f);
            snprintf(p, sizeof p, "%s/game_%d.png", dir, f);
            gfx_save_png(p, g_fb);
        }
    }
    gfx_draw_space_bg(0, true, 0);
    gfx_copper_bars(6, 90, 40);
    gfx_draw_logo(14, 30);
    static const char *items[] = { "START GAME", "HOW TO PLAY", "HIGH SCORES", "SETTINGS", "CONTROLS", "CREDITS", "QUIT" };
    for (int i = 0; i < 7; i++) gfx_menu_item(104 + i * 11, items[i], i == 1, 30);
    gfx_sine_scroller(190, "SHOOTER UPPER ... EVERY SOUND IS MATH ... GREETINGS TO EVERYONE WHO WAITED FOR A TAPE TO LOAD ...   ", 120);
    gfx_title(4, "WAVE 12", RAMP_RED);
    snprintf(p, sizeof p, "%s/title.png", dir); gfx_save_png(p, g_fb);
    static uint32_t crt[SCREEN_W*SCREEN_H];
    gfx_crt(crt);
    snprintf(p, sizeof p, "%s/title_crt.png", dir); gfx_save_png(p, crt);
    return 0;
}
