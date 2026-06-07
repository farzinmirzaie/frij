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
//   save() : writes the cache, then upserts to Supabase (best effort)
//   pull() : GETs from Supabase into the cache
//
// Supabase config is read once from the .env file in the working directory.
// ============================================================================
#include <sys/stat.h>

#include <string>

#include <curl/curl.h>

#include <ArduinoJson.h>

#define STORE_DIR ".frij_store"

static std::string s_url;    // e.g. https://xxx.supabase.co
static std::string s_key;    // publishable / anon key
static std::string s_table;  // e.g. store

// ---- .env parsing ---------------------------------------------------------

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

// ---- local cache ----------------------------------------------------------

static void path_for(const char* key, char* out, size_t n)
{
    snprintf(out, n, "%s/%s.json", STORE_DIR, key);
}

static bool cache_write(const char* key, const char* text)
{
    char path[128];
    path_for(key, path, sizeof(path));
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return false;
    }
    fputs(text, f);
    fclose(f);
    return true;
}

// ---- libcurl helpers ------------------------------------------------------

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

// ---- public API -----------------------------------------------------------

void frij_store_init(void)
{
    mkdir(STORE_DIR, 0755);
    load_env();
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

bool frij_store_load(const char* key, char* buf, size_t buf_size)
{
    char path[128];
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

static void cloud_push(const char* key, const char* json)
{
    if (!cloud_ready()) {
        return;
    }
    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        return;
    }
    // PostgREST upsert: POST a row, merge on the primary key.
    std::string url  = s_url + "/rest/v1/" + s_table + "?on_conflict=key";
    std::string body = std::string("{\"key\":\"") + key + "\",\"value\":" + json + "}";

    struct curl_slist* h = NULL;
    h                    = auth_headers(h);
    h                    = curl_slist_append(h, "Content-Type: application/json");
    h                    = curl_slist_append(h, "Prefer: resolution=merge-duplicates,return=minimal");

    std::string resp;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_perform(curl);  // best effort; cache already holds the value

    curl_slist_free_all(h);
    curl_easy_cleanup(curl);
}

bool frij_store_save(const char* key, const char* json)
{
    bool ok = cache_write(key, json);
    cloud_push(key, json);
    return ok;
}

bool frij_store_pull(const char* key)
{
    if (!cloud_ready()) {
        return false;
    }
    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        return false;
    }
    std::string url = s_url + "/rest/v1/" + s_table + "?key=eq." + key + "&select=value";

    struct curl_slist* h = NULL;
    h                    = auth_headers(h);

    std::string resp;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(h);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK) {
        return false;
    }

    // Response is [{"value": <X>}]. Pull X back out and cache it as text.
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
    return cache_write(key, text.c_str());
}

#else
// ============================================================================
// Device backend: TODO — Supabase over WiFiClientSecure + NVS/LittleFS cache.
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
#endif
