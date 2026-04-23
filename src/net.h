#ifndef NET_H
#define NET_H

/* Initialize Vita networking (WiFi, HTTP, SSL) */
int net_init(void);

/* Wait up to 10 seconds for WiFi to be connected. Returns 0 on success, -1 on timeout. */
int net_wait_connected(void);

/* Shut down networking */
void net_cleanup(void);

/* Perform an HTTP request. Returns response body (caller must free) or NULL on error.
   method: "GET", "POST", etc.
   url: full URL
   body: request body (can be NULL for GET)
   out_status: if non-NULL, receives HTTP status code (0 on transport failure).
   Returns NULL and frees the body for non-2xx responses — callers can still
   inspect *out_status to distinguish auth failures from transport failures. */
char *net_request(const char *method, const char *url, const char *body, int *out_status);

/* Last HTTP status observed by net_request. 0 if no request completed or
   transport failed before a response was received. */
extern int net_last_status;

#endif
