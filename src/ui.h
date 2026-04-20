#ifndef UI_H
#define UI_H

#include "ha_api.h"

/* Initialize vita2d and touch */
int ui_init(void);

/* Clean up */
void ui_cleanup(void);

/* Draw the full UI frame */
void ui_draw(int current_tab, ha_entity_list_t *lists[], int num_lists,
             int cursor_idx, int sub_select, int scroll_offset,
             const char *status_msg);

/* Draw the color picker overlay for RGB lights.
   overlay_sel: 0=hue, 1=saturation, 2=brightness */
void ui_draw_color_overlay(ha_entity_t *entity, int overlay_sel);

/* Draw the Fire TV remote overlay.
   remote_sel: 0-8 button index */
void ui_draw_remote_overlay(ha_entity_t *entity, int remote_sel);

/* Draw a full-screen error when the runtime config file is missing or invalid.
   `reason` is a short human-readable message (e.g. "'host' is not set"). */
void ui_draw_config_error(const char *reason, const char *config_path, int template_created);

/* Get the card Y position for a given index (accounting for scroll) */
int ui_card_y(int index, int scroll_offset);

/* Clamp scroll so cursor is visible */
int ui_clamp_scroll(int cursor_idx, int count, int scroll_offset);

#endif
