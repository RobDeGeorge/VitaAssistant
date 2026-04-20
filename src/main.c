#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/apputil.h>
#include <vita2d.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "config_runtime.h"
#include "net.h"
#include "ha_api.h"
#include "ui.h"

static int current_tab = TAB_HOME;
static int scroll_offset = 0;
static int cursor_idx = 0;
static int sub_select = 0;
static char status_msg[128] = "Connecting...";

/* Color overlay state */
static int color_overlay = 0;
static int overlay_sel = 0;

/* Remote overlay state */
static int remote_overlay = 0;
static int remote_sel = 4;

/* Analog stick deadzone and rate limiting */
#define STICK_DEADZONE 50
#define STICK_THRESHOLD 100
static int analog_timer = 0;
#define ANALOG_DELAY 8 /* frames between analog repeats */

static ha_entity_list_t home_list;
static ha_entity_list_t lights;
static ha_entity_list_t climate;
static ha_entity_list_t scenes;
static ha_entity_list_t devices;

#define NUM_LISTS 5
static ha_entity_list_t *all_lists[NUM_LISTS];

static int refresh_interval = 3600; /* ~60 sec, use Select for manual refresh */
static int frame_counter = 0;

/* Brightness send rate limiting — don't send API call every frame */
static int brightness_send_timer = 0;
#define BRIGHTNESS_SEND_DELAY 10

static void fetch_all(void) {
    snprintf(status_msg, sizeof(status_msg), "Refreshing...");

    ha_entity_list_t counters, sensors;
    ha_fetch_entities("counter", &counters);
    ha_fetch_entities("sensor", &sensors);

    home_list.count = 0;
    for (int i = 0; i < counters.count && home_list.count < MAX_ENTITIES; i++)
        home_list.items[home_list.count++] = counters.items[i];
    for (int i = 0; i < sensors.count && home_list.count < MAX_ENTITIES; i++) {
        if (strstr(sensors.items[i].entity_id, "backup") ||
            strstr(sensors.items[i].entity_id, "sun_next") ||
            strstr(sensors.items[i].entity_id, "last_boot"))
            continue;
        home_list.items[home_list.count++] = sensors.items[i];
    }

    ha_fetch_entities("light", &lights);
    ha_fetch_entities("climate", &climate);
    ha_fetch_entities("scene", &scenes);

    ha_entity_list_t switches, media;
    ha_fetch_entities("switch", &switches);
    ha_fetch_entities("media_player", &media);

    devices.count = 0;
    for (int i = 0; i < switches.count && devices.count < MAX_ENTITIES; i++)
        devices.items[devices.count++] = switches.items[i];
    for (int i = 0; i < media.count && devices.count < MAX_ENTITIES; i++)
        devices.items[devices.count++] = media.items[i];

    snprintf(status_msg, sizeof(status_msg), "H:%d L:%d C:%d S:%d D:%d",
             home_list.count, lights.count, climate.count, scenes.count, devices.count);
}

static ha_entity_list_t *current_list(void) {
    if (current_tab < NUM_LISTS)
        return all_lists[current_tab];
    return &home_list;
}

static void clamp_cursor(void) {
    ha_entity_list_t *list = current_list();
    if (list->count == 0) {
        cursor_idx = 0;
        return;
    }
    if (cursor_idx < 0) cursor_idx = 0;
    if (cursor_idx >= list->count) cursor_idx = list->count - 1;
    scroll_offset = ui_clamp_scroll(cursor_idx, list->count, scroll_offset);
}

static void do_action_on_cursor(void) {
    ha_entity_list_t *list = current_list();
    if (cursor_idx < 0 || cursor_idx >= list->count) return;
    ha_entity_t *e = &list->items[cursor_idx];

    if (strcmp(e->domain, "scene") == 0) {
        snprintf(status_msg, sizeof(status_msg), "Activating %s...", e->friendly_name);
        ha_activate_scene(e->entity_id);
        fetch_all();
    } else if (strcmp(e->domain, "light") == 0 || strcmp(e->domain, "switch") == 0) {
        snprintf(status_msg, sizeof(status_msg), "Toggling %s...", e->friendly_name);
        ha_toggle(e->entity_id);
        fetch_all();
    } else if (strcmp(e->domain, "counter") == 0 || strcmp(e->domain, "sensor") == 0) {
        /* Info only */
    } else {
        snprintf(status_msg, sizeof(status_msg), "Toggling %s...", e->friendly_name);
        ha_toggle(e->entity_id);
        fetch_all();
    }
}

static ha_entity_t *get_cursor_entity(void) {
    ha_entity_list_t *list = current_list();
    if (list->count > 0 && cursor_idx >= 0 && cursor_idx < list->count)
        return &list->items[cursor_idx];
    return NULL;
}

/* Get analog stick direction: -1, 0, or 1 */
static int stick_dir(unsigned char val) {
    if (val < 128 - STICK_THRESHOLD) return -1;
    if (val > 128 + STICK_THRESHOLD) return 1;
    return 0;
}

/* Check if analog stick is outside deadzone */
static int stick_active(unsigned char lx, unsigned char ly) {
    int dx = (int)lx - 128;
    int dy = (int)ly - 128;
    return (abs(dx) > STICK_DEADZONE || abs(dy) > STICK_DEADZONE);
}

static void handle_color_overlay_input(unsigned int pressed, SceCtrlData *pad, ha_entity_t *e) {
    /* D-pad Up/Down: switch between hue/sat/brightness sliders */
    if (pressed & SCE_CTRL_UP) {
        overlay_sel--;
        if (overlay_sel < 0) overlay_sel = 2;
    }
    if (pressed & SCE_CTRL_DOWN) {
        overlay_sel++;
        if (overlay_sel > 2) overlay_sel = 0;
    }

    /* D-pad Left/Right: coarse adjust */
    if (pressed & SCE_CTRL_LEFT) {
        if (overlay_sel == 0) {
            e->hs_hue -= 10;
            if (e->hs_hue < 0) e->hs_hue += 360;
        } else if (overlay_sel == 1) {
            e->hs_sat -= 5;
            if (e->hs_sat < 0) e->hs_sat = 0;
        } else {
            e->brightness -= 15;
            if (e->brightness < 1) e->brightness = 1;
        }
    }
    if (pressed & SCE_CTRL_RIGHT) {
        if (overlay_sel == 0) {
            e->hs_hue += 10;
            if (e->hs_hue >= 360) e->hs_hue -= 360;
        } else if (overlay_sel == 1) {
            e->hs_sat += 5;
            if (e->hs_sat > 100) e->hs_sat = 100;
        } else {
            e->brightness += 15;
            if (e->brightness > 255) e->brightness = 255;
        }
    }

    /* Analog stick: fine adjust (1 unit at a time) */
    int sx = stick_dir(pad->lx);
    if (sx != 0) {
        analog_timer++;
        if (analog_timer >= ANALOG_DELAY) {
            analog_timer = 0;
            if (overlay_sel == 0) {
                e->hs_hue += sx;
                if (e->hs_hue < 0) e->hs_hue += 360;
                if (e->hs_hue >= 360) e->hs_hue -= 360;
            } else if (overlay_sel == 1) {
                e->hs_sat += sx;
                if (e->hs_sat < 0) e->hs_sat = 0;
                if (e->hs_sat > 100) e->hs_sat = 100;
            } else {
                /* ~1% brightness = ~3 out of 255 */
                e->brightness += sx * 3;
                if (e->brightness < 1) e->brightness = 1;
                if (e->brightness > 255) e->brightness = 255;
            }
        }
    } else if (!stick_active(pad->lx, pad->ly)) {
        analog_timer = ANALOG_DELAY; /* reset so next tilt fires immediately */
    }

    /* X: apply color changes in one call */
    if (pressed & SCE_CTRL_CROSS) {
        snprintf(status_msg, sizeof(status_msg), "Setting color %s...", e->friendly_name);
        ha_set_hs_color_brightness(e->entity_id, e->hs_hue, e->hs_sat, e->brightness);
        snprintf(status_msg, sizeof(status_msg), "Color set! H:%d S:%d B:%d%%",
                 e->hs_hue, e->hs_sat, (e->brightness * 100) / 255);
    }

    /* Circle or Triangle: close overlay */
    if ((pressed & SCE_CTRL_CIRCLE) || (pressed & SCE_CTRL_TRIANGLE)) {
        color_overlay = 0;
    }
}

static void do_remote_action(int btn) {
    switch (btn) {
        case 0: /* Vol+ */
            ha_remote_send_command(g_config.fire_tv_remote, "VOLUME_UP");
            snprintf(status_msg, sizeof(status_msg), "Volume Up");
            break;
        case 1: /* Up */
            ha_remote_send_command(g_config.fire_tv_remote, "UP");
            break;
        case 2: /* Power */
            ha_remote_send_command(g_config.fire_tv_remote, "POWER");
            snprintf(status_msg, sizeof(status_msg), "Power");
            break;
        case 3: /* Left */
            ha_remote_send_command(g_config.fire_tv_remote, "LEFT");
            break;
        case 4: /* OK/Select */
            ha_remote_send_command(g_config.fire_tv_remote, "CENTER");
            break;
        case 5: /* Right */
            ha_remote_send_command(g_config.fire_tv_remote, "RIGHT");
            break;
        case 6: /* Vol- */
            ha_remote_send_command(g_config.fire_tv_remote, "VOLUME_DOWN");
            snprintf(status_msg, sizeof(status_msg), "Volume Down");
            break;
        case 7: /* Down */
            ha_remote_send_command(g_config.fire_tv_remote, "DOWN");
            break;
        case 8: /* Mute */
            ha_remote_send_command(g_config.fire_tv_remote, "MUTE");
            snprintf(status_msg, sizeof(status_msg), "Mute");
            break;
        case 9: /* Play/Pause */
            ha_media_play_pause(g_config.fire_tv_media);
            snprintf(status_msg, sizeof(status_msg), "Play/Pause");
            break;
        case 10: /* Back */
            ha_remote_send_command(g_config.fire_tv_remote, "BACK");
            break;
        case 11: /* Home */
            ha_remote_send_command(g_config.fire_tv_remote, "HOME");
            break;
    }
}

static void handle_remote_overlay_input(unsigned int pressed, SceCtrlData *pad) {
    /* D-pad: navigate button grid */
    int row = remote_sel / 3;
    int col = remote_sel % 3;

    if (pressed & SCE_CTRL_UP) {
        row--; if (row < 0) row = 3;
        remote_sel = row * 3 + col;
    }
    if (pressed & SCE_CTRL_DOWN) {
        row++; if (row > 3) row = 0;
        remote_sel = row * 3 + col;
    }
    if (pressed & SCE_CTRL_LEFT) {
        col--; if (col < 0) col = 2;
        remote_sel = row * 3 + col;
    }
    if (pressed & SCE_CTRL_RIGHT) {
        col++; if (col > 2) col = 0;
        remote_sel = row * 3 + col;
    }

    /* Analog stick: directly send Fire TV navigation */
    int sx = stick_dir(pad->lx);
    int sy = stick_dir(pad->ly);
    if (sx != 0 || sy != 0) {
        analog_timer++;
        if (analog_timer >= ANALOG_DELAY) {
            analog_timer = 0;
            if (sy < 0) ha_remote_send_command(g_config.fire_tv_remote, "UP");
            if (sy > 0) ha_remote_send_command(g_config.fire_tv_remote, "DOWN");
            if (sx < 0) ha_remote_send_command(g_config.fire_tv_remote, "LEFT");
            if (sx > 0) ha_remote_send_command(g_config.fire_tv_remote, "RIGHT");
        }
    } else {
        analog_timer = ANALOG_DELAY;
    }

    /* X: press selected button */
    if (pressed & SCE_CTRL_CROSS) {
        do_remote_action(remote_sel);
    }

    /* Square: OK/Select shortcut */
    if (pressed & SCE_CTRL_SQUARE) {
        ha_remote_send_command(g_config.fire_tv_remote, "CENTER");
    }

    /* L trigger: volume down (repeats while held) */
    static int vol_repeat_timer = 0;
    if (pad->buttons & SCE_CTRL_LTRIGGER) {
        if (pressed & SCE_CTRL_LTRIGGER) {
            ha_remote_send_command(g_config.fire_tv_remote, "VOLUME_DOWN");
            snprintf(status_msg, sizeof(status_msg), "Volume Down");
            vol_repeat_timer = 0;
        } else {
            vol_repeat_timer++;
            if (vol_repeat_timer > 15 && vol_repeat_timer % 8 == 0) {
                ha_remote_send_command(g_config.fire_tv_remote, "VOLUME_DOWN");
                snprintf(status_msg, sizeof(status_msg), "Volume Down");
            }
        }
    }

    /* R trigger: volume up (repeats while held) */
    if (pad->buttons & SCE_CTRL_RTRIGGER) {
        if (pressed & SCE_CTRL_RTRIGGER) {
            ha_remote_send_command(g_config.fire_tv_remote, "VOLUME_UP");
            snprintf(status_msg, sizeof(status_msg), "Volume Up");
            vol_repeat_timer = 0;
        } else {
            vol_repeat_timer++;
            if (vol_repeat_timer > 15 && vol_repeat_timer % 8 == 0) {
                ha_remote_send_command(g_config.fire_tv_remote, "VOLUME_UP");
                snprintf(status_msg, sizeof(status_msg), "Volume Up");
            }
        }
    }

    if (!(pad->buttons & (SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER))) {
        vol_repeat_timer = 0;
    }

    /* Circle: back button */
    if (pressed & SCE_CTRL_CIRCLE) {
        ha_remote_send_command(g_config.fire_tv_remote, "BACK");
    }

    /* Triangle: close overlay */
    if (pressed & SCE_CTRL_TRIANGLE) {
        remote_overlay = 0;
    }
}

int main(void) {
    sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    all_lists[TAB_HOME] = &home_list;
    all_lists[TAB_LIGHTS] = &lights;
    all_lists[TAB_CLIMATE] = &climate;
    all_lists[TAB_SCENES] = &scenes;
    all_lists[TAB_DEVICES] = &devices;

    if (ui_init() < 0) {
        sceKernelExitProcess(0);
        return 1;
    }

    /* Load runtime config. If missing, write a commented template to
       ux0:data/VitaAssistant/config.txt and show the user an error screen
       telling them how to edit it. */
    int template_created = config_write_template_if_missing(CONFIG_PATH);
    char cfg_err[128] = {0};
    if (template_created == 1 || config_load(CONFIG_PATH, cfg_err, sizeof(cfg_err)) < 0) {
        /* Prime the framebuffer so the first visible frame is the error screen,
           not leftover garbage from vita2d_init. */
        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_end_drawing();
        vita2d_swap_buffers();

        unsigned int old_buttons_err = 0;
        while (1) {
            SceCtrlData pad;
            sceCtrlPeekBufferPositive(0, &pad, 1);
            unsigned int pressed = pad.buttons & ~old_buttons_err;
            old_buttons_err = pad.buttons;

            ui_draw_config_error(cfg_err, CONFIG_PATH, template_created == 1);

            if (pressed & SCE_CTRL_START) break;
        }
        ui_cleanup();
        sceAppUtilShutdown();
        sceKernelExitProcess(0);
        return 0;
    }

    /* Draw a loading frame so the screen isn't blank during network init */
    ui_draw(current_tab, all_lists, NUM_LISTS, cursor_idx, sub_select, scroll_offset, status_msg);

    if (net_init() < 0) {
        snprintf(status_msg, sizeof(status_msg), "Network error! Check WiFi");
    } else {
        /* Wait for WiFi to actually be connected before making requests */
        snprintf(status_msg, sizeof(status_msg), "Waiting for WiFi...");
        ui_draw(current_tab, all_lists, NUM_LISTS, cursor_idx, sub_select, scroll_offset, status_msg);

        if (net_wait_connected() < 0) {
            snprintf(status_msg, sizeof(status_msg), "WiFi timeout! Press Select to retry");
        } else {
            fetch_all();
        }
    }

    unsigned int old_buttons = 0;
    int repeat_timer = 0;

    while (1) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);

        unsigned int pressed = pad.buttons & ~old_buttons;
        old_buttons = pad.buttons;

        ha_entity_t *cur_entity = get_cursor_entity();

        /* Color overlay */
        if (color_overlay && cur_entity) {
            handle_color_overlay_input(pressed, &pad, cur_entity);

            vita2d_start_drawing();
            vita2d_clear_screen();
            ui_draw_color_overlay(cur_entity, overlay_sel);
            vita2d_end_drawing();
            vita2d_swap_buffers();

            if (pressed & SCE_CTRL_START) break;
            continue;
        }

        /* Remote overlay */
        if (remote_overlay) {
            handle_remote_overlay_input(pressed, &pad);

            ha_entity_t remote_entity;
            memset(&remote_entity, 0, sizeof(remote_entity));
            strncpy(remote_entity.entity_id, g_config.fire_tv_media, sizeof(remote_entity.entity_id) - 1);
            strncpy(remote_entity.friendly_name, "Fire TV", sizeof(remote_entity.friendly_name) - 1);
            for (int i = 0; i < devices.count; i++) {
                if (strcmp(devices.items[i].entity_id, g_config.fire_tv_media) == 0) {
                    remote_entity = devices.items[i];
                    break;
                }
            }

            vita2d_start_drawing();
            vita2d_clear_screen();
            ui_draw_remote_overlay(&remote_entity, remote_sel);
            vita2d_end_drawing();
            vita2d_swap_buffers();

            if (pressed & SCE_CTRL_START) break;
            continue;
        }

        /* Tab switching: L/R triggers */
        if (pressed & SCE_CTRL_LTRIGGER) {
            current_tab = (current_tab - 1 + TAB_COUNT) % TAB_COUNT;
            cursor_idx = 0;
            scroll_offset = 0;
            sub_select = 0;
        }
        if (pressed & SCE_CTRL_RTRIGGER) {
            current_tab = (current_tab + 1) % TAB_COUNT;
            cursor_idx = 0;
            scroll_offset = 0;
            sub_select = 0;
        }

        /* D-pad Up/Down: navigate items */
        if (pressed & SCE_CTRL_UP) {
            cursor_idx--;
            sub_select = 0;
            clamp_cursor();
            repeat_timer = 0;
        }
        if (pressed & SCE_CTRL_DOWN) {
            cursor_idx++;
            sub_select = 0;
            clamp_cursor();
            repeat_timer = 0;
        }

        /* Hold up/down for repeat scrolling */
        if (pad.buttons & (SCE_CTRL_UP | SCE_CTRL_DOWN)) {
            repeat_timer++;
            if (repeat_timer > 20 && repeat_timer % 5 == 0) {
                if (pad.buttons & SCE_CTRL_UP) cursor_idx--;
                if (pad.buttons & SCE_CTRL_DOWN) cursor_idx++;
                clamp_cursor();
            }
        } else {
            repeat_timer = 0;
        }

        /* D-pad Left/Right: coarse brightness (~6%) or climate temp */
        if (pressed & SCE_CTRL_LEFT) {
            if (cur_entity && strcmp(cur_entity->domain, "light") == 0 &&
                strcmp(cur_entity->state, "on") == 0) {
                cur_entity->brightness -= 15;
                if (cur_entity->brightness < 1) cur_entity->brightness = 1;
                ha_set_brightness(cur_entity->entity_id, cur_entity->brightness);
            } else if (cur_entity && strcmp(cur_entity->domain, "climate") == 0) {
                cur_entity->target_temp -= 1.0f;
                ha_set_temperature(cur_entity->entity_id, cur_entity->target_temp);
                snprintf(status_msg, sizeof(status_msg), "Temp -> %.0f\xc2\xb0", cur_entity->target_temp);
            }
        }
        if (pressed & SCE_CTRL_RIGHT) {
            if (cur_entity && strcmp(cur_entity->domain, "light") == 0 &&
                strcmp(cur_entity->state, "on") == 0) {
                cur_entity->brightness += 15;
                if (cur_entity->brightness > 255) cur_entity->brightness = 255;
                ha_set_brightness(cur_entity->entity_id, cur_entity->brightness);
            } else if (cur_entity && strcmp(cur_entity->domain, "climate") == 0) {
                cur_entity->target_temp += 1.0f;
                ha_set_temperature(cur_entity->entity_id, cur_entity->target_temp);
                snprintf(status_msg, sizeof(status_msg), "Temp -> %.0f\xc2\xb0", cur_entity->target_temp);
            }
        }

        /* Analog stick Left/Right: fine brightness (~1%) on lights */
        if (cur_entity && strcmp(cur_entity->domain, "light") == 0 &&
            strcmp(cur_entity->state, "on") == 0) {
            int sx = stick_dir(pad.lx);
            if (sx != 0) {
                analog_timer++;
                if (analog_timer >= ANALOG_DELAY) {
                    analog_timer = 0;
                    cur_entity->brightness += sx * 3; /* ~1% of 255 */
                    if (cur_entity->brightness < 1) cur_entity->brightness = 1;
                    if (cur_entity->brightness > 255) cur_entity->brightness = 255;
                    /* Rate-limit API calls for fine adjust */
                    brightness_send_timer++;
                    if (brightness_send_timer >= BRIGHTNESS_SEND_DELAY) {
                        brightness_send_timer = 0;
                        ha_set_brightness(cur_entity->entity_id, cur_entity->brightness);
                    }
                }
            } else {
                /* Stick released — send final brightness if we have a pending change */
                if (brightness_send_timer > 0) {
                    ha_set_brightness(cur_entity->entity_id, cur_entity->brightness);
                    brightness_send_timer = 0;
                }
                analog_timer = ANALOG_DELAY;
            }
        }

        /* X button: toggle / activate */
        if (pressed & SCE_CTRL_CROSS) {
            do_action_on_cursor();
        }

        /* Triangle: open context overlay */
        if (pressed & SCE_CTRL_TRIANGLE) {
            if (cur_entity && strcmp(cur_entity->domain, "light") == 0 &&
                cur_entity->has_rgb && strcmp(cur_entity->state, "on") == 0) {
                color_overlay = 1;
                overlay_sel = 0;
                analog_timer = ANALOG_DELAY;
            } else if (cur_entity && strcmp(cur_entity->domain, "media_player") == 0) {
                remote_overlay = 1;
                remote_sel = 4;
                analog_timer = ANALOG_DELAY;
            }
        }

        /* Select: manual refresh */
        if (pressed & SCE_CTRL_SELECT) {
            fetch_all();
            clamp_cursor();
        }

        /* Circle: cycle sub-select on lights */
        if (pressed & SCE_CTRL_CIRCLE) {
            if (cur_entity && strcmp(cur_entity->domain, "light") == 0 &&
                strcmp(cur_entity->state, "on") == 0) {
                sub_select++;
                if (sub_select > 2) sub_select = 0;
            } else {
                sub_select = 0;
            }
        }

        /* Touch input */
        SceTouchData touch;
        static int touch_was_down = 0;
        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
        if (touch.reportNum > 0 && !touch_was_down) {
            int tx = touch.report[0].x / 2;
            int ty = touch.report[0].y / 2;

            if (ty >= SCREEN_H - TAB_BAR_H) {
                int tab_w = SCREEN_W / TAB_COUNT;
                int new_tab = tx / tab_w;
                if (new_tab >= 0 && new_tab < TAB_COUNT) {
                    current_tab = new_tab;
                    cursor_idx = 0;
                    scroll_offset = 0;
                    sub_select = 0;
                }
            } else if (ty >= CONTENT_Y && ty < CONTENT_BOTTOM) {
                ha_entity_list_t *list = current_list();
                for (int i = 0; i < list->count; i++) {
                    int cy = ui_card_y(i, scroll_offset);
                    if (ty >= cy && ty < cy + CARD_H) {
                        cursor_idx = i;
                        if (tx >= SCREEN_W - 90) {
                            do_action_on_cursor();
                        } else {
                            sub_select = 0;
                        }
                        break;
                    }
                }
            }
        }
        touch_was_down = (touch.reportNum > 0);

        /* Auto refresh */
        frame_counter++;
        if (frame_counter >= refresh_interval) {
            frame_counter = 0;
            fetch_all();
        }

        /* Start = exit */
        if (pressed & SCE_CTRL_START) break;

        /* Draw */
        ui_draw(current_tab, all_lists, NUM_LISTS, cursor_idx, sub_select, scroll_offset, status_msg);
    }

    net_cleanup();
    ui_cleanup();
    sceAppUtilShutdown();
    sceKernelExitProcess(0);
    return 0;
}
