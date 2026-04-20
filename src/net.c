#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/net/http.h>
#include <psp2/sysmodule.h>
#include <psp2/libssl.h>
#include <psp2/kernel/processmgr.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "net.h"
#include "config.h"
#include "config_runtime.h"

#define NET_HEAP_SIZE  (1 * 1024 * 1024)
#define HTTP_HEAP_SIZE (1 * 1024 * 1024)
#define SSL_HEAP_SIZE  (256 * 1024)

static int http_template = -1;

int net_wait_connected(void) {
    /* Wait up to 10 seconds for WiFi to be connected */
    for (int i = 0; i < 100; i++) {
        int state = 0;
        sceNetCtlInetGetState(&state);
        if (state == 3) return 0; /* 3 = connected with IP */
        sceKernelDelayThread(100 * 1000); /* 100ms */
    }
    return -1;
}

int net_init(void) {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);
    sceSysmoduleLoadModule(SCE_SYSMODULE_SSL);

    SceNetInitParam param;
    static char net_memory[NET_HEAP_SIZE];
    param.memory = net_memory;
    param.size = sizeof(net_memory);
    param.flags = 0;
    sceNetInit(&param);
    sceNetCtlInit();

    sceSslInit(SSL_HEAP_SIZE);
    sceHttpInit(HTTP_HEAP_SIZE);

    http_template = sceHttpCreateTemplate("VitaAssistant/1.0", SCE_HTTP_VERSION_1_1, SCE_TRUE);
    if (http_template < 0) return -1;

    /* Set timeouts so requests don't block forever */
    sceHttpSetConnectTimeOut(http_template, 5 * 1000 * 1000);  /* 5 sec connect */
    sceHttpSetRecvTimeOut(http_template, 10 * 1000 * 1000);     /* 10 sec read */
    sceHttpSetSendTimeOut(http_template, 5 * 1000 * 1000);      /* 5 sec send */
    sceHttpSetResolveTimeOut(http_template, 5 * 1000 * 1000);   /* 5 sec DNS */

    return 0;
}

void net_cleanup(void) {
    if (http_template >= 0) sceHttpDeleteTemplate(http_template);
    sceHttpTerm();
    sceSslTerm();
    sceNetCtlTerm();
    sceNetTerm();
}

char *net_request(const char *method, const char *url, const char *body, int *out_len) {
    int conn = sceHttpCreateConnectionWithURL(http_template, url, SCE_FALSE);
    if (conn < 0) return NULL;

    int method_id = SCE_HTTP_METHOD_GET;
    if (strcmp(method, "POST") == 0) method_id = SCE_HTTP_METHOD_POST;

    int req = sceHttpCreateRequestWithURL(conn, method_id, url, body ? strlen(body) : 0);
    if (req < 0) {
        sceHttpDeleteConnection(conn);
        return NULL;
    }

    /* Set auth header */
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", g_config.token);
    sceHttpAddRequestHeader(req, "Authorization", auth_header, SCE_HTTP_HEADER_OVERWRITE);
    sceHttpAddRequestHeader(req, "Content-Type", "application/json", SCE_HTTP_HEADER_OVERWRITE);

    int ret;
    if (body && strlen(body) > 0) {
        ret = sceHttpSendRequest(req, body, strlen(body));
    } else {
        ret = sceHttpSendRequest(req, NULL, 0);
    }

    if (ret < 0) {
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        return NULL;
    }

    /* Read response */
    int status_code = 0;
    sceHttpGetStatusCode(req, &status_code);

    /* Read body in chunks */
    size_t total = 0;
    size_t capacity = 4096;
    char *result = malloc(capacity);
    if (!result) {
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        return NULL;
    }

    while (1) {
        if (total + 1024 > capacity) {
            capacity *= 2;
            char *tmp = realloc(result, capacity);
            if (!tmp) { free(result); result = NULL; break; }
            result = tmp;
        }
        int read = sceHttpReadData(req, result + total, 1024);
        if (read < 0) { free(result); result = NULL; break; }
        if (read == 0) break;
        total += read;
    }

    if (result) {
        result[total] = '\0';
        if (out_len) *out_len = total;
    }

    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    return result;
}
