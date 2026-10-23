/*
 * server.c - minimal single-threaded HTTP server for the Welcome Hub UI.
 * Routes: GET /api/state, GET /api/info, POST /api/toggle, GET / (web/index.html).
 */
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "config.h"
#include "db.h"
#include "server.h"

/*
 * Embed web/index.html into the payload at build time (no external tools
 * needed). Defines: const uint8_t web_index_html[]; const size_t web_index_html_size;
 * The path is relative to the directory `make` runs in.
 */
#define INCASSET(name, file)                    \
    __asm__(".section .rodata\n"                \
            ".global " #name "\n"               \
            ".global " #name "_end\n"           \
            ".global " #name "_size\n"          \
            ".align 16\n"                       \
            #name ":\n"                         \
            ".incbin \"" file "\"\n"            \
            #name "_end:\n"                     \
            #name "_size:\n"                    \
            ".quad " #name "_end - " #name "\n" \
            ".previous\n");                     \
    extern const uint8_t name[];                \
    extern const size_t name##_size;

INCASSET(web_index_html, "web/index.html");

static void get_active_local_ip(char *buffer, size_t buffer_size) {
    struct ifaddrs *if_list = NULL;
    struct ifaddrs *entry = NULL;
    struct in_addr loopback_addr = {0};

    if(buffer_size == 0) {
        return;
    }
    buffer[0] = '\0';

    if(getifaddrs(&if_list) != 0) {
        snprintf(buffer, buffer_size, "127.0.0.1");
        return;
    }

    loopback_addr.s_addr = htonl(INADDR_LOOPBACK);

    for(entry = if_list; entry != NULL; entry = entry->ifa_next) {
        struct sockaddr_in *addr = NULL;
        char candidate[64];

        if(entry->ifa_addr == NULL || entry->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        addr = (struct sockaddr_in *)entry->ifa_addr;
        if(addr->sin_addr.s_addr == loopback_addr.s_addr) {
            continue;
        }
        if(addr->sin_addr.s_addr == 0u) {
            continue;
        }

        if(inet_ntop(AF_INET, &addr->sin_addr, candidate, sizeof(candidate)) != NULL) {
            snprintf(buffer, buffer_size, "%s", candidate);
            freeifaddrs(if_list);
            return;
        }
    }

    for(entry = if_list; entry != NULL; entry = entry->ifa_next) {
        struct sockaddr_in *addr = NULL;
        char candidate[64];

        if(entry->ifa_addr == NULL || entry->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        addr = (struct sockaddr_in *)entry->ifa_addr;
        if(inet_ntop(AF_INET, &addr->sin_addr, candidate, sizeof(candidate)) != NULL) {
            snprintf(buffer, buffer_size, "%s", candidate);
            freeifaddrs(if_list);
            return;
        }
    }

    freeifaddrs(if_list);
    snprintf(buffer, buffer_size, "127.0.0.1");
}

static void format_firmware_version_string(char *buffer, size_t buffer_size,
                                          uint32_t firmware_version) {
    unsigned int major_tens;
    unsigned int major_ones;
    unsigned int minor_tens;
    unsigned int minor_ones;
    unsigned int major;
    unsigned int minor;

    if(buffer_size == 0) {
        return;
    }
    buffer[0] = '\0';

    if(firmware_version == 0u) {
        snprintf(buffer, buffer_size, "00.00");
        return;
    }

    major_tens = (firmware_version >> 28) & 0xFu;
    major_ones = (firmware_version >> 24) & 0xFu;
    minor_tens = (firmware_version >> 20) & 0xFu;
    minor_ones = (firmware_version >> 16) & 0xFu;

    major = (major_tens * 10u) + major_ones;
    minor = (minor_tens * 10u) + minor_ones;

    snprintf(buffer, buffer_size, "%02u.%02u", major, minor);
}

static void json_escape_string(char *out, size_t out_size, const char *in) {
    size_t pos = 0;

    if(out_size == 0) {
        return;
    }
    out[0] = '\0';
    if(in == NULL) {
        return;
    }

    while(*in != '\0' && pos + 7 < out_size) {
        unsigned char ch = (unsigned char)*in;

        switch(ch) {
            case '\\': out[pos++] = '\\'; out[pos++] = '\\'; break;
            case '"': out[pos++] = '\\'; out[pos++] = '"'; break;
            case '\n': out[pos++] = '\\'; out[pos++] = 'n'; break;
            case '\r': out[pos++] = '\\'; out[pos++] = 'r'; break;
            case '\t': out[pos++] = '\\'; out[pos++] = 't'; break;
            case '\f': out[pos++] = '\\'; out[pos++] = 'f'; break;
            case '\b': out[pos++] = '\\'; out[pos++] = 'b'; break;
            default:
                if(ch < 0x20u) {
                    int written = snprintf(out + pos, out_size - pos, "\\u%04x", ch);
                    if(written > 0 && (size_t)written < out_size - pos) {
                        pos += (size_t)written;
                    } else {
                        out[pos] = '\0';
                        return;
                    }
                } else {
                    out[pos++] = (char)ch;
                }
                break;
        }
        in++;
    }

    out[pos] = '\0';
}

static void send_all(int fd, const char *data, size_t len) {
    while(len > 0) {
        ssize_t n = send(fd, data, len, 0);
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            return;
        }
        if(n == 0) {
            return;
        }
        data += n;
        len -= (size_t)n;
    }
}

static void send_http_response(int fd, const char *body, size_t length,
                              int status_code, const char *content_type) {
    char header[512];
    int header_len;

    header_len = snprintf(header, sizeof(header),
                          "HTTP/1.1 %d %s\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %zu\r\n"
                          "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
                          "Pragma: no-cache\r\n"
                          "Expires: 0\r\n"
                          "Connection: close\r\n\r\n",
                          status_code, status_code == 200 ? "OK" : "Error",
                          content_type, length);
    if(header_len > 0) {
        send_all(fd, header, (size_t)header_len);
    }
    if(length > 0) {
        send_all(fd, body, length);
    }
}

/*
 * Read a full HTTP request: headers plus Content-Length body. A single recv()
 * is not enough because browsers may send headers and body in separate packets.
 * Returns bytes read, or -1 on failure. *body_out points at the body (or NULL).
 */
static int read_full_request(int fd, char *buf, size_t size, char **body_out) {
    size_t used = 0;
    char *hdr_end = NULL;
    size_t content_length = 0;

    *body_out = NULL;
    while(used < size - 1) {
        ssize_t n = recv(fd, buf + used, size - 1 - used, 0);
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            break;
        }
        if(n == 0) {
            break;
        }
        used += (size_t)n;
        buf[used] = '\0';

        if(!hdr_end) {
            hdr_end = strstr(buf, "\r\n\r\n");
            if(hdr_end) {
                char *cl = strstr(buf, "Content-Length:");
                if(!cl) {
                    cl = strstr(buf, "content-length:");
                }
                if(cl && cl < hdr_end) {
                    content_length = (size_t)strtoul(cl + 15, NULL, 10);
                }
                hdr_end += 4;
            }
        }
        if(hdr_end && used >= (size_t)(hdr_end - buf) + content_length) {
            break;
        }
    }
    if(used == 0) {
        return -1;
    }
    buf[used] = '\0';
    if(hdr_end) {
        *body_out = hdr_end;
    }
    return (int)used;
}

static int parse_enabled(const char *body, int *value) {
    const char *p;

    if(!body) {
        return -1;
    }
    p = strstr(body, "\"enabled\"");
    if(!p) {
        return -1;
    }
    p = strchr(p, ':');
    if(!p) {
        return -1;
    }
    p++;
    while(*p == ' ' || *p == '\t') {
        p++;
    }
    if(*p == '1' || strncmp(p, "true", 4) == 0) {
        *value = 1;
        return 0;
    }
    if(*p == '0' || strncmp(p, "false", 5) == 0) {
        *value = 0;
        return 0;
    }
    return -1;
}

static int handle_http_request(int client_fd, const char *database_path, int port,
                              uint32_t firmware_version) {
    char request[4096];
    char *body = NULL;
    int visible = 0;
    char response[512];
    char error_message[256] = {0};
    char escaped[256];

    if(read_full_request(client_fd, request, sizeof(request), &body) < 0) {
        close(client_fd);
        return 0;
    }

    if(strncmp(request, "GET /api/state", 14) == 0) {
        if(welcomehub_get_visible(database_path, &visible, error_message,
                                  sizeof(error_message)) == 0) {
            snprintf(response, sizeof(response), "{\"enabled\":%s}",
                     visible ? "true" : "false");
        } else {
            json_escape_string(escaped, sizeof(escaped), error_message);
            snprintf(response, sizeof(response),
                     "{\"enabled\":false,\"error\":\"%s\"}", escaped);
        }
        send_http_response(client_fd, response, strlen(response), 200,
                           "application/json");
    } else if(strncmp(request, "GET /api/info", 13) == 0) {
        char ip[64];
        char firmware_label[16];

        get_active_local_ip(ip, sizeof(ip));
        format_firmware_version_string(firmware_label, sizeof(firmware_label),
                                       firmware_version);
        snprintf(response, sizeof(response),
                 "{\"ip\":\"%s\",\"port\":%d,\"firmware\":\"%s\","
                 "\"version\":\"%s\"}", ip, port, firmware_label,
                 WELCOMEHUB_PAYLOAD_VERSION);
        send_http_response(client_fd, response, strlen(response), 200,
                           "application/json");
    } else if(strncmp(request, "POST /api/toggle", 16) == 0) {
        int next_visible = 0;
        int after = 0;

        if(!strstr(request, "application/json") ||
           parse_enabled(body, &next_visible) != 0) {
            snprintf(response, sizeof(response),
                     "{\"ok\":false,\"enabled\":false,\"error\":\"Bad request.\"}");
            send_http_response(client_fd, response, strlen(response), 400,
                               "application/json");
        } else if(welcomehub_set_visible(database_path, next_visible, error_message,
                                         sizeof(error_message)) != 0 ||
                  welcomehub_get_visible(database_path, &after, error_message,
                                         sizeof(error_message)) != 0) {
            json_escape_string(escaped, sizeof(escaped), error_message);
            snprintf(response, sizeof(response),
                     "{\"ok\":false,\"enabled\":%s,\"error\":\"%s\"}",
                     next_visible ? "false" : "true", escaped);
            send_http_response(client_fd, response, strlen(response), 500,
                               "application/json");
        } else {
            snprintf(response, sizeof(response), "{\"ok\":true,\"enabled\":%s}",
                     after ? "true" : "false");
            send_http_response(client_fd, response, strlen(response), 200,
                               "application/json");
        }
    } else if(strncmp(request, "GET /", 5) == 0) {
        send_http_response(client_fd, (const char *)web_index_html,
                           web_index_html_size, 200, "text/html");
    } else {
        const char *not_found = "{\"ok\":false}";
        send_http_response(client_fd, not_found, strlen(not_found), 404,
                           "application/json");
    }

    close(client_fd);
    return 0;
}

int welcomehub_server_run(const char *database_path, int port,
                          uint32_t firmware_version) {
    int server_fd;
    struct sockaddr_in addr;
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        return -1;
    }

    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
        close(server_fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if(bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(server_fd);
        return -1;
    }

    if(listen(server_fd, 8) != 0) {
        close(server_fd);
        return -1;
    }

    /* A write to a closed socket must not kill the payload. */
    signal(SIGPIPE, SIG_IGN);

    for(;;) {
        struct timeval tv;
        int client_fd = accept(server_fd, NULL, NULL);
        if(client_fd < 0) {
            if(errno == EINTR) {
                continue;
            }
            close(server_fd);
            return -1;
        }

        /* An idle connection must not freeze the single-threaded server. */
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        handle_http_request(client_fd, database_path, port, firmware_version);
    }

    close(server_fd);
    return 0;
}

