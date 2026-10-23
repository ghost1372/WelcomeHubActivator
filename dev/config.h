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

/* Branding shown in the web UI */
#define WELCOMEHUB_APP_NAME     "WelcomeHubActivator"
#define WELCOMEHUB_UI_TITLE     "Activator"
#define WELCOMEHUB_UI_EYEBROW   "Welcome Hub"
#define WELCOMEHUB_AUTHOR       "Mahdi Hosseini"
#define WELCOMEHUB_REPO_PATH    "github.com/ghost1372/WelcomeHubActivator"
#define WELCOMEHUB_REPO_URL     "https://" WELCOMEHUB_REPO_PATH

/* Web server */
#define WELCOMEHUB_WEB_PORT     7777

/* Console database */
#define WELCOMEHUB_DB_PATH      "/system_data/priv/mms/app.db"
#define WELCOMEHUB_DB_BACKUP    "/system_data/priv/mms/app.bckdb"
#define WELCOMEHUB_TITLE_ID     "NPXS40063"

/* First firmware (major.minor, BCD) that needs the activation fix: 10.00 */
#define WELCOMEHUB_MIN_FW       0x0A000000u

#endif
