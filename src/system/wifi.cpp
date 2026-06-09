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

// ---- device: TODO (esp_wifi: scan, connect with NVS-stored creds) ----------

void        frij_wifi_init(void) {}
bool        frij_wifi_enabled(void) { return false; }
void        frij_wifi_set_enabled(bool on) { (void)on; }
int         frij_wifi_scan(frij_wifi_net_t* out, int max) { (void)out; (void)max; return 0; }
const char* frij_wifi_connected(void) { return NULL; }
bool        frij_wifi_connect(const char* ssid, const char* password) { (void)ssid; (void)password; return false; }
void        frij_wifi_disconnect(void) {}
void        frij_wifi_forget(const char* ssid) { (void)ssid; }

#endif
