#ifndef WELCOMEHUB_SERVER_H
#define WELCOMEHUB_SERVER_H

#include <stdint.h>

/* Run the blocking HTTP server (UI page and API routes). Returns -1 on error. */
int welcomehub_server_run(const char *database_path, int port,
                          uint32_t firmware_version);

#endif
