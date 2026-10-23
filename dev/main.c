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
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ps5/kernel.h>
#include "config.h"
#include "db.h"
#include "server.h"

int sceUserServiceInitialize(void *arg);
int sceSystemServiceLaunchWebBrowser(const char *uri, void *arg);

static void launch_ps5_browser(void) {
	char url[128];
	int init_result;

	init_result = sceUserServiceInitialize(NULL);
	if(init_result != 0) {
		return;
	}

	snprintf(url, sizeof(url), "http://127.0.0.1:%d", WELCOMEHUB_WEB_PORT);
	(void)sceSystemServiceLaunchWebBrowser(url, NULL);
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



int
main(void) {
	static const char database_path[] = WELCOMEHUB_DB_PATH;
	static const char backup_path[] = WELCOMEHUB_DB_BACKUP;
	uint32_t firmware_version = kernel_get_fw_version();

	if(firmware_version >= WELCOMEHUB_MIN_FW) {
		char error_message[256];

		/* Best effort: EEXIST just means a backup is already there. */
		(void)copy_file(database_path, backup_path);
		/* Keep the UI available even if activation fails. */
		(void)welcomehub_activate(database_path, error_message, sizeof error_message);
	}

	launch_ps5_browser();
	welcomehub_server_run(database_path, WELCOMEHUB_WEB_PORT, firmware_version);
	return 0;
}
