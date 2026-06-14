#ifndef FRIJ_WIFI_H
#define FRIJ_WIFI_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Wi-Fi — a board service (real radio on device, an in-memory mock on the
 * emulator). Apps/Settings call these neutral functions and render the UI; they
 * never touch the radio directly. The mock keeps a list + connected/known state
 * so the Network settings screen is fully interactive without hardware.
 */

#define FRIJ_WIFI_SSID_MAX 33

typedef struct {
    char    ssid[FRIJ_WIFI_SSID_MAX];
    int8_t  rssi;       // signal in dBm (closer to 0 = stronger)
    bool    secured;    // needs a password
    bool    known;      // saved (we have credentials)
    bool    connected;  // the active connection
} frij_wifi_net_t;

void frij_wifi_init(void);

// The Wi-Fi master switch.
bool frij_wifi_enabled(void);
void frij_wifi_set_enabled(bool on);

// Fill `out` with up to `max` visible networks (connected first, then by
// signal). Returns the number written (0 when Wi-Fi is off).
int frij_wifi_scan(frij_wifi_net_t* out, int max);

// SSID of the active connection, or NULL when not connected.
const char* frij_wifi_connected(void);

// Join `ssid` (mock: succeeds immediately and remembers it; password ignored on
// the emulator). Disconnect drops the active link; forget drops credentials.
bool frij_wifi_connect(const char* ssid, const char* password);
void frij_wifi_disconnect(void);
void frij_wifi_forget(const char* ssid);

#endif  // FRIJ_WIFI_H
