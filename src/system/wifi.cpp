#include "wifi.h"

#include <string.h>

/*
 * Wi-Fi is board-specific. Like brightness/haptics this file carries both
 * targets behind a guard: a working in-memory mock for the emulator, and a
 * device stub to be filled in with the real radio (esp_wifi).
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
    if (!on) {
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

#else

// ---- device: real radio (Arduino WiFi) + one saved network in NVS ----------
//
// Credentials live in NVS via Preferences (namespace "wifi": ssid + pw). We keep
// ONE saved network (the home AP) — NVS keys cap at 15 chars so we can't key by
// SSID, and a watch realistically pairs to one network. `known` = "this is the
// saved SSID". scan()/connect() block briefly (the radio is synchronous); fine
// for user-initiated actions — could be made async later.

#include <Preferences.h>
#include <WiFi.h>

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
        char ss[FRIJ_WIFI_SSID_MAX];
        if (saved_ssid(ss, sizeof(ss))) {
            frij_wifi_connect(ss, NULL);  // reconnect with stored creds
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

int frij_wifi_scan(frij_wifi_net_t* out, int max)
{
    if (!s_enabled) {
        return 0;
    }
    int found = WiFi.scanNetworks();  // blocking (~1-2s)
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

#endif
