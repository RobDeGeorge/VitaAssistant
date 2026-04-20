#include <vita2d.h>
#include <psp2/touch.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "ui.h"
#include "config.h"

static vita2d_pgf *font = NULL;

int ui_init(void) {
    vita2d_init();
    vita2d_set_clear_color(COLOR_BG);

    font = vita2d_load_default_pgf();
    if (!font) return -1;

    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    return 0;
}

void ui_cleanup(void) {
    if (font) vita2d_free_pgf(font);
    vita2d_fini();
}

static void draw_header(const char *status_msg) {
    vita2d_draw_rectangle(0, 0, SCREEN_W, HEADER_H, COLOR_HEADER);
    vita2d_pgf_draw_text(font, 20, 33, COLOR_ACCENT, 1.2f, "VitaAssistant");

    if (status_msg && status_msg[0]) {
        vita2d_pgf_draw_text(font, SCREEN_W - 350, 33, COLOR_TEXT_DIM, 0.8f, status_msg);
    }
}

static const char *tab_names[TAB_COUNT] = { "Home", "Lights", "Climate", "Scenes", "Devices" };

static void draw_tab_bar(int current_tab) {
    int tab_y = SCREEN_H - TAB_BAR_H;
    vita2d_draw_rectangle(0, tab_y, SCREEN_W, TAB_BAR_H, COLOR_TAB_BG);

    int tab_w = SCREEN_W / TAB_COUNT;
    for (int i = 0; i < TAB_COUNT; i++) {
        int tx = i * tab_w;
        unsigned int color = (i == current_tab) ? COLOR_TAB_SEL : COLOR_TEXT_DIM;

        if (i == current_tab) {
            vita2d_draw_rectangle(tx + 10, tab_y + 2, tab_w - 20, 3, COLOR_TAB_SEL);
        }

        int text_w = vita2d_pgf_text_width(font, 1.0f, tab_names[i]);
        vita2d_pgf_draw_text(font, tx + (tab_w - text_w) / 2, tab_y + 38, color, 1.0f, tab_names[i]);
    }
}

static void draw_toggle(int x, int y, int on, int selected) {
    unsigned int bg = on ? COLOR_ON : COLOR_OFF;
    if (selected) {
        vita2d_draw_rectangle(x - 2, y - 2, 54, 30, COLOR_ACCENT);
    }
    vita2d_draw_rectangle(x, y, 50, 26, bg);
    int knob_x = on ? x + 26 : x + 2;
    vita2d_draw_rectangle(knob_x, y + 2, 22, 22, COLOR_TEXT);
}

static void draw_slider_bar(int x, int y, int w, int value, int max_val, unsigned int fill_color, int selected) {
    if (selected) {
        vita2d_draw_rectangle(x - 2, y - 2, w + 4, 16, COLOR_ACCENT);
    }
    vita2d_draw_rectangle(x, y, w, 12, COLOR_OFF);
    int fill_w = (value * w) / max_val;
    if (fill_w < 0) fill_w = 0;
    if (fill_w > w) fill_w = w;
    vita2d_draw_rectangle(x, y, fill_w, 12, fill_color);
    vita2d_draw_rectangle(x + fill_w - 3, y - 2, 6, 16, COLOR_TEXT);
}

/* Convert HSV to RGBA8 for color preview */
static unsigned int hsv_to_rgba(int hue, int sat, int val) {
    float h = hue / 60.0f;
    float s = sat / 100.0f;
    float v = val / 255.0f;
    int hi = (int)h % 6;
    float f = h - (int)h;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    float r, g, b;
    switch (hi) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return RGBA8((int)(r * 255), (int)(g * 255), (int)(b * 255), 255);
}

static void draw_entity_card(ha_entity_t *entity, int x, int y, int w, int is_cursor, int sub_select) {
    int is_on = (strcmp(entity->state, "on") == 0);
    unsigned int bg = is_cursor ? COLOR_CARD_SEL : COLOR_CARD_BG;

    vita2d_draw_rectangle(x, y, w, CARD_H, bg);

    if (is_cursor) {
        vita2d_draw_rectangle(x, y, 4, CARD_H, COLOR_ACCENT);
    }

    if (strcmp(entity->domain, "light") == 0) {
        vita2d_pgf_draw_text(font, x + 15, y + 22, COLOR_TEXT, 0.9f, entity->friendly_name);

        unsigned int state_color = is_on ? COLOR_ON : COLOR_TEXT_DIM;
        char state_str[64];
        if (is_on && entity->brightness > 0) {
            snprintf(state_str, sizeof(state_str), "%s (%d%%)", entity->state, (entity->brightness * 100) / 255);
        } else {
            strncpy(state_str, entity->state, sizeof(state_str) - 1);
            state_str[sizeof(state_str) - 1] = '\0';
        }
        vita2d_pgf_draw_text(font, x + 15, y + 42, state_color, 0.75f, state_str);

        /* RGB color swatch + triangle hint */
        if (entity->has_rgb && is_on) {
            unsigned int swatch;
            if (entity->rgb_r || entity->rgb_g || entity->rgb_b) {
                swatch = RGBA8(entity->rgb_r, entity->rgb_g, entity->rgb_b, 255);
            } else {
                swatch = hsv_to_rgba(entity->hs_hue, entity->hs_sat, entity->brightness > 0 ? entity->brightness : 255);
            }
            vita2d_draw_rectangle(x + w - 140, y + 10, 24, 24, swatch);
            /* Border around swatch */
            vita2d_draw_rectangle(x + w - 141, y + 9, 26, 1, COLOR_TEXT_DIM);
            vita2d_draw_rectangle(x + w - 141, y + 34, 26, 1, COLOR_TEXT_DIM);
            vita2d_draw_rectangle(x + w - 141, y + 9, 1, 26, COLOR_TEXT_DIM);
            vita2d_draw_rectangle(x + w - 116, y + 9, 1, 26, COLOR_TEXT_DIM);

            if (is_cursor) {
                vita2d_pgf_draw_text(font, x + w - 140, y + 55, COLOR_TEXT_DIM, 0.6f, "\xe2\x96\xb3 Color");
            }
        }

        /* Toggle on the right */
        draw_toggle(x + w - 70, y + 19, is_on, is_cursor && sub_select == 1);

        /* Brightness bar */
        if (is_on && entity->brightness > 0) {
            draw_slider_bar(x + 15, y + 50, w / 2 - 40, entity->brightness, 255, COLOR_WARM, is_cursor && sub_select == 2);
        }
    } else if (strcmp(entity->domain, "climate") == 0) {
        vita2d_pgf_draw_text(font, x + 15, y + 22, COLOR_TEXT, 0.9f, entity->friendly_name);

        char info[64];
        snprintf(info, sizeof(info), "Now: %.0f\xc2\xb0 F  |  %s", entity->temperature, entity->hvac_mode);
        vita2d_pgf_draw_text(font, x + 15, y + 42, COLOR_TEXT_DIM, 0.75f, info);

        /* Target temp with +/- buttons */
        char target[32];
        snprintf(target, sizeof(target), "Target: %.0f\xc2\xb0", entity->target_temp);
        unsigned int tc = is_cursor ? COLOR_ACCENT : COLOR_WARM;
        vita2d_pgf_draw_text(font, x + w - 220, y + 22, tc, 0.9f, target);

        if (is_cursor) {
            /* Draw -/+ indicators */
            vita2d_draw_rectangle(x + w - 80, y + 10, 30, 25, COLOR_RED);
            vita2d_pgf_draw_text(font, x + w - 73, y + 28, COLOR_TEXT, 0.9f, "-");
            vita2d_draw_rectangle(x + w - 42, y + 10, 30, 25, COLOR_ON);
            vita2d_pgf_draw_text(font, x + w - 35, y + 28, COLOR_TEXT, 0.9f, "+");
            vita2d_pgf_draw_text(font, x + w - 220, y + 48, COLOR_TEXT_DIM, 0.65f, "Left/Right to adjust");
        }
    } else if (strcmp(entity->domain, "scene") == 0) {
        vita2d_pgf_draw_text(font, x + 15, y + 22, COLOR_TEXT, 0.9f, entity->friendly_name);
        vita2d_pgf_draw_text(font, x + 15, y + 42, COLOR_TEXT_DIM, 0.75f, "Scene");

        unsigned int btn_color = is_cursor ? COLOR_ACCENT : COLOR_OFF;
        vita2d_draw_rectangle(x + w - 110, y + 15, 90, 35, btn_color);
        vita2d_pgf_draw_text(font, x + w - 95, y + 40, COLOR_TEXT, 0.85f, "Activate");
    } else if (strcmp(entity->domain, "counter") == 0 || strcmp(entity->domain, "sensor") == 0) {
        vita2d_pgf_draw_text(font, x + 15, y + 22, COLOR_TEXT, 0.9f, entity->friendly_name);
        vita2d_pgf_draw_text(font, x + 15, y + 42, COLOR_WARM, 0.85f, entity->state);
    } else {
        vita2d_pgf_draw_text(font, x + 15, y + 22, COLOR_TEXT, 0.9f, entity->friendly_name);
        unsigned int state_color = is_on ? COLOR_ON : COLOR_TEXT_DIM;
        vita2d_pgf_draw_text(font, x + 15, y + 42, state_color, 0.75f, entity->state);
        draw_toggle(x + w - 70, y + 19, is_on, is_cursor && sub_select == 1);
    }
}

int ui_card_y(int index, int scroll_offset) {
    return CONTENT_Y + (index * CARD_FULL_H) - scroll_offset;
}

int ui_clamp_scroll(int cursor_idx, int count, int scroll_offset) {
    if (count <= 0) return 0;
    int card_y = CONTENT_Y + cursor_idx * CARD_FULL_H - scroll_offset;

    if (card_y < CONTENT_Y) {
        scroll_offset -= (CONTENT_Y - card_y);
    }
    if (card_y + CARD_H > CONTENT_BOTTOM) {
        scroll_offset += (card_y + CARD_H - CONTENT_BOTTOM);
    }
    if (scroll_offset < 0) scroll_offset = 0;

    int total_h = count * CARD_FULL_H;
    int max_scroll = total_h - CONTENT_H;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_offset > max_scroll) scroll_offset = max_scroll;

    return scroll_offset;
}

void ui_draw_color_overlay(ha_entity_t *entity, int overlay_sel) {
    /* Darken background */
    vita2d_draw_rectangle(0, 0, SCREEN_W, SCREEN_H, RGBA8(0, 0, 0, 180));

    /* Overlay panel */
    int ox = 80, oy = 60, ow = SCREEN_W - 160, oh = SCREEN_H - 120;
    vita2d_draw_rectangle(ox, oy, ow, oh, COLOR_CARD_BG);
    vita2d_draw_rectangle(ox, oy, ow, 2, COLOR_ACCENT);

    /* Title */
    char title[128];
    snprintf(title, sizeof(title), "Color: %s", entity->friendly_name);
    vita2d_pgf_draw_text(font, ox + 20, oy + 30, COLOR_TEXT, 1.0f, title);

    /* Live color preview — show color at full brightness for accuracy,
       brightness is controlled separately via the slider */
    int preview_w = 160;
    int preview_h = 140;
    unsigned int preview_color = hsv_to_rgba(entity->hs_hue, entity->hs_sat, 255);
    int px = ox + ow - preview_w - 20;
    int py = oy + 15;
    /* Border */
    vita2d_draw_rectangle(px - 2, py - 2, preview_w + 4, preview_h + 4, COLOR_TEXT_DIM);
    vita2d_draw_rectangle(px, py, preview_w, preview_h, preview_color);
    /* Label */
    char color_info[48];
    snprintf(color_info, sizeof(color_info), "H:%d S:%d B:%d%%",
             entity->hs_hue, entity->hs_sat, (entity->brightness * 100) / 255);
    vita2d_pgf_draw_text(font, px, py + preview_h + 18, COLOR_TEXT_DIM, 0.7f, color_info);

    int slider_x = ox + 30;
    int slider_w = ow / 2 - 20;
    int slider_y_start = oy + 120;
    int slider_spacing = 70;

    /* Hue slider: rainbow bar */
    vita2d_pgf_draw_text(font, slider_x, slider_y_start - 10, overlay_sel == 0 ? COLOR_ACCENT : COLOR_TEXT_DIM, 0.85f, "Hue");
    char hue_val[16];
    snprintf(hue_val, sizeof(hue_val), "%d\xc2\xb0", entity->hs_hue);
    vita2d_pgf_draw_text(font, slider_x + slider_w - 40, slider_y_start - 10, COLOR_TEXT, 0.85f, hue_val);

    if (overlay_sel == 0) {
        vita2d_draw_rectangle(slider_x - 2, slider_y_start - 2, slider_w + 4, 24, COLOR_ACCENT);
    }
    int segment_w = slider_w / 6;
    for (int i = 0; i < 6; i++) {
        unsigned int c = hsv_to_rgba(i * 60, 100, 255);
        vita2d_draw_rectangle(slider_x + i * segment_w, slider_y_start, segment_w, 20, c);
    }
    int hue_knob_x = slider_x + (entity->hs_hue * slider_w) / 360;
    vita2d_draw_rectangle(hue_knob_x - 3, slider_y_start - 4, 6, 28, COLOR_TEXT);

    /* Saturation slider */
    int sat_y = slider_y_start + slider_spacing;
    vita2d_pgf_draw_text(font, slider_x, sat_y - 10, overlay_sel == 1 ? COLOR_ACCENT : COLOR_TEXT_DIM, 0.85f, "Saturation");
    char sat_val[16];
    snprintf(sat_val, sizeof(sat_val), "%d%%", entity->hs_sat);
    vita2d_pgf_draw_text(font, slider_x + slider_w - 40, sat_y - 10, COLOR_TEXT, 0.85f, sat_val);
    draw_slider_bar(slider_x, sat_y, slider_w, entity->hs_sat, 100,
        hsv_to_rgba(entity->hs_hue, 100, 255), overlay_sel == 1);

    /* Brightness slider */
    int bri_y = sat_y + slider_spacing;
    vita2d_pgf_draw_text(font, slider_x, bri_y - 10, overlay_sel == 2 ? COLOR_ACCENT : COLOR_TEXT_DIM, 0.85f, "Brightness");
    char bri_val[16];
    snprintf(bri_val, sizeof(bri_val), "%d%%", (entity->brightness * 100) / 255);
    vita2d_pgf_draw_text(font, slider_x + slider_w - 40, bri_y - 10, COLOR_TEXT, 0.85f, bri_val);
    draw_slider_bar(slider_x, bri_y, slider_w, entity->brightness, 255, COLOR_WARM, overlay_sel == 2);

    /* Controls hint */
    vita2d_pgf_draw_text(font, ox + 20, oy + oh - 20, COLOR_TEXT_DIM, 0.7f,
        "Up/Down: slider   Left/Right: adjust   X: apply   O: close");
}

/* Remote button layout:
   0=Vol+  1=Up     2=Power
   3=Left  4=OK     5=Right
   6=Vol-  7=Down   8=Mute
   9=Play/Pause  10=Back  11=Home
*/
#define REMOTE_BTN_COUNT 12

static const char *remote_btn_labels[REMOTE_BTN_COUNT] = {
    "Vol+", "Up", "Power",
    "Left", "OK", "Right",
    "Vol-", "Down", "Mute",
    "Play", "Back", "Home"
};

void ui_draw_remote_overlay(ha_entity_t *entity, int remote_sel) {
    vita2d_draw_rectangle(0, 0, SCREEN_W, SCREEN_H, RGBA8(0, 0, 0, 180));

    int ox = 180, oy = 40, ow = SCREEN_W - 360, oh = SCREEN_H - 80;
    vita2d_draw_rectangle(ox, oy, ow, oh, COLOR_CARD_BG);
    vita2d_draw_rectangle(ox, oy, ow, 2, COLOR_ACCENT);

    vita2d_pgf_draw_text(font, ox + 20, oy + 30, COLOR_TEXT, 1.0f, "Fire TV Remote");

    /* Status */
    char status[64];
    snprintf(status, sizeof(status), "Status: %s", entity->state);
    unsigned int sc = (strcmp(entity->state, "playing") == 0) ? COLOR_ON : COLOR_TEXT_DIM;
    vita2d_pgf_draw_text(font, ox + 20, oy + 52, sc, 0.75f, status);

    /* 4x3 button grid */
    int btn_w = 80;
    int btn_h = 50;
    int gap = 10;
    int grid_w = 3 * btn_w + 2 * gap;
    int grid_x = ox + (ow - grid_w) / 2;
    int grid_y = oy + 70;

    for (int i = 0; i < REMOTE_BTN_COUNT; i++) {
        int row = i / 3;
        int col = i % 3;
        int bx = grid_x + col * (btn_w + gap);
        int by = grid_y + row * (btn_h + gap);

        unsigned int bg;
        if (i == remote_sel) {
            bg = COLOR_ACCENT;
        } else if (i == 2) {
            bg = COLOR_RED; /* Power */
        } else if (i == 4) {
            bg = COLOR_ON;  /* OK */
        } else {
            bg = COLOR_OFF;
        }

        vita2d_draw_rectangle(bx, by, btn_w, btn_h, bg);

        int tw = vita2d_pgf_text_width(font, 0.85f, remote_btn_labels[i]);
        vita2d_pgf_draw_text(font, bx + (btn_w - tw) / 2, by + 32, COLOR_TEXT, 0.85f, remote_btn_labels[i]);
    }

    vita2d_pgf_draw_text(font, ox + 15, oy + oh - 35, COLOR_TEXT_DIM, 0.6f,
        "Stick/D-pad: navigate   Square: OK   O: Back");
    vita2d_pgf_draw_text(font, ox + 15, oy + oh - 15, COLOR_TEXT_DIM, 0.6f,
        "L: Vol-   R: Vol+   X: press button   \xe2\x96\xb3: close");
}

void ui_draw(int current_tab, ha_entity_list_t *lists[], int num_lists,
             int cursor_idx, int sub_select, int scroll_offset,
             const char *status_msg) {
    vita2d_start_drawing();
    vita2d_clear_screen();

    draw_header(status_msg);

    ha_entity_list_t *list = NULL;
    if (current_tab < num_lists) {
        list = lists[current_tab];
    }

    if (list && list->count > 0) {
        int card_w = SCREEN_W - 40;
        for (int i = 0; i < list->count; i++) {
            int y = ui_card_y(i, scroll_offset);

            if (y + CARD_H > CONTENT_Y && y < CONTENT_BOTTOM) {
                int is_cur = (i == cursor_idx);
                draw_entity_card(&list->items[i], 20, y, card_w, is_cur, is_cur ? sub_select : 0);
            }
        }
    } else if (list) {
        vita2d_pgf_draw_text(font, SCREEN_W / 2 - 60, SCREEN_H / 2, COLOR_TEXT_DIM, 1.0f, "No items");
    }

    /* Overdraw to clip scrolled content */
    vita2d_draw_rectangle(0, 0, SCREEN_W, CONTENT_Y, COLOR_BG);
    draw_header(status_msg);

    vita2d_draw_rectangle(0, CONTENT_BOTTOM, SCREEN_W, TAB_BAR_H + 10, COLOR_BG);
    draw_tab_bar(current_tab);

    /* Hints */
    vita2d_pgf_draw_text(font, SCREEN_W - 220, HEADER_H - 5, COLOR_TEXT_DIM, 0.55f,
        "L/R:Tab  Select:Refresh  X:Action");

    vita2d_end_drawing();
    vita2d_swap_buffers();
}

void ui_draw_config_error(const char *reason, const char *config_path, int template_created) {
    vita2d_start_drawing();
    vita2d_clear_screen();

    vita2d_draw_rectangle(0, 0, SCREEN_W, HEADER_H, COLOR_HEADER);
    vita2d_pgf_draw_text(font, 20, 33, COLOR_RED, 1.2f, "Config required");

    int y = HEADER_H + 40;

    if (template_created) {
        vita2d_pgf_draw_text(font, 30, y, COLOR_TEXT, 1.0f,
            "A template config file has been created for you.");
        y += 30;
        vita2d_pgf_draw_text(font, 30, y, COLOR_TEXT, 1.0f,
            "Edit it with your Home Assistant details, then relaunch.");
    } else {
        vita2d_pgf_draw_text(font, 30, y, COLOR_TEXT, 1.0f,
            "VitaAssistant could not load its config:");
        y += 30;
        if (reason && reason[0]) {
            char buf[256];
            snprintf(buf, sizeof(buf), "  %s", reason);
            vita2d_pgf_draw_text(font, 30, y, COLOR_WARM, 1.0f, buf);
        }
    }

    y += 60;
    vita2d_pgf_draw_text(font, 30, y, COLOR_TEXT_DIM, 0.9f, "Config file:");
    y += 26;
    vita2d_pgf_draw_text(font, 30, y, COLOR_ACCENT, 0.95f,
        config_path ? config_path : "ux0:data/VitaAssistant/config.txt");

    y += 50;
    vita2d_pgf_draw_text(font, 30, y, COLOR_TEXT_DIM, 0.9f,
        "Edit the file on the Vita with VitaShell:");
    y += 26;
    vita2d_pgf_draw_text(font, 30, y, COLOR_TEXT_DIM, 0.9f,
        "  navigate to the file, press Triangle, Open decrypted.");

    y += 50;
    vita2d_pgf_draw_text(font, 30, y, COLOR_TEXT_DIM, 0.9f,
        "Required keys: host, token.   Optional: port, fire_tv_media, fire_tv_remote.");

    vita2d_pgf_draw_text(font, SCREEN_W / 2 - 80, SCREEN_H - 30, COLOR_TEXT, 1.0f,
        "Press Start to exit");

    vita2d_end_drawing();
    vita2d_swap_buffers();
}
