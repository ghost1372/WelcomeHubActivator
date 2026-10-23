#ifndef WELCOMEHUB_CONFIG_H
#define WELCOMEHUB_CONFIG_H

/*
 * Central place for every project-specific string and constant.
 * Change them here; nothing else in the code base hard-codes them.
 */

/* Version is normally injected by the Makefile (-DWELCOMEHUB_PAYLOAD_VERSION). */
#ifndef WELCOMEHUB_PAYLOAD_VERSION
#define WELCOMEHUB_PAYLOAD_VERSION "dev"
#endif

/* Branding (app name, title, author, repo link) lives directly in
 * web/index.html, which is embedded into the payload at build time. */

/* Name shown in the PS5 running-process list (thread name, max 19 chars). */
#ifndef PAYLOAD_PROCESS_NAME
#define PAYLOAD_PROCESS_NAME    "WelcomeHub.elf"
#endif

/* Web server */
#define WELCOMEHUB_WEB_PORT     7777

/* Console database */
#define WELCOMEHUB_DB_PATH      "/system_data/priv/mms/app.db"
#define WELCOMEHUB_DB_BACKUP    "/system_data/priv/mms/app.bckdb"
#define WELCOMEHUB_TITLE_ID     "NPXS40063"

/* First firmware (major.minor, BCD) that needs the activation fix: 10.00 */
#define WELCOMEHUB_MIN_FW       0x0A000000u

#endif
