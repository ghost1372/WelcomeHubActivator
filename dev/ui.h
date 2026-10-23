#ifndef WELCOMEHUB_UI_H
#define WELCOMEHUB_UI_H

#include <stddef.h>
#include <stdint.h>

/* Render the dashboard HTML into page. Leaves page empty on overflow. */
void ui_build_page(char *page, size_t page_size, int port,
                   uint32_t firmware_version);

#endif
