#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>

#include "config.h"
#include "ui.h"


static const char UI_HTML_PREFIX[] =
    "<!doctype html>"
    "<html lang=\"en\">"
    "<head>"
    "  <meta charset=\"utf-8\">"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "  <meta name=\"theme-color\" content=\"#090b12\">"
    "  <link rel=\"icon\" href=\"data:,\">"
    "  <title>" WELCOMEHUB_APP_NAME "</title>"
    "  <style>"
    "    :root {"
    "      color-scheme: dark;"
    "      font-size: 17px;"
    "      font-size: clamp(17px, 1.5vw, 53px);"
    "      font-family: Inter, \"Segoe UI\", sans-serif;"
    "      --bg: #090b12;"
    "      --panel: rgba(18, 22, 34, .78);"
    "      --line: rgba(180, 193, 230, .12);"
    "      --muted: #8b93aa;"
    "      --text: #f2f5ff;"
    "      --cyan: #68e4ff;"
    "      --violet: #9b83ff;"
    "      --green: #72edb0;"
    "      --red: #ff758d;"
    "    }"
    ""
    "    * { box-sizing: border-box; }"
    ""
    "    body {"
    "      min-height: 100vh;"
    "      margin: 0;"
    "      display: grid;"
    "      place-items: center;"
    "      padding: 1.25rem 1rem;"
    "      overflow-x: hidden;"
    "      color: var(--text);"
    "      background-color: var(--bg);"
    "      background-image:"
    "        radial-gradient(ellipse at 14%% 8%%, rgba(69, 83, 173, .23), transparent 33rem),"
    "        radial-gradient(ellipse at 88%% 84%%, rgba(35, 157, 174, .13), transparent 30rem),"
    "        linear-gradient(rgba(255,255,255,.018) 1px, transparent 1px),"
    "        linear-gradient(90deg, rgba(255,255,255,.018) 1px, transparent 1px);"
    "      background-size: auto, auto, 2.875rem 2.875rem, 2.875rem 2.875rem;"
    "    }"
    ""
    ""
    "    .shell { width: min(100%%, 36rem); }"
    ""
    "    .topline { display: flex; justify-content: center; margin: 0 0 0.75rem; }"
    "    .fw-chip { display: inline-flex; align-items: center; gap: 0.75rem; padding: 0.5rem 1.125rem; border: 1px solid rgba(104,228,255,.22); border-radius: 62.4375rem; background: linear-gradient(110deg, rgba(104,228,255,.09), rgba(155,131,255,.08)); box-shadow: inset 0 1px rgba(255,255,255,.05), 0 0.5rem 1.5rem rgba(0,0,0,.25); font-size: 0.9375rem; }"
    "    .fw-chip-label { color: var(--muted); font-weight: 700; letter-spacing: .16em; text-transform: uppercase; }"
    "    .fw-chip-sep { width: 1px; height: 1.125rem; background: var(--line); }"
    "    .fw-chip-value { color: var(--cyan); font-family: \"SFMono-Regular\", Consolas, monospace; font-weight: 700; letter-spacing: .04em; }"
    ""
    "    .card {"
    "      position: relative;"
    "      overflow: hidden;"
    "      padding: 1.5rem;"
    "      border: 1px solid var(--line);"
    "      border-radius: 1.5rem;"
    "      background: linear-gradient(145deg, rgba(22, 27, 42, .92), rgba(13, 16, 25, .91));"
    "      box-shadow: 0 1rem 2.5rem rgba(0,0,0,.4), inset 0 1px rgba(255,255,255,.045);"
    "    }"
    ""
    "    .card::before {"
    "      position: absolute;"
    "      top: -6.875rem;"
    "      right: -5rem;"
    "      width: 15.625rem;"
    "      height: 15.625rem;"
    "      border-radius: 50%%;"
    "      background: radial-gradient(circle, rgba(105, 91, 255, .16), transparent 70%%);"
    "      content: \"\";"
    "      pointer-events: none;"
    "    }"
    ""
    "    .brand-row { position: relative; display: flex; align-items: center; gap: 0.9375rem; }"
    "    .brand-mark {"
    "      display: grid;"
    "      width: 3.25rem;"
    "      height: 3.25rem;"
    "      place-items: center;"
    "      border: 1px solid rgba(104,228,255,.28);"
    "      border-radius: 1rem;"
    "      color: var(--cyan);"
    "      background: linear-gradient(145deg, rgba(104,228,255,.14), rgba(155,131,255,.12));"
    "      box-shadow: inset 0 0 1.25rem rgba(104,228,255,.07);"
    "      font-size: 1.875rem;"
    "    }"
    ""
    "    h1 { margin: 0; font-size: 1.875rem; letter-spacing: -.04em; }"
    "    .version {"
    "      margin-left: 0.5rem;"
    "      color: var(--cyan);"
    "      font-size: 1rem;"
    "      font-weight: 600;"
    "      letter-spacing: .04em;"
    "      vertical-align: 0.1875rem;"
    "    }"
    ""
    "    .divider { height: 1px; margin: 1rem 0; background: var(--line); }"
    ""
    "    .status-line { display: flex; align-items: center; justify-content: space-between; gap: 1rem; }"
    "    .status-control { display: flex; align-items: center; gap: 0.75rem; min-width: 0; }"
    "    .eyebrow--compact {"
    "      margin: 0 0 0.25rem;"
    "      color: var(--muted);"
    "      font-size: 0.8125rem;"
    "      font-weight: 700;"
    "      letter-spacing: .2em;"
    "      text-transform: uppercase;"
    "    }"
    "    .state-pill {"
    "      display: inline-flex;"
    "      align-items: center;"
    "      gap: 0.5rem;"
    "      padding: 0.4375rem 0.625rem;"
    "      border: 1px solid rgba(255,117,141,.2);"
    "      border-radius: 62.4375rem;"
    "      color: var(--red);"
    "      background: rgba(255,117,141,.07);"
    "      font-size: 0.9375rem;"
    "      font-weight: 700;"
    "      letter-spacing: .12em;"
    "      transition: color .2s, background-color .2s, border-color .2s;"
    "    }"
    ""
    "    .state-pill::before {"
    "      width: 0.375rem;"
    "      height: 0.375rem;"
    "      border-radius: 50%%;"
    "      background: currentColor;"
    "      box-shadow: 0 0 0.5625rem currentColor;"
    "      content: \"\";"
    "    }"
    ""
    "    .is-active .state-pill {"
    "      border-color: rgba(114,237,176,.22);"
    "      color: var(--green);"
    "      background: rgba(114,237,176,.07);"
    "    }"
    ""
    "    .connection {"
    "      display: flex;"
    "      align-items: center;"
    "      justify-content: center;"
    "      margin-top: 0.9rem;"
    "      padding: 0.8rem;"
    "      border: 1px solid var(--line);"
    "      border-radius: 0.9375rem;"
    "      background: rgba(5, 8, 15, .38);"
    "      text-align: center;"
    "    }"
    ""
    "    .section-divider { height: 1px; margin: 0.8rem 0; background: var(--line); }"
    ""
    "    .connection-label {"
    "      display: inline-block;"
    "      margin-right: 0.5rem;"
    "      color: var(--muted);"
    "      font-size: 0.8125rem;"
    "      font-weight: 700;"
    "      letter-spacing: .16em;"
    "      text-transform: uppercase;"
    "    }"
    ""
    "    .connection-value {"
    "      font-family: \"SFMono-Regular\", Consolas, monospace;"
    "      font-size: 1.25rem;"
    "      font-weight: 600;"
    "      letter-spacing: .02em;"
    "    }"
    "    .toggle { position: relative; width: 4.75rem; height: 2.625rem; flex: 0 0 auto; margin: 0; -webkit-tap-highlight-color: transparent; touch-action: manipulation; }"
    "    .toggle input { position: absolute; width: 1px; height: 1px; opacity: 0; }"
    "    .toggle-track { position: absolute; inset: 0; cursor: pointer; border: 1px solid rgba(255,255,255,.12); border-radius: 6.1875rem; background: #292d3b; transition: border-color .2s; }"
    "    .toggle-track::before { position: absolute; inset: -1px; border-radius: inherit; background: linear-gradient(105deg, #476eff, #30c9df); box-shadow: 0 0 1.5rem rgba(66,183,255,.25); opacity: 0; transition: opacity .2s; will-change: opacity; content: \"\"; }"
    "    .toggle-track::after { position: absolute; top: 0.25rem; left: 0.25rem; width: 2rem; height: 2rem; border-radius: 50%%; background: #c0c5d4; box-shadow: 0 2px 0.5rem rgba(0,0,0,.35); transform: translateX(0); transition: transform .2s ease-out; will-change: transform; content: \"\"; }"
    "    .toggle input:checked + .toggle-track { border-color: rgba(104,228,255,.5); }"
    "    .toggle input:checked + .toggle-track::before { opacity: 1; }"
    "    .toggle input:checked + .toggle-track::after { transform: translateX(2.125rem); background: #fff; }"
    "    .toggle input:focus-visible + .toggle-track { outline: 2px solid var(--cyan); outline-offset: 0.25rem; }"
    "    .control-hint { margin: 0; color: var(--muted); font-size: 1rem; line-height: 1.4; }"
    "    .control-hint strong { color: var(--text); font-weight: 600; }"
    ""
    "    .message {"
    "      display: flex;"
    "      gap: 0.75rem;"
    "      margin-top: 0.9rem;"
    "      padding: 0.8rem;"
    "      border: 1px solid rgba(104,228,255,.15);"
    "      border-radius: 0.9375rem;"
    "      background: linear-gradient(110deg, rgba(104,228,255,.055), rgba(155,131,255,.045));"
    "    }"
    ""
    "    .message-icon { color: var(--cyan); font-size: 1.375rem; line-height: 1.25; }"
    "    .message h2 { margin: 0 0 0.3125rem; font-size: 1rem; letter-spacing: .04em; }"
    "    .message p { margin: 0; color: #a7aec2; font-size: 1rem; line-height: 1.55; }"
    ""
    "    footer {"
    "      display: flex;"
    "      justify-content: space-between;"
    "      gap: 0.75rem;"
    "      margin: 0.75rem 0.3125rem 0;"
    "      color: #747c91;"
    "      font-size: 0.9375rem;"
    "    }"
    ""
    "    footer a { color: var(--cyan); text-decoration: none; }"
    "    footer a:hover, footer a:focus { text-decoration: underline; }"
    "    footer strong { color: #aeb6cc; font-weight: 600; }"
    "    @media (max-width: 26.875rem) {"
    "      .card { padding: 1.2rem 1rem; }"
    "      .connection { gap: 0.5625rem; padding: 0.875rem 0.6875rem; }"
    "      .connection-value { font-size: 1.0625rem; }"
    "      footer { flex-direction: column; }"
    "    }"
    "  </style>"
    "</head>"
    "<body>"
    "  <main class=\"shell\">"
    "    <div class=\"topline\">"
    "      <span class=\"fw-chip\">"
    "        <span class=\"fw-chip-label\">PS5 Firmware</span>"
    "        <span class=\"fw-chip-sep\"></span>"
    "        <span class=\"fw-chip-value\">%s</span>"
    "      </span>"
    "    </div>"
    ""
    "    <section class=\"card\" id=\"panel\" aria-label=\"" WELCOMEHUB_APP_NAME " dashboard\">"
    "      <header class=\"brand-row\">"
    "        <div class=\"brand-mark\" aria-hidden=\"true\">W</div>"
    "        <div>"
    "          <p class=\"eyebrow--compact\">" WELCOMEHUB_UI_EYEBROW "</p>"
    "          <h1>%s <span class=\"version\">%s</span></h1>"
    "        </div>"
    "      </header>"
    ""
    "      <div class=\"divider\"></div>"
    ""
    "      <div class=\"connection\" aria-label=\"Access address\">"
    "        <div>"
    "          <span class=\"connection-label\">Access</span>"
    "          <span class=\"connection-value\">%s:%d</span>"
    "        </div>"
    "      </div>"
    ""
    "      <div class=\"section-divider\"></div>"
    ""
    "      <section aria-label=\"Status panel\">"
    "        <div class=\"status-line\">"
    "          <div class=\"status-control\">"
    "            <label class=\"toggle\" aria-label=\"Toggle activation state\">"
    "              <input id=\"statusToggle\" type=\"checkbox\" role=\"switch\" aria-checked=\"false\">"
    "              <span class=\"toggle-track\"></span>"
    "            </label>"
    "            <p class=\"control-hint\"><strong>Toggle</strong> to switch the state</p>"
    "          </div>"
    "          <span class=\"state-pill\" id=\"statePill\">DEACTIVATED</span>"
    "        </div>"
    "      </section>"
    ""
    "      <aside class=\"message\" aria-live=\"polite\">"
    "        <span class=\"message-icon\" aria-hidden=\"true\">✦</span>"
    "        <div>"
    "          <h2 id=\"messageTitle\">Note</h2>"
    "          <p id=\"messageText\">Sign out and sign in again to take effect.</p>"
    "        </div>"
    "      </aside>"
    "    </section>"
    ""
    "    <footer>"
    "      <span>Developed by <strong>" WELCOMEHUB_AUTHOR "</strong></span>"
    "      <a href=\"" WELCOMEHUB_REPO_URL "\">" WELCOMEHUB_REPO_PATH "</a>"
    "    </footer>"
    "  </main>"
    "  <script>"
    "    const panel = document.getElementById('panel');"
    "    const statusToggle = document.getElementById('statusToggle');"
    "    const statePill = document.getElementById('statePill');"
    "    const messageTitle = document.getElementById('messageTitle');"
    "    const messageText = document.getElementById('messageText');"
    "    function showMessage(title, text) {"
    "      messageTitle.textContent = title;"
    "      messageText.textContent = text;"
    "    }"
    "    function applyState(active, error) {"
    "      panel.classList.toggle('is-active', active);"
    "      statusToggle.checked = active;"
    "      statusToggle.setAttribute('aria-checked', String(active));"
    "      statePill.textContent = active ? 'ACTIVE' : 'DEACTIVATED';"
    "      if (error) {"
    "        showMessage('Error', error);"
    "      } else {"
    "        showMessage('Note', 'Sign out and sign in again to take effect.');"
    "      }"
    "    }"
    "    let confirmed = false, seq = 0;"
    "    fetch('/api/state').then(r => r.json()).then(d => { confirmed = Boolean(d.enabled); applyState(confirmed, d.error || ''); }).catch(() => applyState(false, 'Could not read state.'));"
    "    statusToggle.addEventListener('change', function() {"
    "      const active = this.checked, mine = ++seq;"
    "      applyState(active, '');"
    "      fetch('/api/toggle', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ enabled: active ? 1 : 0 }) })"
    "        .then(r => r.json().then(data => ({ ok: r.ok, data: data })))"
    "        .then(res => {"
    "          if (!res.ok || res.data.ok === false) { throw new Error(res.data.error || 'Toggle failed.'); }"
    "          confirmed = Boolean(res.data.enabled);"
    "          if (mine === seq) { applyState(confirmed, res.data.error || ''); }"
    "        })"
    "        .catch(err => { if (mine === seq) { applyState(confirmed, err && err.message ? err.message : 'Toggle failed.'); } });"
    "    });"
    "  </script>"
    "</body>"
    "</html>";


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

void ui_build_page(char *page, size_t page_size, int port,
                         uint32_t firmware_version) {
    char ip[64];
    char firmware_label[16];
    size_t used = 0;
    int written;

    get_active_local_ip(ip, sizeof(ip));
    format_firmware_version_string(firmware_label, sizeof(firmware_label),
                                  firmware_version);

    if(page_size == 0) {
        return;
    }

    page[0] = '\0';
    used = 0;

    written = snprintf(page + used, page_size - used, UI_HTML_PREFIX,
                       firmware_label, WELCOMEHUB_UI_TITLE,
                       WELCOMEHUB_PAYLOAD_VERSION, ip, port);
    if(written < 0 || (size_t)written >= page_size - used) {
        page[0] = '\0';
        return;
    }
    used += (size_t)written;
    page[used] = '\0';
}
