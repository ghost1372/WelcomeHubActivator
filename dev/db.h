#ifndef WELCOMEHUB_DB_H
#define WELCOMEHUB_DB_H

#include <stddef.h>

/* Read the Welcome Hub visible flag. Returns 0 on success. */
int welcomehub_get_visible(const char *database_path, int *visible,
                           char *error_message, size_t error_message_size);

/* Set the visible flag in every tbl_iconinfo_* table. Returns 0 on success. */
int welcomehub_set_visible(const char *database_path, int visible,
                           char *error_message, size_t error_message_size);

/* One-time fix: make sure the Welcome Hub entries exist. Returns 0 on success. */
int welcomehub_activate(const char *database_path, char *error_message, size_t error_message_size);

#endif
