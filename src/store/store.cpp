#include "store.h"

#include <stdio.h>
#include <string.h>

#if defined(__has_include)
#  if __has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>)
#    define FRIJ_NATIVE 1
#  endif
#endif

#ifdef FRIJ_NATIVE
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

#else
// ============================================================================
// Device backend: TODO — Supabase over WiFiClientSecure + NVS/LittleFS cache,
// the network calls dispatched to a FreeRTOS task (same "never block UI" rule).
// ============================================================================
void frij_store_init(void) {}

bool frij_store_load(const char* key, char* buf, size_t buf_size)
{
    (void)key;
    (void)buf;
    (void)buf_size;
    return false;
}

bool frij_store_save(const char* key, const char* json)
{
    (void)key;
    (void)json;
    return false;
}

bool frij_store_pull(const char* key)
{
    (void)key;
    return false;
}

void frij_store_pull_async(const char* key)
{
    (void)key;
}
#endif
