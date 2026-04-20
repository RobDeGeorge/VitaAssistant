#ifndef CONFIG_RUNTIME_H
#define CONFIG_RUNTIME_H

#include <stddef.h>

#define CONFIG_PATH "ux0:data/VitaAssistant/config.txt"
#define CONFIG_DIR  "ux0:data/VitaAssistant"

#define CONFIG_STR_MAX 256

typedef struct {
    char host[CONFIG_STR_MAX];
    int  port;
    char token[CONFIG_STR_MAX * 4];
    char fire_tv_media[CONFIG_STR_MAX];
    char fire_tv_remote[CONFIG_STR_MAX];
    char base_url[CONFIG_STR_MAX + 32];
} runtime_config_t;

extern runtime_config_t g_config;

/* Write the default commented template to `path` if no file is already there.
   Returns 1 if the template was written, 0 if a file already existed,
   -1 on I/O error. */
int config_write_template_if_missing(const char *path);

/* Parse `path` into g_config. On failure, writes a short human-readable reason
   to err (size err_size) and returns -1. Returns 0 on success. */
int config_load(const char *path, char *err, size_t err_size);

#endif
