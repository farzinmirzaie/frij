#include "wifi.h"

#include <stdio.h>
#include <string.h>

/*
 * Wi-Fi is board-specific. Like brightness/haptics this file carries both
 * targets behind a guard: an in-memory mock for the emulator, and the real
 * radio (Arduino WiFi) on device.
 */
#if defined(__has_include) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))

// ---- emulator: in-memory mock ----------------------------------------------

static bool s_enabled = true;

// A believable neighbourhood. `known`/`connected` change as the user acts.
static frij_wifi_net_t s_nets[] = {
    {"StashAway",       -47, true,  true,  true },
    {"StashAway-Guest", -52, false, false, false},
    {"Linksys-5G",      -63, true,  true,  false},
    {"CoffeeBean WiFi", -71, true,  false, false},
    {"Pixel_4271",      -78, true,  false, false},
    {"Neighbour 2.4G",  -84, true,  false, false},
};
static const int S_N = (int)(sizeof(s_nets) / sizeof(s_nets[0]));

static frij_wifi_net_t* find(const char* ssid)
{
    for (int i = 0; i < S_N; i++) {
        if (strncmp(s_nets[i].ssid, ssid, FRIJ_WIFI_SSID_MAX) == 0) {
            return &s_nets[i];
        }
    }
    return NULL;
}

void frij_wifi_init(void) {}

bool frij_wifi_enabled(void)
{
    return s_enabled;
}

void frij_wifi_set_enabled(bool on)
{
    s_enabled = on;
    if (on) {
        // Auto-connect to a saved (known) network, mirroring the device.
        if (!frij_wifi_connected()) {
            for (int i = 0; i < S_N; i++) {
                if (s_nets[i].known) {
                    frij_wifi_connect(s_nets[i].ssid, NULL);
                    break;
                }
            }
        }
    } else {
        frij_wifi_disconnect();
    }
}

int frij_wifi_scan(frij_wifi_net_t* out, int max)
{
    if (!s_enabled) {
        return 0;
    }
    int n = S_N < max ? S_N : max;
    for (int i = 0; i < n; i++) {
        out[i] = s_nets[i];
    }
    // connected first, then strongest signal (small list -> simple bubble sort)
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            bool swap = (out[j + 1].connected && !out[j].connected) ||
                        (out[j + 1].connected == out[j].connected && out[j + 1].rssi > out[j].rssi);
            if (swap) {
                frij_wifi_net_t t = out[j];
                out[j]           = out[j + 1];
                out[j + 1]       = t;
            }
        }
    }
    return n;
}

const char* frij_wifi_connected(void)
{
    for (int i = 0; i < S_N; i++) {
        if (s_nets[i].connected) {
            return s_nets[i].ssid;
        }
    }
    return NULL;
}

bool frij_wifi_connect(const char* ssid, const char* password)
{
    (void)password;  // mock: credentials aren't checked on the emulator
    frij_wifi_net_t* net = find(ssid);
    if (!net) {
        return false;
    }
    frij_wifi_disconnect();
    net->known     = true;
    net->connected = true;
    return true;
}

void frij_wifi_disconnect(void)
{
    for (int i = 0; i < S_N; i++) {
        s_nets[i].connected = false;
    }
}

void frij_wifi_forget(const char* ssid)
{
    frij_wifi_net_t* net = find(ssid);
    if (net) {
        net->known     = false;
        net->connected = false;
    }
}

void frij_wifi_diag(char* buf, int n)
{
    snprintf(buf, n, "(mock: %d nets)", S_N);
}

// Mock async: everything completes instantly, so poll returns "done" right away.
void frij_wifi_scan_start(void) {}

int frij_wifi_scan_poll(frij_wifi_net_t* out, int max)
{
    return frij_wifi_scan(out, max);  // mock data is always ready
}

static frij_wifi_state_t s_mock_conn = FRIJ_WIFI_IDLE;

void frij_wifi_connect_start(const char* ssid, const char* password)
{
    s_mock_conn = frij_wifi_connect(ssid, password) ? FRIJ_WIFI_CONNECTED : FRIJ_WIFI_FAILED;
}

frij_wifi_state_t frij_wifi_connect_poll(void)
{
    return s_mock_conn;
}

#else

// ---- device: real radio (Arduino WiFi) + one saved network in NVS ----------
//
// Credentials live in NVS via Preferences (namespace "wifi": ssid + pw). We keep
// ONE saved network (the home AP) — NVS keys cap at 15 chars so we can't key by
// SSID, and a watch realistically pairs to one network. `known` = "this is the
// saved SSID". The radio scan/connect calls block for seconds, so the worker
// section at the bottom wraps them off the LVGL thread (scan_start/connect_start).

#include <Preferences.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static bool        s_enabled = false;
static Preferences s_prefs;

static bool saved_ssid(char* out, size_t n)
{
    s_prefs.begin("wifi", /*readOnly=*/true);
    String s = s_prefs.getString("ssid", "");
    s_prefs.end();
    if (s.length() == 0) {
        return false;
    }
    strncpy(out, s.c_str(), n - 1);
    out[n - 1] = '\0';
    return true;
}

void frij_wifi_init(void)
{
    WiFi.mode(WIFI_OFF);
}

bool frij_wifi_enabled(void)
{
    return s_enabled;
}

void frij_wifi_set_enabled(bool on)
{
    s_enabled = on;
    if (on) {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);  // keep the link up if it drops
        char ss[FRIJ_WIFI_SSID_MAX];
        if (saved_ssid(ss, sizeof(ss))) {
            // Auto-connect to the saved network — fire-and-forget so enabling
            // Wi-Fi (incl. at boot) never stalls the UI; setAutoReconnect retries.
            s_prefs.begin("wifi", true);
            String pw = s_prefs.getString("pw", "");
            s_prefs.end();
            WiFi.begin(ss, pw.c_str());
        }
    } else {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }
}

const char* frij_wifi_connected(void)
{
    static char buf[FRIJ_WIFI_SSID_MAX];
    if (WiFi.status() != WL_CONNECTED) {
        return NULL;
    }
    strncpy(buf, WiFi.SSID().c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    return buf;
}

static int s_last_raw    = -99;  // last scanNetworks() return (for frij_wifi_diag)
static int s_last_status = -99;  // last WiFi.status()

int frij_wifi_scan(frij_wifi_net_t* out, int max)
{
    if (!s_enabled) {
        s_last_raw = -100;  // "disabled"
        return 0;
    }
    // Make sure the STA driver is started; the ESP32 can scan while connected
    // (it briefly hops channels and returns). Do NOT WiFi.disconnect() here —
    // that dropped a live connection on every rescan (e.g. re-opening Network).
    WiFi.mode(WIFI_STA);
    int found = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);  // blocking ~1-2s
    if (found < 0) {
        // -1 running / -2 failed: power-cycle the radio (the reliable reset for a
        // wedged scan engine) and try once more.
        WiFi.scanDelete();
        WiFi.mode(WIFI_OFF);
        delay(200);
        WiFi.mode(WIFI_STA);
        delay(400);
        found = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
    }
    s_last_raw    = found;
    s_last_status = (int)WiFi.status();
    if (found <= 0) {
        return 0;
    }
    char        ss[FRIJ_WIFI_SSID_MAX];
    bool        has_saved = saved_ssid(ss, sizeof(ss));
    const char* cur       = frij_wifi_connected();

    int n = 0;
    for (int i = 0; i < found && n < max; i++) {
        frij_wifi_net_t* o = &out[n];
        strncpy(o->ssid, WiFi.SSID(i).c_str(), FRIJ_WIFI_SSID_MAX - 1);
        o->ssid[FRIJ_WIFI_SSID_MAX - 1] = '\0';
        if (o->ssid[0] == '\0') {
            continue;  // skip hidden/empty SSIDs
        }
        o->rssi      = (int8_t)WiFi.RSSI(i);
        o->secured   = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        o->known     = has_saved && strcmp(o->ssid, ss) == 0;
        o->connected = cur && strcmp(o->ssid, cur) == 0;
        n++;
    }
    WiFi.scanDelete();
    return n;
}

bool frij_wifi_connect(const char* ssid, const char* password)
{
    String pw;
    if (password && password[0]) {
        pw = password;
    } else {  // no password given → use the saved one (if this is the saved SSID)
        s_prefs.begin("wifi", true);
        if (s_prefs.getString("ssid", "") == ssid) {
            pw = s_prefs.getString("pw", "");
        }
        s_prefs.end();
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pw.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
        delay(100);
    }
    bool ok = WiFi.status() == WL_CONNECTED;
    if (ok) {  // remember it as the saved network
        s_prefs.begin("wifi", false);
        s_prefs.putString("ssid", ssid);
        s_prefs.putString("pw", pw);
        s_prefs.end();
    }
    return ok;
}

void frij_wifi_disconnect(void)
{
    WiFi.disconnect();
}

void frij_wifi_forget(const char* ssid)
{
    s_prefs.begin("wifi", false);
    if (s_prefs.getString("ssid", "") == ssid) {
        s_prefs.remove("ssid");
        s_prefs.remove("pw");
    }
    s_prefs.end();
    WiFi.disconnect();
}

void frij_wifi_diag(char* buf, int n)
{
    // scan = last scanNetworks() return (>=0 count, -1 running, -2 failed,
    // -100 disabled); wifi = WiFi.status(); en = master switch.
    snprintf(buf, n, "scan=%d wifi=%d en=%d", s_last_raw, s_last_status, s_enabled ? 1 : 0);
}

// ---- async: run the blocking scan/connect on a worker task -----------------
// The radio calls block for seconds; on the LVGL thread that froze the whole UI.
// A short-lived FreeRTOS task runs the (unchanged) blocking helper and writes a
// plain-struct result; the UI polls. The task never touches LVGL — safe.

static volatile bool   s_scan_busy = false;
static frij_wifi_net_t s_scan_buf[16];
static volatile int    s_scan_count = 0;

static void scan_task(void* arg)
{
    (void)arg;
    int n        = frij_wifi_scan(s_scan_buf, (int)(sizeof(s_scan_buf) / sizeof(s_scan_buf[0])));
    s_scan_count = n;
    s_scan_busy  = false;
    vTaskDelete(NULL);
}

void frij_wifi_scan_start(void)
{
    if (s_scan_busy) {
        return;
    }
    if (!s_enabled) {
        s_last_raw   = -100;
        s_scan_count = 0;
        return;
    }
    s_scan_busy = true;
    if (xTaskCreate(scan_task, "wifiscan", 6144, NULL, 1, NULL) != pdPASS) {
        s_scan_busy  = false;  // couldn't spawn — fall back to a blocking scan
        s_scan_count = frij_wifi_scan(s_scan_buf, (int)(sizeof(s_scan_buf) / sizeof(s_scan_buf[0])));
    }
}

int frij_wifi_scan_poll(frij_wifi_net_t* out, int max)
{
    if (s_scan_busy) {
        return -1;
    }
    int n = s_scan_count;
    if (n > max) {
        n = max;
    }
    if (n > 0) {
        memcpy(out, s_scan_buf, (size_t)n * sizeof(frij_wifi_net_t));
    }
    return n;
}

static volatile frij_wifi_state_t s_conn = FRIJ_WIFI_IDLE;
static char                       s_conn_ssid[FRIJ_WIFI_SSID_MAX];
static char                       s_conn_pw[64];

static void connect_task(void* arg)
{
    (void)arg;
    bool ok = frij_wifi_connect(s_conn_ssid, s_conn_pw);
    s_conn  = ok ? FRIJ_WIFI_CONNECTED : FRIJ_WIFI_FAILED;
    vTaskDelete(NULL);
}

void frij_wifi_connect_start(const char* ssid, const char* password)
{
    strncpy(s_conn_ssid, ssid ? ssid : "", sizeof(s_conn_ssid) - 1);
    s_conn_ssid[sizeof(s_conn_ssid) - 1] = '\0';
    strncpy(s_conn_pw, password ? password : "", sizeof(s_conn_pw) - 1);
    s_conn_pw[sizeof(s_conn_pw) - 1] = '\0';
    s_conn                           = FRIJ_WIFI_CONNECTING;
    if (xTaskCreate(connect_task, "wificonn", 6144, NULL, 1, NULL) != pdPASS) {
        bool ok = frij_wifi_connect(s_conn_ssid, s_conn_pw);  // fallback: blocking
        s_conn  = ok ? FRIJ_WIFI_CONNECTED : FRIJ_WIFI_FAILED;
    }
}

frij_wifi_state_t frij_wifi_connect_poll(void)
{
    return s_conn;
}

#endif
