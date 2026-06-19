#include "store.h"

#include <stdio.h>
#include <stdlib.h>  // atoi
#include <string.h>

#if defined(__has_include)
#  if __has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>)
#    define FRIJ_NATIVE 1
#  endif
#endif

#ifdef FRIJ_NATIVE
#include <dirent.h>
// ============================================================================
// Emulator backend: local file cache (.frij_store/) + Supabase over HTTPS.
//
//   load() : reads the local cache (fast, no network)
//   save() : writes the cache atomically, then pushes to Supabase on a thread
//   pull() : blocking GET (boot only); pull_async() does it on a thread
//
// Network never runs on the UI thread — threads do the libcurl work and the
// cache is updated atomically (write temp + rename), so UI reads never block
// or see a half-written file. Supabase config is read once from .env.
// ============================================================================
#include <sys/stat.h>

#include <string>
#include <thread>

#include <curl/curl.h>

#include <ArduinoJson.h>

#define STORE_DIR ".frij_store"

static std::string s_url;
static std::string s_key;
static std::string s_table;

// ---- .env -----------------------------------------------------------------

static std::string trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

static void load_env(void)
{
    FILE* f = fopen(".env", "rb");
    if (f == NULL) {
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        std::string s = trim(line);
        if (s.empty() || s[0] == '#') {
            continue;
        }
        size_t eq = s.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string k = trim(s.substr(0, eq));
        std::string v = trim(s.substr(eq + 1));
        if (k == "SUPABASE_URL") {
            s_url = v;
        } else if (k == "SUPABASE_ANON_KEY") {
            s_key = v;
        } else if (k == "SUPABASE_TABLE") {
            s_table = v;
        }
    }
    fclose(f);
    if (s_table.empty()) {
        s_table = "store";
    }
}

static bool cloud_ready(void)
{
    return !s_url.empty() && !s_key.empty();
}

// ---- local cache (atomic) -------------------------------------------------

static void path_for(const char* key, char* out, size_t n)
{
    snprintf(out, n, "%s/%s.json", STORE_DIR, key);
}

static bool cache_write(const char* key, const char* text)
{
    char path[160];
    char tmp[176];
    path_for(key, path, sizeof(path));
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE* f = fopen(tmp, "wb");
    if (f == NULL) {
        return false;
    }
    fputs(text, f);
    fclose(f);
    return rename(tmp, path) == 0;  // atomic swap
}

// ---- libcurl --------------------------------------------------------------

static size_t collect(void* data, size_t size, size_t nmemb, void* user)
{
    ((std::string*)user)->append((char*)data, size * nmemb);
    return size * nmemb;
}

static struct curl_slist* auth_headers(struct curl_slist* h)
{
    std::string apikey = "apikey: " + s_key;
    std::string bearer = "Authorization: Bearer " + s_key;
    h = curl_slist_append(h, apikey.c_str());
    h = curl_slist_append(h, bearer.c_str());
    return h;
}

// Runs on a worker thread. GET the row, cache the value text. Returns success.
static bool cloud_fetch_to_cache(const std::string& key)
{
    if (!cloud_ready()) {
        return false;
    }
    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        return false;
    }
    std::string url = s_url + "/rest/v1/" + s_table + "?key=eq." + key + "&select=value";

    struct curl_slist* h = auth_headers(NULL);
    std::string        resp;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
    CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(h);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK) {
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, resp) != DeserializationError::Ok) {
        return false;
    }
    if (!doc.is<JsonArray>() || doc.size() == 0) {
        return false;
    }
    JsonVariant value = doc[0]["value"];
    if (value.isNull()) {
        return false;
    }
    std::string text;
    serializeJson(value, text);
    return cache_write(key.c_str(), text.c_str());
}

// Runs on a worker thread. Upsert the row.
static void cloud_push_body(const std::string& key, const std::string& json)
{
    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        return;
    }
    std::string url  = s_url + "/rest/v1/" + s_table + "?on_conflict=key";
    std::string body = std::string("{\"key\":\"") + key + "\",\"value\":" + json + "}";

    struct curl_slist* h = auth_headers(NULL);
    h                    = curl_slist_append(h, "Content-Type: application/json");
    h                    = curl_slist_append(h, "Prefer: resolution=merge-duplicates,return=minimal");

    std::string resp;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
    curl_easy_perform(curl);
    curl_slist_free_all(h);
    curl_easy_cleanup(curl);
}

// ---- public API -----------------------------------------------------------

void frij_store_init(void)
{
    mkdir(STORE_DIR, 0755);
    load_env();
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

bool frij_store_load(const char* key, char* buf, size_t buf_size)
{
    char path[160];
    path_for(key, path, sizeof(path));
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }
    size_t n = fread(buf, 1, buf_size - 1, f);
    buf[n]   = '\0';
    fclose(f);
    return true;
}

bool frij_store_save(const char* key, const char* json)
{
    bool ok = cache_write(key, json);
    if (cloud_ready()) {
        std::thread([k = std::string(key), j = std::string(json)] { cloud_push_body(k, j); }).detach();
    }
    return ok;
}

bool frij_store_pull(const char* key)
{
    return cloud_fetch_to_cache(std::string(key));
}

void frij_store_pull_async(const char* key)
{
    if (!cloud_ready()) {
        return;
    }
    std::thread([k = std::string(key)] { cloud_fetch_to_cache(k); }).detach();
}

void frij_store_clear(void)
{
    DIR* d = opendir(STORE_DIR);
    if (!d) {
        return;
    }
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') {
            continue;  // skip "." / ".."
        }
        char path[200];
        snprintf(path, sizeof(path), "%s/%s", STORE_DIR, ent->d_name);
        remove(path);
    }
    closedir(d);
}

bool frij_store_cloud_config(char* url, size_t url_n, char* key, size_t key_n)
{
    if (!cloud_ready()) {
        return false;
    }
    snprintf(url, url_n, "%s", s_url.c_str());
    snprintf(key, key_n, "%s", s_key.c_str());
    return true;
}

#else
// ============================================================================
// Device backend: LittleFS file cache + Supabase over WiFiClientSecure.
//
//   load() : reads the file cache (fast, no network)
//   save() : writes the cache, then queues a cloud upsert
//   pull_async() : queues a cloud GET that refreshes the cache
//
// All network runs on ONE serialized worker task (a queue of ops), so TLS never
// touches the LVGL thread and there's never two TLS sessions (RAM) at once.
//
// Why a filesystem, not NVS: NVS lives in a tiny 20 KB partition and caps a
// single value at ~4 KB — the 3 KB events blob blew it up (NOT_ENOUGH_SPACE),
// so events never cached. LittleFS uses the 3.5 MB data partition and has no
// per-blob ceiling, mirroring the emulator's file backend.
// ============================================================================
#include <Arduino.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#define STORE_TABLE "store"  // Supabase table (matches the bridge default)

static QueueHandle_t     s_q      = NULL;
// LittleFS is NOT reentrant: the network worker writes the cache while the LVGL
// thread reads/writes it. Serialize every filesystem touch through this mutex.
static SemaphoreHandle_t s_fs_mtx = NULL;

// Cache file path for a key: "/<key>" in LittleFS (keys are short slugs).
static void store_path(const char* key, char* out, size_t n)
{
    snprintf(out, n, "/%s", key);
}

// Write text to the cache file. Caller MUST hold s_fs_mtx.
static bool fs_write(const char* key, const char* text, size_t len)
{
    char path[64];
    store_path(key, path, sizeof(path));
    File f = LittleFS.open(path, "w");
    if (!f) {
        return false;
    }
    size_t w = f.write((const uint8_t*)text, len);
    f.close();
    return w == len;
}

// A queued network op: a pull (json == NULL) or a push (json = heap-owned body).
typedef struct {
    bool  push;
    char  key[24];
    char* json;
} store_op_t;

// Worker-thread only (TLS is stack/heap heavy + blocks). GET the row, write the
// value text into the file cache.
static bool fetch_to_cache(const char* key)
{
    if (WiFi.status() != WL_CONNECTED) {
        log_d("[store] fetch %s: not connected (status=%d)", key, WiFi.status());
        return false;
    }
    char url[160], anon[512];
    if (!frij_store_cloud_config(url, sizeof(url), anon, sizeof(anon))) {
        log_d("[store] fetch %s: no cloud config (creds not baked)", key);
        return false;
    }
    WiFiClientSecure client;
    client.setInsecure();  // TODO: pin the Supabase cert
    HTTPClient https;
    String full = String(url) + "/rest/v1/" STORE_TABLE "?key=eq." + key + "&select=value";
    if (!https.begin(client, full)) {
        log_d("[store] fetch %s: https.begin failed (%s)", key, full.c_str());
        return false;
    }
    https.addHeader("apikey", anon);
    https.addHeader("Authorization", String("Bearer ") + anon);
    int code = https.GET();
    if (code != 200) {
        log_d("[store] fetch %s: HTTP %d", key, code);
        https.end();
        return false;
    }
    String payload = https.getString();
    https.end();
    log_d("[store] fetch %s: HTTP 200, %d bytes", key, (int)payload.length());

    JsonDocument doc;
    DeserializationError jerr = deserializeJson(doc, payload);
    if (jerr != DeserializationError::Ok) {
        log_d("[store] fetch %s: JSON parse failed (%s)", key, jerr.c_str());
        return false;
    }
    if (!doc.is<JsonArray>() || doc.size() == 0) {
        log_d("[store] fetch %s: empty/!array (size=%d)", key, (int)doc.size());
        return false;
    }
    JsonVariant v = doc[0]["value"];
    if (v.isNull()) {
        log_d("[store] fetch %s: value null", key);
        return false;
    }
    String text;
    serializeJson(v, text);

    if (s_fs_mtx) {
        xSemaphoreTake(s_fs_mtx, portMAX_DELAY);
    }
    bool ok = fs_write(key, text.c_str(), text.length());
    if (s_fs_mtx) {
        xSemaphoreGive(s_fs_mtx);
    }
    log_d("[store] fetch %s: cached %d bytes to LittleFS (%s)", key, (int)text.length(),
          ok ? "ok" : "WRITE FAILED");
    return ok;
}

// Worker-thread only. Upsert the row.
static void push_body(const char* key, const char* json)
{
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    char url[160], anon[512];
    if (!frij_store_cloud_config(url, sizeof(url), anon, sizeof(anon))) {
        return;
    }
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    String full = String(url) + "/rest/v1/" STORE_TABLE "?on_conflict=key";
    if (!https.begin(client, full)) {
        return;
    }
    https.addHeader("apikey", anon);
    https.addHeader("Authorization", String("Bearer ") + anon);
    https.addHeader("Content-Type", "application/json");
    https.addHeader("Prefer", "resolution=merge-duplicates,return=minimal");
    String body = String("{\"key\":\"") + key + "\",\"value\":" + json + "}";
    https.POST(body);
    https.end();
}

static void store_worker(void* arg)
{
    (void)arg;
    store_op_t* op = NULL;
    for (;;) {
        if (xQueueReceive(s_q, &op, portMAX_DELAY) == pdTRUE && op) {
            if (op->push) {
                push_body(op->key, op->json ? op->json : "null");
            } else {
                fetch_to_cache(op->key);
            }
            free(op->json);
            free(op);
            // Breathe between ops. Each TLS handshake briefly spikes internal
            // DMA-capable RAM; back-to-back ops (the 5-key boot pull) once
            // collided with a panel flush and starved getDMABuffer -> crash.
            // A gap lets the render task grab its DMA buffer between sessions.
            vTaskDelay(pdMS_TO_TICKS(150));
        }
    }
}

static void enqueue(bool push, const char* key, char* json)
{
    if (!s_q) {
        free(json);
        return;
    }
    store_op_t* op = (store_op_t*)malloc(sizeof(store_op_t));
    if (!op) {
        free(json);
        return;
    }
    op->push = push;
    strncpy(op->key, key, sizeof(op->key) - 1);
    op->key[sizeof(op->key) - 1] = '\0';
    op->json = json;
    if (xQueueSend(s_q, &op, 0) != pdTRUE) {  // queue full -> drop the op
        free(op->json);
        free(op);
    }
}

void frij_store_init(void)
{
    s_fs_mtx = xSemaphoreCreateMutex();
    LittleFS.begin(/*formatOnFail=*/true);  // mounts the data partition; formats once on first boot
    s_q = xQueueCreate(8, sizeof(store_op_t*));
    // 12 KB stack: WiFiClientSecure's TLS handshake is stack-hungry. Pin to core 0
    // (PRO, with Wi-Fi) so the recurring sync TLS stays off the render core (1).
    xTaskCreatePinnedToCore(store_worker, "frijstore", 12288, NULL, 1, NULL, 0);
}

bool frij_store_load(const char* key, char* buf, size_t buf_size)
{
    if (s_fs_mtx) {
        xSemaphoreTake(s_fs_mtx, portMAX_DELAY);
    }
    char path[64];
    store_path(key, path, sizeof(path));
    size_t n = 0;
    File   f = LittleFS.open(path, "r");
    if (f) {
        n      = f.readBytes(buf, buf_size - 1);
        buf[n] = '\0';
        f.close();
    }
    if (s_fs_mtx) {
        xSemaphoreGive(s_fs_mtx);
    }
    return n > 0;
}

bool frij_store_save(const char* key, const char* json)
{
    if (s_fs_mtx) {
        xSemaphoreTake(s_fs_mtx, portMAX_DELAY);
    }
    bool ok = fs_write(key, json, strlen(json));
    if (s_fs_mtx) {
        xSemaphoreGive(s_fs_mtx);
    }
    enqueue(true, key, strdup(json));  // best-effort cloud upsert
    return ok;
}

bool frij_store_pull(const char* key)
{
    return fetch_to_cache(key);  // blocking — callers use pull_async on the UI
}

void frij_store_pull_async(const char* key)
{
    enqueue(false, key, NULL);
}

void frij_store_clear(void)
{
    if (s_fs_mtx) {
        xSemaphoreTake(s_fs_mtx, portMAX_DELAY);
    }
    LittleFS.format();  // wipe the whole cache (erase-all-data); FS stays mounted
    if (s_fs_mtx) {
        xSemaphoreGive(s_fs_mtx);
    }
}

bool frij_store_cloud_config(char* url, size_t url_n, char* key, size_t key_n)
{
    // Baked in at build time (platformio device env pulls them from the shell).
#if defined(FRIJ_SUPABASE_URL) && defined(FRIJ_SUPABASE_ANON_KEY)
    snprintf(url, url_n, "%s", FRIJ_SUPABASE_URL);
    snprintf(key, key_n, "%s", FRIJ_SUPABASE_ANON_KEY);
    return url[0] && key[0];
#else
    (void)url;
    (void)url_n;
    (void)key;
    (void)key_n;
    return false;
#endif
}

#endif

// ---- typed accessors (backend-agnostic; built on load/save) ----------------

int frij_store_load_int(const char* key, int def)
{
    char b[24];
    return frij_store_load(key, b, sizeof(b)) ? atoi(b) : def;
}

void frij_store_save_int(const char* key, int value)
{
    char b[24];
    snprintf(b, sizeof(b), "%d", value);
    frij_store_save(key, b);
}

bool frij_store_load_bool(const char* key, bool def)
{
    char b[8];
    return frij_store_load(key, b, sizeof(b)) ? (b[0] == '1') : def;
}

void frij_store_save_bool(const char* key, bool value)
{
    frij_store_save(key, value ? "1" : "0");
}
