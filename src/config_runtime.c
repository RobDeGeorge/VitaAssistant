#include "config_runtime.h"

#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

runtime_config_t g_config = {
    .port = 8123,
};

static const char *DEFAULT_TEMPLATE =
    "# VitaAssistant configuration\n"
    "#\n"
    "# Edit the lines below: remove the leading '#' and fill in your details.\n"
    "# Save this file and relaunch VitaAssistant.\n"
    "#\n"
    "# You can edit this file directly on the Vita using VitaShell's\n"
    "# built-in text editor (Triangle -> Open decrypted).\n"
    "#\n"
    "# ---- Required ----\n"
    "#\n"
    "# Home Assistant host (IP address or hostname on your LAN)\n"
    "#host=192.168.1.100\n"
    "\n"
    "# Home Assistant port (default 8123)\n"
    "#port=8123\n"
    "\n"
    "# Long-lived access token.\n"
    "# Create one in HA: Profile -> Security -> Long-lived access tokens.\n"
    "#token=paste-your-long-lived-token-here\n"
    "\n"
    "# ---- Optional: Fire TV remote overlay ----\n"
    "#\n"
    "# Entity IDs from your Home Assistant. Leave blank if you don't use a Fire TV.\n"
    "#fire_tv_media=media_player.your_fire_tv\n"
    "#fire_tv_remote=remote.your_fire_tv\n";

static int path_exists(const char *path) {
    SceIoStat st;
    return sceIoGetstat(path, &st) >= 0;
}

int config_write_template_if_missing(const char *path) {
    if (path_exists(path)) return 0;

    sceIoMkdir(CONFIG_DIR, 0777);

    SceUID fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (fd < 0) return -1;

    size_t len = strlen(DEFAULT_TEMPLATE);
    int wrote = sceIoWrite(fd, DEFAULT_TEMPLATE, len);
    sceIoClose(fd);

    return (wrote == (int)len) ? 1 : -1;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
                       end[-1] == '\r' || end[-1] == '\n')) end--;
    *end = '\0';
    return s;
}

static void copy_str(char *dst, size_t cap, const char *src) {
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

int config_load(const char *path, char *err, size_t err_size) {
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(err, err_size, "Cannot open %s", path);
        return -1;
    }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);
        if (*p == '\0' || *p == '#') continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);
        if (*val == '\0') continue;

        if (!strcmp(key, "host"))             copy_str(g_config.host, sizeof(g_config.host), val);
        else if (!strcmp(key, "port"))        g_config.port = atoi(val);
        else if (!strcmp(key, "token"))       copy_str(g_config.token, sizeof(g_config.token), val);
        else if (!strcmp(key, "fire_tv_media"))  copy_str(g_config.fire_tv_media, sizeof(g_config.fire_tv_media), val);
        else if (!strcmp(key, "fire_tv_remote")) copy_str(g_config.fire_tv_remote, sizeof(g_config.fire_tv_remote), val);
    }
    fclose(f);

    if (g_config.host[0] == '\0') {
        snprintf(err, err_size, "'host' is not set");
        return -1;
    }
    if (g_config.token[0] == '\0') {
        snprintf(err, err_size, "'token' is not set");
        return -1;
    }
    if (g_config.port <= 0 || g_config.port > 65535) {
        snprintf(err, err_size, "'port' is invalid");
        return -1;
    }

    snprintf(g_config.base_url, sizeof(g_config.base_url),
             "http://%s:%d", g_config.host, g_config.port);

    return 0;
}
