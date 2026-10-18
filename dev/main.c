/*
 * MIT License
 *
 * Copyright (c) 2026 Mahdi Hosseini (@ghost1372)
 * Project: WelcomeHubActivator
 * Repository: https://github.com/ghost1372/WelcomeHubActivator
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ps5/kernel.h>
#include "sqlite/sqlite3.h"


typedef struct notify_request {
	char useless1[45];
	char message[3075];
} notify_request_t;


int sceKernelSendNotificationRequest(int, notify_request_t*, size_t, int);


static int
send_notification(const char *message, uint32_t firmware_version) {
	notify_request_t req = {0};
	unsigned int major = (((firmware_version >> 28) & 0x0f) * 10) +
						((firmware_version >> 24) & 0x0f);
	unsigned int minor = (((firmware_version >> 20) & 0x0f) * 10) +
						((firmware_version >> 16) & 0x0f);

	snprintf(req.message, sizeof req.message,
			 "%s\nPlayStation 5 FW: %u.%02u\nBy Mahdi Hosseini",
			 message, major, minor);
	return sceKernelSendNotificationRequest(0, &req, sizeof req, 0);
}


static int
copy_file(const char *source_path, const char *destination_path) {
	char buffer[16384];
	struct stat source_stat;
	int source_fd = open(source_path, O_RDONLY);
	int destination_fd;
	int saved_errno = 0;

	if(source_fd < 0) {
		return -1;
	}
	if(fstat(source_fd, &source_stat) != 0) {
		saved_errno = errno;
		close(source_fd);
		errno = saved_errno;
		return -1;
	}

	destination_fd = open(destination_path, O_WRONLY | O_CREAT | O_EXCL,
						  source_stat.st_mode & 0777);
	if(destination_fd < 0) {
		saved_errno = errno;
		close(source_fd);
		errno = saved_errno;
		return -1;
	}

	for(;;) {
		ssize_t bytes_read = read(source_fd, buffer, sizeof buffer);
		ssize_t offset = 0;

		if(bytes_read < 0) {
			if(errno == EINTR) {
				continue;
			}
			saved_errno = errno;
			break;
		}
		if(bytes_read == 0) {
			break;
		}

		while(offset < bytes_read) {
			ssize_t bytes_written = write(destination_fd, buffer + offset,
										  (size_t)(bytes_read - offset));
			if(bytes_written < 0) {
				if(errno == EINTR) {
					continue;
				}
				saved_errno = errno;
				break;
			}
			if(bytes_written == 0) {
				saved_errno = EIO;
				break;
			}
			offset += bytes_written;
		}
		if(saved_errno) {
			break;
		}
	}

	if(!saved_errno && fchmod(destination_fd, source_stat.st_mode & 0777) != 0) {
		saved_errno = errno;
	}
	if(close(source_fd) != 0 && !saved_errno) {
		saved_errno = errno;
	}
	if(close(destination_fd) != 0 && !saved_errno) {
		saved_errno = errno;
	}

	if(saved_errno) {
		unlink(destination_path);
		errno = saved_errno;
		return -1;
	}
	return 0;
}


static int
set_sqlite_error(char *message, size_t message_size, const char *operation,
				 sqlite3 *db) {
	snprintf(message, message_size, "%s: %s", operation, sqlite3_errmsg(db));
	return -1;
}


static void
append_error(char *message, size_t message_size, const char *context,
			 const char *detail) {
	size_t used = strlen(message);

	if(used < message_size) {
		snprintf(message + used, message_size - used, "%s%s", context, detail);
	}
}


static int
table_has_required_columns(sqlite3 *db, const char *table_name,
						   int *has_required_columns, char *error_message,
						   size_t error_message_size) {
	char *sql = sqlite3_mprintf("PRAGMA table_info(\"%w\")", table_name);
	sqlite3_stmt *statement = NULL;
	int has_title_id = 0;
	int has_visible = 0;
	int result;

	if(!sql) {
		snprintf(error_message, error_message_size, "Out of memory reading table schema.");
		return -1;
	}

	result = sqlite3_prepare_v2(db, sql, -1, &statement, NULL);
	sqlite3_free(sql);
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size, "Read table schema failed", db);
		sqlite3_finalize(statement);
		return -1;
	}

	while((result = sqlite3_step(statement)) == SQLITE_ROW) {
		const unsigned char *column_name = sqlite3_column_text(statement, 1);

		if(column_name && sqlite3_stricmp((const char *)column_name, "titleId") == 0) {
			has_title_id = 1;
		}
		if(column_name && sqlite3_stricmp((const char *)column_name, "visible") == 0) {
			has_visible = 1;
		}
	}
	if(result != SQLITE_DONE) {
		set_sqlite_error(error_message, error_message_size, "Read table schema failed", db);
		sqlite3_finalize(statement);
		return -1;
	}

	result = sqlite3_finalize(statement);
	if(result != SQLITE_OK) {
		return set_sqlite_error(error_message, error_message_size,
								"Finish reading table schema failed", db);
	}

	*has_required_columns = has_title_id && has_visible;
	return 0;
}


static int
process_iconinfo_table(sqlite3 *db, const char *table_name, int *has_target,
					   int *changed, char *error_message,
					   size_t error_message_size) {
	char *sql = NULL;
	sqlite3_stmt *statement = NULL;
	int has_required_columns = 0;
	int visible;
	int result;

	*has_target = 0;
	*changed = 0;
	if(table_has_required_columns(db, table_name, &has_required_columns,
								  error_message, error_message_size) != 0) {
		return -1;
	}
	if(!has_required_columns) {
		return 0;
	}

	sql = sqlite3_mprintf(
		"SELECT \"visible\" FROM \"%w\" WHERE \"titleId\" = ?1",
		table_name);
	if(!sql) {
		snprintf(error_message, error_message_size,
				 "Out of memory checking icon table.");
		return -1;
	}
	result = sqlite3_prepare_v2(db, sql, -1, &statement, NULL);
	sqlite3_free(sql);
	sql = NULL;
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size,
						 "Prepare icon table query failed", db);
		goto fail;
	}
	result = sqlite3_bind_text(statement, 1, "NPXS40063", -1, SQLITE_STATIC);
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size,
						 "Bind Welcome Hub title ID failed", db);
		goto fail;
	}
	result = sqlite3_step(statement);
	if(result == SQLITE_DONE) {
		result = sqlite3_finalize(statement);
		statement = NULL;
		if(result != SQLITE_OK) {
			return set_sqlite_error(error_message, error_message_size,
								"Finish icon table query failed", db);
		}
		return 0;
	}
	if(result != SQLITE_ROW) {
		set_sqlite_error(error_message, error_message_size,
						 "Read icon table query failed", db);
		goto fail;
	}
	visible = sqlite3_column_int(statement, 0);
	result = sqlite3_step(statement);
	if(result == SQLITE_ROW) {
		snprintf(error_message, error_message_size,
				 "Multiple NPXS40063 entries were found in %s.", table_name);
		goto fail;
	}
	if(result != SQLITE_DONE) {
		set_sqlite_error(error_message, error_message_size,
						 "Read icon table query failed", db);
		goto fail;
	}
	result = sqlite3_finalize(statement);
	statement = NULL;
	if(result != SQLITE_OK) {
		return set_sqlite_error(error_message, error_message_size,
							"Finish icon table query failed", db);
	}
	if(visible != 0 && visible != 1) {
		snprintf(error_message, error_message_size,
				 "NPXS40063 has an unexpected visible value in %s.", table_name);
		return -1;
	}
	*has_target = 1;
	if(visible == 1) {
		return 0;
	}

	sql = sqlite3_mprintf(
		"UPDATE \"%w\" SET \"visible\" = 1 "
		"WHERE \"titleId\" = ?1 AND \"visible\" = 0", table_name);
	if(!sql) {
		snprintf(error_message, error_message_size,
				 "Out of memory preparing icon table update.");
		return -1;
	}
	result = sqlite3_prepare_v2(db, sql, -1, &statement, NULL);
	sqlite3_free(sql);
	sql = NULL;
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size,
						 "Prepare icon table update failed", db);
		goto fail;
	}
	result = sqlite3_bind_text(statement, 1, "NPXS40063", -1, SQLITE_STATIC);
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size,
						 "Bind Welcome Hub title ID failed", db);
		goto fail;
	}
	result = sqlite3_step(statement);
	if(result != SQLITE_DONE) {
		set_sqlite_error(error_message, error_message_size,
						 "Update icon table visibility failed", db);
		goto fail;
	}
	result = sqlite3_changes(db);
	if(sqlite3_finalize(statement) != SQLITE_OK) {
		statement = NULL;
		set_sqlite_error(error_message, error_message_size,
						 "Finish icon table update failed", db);
		goto fail;
	}
	statement = NULL;
	if(result != 1) {
		snprintf(error_message, error_message_size,
				 "Icon table %s update affected %d rows; expected one.",
				 table_name, result);
		goto fail;
	}
	*changed = 1;
	return 0;

fail:
	if(statement) {
		sqlite3_finalize(statement);
	}
	sqlite3_free(sql);
	return -1;
}


static int
activate_welcome_hub(const char *database_path, int *already_activated,
						 char *error_message, size_t error_message_size) {
	static const char query[] =
		"SELECT name FROM sqlite_master "
		"WHERE type = 'table' AND name GLOB 'tbl_iconinfo_*'";
	sqlite3 *db = NULL;
	sqlite3_stmt *statement = NULL;
	int transaction_open = 0;
	int table_count = 0;
	int target_count = 0;
	int updated_count = 0;
	int result;

	*already_activated = 0;
	result = sqlite3_open_v2(database_path, &db,
							 SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL);
	if(result != SQLITE_OK) {
		if(db) {
			set_sqlite_error(error_message, error_message_size, "Open app.db failed", db);
			sqlite3_close(db);
		} else {
			snprintf(error_message, error_message_size,
					 "Open app.db failed (SQLite error %d).", result);
		}
		return -1;
	}

	result = sqlite3_busy_timeout(db, 5000);
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size,
						 "Set database timeout failed", db);
		goto fail;
	}
	result = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size,
						 "Begin database transaction failed", db);
		goto fail;
	}
	transaction_open = 1;

	result = sqlite3_prepare_v2(db, query, -1, &statement, NULL);
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size,
						 "Find icon tables failed", db);
		goto fail;
	}
	while((result = sqlite3_step(statement)) == SQLITE_ROW) {
		const unsigned char *table_name = sqlite3_column_text(statement, 0);
		int has_target = 0;
		int changed = 0;

		if(!table_name) {
			snprintf(error_message, error_message_size,
					 "Could not read an icon table name.");
			goto fail;
		}
		table_count++;
		if(process_iconinfo_table(db, (const char *)table_name, &has_target,
							  &changed, error_message, error_message_size) != 0) {
			goto fail;
		}
		target_count += has_target;
		updated_count += changed;
	}
	if(result != SQLITE_DONE) {
		set_sqlite_error(error_message, error_message_size,
						 "Read icon tables failed", db);
		goto fail;
	}
	result = sqlite3_finalize(statement);
	statement = NULL;
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size,
						 "Finish reading icon tables failed", db);
		goto fail;
	}
	if(table_count == 0) {
		snprintf(error_message, error_message_size,
				 "No tbl_iconinfo_ tables were found.");
		goto fail;
	}
	if(target_count == 0) {
		snprintf(error_message, error_message_size,
				 "NPXS40063 was not found in any tbl_iconinfo_ table.");
		goto fail;
	}

	result = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size,
						 "Commit Welcome Hub update failed", db);
		goto fail;
	}
	transaction_open = 0;
	result = sqlite3_close(db);
	if(result != SQLITE_OK) {
		snprintf(error_message, error_message_size,
				 "Close app.db failed: %s", sqlite3_errstr(result));
		return -1;
	}
	*already_activated = updated_count == 0;
	return 0;

fail:
	if(statement) {
		sqlite3_finalize(statement);
	}
	if(transaction_open) {
		char *rollback_error = NULL;
		result = sqlite3_exec(db, "ROLLBACK", NULL, NULL, &rollback_error);
		if(result != SQLITE_OK) {
			append_error(error_message, error_message_size,
							 "; rollback failed: ",
							 rollback_error ? rollback_error : sqlite3_errmsg(db));
		}
		sqlite3_free(rollback_error);
	}
	result = sqlite3_close(db);
	if(result != SQLITE_OK) {
		append_error(error_message, error_message_size,
					 "; close failed: ", sqlite3_errstr(result));
	}
	return -1;
}


int
main(void) {
	static const char database_path[] = "/system_data/priv/mms/app.db";
	static const char backup_path[] = "/system_data/priv/mms/app.bckdb";
	uint32_t firmware_version = kernel_get_fw_version();
	struct stat backup_stat;
	int already_activated;

	if(firmware_version < 0x0A000000u) {
		return send_notification("Firmware not supported. Requires 10.00 or higher.",
								 firmware_version);
	}

	if(copy_file(database_path, backup_path) != 0) {
		int error = errno;
		if(error != EEXIST) {
			char message[256];
			snprintf(message, sizeof message, "Backup failed: %s", strerror(error));
			send_notification(message, firmware_version);
			return -1;
		}
		if(lstat(backup_path, &backup_stat) != 0 ||
		   !S_ISREG(backup_stat.st_mode) || backup_stat.st_size == 0) {
			return send_notification(
				"Existing app.bckdb is invalid; database unchanged.", firmware_version);
		}
	}

	{
		char error_message[256];
		if(activate_welcome_hub(database_path, &already_activated,
								error_message, sizeof error_message) != 0) {
			return send_notification(error_message, firmware_version);
		}
	}

	if(already_activated) {
		return send_notification("Welcome Hub is already activated.", firmware_version);
	}
	return send_notification(
		"Welcome Hub activated. Please sign out and sign in again to see the changes.",
		firmware_version);
}

