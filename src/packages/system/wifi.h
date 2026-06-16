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

// Async scan — the radio scan blocks 1-2s, which froze the UI, so it runs off
// the LVGL thread (a worker task on device; instant on the emulator). Call
// frij_wifi_scan_start() once, then poll frij_wifi_scan_poll(): it returns -1
// while the scan is still running, else the number of networks written to `out`.
void frij_wifi_scan_start(void);
int  frij_wifi_scan_poll(frij_wifi_net_t* out, int max);

// Async connect — joining blocks up to several seconds, so it's off-thread too.
// connect_start() kicks it off; connect_poll() reports progress.
typedef enum {
    FRIJ_WIFI_IDLE,
    FRIJ_WIFI_CONNECTING,
    FRIJ_WIFI_CONNECTED,
    FRIJ_WIFI_FAILED,
} frij_wifi_state_t;
void              frij_wifi_connect_start(const char* ssid, const char* password);
frij_wifi_state_t frij_wifi_connect_poll(void);

// SSID of the active connection, or NULL when not connected.
const char* frij_wifi_connected(void);

// Short diagnostic of the last scan (device: raw scanNetworks() return + radio
// status; emulator: a mock note). Surfaced in the Network "No networks" state so
// an empty list can be debugged without a serial console.
void frij_wifi_diag(char* buf, int n);

// Join `ssid` (mock: succeeds immediately and remembers it; password ignored on
// the emulator). Disconnect drops the active link; forget drops credentials.
bool frij_wifi_connect(const char* ssid, const char* password);
void frij_wifi_disconnect(void);
void frij_wifi_forget(const char* ssid);

#endif  // FRIJ_WIFI_H
