#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

#include "ha_api.h"
#include "net.h"
#include "config.h"
#include "config_runtime.h"

int ha_fetch_entities(const char *domain, ha_entity_list_t *list) {
    list->count = 0;

    char url[256];
    snprintf(url, sizeof(url), "%s/api/states", g_config.base_url);

    char *resp = net_request("GET", url, NULL, NULL);
    if (!resp) return -1;

    cJSON *root = cJSON_Parse(resp);
    free(resp);
    if (!root) return -1;

    int n = cJSON_GetArraySize(root);
    for (int i = 0; i < n && list->count < MAX_ENTITIES; i++) {
        cJSON *entity = cJSON_GetArrayItem(root, i);
        cJSON *eid = cJSON_GetObjectItem(entity, "entity_id");
        if (!eid || !eid->valuestring) continue;

        /* Check domain prefix */
        char entity_domain[32] = {0};
        const char *dot = strchr(eid->valuestring, '.');
        if (!dot) continue;
        size_t dlen = dot - eid->valuestring;
        if (dlen >= sizeof(entity_domain)) continue;
        strncpy(entity_domain, eid->valuestring, dlen);
        entity_domain[dlen] = '\0';

        if (strcmp(entity_domain, domain) != 0) continue;

        ha_entity_t *e = &list->items[list->count];
        memset(e, 0, sizeof(*e));
        strncpy(e->entity_id, eid->valuestring, sizeof(e->entity_id) - 1);
        strncpy(e->domain, entity_domain, sizeof(e->domain) - 1);

        cJSON *state = cJSON_GetObjectItem(entity, "state");
        if (state && state->valuestring)
            strncpy(e->state, state->valuestring, sizeof(e->state) - 1);

        cJSON *attrs = cJSON_GetObjectItem(entity, "attributes");
        if (attrs) {
            cJSON *fname = cJSON_GetObjectItem(attrs, "friendly_name");
            if (fname && fname->valuestring)
                strncpy(e->friendly_name, fname->valuestring, sizeof(e->friendly_name) - 1);

            cJSON *bright = cJSON_GetObjectItem(attrs, "brightness");
            if (bright && cJSON_IsNumber(bright))
                e->brightness = bright->valueint;

            /* RGB color support */
            cJSON *rgb = cJSON_GetObjectItem(attrs, "rgb_color");
            if (rgb && cJSON_IsArray(rgb) && cJSON_GetArraySize(rgb) == 3) {
                e->has_rgb = 1;
                e->rgb_r = cJSON_GetArrayItem(rgb, 0)->valueint;
                e->rgb_g = cJSON_GetArrayItem(rgb, 1)->valueint;
                e->rgb_b = cJSON_GetArrayItem(rgb, 2)->valueint;
            }

            /* HS color support */
            cJSON *hs = cJSON_GetObjectItem(attrs, "hs_color");
            if (hs && cJSON_IsArray(hs) && cJSON_GetArraySize(hs) == 2) {
                e->has_rgb = 1;
                e->hs_hue = (int)cJSON_GetArrayItem(hs, 0)->valuedouble;
                e->hs_sat = (int)cJSON_GetArrayItem(hs, 1)->valuedouble;
            }

            /* Check supported_color_modes for RGB capability */
            cJSON *modes = cJSON_GetObjectItem(attrs, "supported_color_modes");
            if (modes && cJSON_IsArray(modes)) {
                int nm = cJSON_GetArraySize(modes);
                for (int j = 0; j < nm; j++) {
                    cJSON *m = cJSON_GetArrayItem(modes, j);
                    if (m && m->valuestring) {
                        if (strcmp(m->valuestring, "rgb") == 0 ||
                            strcmp(m->valuestring, "hs") == 0 ||
                            strcmp(m->valuestring, "xy") == 0 ||
                            strcmp(m->valuestring, "rgbw") == 0 ||
                            strcmp(m->valuestring, "rgbww") == 0) {
                            e->has_rgb = 1;
                        }
                    }
                }
            }

            cJSON *temp = cJSON_GetObjectItem(attrs, "current_temperature");
            if (temp && cJSON_IsNumber(temp))
                e->temperature = (float)temp->valuedouble;

            cJSON *target = cJSON_GetObjectItem(attrs, "temperature");
            if (target && cJSON_IsNumber(target))
                e->target_temp = (float)target->valuedouble;

            cJSON *hvac = cJSON_GetObjectItem(attrs, "hvac_action");
            if (hvac && hvac->valuestring)
                strncpy(e->hvac_mode, hvac->valuestring, sizeof(e->hvac_mode) - 1);
        }

        if (e->friendly_name[0] == '\0')
            strncpy(e->friendly_name, eid->valuestring, sizeof(e->friendly_name) - 1);

        list->count++;
    }

    cJSON_Delete(root);
    return 0;
}

static int ha_call_service(const char *domain, const char *service, const char *entity_id, const char *extra_json) {
    char url[256];
    snprintf(url, sizeof(url), "%s/api/services/%s/%s", g_config.base_url, domain, service);

    char body[512];
    if (extra_json) {
        snprintf(body, sizeof(body), "{\"entity_id\":\"%s\",%s}", entity_id, extra_json);
    } else {
        snprintf(body, sizeof(body), "{\"entity_id\":\"%s\"}", entity_id);
    }

    char *resp = net_request("POST", url, body, NULL);
    if (resp) { free(resp); return 0; }
    return -1;
}

static const char *get_domain(const char *entity_id) {
    static char domain[32];
    const char *dot = strchr(entity_id, '.');
    if (!dot) return "light";
    size_t len = dot - entity_id;
    if (len >= sizeof(domain)) len = sizeof(domain) - 1;
    strncpy(domain, entity_id, len);
    domain[len] = '\0';
    return domain;
}

int ha_toggle(const char *entity_id) {
    return ha_call_service(get_domain(entity_id), "toggle", entity_id, NULL);
}

int ha_turn_on(const char *entity_id) {
    return ha_call_service(get_domain(entity_id), "turn_on", entity_id, NULL);
}

int ha_turn_off(const char *entity_id) {
    return ha_call_service(get_domain(entity_id), "turn_off", entity_id, NULL);
}

int ha_set_brightness(const char *entity_id, int brightness) {
    char extra[64];
    snprintf(extra, sizeof(extra), "\"brightness\":%d", brightness);
    return ha_call_service("light", "turn_on", entity_id, extra);
}

int ha_set_rgb(const char *entity_id, int r, int g, int b) {
    char extra[128];
    snprintf(extra, sizeof(extra), "\"rgb_color\":[%d,%d,%d]", r, g, b);
    return ha_call_service("light", "turn_on", entity_id, extra);
}

int ha_set_hs_color_brightness(const char *entity_id, int hue, int sat, int brightness) {
    char extra[128];
    snprintf(extra, sizeof(extra), "\"hs_color\":[%d,%d],\"brightness\":%d", hue, sat, brightness);
    return ha_call_service("light", "turn_on", entity_id, extra);
}

int ha_set_hs_color(const char *entity_id, int hue, int sat) {
    char extra[128];
    snprintf(extra, sizeof(extra), "\"hs_color\":[%d,%d]", hue, sat);
    return ha_call_service("light", "turn_on", entity_id, extra);
}

int ha_set_temperature(const char *entity_id, float temp) {
    char extra[64];
    snprintf(extra, sizeof(extra), "\"temperature\":%.1f", temp);
    return ha_call_service("climate", "set_temperature", entity_id, extra);
}

int ha_set_hvac_mode(const char *entity_id, const char *mode) {
    char extra[64];
    snprintf(extra, sizeof(extra), "\"hvac_mode\":\"%s\"", mode);
    return ha_call_service("climate", "set_hvac_mode", entity_id, extra);
}

int ha_activate_scene(const char *entity_id) {
    return ha_call_service("scene", "turn_on", entity_id, NULL);
}

int ha_media_play_pause(const char *entity_id) {
    return ha_call_service("media_player", "media_play_pause", entity_id, NULL);
}

int ha_media_volume_up(const char *entity_id) {
    return ha_call_service("media_player", "volume_up", entity_id, NULL);
}

int ha_media_volume_down(const char *entity_id) {
    return ha_call_service("media_player", "volume_down", entity_id, NULL);
}

int ha_media_volume_mute(const char *entity_id) {
    return ha_call_service("media_player", "volume_mute", entity_id, NULL);
}

int ha_media_next(const char *entity_id) {
    return ha_call_service("media_player", "media_next_track", entity_id, NULL);
}

int ha_media_prev(const char *entity_id) {
    return ha_call_service("media_player", "media_previous_track", entity_id, NULL);
}

int ha_media_turn_off(const char *entity_id) {
    return ha_call_service("media_player", "turn_off", entity_id, NULL);
}

int ha_media_select_source(const char *entity_id, const char *source) {
    char extra[256];
    snprintf(extra, sizeof(extra), "\"source\":\"%s\"", source);
    return ha_call_service("media_player", "select_source", entity_id, extra);
}

int ha_remote_send_command(const char *entity_id, const char *command) {
    char url[256];
    snprintf(url, sizeof(url), "%s/api/services/remote/send_command", g_config.base_url);

    char body[256];
    snprintf(body, sizeof(body), "{\"entity_id\":\"%s\",\"command\":\"%s\"}", entity_id, command);

    char *resp = net_request("POST", url, body, NULL);
    if (resp) { free(resp); return 0; }
    return -1;
}
