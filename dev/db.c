/*
 * db.c - Welcome Hub database access (app.db).
 * Reading/writing the "visible" flag and the one-time activation fix.
 */
#include <stdio.h>
#include <string.h>

#include "sqlite/sqlite3.h"
#include "config.h"
#include "db.h"

int welcomehub_get_visible(const char *database_path, int *visible,
                                 char *error_message, size_t error_message_size) {
    sqlite3 *db = NULL;
    sqlite3_stmt *table_stmt = NULL;
    int rc;
    int found = 0;

    *visible = 0;
    rc = sqlite3_open_v2(database_path, &db,
                         SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, NULL);
    if(rc != SQLITE_OK) {
        if(db) {
            snprintf(error_message, error_message_size, "Open app.db failed: %s",
                     sqlite3_errmsg(db));
            sqlite3_close(db);
            return -1;
        }
        snprintf(error_message, error_message_size, "Open app.db failed: %d", rc);
        return -1;
    }
    sqlite3_busy_timeout(db, 5000);

    rc = sqlite3_prepare_v2(db,
        "SELECT name FROM sqlite_master WHERE type = 'table' AND name GLOB 'tbl_iconinfo_*' ORDER BY name",
        -1, &table_stmt, NULL);
    if(rc != SQLITE_OK) {
        snprintf(error_message, error_message_size, "Find icon tables failed: %s",
                 sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    while((rc = sqlite3_step(table_stmt)) == SQLITE_ROW) {
        const unsigned char *table_name = sqlite3_column_text(table_stmt, 0);
        sqlite3_stmt *check_stmt = NULL;
        char *query = NULL;
        int check_rc;

        if(!table_name) {
            continue;
        }

        query = sqlite3_mprintf("SELECT \"visible\" FROM \"%w\" WHERE \"titleId\" = '" WELCOMEHUB_TITLE_ID "' LIMIT 1",
                                table_name);
        if(!query) {
            snprintf(error_message, error_message_size, "Out of memory reading visibility.");
            sqlite3_finalize(table_stmt);
            sqlite3_close(db);
            return -1;
        }

        check_rc = sqlite3_prepare_v2(db, query, -1, &check_stmt, NULL);
        sqlite3_free(query);
        if(check_rc != SQLITE_OK) {
            snprintf(error_message, error_message_size, "Prepare visibility query failed: %s",
                     sqlite3_errmsg(db));
            sqlite3_finalize(table_stmt);
            sqlite3_close(db);
            return -1;
        }

        check_rc = sqlite3_step(check_stmt);
        if(check_rc == SQLITE_ROW) {
            *visible = sqlite3_column_int(check_stmt, 0);
            found = 1;
        }
        sqlite3_finalize(check_stmt);
        if(found) {
            break;
        }
    }

    sqlite3_finalize(table_stmt);
    sqlite3_close(db);

    if(!found) {
        snprintf(error_message, error_message_size, "Welcome Hub entry not found.");
        return -1;
    }

    return 0;
}

int welcomehub_set_visible(const char *database_path, int visible,
                          char *error_message, size_t error_message_size) {
    sqlite3 *db = NULL;
    sqlite3_stmt *table_stmt = NULL;
    sqlite3_stmt *update_stmt = NULL;
    int rc;

    rc = sqlite3_open_v2(database_path, &db,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL);
    if(rc != SQLITE_OK) {
        if(db) {
            snprintf(error_message, error_message_size, "Open app.db failed: %s",
                     sqlite3_errmsg(db));
            sqlite3_close(db);
            return -1;
        }
        snprintf(error_message, error_message_size, "Open app.db failed: %d", rc);
        return -1;
    }
    sqlite3_busy_timeout(db, 5000);

    rc = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    if(rc != SQLITE_OK) {
        snprintf(error_message, error_message_size, "Begin transaction failed: %s",
                 sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    rc = sqlite3_prepare_v2(db,
        "SELECT name FROM sqlite_master WHERE type = 'table' AND name GLOB 'tbl_iconinfo_*' ORDER BY name",
        -1, &table_stmt, NULL);
    if(rc != SQLITE_OK) {
        snprintf(error_message, error_message_size, "Find icon tables failed: %s",
                 sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        sqlite3_close(db);
        return -1;
    }

    while((rc = sqlite3_step(table_stmt)) == SQLITE_ROW) {
        const unsigned char *table_name = sqlite3_column_text(table_stmt, 0);
        char *query = NULL;

        if(!table_name) {
            continue;
        }

        query = sqlite3_mprintf("UPDATE \"%w\" SET \"visible\" = %d WHERE \"titleId\" = '" WELCOMEHUB_TITLE_ID "' AND \"visible\" != %d",
                                table_name, visible, visible);
        if(!query) {
            snprintf(error_message, error_message_size, "Out of memory preparing icon table update.");
            sqlite3_finalize(table_stmt);
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            sqlite3_close(db);
            return -1;
        }

        rc = sqlite3_prepare_v2(db, query, -1, &update_stmt, NULL);
        sqlite3_free(query);
        if(rc != SQLITE_OK) {
            snprintf(error_message, error_message_size, "Prepare update failed: %s",
                     sqlite3_errmsg(db));
            sqlite3_finalize(table_stmt);
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            sqlite3_close(db);
            return -1;
        }

        rc = sqlite3_step(update_stmt);
        sqlite3_finalize(update_stmt);
        update_stmt = NULL;
        if(rc != SQLITE_DONE) {
            snprintf(error_message, error_message_size, "Update icon table failed: %s",
                     sqlite3_errmsg(db));
            sqlite3_finalize(table_stmt);
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            sqlite3_close(db);
            return -1;
        }
    }

    if(rc != SQLITE_DONE) {
        snprintf(error_message, error_message_size, "Read icon tables failed: %s",
                 sqlite3_errmsg(db));
        sqlite3_finalize(table_stmt);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        sqlite3_close(db);
        return -1;
    }
    sqlite3_finalize(table_stmt);

    rc = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    if(rc != SQLITE_OK) {
        snprintf(error_message, error_message_size, "Commit update failed: %s",
                 sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        sqlite3_close(db);
        return -1;
    }

    sqlite3_close(db);
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
process_iconinfo_table(sqlite3 *db, const char *table_name, int *has_target, char *error_message,
					   size_t error_message_size) {
	char *sql = NULL;
	sqlite3_stmt *statement = NULL;
	int has_required_columns = 0;
	int visible;
	int result;

	*has_target = 0;
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
	result = sqlite3_bind_text(statement, 1, WELCOMEHUB_TITLE_ID, -1, SQLITE_STATIC);
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
				 "Multiple " WELCOMEHUB_TITLE_ID " entries were found in %s.", table_name);
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
				 WELCOMEHUB_TITLE_ID " has an unexpected visible value in %s.", table_name);
		return -1;
	}
	*has_target = 1;
	/*
	 * Preserve the user's last chosen state. Re-launching the payload must not
	 * silently re-enable Welcome Hub if it was deliberately turned off.
	 */
	return 0;

fail:
	if(statement) {
		sqlite3_finalize(statement);
	}
	sqlite3_free(sql);
	return -1;
}


static int
ensure_contentinfo_entry(sqlite3 *db, char *error_message,
						size_t error_message_size) {
	static const char table_name[] = "tbl_contentinfo";
	char *sql = sqlite3_mprintf(
		"SELECT 1 FROM \"%w\" WHERE \"titleId\" = ?1",
		table_name);
	sqlite3_stmt *statement = NULL;
	int exists = 0;
	int result;

	if(!sql) {
		snprintf(error_message, error_message_size,
				 "Out of memory checking content info table.");
		return -1;
	}

	result = sqlite3_prepare_v2(db, sql, -1, &statement, NULL);
	sqlite3_free(sql);
	sql = NULL;
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size,
					 "Prepare content info query failed", db);
		goto fail;
	}
	result = sqlite3_bind_text(statement, 1, WELCOMEHUB_TITLE_ID, -1, SQLITE_STATIC);
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size,
					 "Bind Welcome Hub title ID failed", db);
		goto fail;
	}
	result = sqlite3_step(statement);
	if(result == SQLITE_ROW) {
		exists = 1;
	} else if(result != SQLITE_DONE) {
		set_sqlite_error(error_message, error_message_size,
					 "Read content info query failed", db);
		goto fail;
	}
	if(sqlite3_finalize(statement) != SQLITE_OK) {
		statement = NULL;
		set_sqlite_error(error_message, error_message_size,
					 "Finish content info query failed", db);
		goto fail;
	}
	statement = NULL;

	if(exists) {
		sql = sqlite3_mprintf(
			"UPDATE \"%w\" SET \"titleName\" = 'Welcome', "
			"\"installStatus\" = 0, \"dispLocation\" = 147, "
			"\"contentStatus\" = 0, \"viewCategory\" = 'game', "
			"\"categoryType\" = 67108864, \"localConceptId\" = 'cid:local:" WELCOMEHUB_TITLE_ID "' "
			"WHERE \"titleId\" = ?1",
			table_name);
	} else {
		sql = sqlite3_mprintf(
			"INSERT INTO \"%w\" (\"titleId\", \"titleName\", \"installStatus\", "
			"\"dispLocation\", \"contentStatus\", \"viewCategory\", "
			"\"categoryType\", \"localConceptId\") "
			"VALUES (?1, 'Welcome', 0, 147, 0, 'game', 67108864, 'cid:local:" WELCOMEHUB_TITLE_ID "')",
			table_name);
	}
	if(!sql) {
		snprintf(error_message, error_message_size,
				 "Out of memory preparing content info update.");
		return -1;
	}
	result = sqlite3_prepare_v2(db, sql, -1, &statement, NULL);
	sqlite3_free(sql);
	sql = NULL;
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size,
					 "Prepare content info update failed", db);
		goto fail;
	}
	result = sqlite3_bind_text(statement, 1, WELCOMEHUB_TITLE_ID, -1, SQLITE_STATIC);
	if(result != SQLITE_OK) {
		set_sqlite_error(error_message, error_message_size,
					 "Bind Welcome Hub title ID failed", db);
		goto fail;
	}
	result = sqlite3_step(statement);
	if(result != SQLITE_DONE) {
		set_sqlite_error(error_message, error_message_size,
					 "Apply content info fix failed", db);
		goto fail;
	}
	if(sqlite3_finalize(statement) != SQLITE_OK) {
		statement = NULL;
		set_sqlite_error(error_message, error_message_size,
					 "Finish content info update failed", db);
		goto fail;
	}
	statement = NULL;
	return 0;

fail:
	if(statement) {
		sqlite3_finalize(statement);
	}
	sqlite3_free(sql);
	return -1;
}


int
welcomehub_activate(const char *database_path, char *error_message, size_t error_message_size) {
	static const char query[] =
		"SELECT name FROM sqlite_master "
		"WHERE type = 'table' AND name GLOB 'tbl_iconinfo_*'";
	sqlite3 *db = NULL;
	sqlite3_stmt *statement = NULL;
	int transaction_open = 0;
	int table_count = 0;
	int target_count = 0;
	int result;

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

		if(!table_name) {
			snprintf(error_message, error_message_size,
					 "Could not read an icon table name.");
			goto fail;
		}
		table_count++;
		if(process_iconinfo_table(db, (const char *)table_name, &has_target,
							  error_message, error_message_size) != 0) {
			goto fail;
		}
		target_count += has_target;
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
				 WELCOMEHUB_TITLE_ID " was not found in any tbl_iconinfo_ table.");
		goto fail;
	}

	if(ensure_contentinfo_entry(db, error_message, error_message_size) != 0) {
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


