#ifndef HA_API_H
#define HA_API_H

#include "config.h"

typedef struct {
    char entity_id[128];
    char friendly_name[128];
    char state[64];
    char domain[32];
    int brightness;       /* 0-255 for lights */
    int rgb_r, rgb_g, rgb_b; /* RGB color for lights */
    int has_rgb;          /* whether this light supports RGB */
    int hs_hue;           /* hue 0-360 */
    int hs_sat;           /* saturation 0-100 */
    float temperature;    /* for climate */
    float target_temp;    /* for climate */
    char hvac_mode[32];   /* for climate */
} ha_entity_t;

typedef struct {
    ha_entity_t items[MAX_ENTITIES];
    int count;
} ha_entity_list_t;

/* Fetch all Home Assistant states in a single GET and bucket them into the
   provided lists. Pass NULL for any list you don't care about. `home_list`
   receives counters plus filtered sensors; `devices` receives switches plus
   media_players. Returns 0 on success, -1 on transport/HTTP failure (inspect
   net_last_status for the HTTP code). */
int ha_fetch_all_states(ha_entity_list_t *home_list,
                        ha_entity_list_t *lights,
                        ha_entity_list_t *climate,
                        ha_entity_list_t *scenes,
                        ha_entity_list_t *devices);

/* Toggle a light or switch */
int ha_toggle(const char *entity_id);

/* Turn on/off */
int ha_turn_on(const char *entity_id);
int ha_turn_off(const char *entity_id);

/* Set light brightness (0-255) */
int ha_set_brightness(const char *entity_id, int brightness);

/* Set light RGB color */
int ha_set_rgb(const char *entity_id, int r, int g, int b);

/* Set light HS color (hue 0-360, sat 0-100) */
int ha_set_hs_color(const char *entity_id, int hue, int sat);

/* Set light HS color and brightness in a single call */
int ha_set_hs_color_brightness(const char *entity_id, int hue, int sat, int brightness);

/* Set thermostat temperature */
int ha_set_temperature(const char *entity_id, float temp);

/* Set HVAC mode (heat, cool, auto, off) */
int ha_set_hvac_mode(const char *entity_id, const char *mode);

/* Activate a scene */
int ha_activate_scene(const char *entity_id);

/* Media player controls */
int ha_media_play_pause(const char *entity_id);
int ha_media_volume_up(const char *entity_id);
int ha_media_volume_down(const char *entity_id);
int ha_media_volume_mute(const char *entity_id);
int ha_media_next(const char *entity_id);
int ha_media_prev(const char *entity_id);
int ha_media_turn_off(const char *entity_id);
int ha_media_select_source(const char *entity_id, const char *source);

/* ADB remote commands for Fire TV */
int ha_remote_send_command(const char *entity_id, const char *command);

#endif
