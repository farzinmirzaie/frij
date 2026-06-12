#include "ai.h"

#include <stdio.h>

#include "store/store.h"

#if defined(__has_include)
#  if __has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>)
#    define FRIJ_NATIVE 1
#  endif
#endif

#ifdef FRIJ_NATIVE
// ============================================================================
// Emulator backend: POST {q} to the "ask" edge function on a worker thread
// (libcurl, like the store's cloud push). The UI polls frij_ai_state().
// ============================================================================
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include <curl/curl.h>

#include <ArduinoJson.h>

static std::atomic<int> s_state{FRIJ_AI_IDLE};
static std::mutex       s_lock;   // guards the result strings
static std::string      s_res_q;
static std::string      s_res_a;

bool frij_ai_available(void)
{
    char url[8], key[8];  // existence check only
    return frij_store_cloud_config(url, sizeof(url), key, sizeof(key));
}

static size_t collect(void* data, size_t size, size_t nmemb, void* user)
{
    ((std::string*)user)->append((char*)data, size * nmemb);
    return size * nmemb;
}

static void worker(std::string question)
{
    char url[256], key[512];
    if (!frij_store_cloud_config(url, sizeof(url), key, sizeof(key))) {
        std::lock_guard<std::mutex> g(s_lock);
        s_res_a = "Cloud isn't configured.";
        s_state = FRIJ_AI_ERROR;
        return;
    }

    // request body {"q": "..."} (ArduinoJson handles the escaping)
    JsonDocument doc;
    doc["q"] = question;
    std::string body;
    serializeJson(doc, body);

    std::string endpoint = std::string(url) + "/functions/v1/ask";
    std::string resp;
    CURL*       c  = curl_easy_init();
    long        http = 0;
    CURLcode    rc   = CURLE_FAILED_INIT;
    if (c) {
        struct curl_slist* h = NULL;
        h = curl_slist_append(h, ("apikey: " + std::string(key)).c_str());
        h = curl_slist_append(h, ("Authorization: Bearer " + std::string(key)).c_str());
        h = curl_slist_append(h, "Content-Type: application/json");
        curl_easy_setopt(c, CURLOPT_URL, endpoint.c_str());
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, collect);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
        curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);  // Gemini + tools can take a bit
        rc = curl_easy_perform(c);
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
        curl_slist_free_all(h);
        curl_easy_cleanup(c);
    }

    std::lock_guard<std::mutex> g(s_lock);
    JsonDocument out;
    if (rc == CURLE_OK && deserializeJson(out, resp) == DeserializationError::Ok &&
        http == 200 && out["a"].is<const char*>()) {
        s_res_q = (const char*)(out["q"] | question.c_str());
        s_res_a = (const char*)out["a"];
        s_state = FRIJ_AI_DONE;
    } else {
        const char* err = out["error"] | "";
        s_res_q = question;
        s_res_a = err[0] ? err : "Couldn't reach Frij AI. Check the connection.";
        s_state = FRIJ_AI_ERROR;
    }
}

bool frij_ai_ask(const char* question)
{
    if (!frij_ai_available() || s_state == FRIJ_AI_BUSY || question == NULL) {
        return false;
    }
    {
        std::lock_guard<std::mutex> g(s_lock);
        s_res_q.clear();
        s_res_a.clear();
    }
    s_state = FRIJ_AI_BUSY;
    std::thread(worker, std::string(question)).detach();
    return true;
}

frij_ai_state_t frij_ai_state(void)
{
    return (frij_ai_state_t)s_state.load();
}

void frij_ai_take(char* q, size_t q_n, char* a, size_t a_n)
{
    std::lock_guard<std::mutex> g(s_lock);
    if (q && q_n) {
        snprintf(q, q_n, "%s", s_res_q.c_str());
    }
    if (a && a_n) {
        snprintf(a, a_n, "%s", s_res_a.c_str());
    }
    s_state = FRIJ_AI_IDLE;
}

// The emulator has no mic — voice is a no-op so the assistant uses the text
// mock path (a sample question stands in for the recording).
bool frij_ai_listen_start(void)
{
    return false;
}

bool frij_ai_listen_ask(void)
{
    return false;
}

void frij_ai_listen_cancel(void) {}

#else
// ============================================================================
// Device backend: push-to-talk over the ES8311 mic (M5.Mic) -> base64 WAV ->
// the "ask" edge function's audio path (Gemini transcribes + answers). The
// record + POST run on a FreeRTOS task so the UI never blocks; the UI polls
// frij_ai_state() exactly like the emulator. Text ask is unused on device.
//
// Cloud config (Supabase URL + anon key) is baked in at build time — see
// frij_store_cloud_config() and the platformio device env's build flags.
// ============================================================================
#include <M5Unified.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MIC_RATE     16000              // 16 kHz mono — plenty for speech
#define MAX_SECONDS  12                 // push-to-talk cap
#define MAX_SAMPLES  (MIC_RATE * MAX_SECONDS)

static std::atomic<int>  s_state{FRIJ_AI_IDLE};
static std::atomic<bool> s_capturing{false};
static std::atomic<bool> s_cancelled{false};
static int16_t*          s_pcm   = nullptr;   // PSRAM capture buffer
static volatile size_t   s_pcm_n = 0;         // samples captured
static char              s_res_q[200];
static char              s_res_a[640];
static TaskHandle_t      s_task = nullptr;

bool frij_ai_available(void)
{
    char url[8], key[8];
    return frij_store_cloud_config(url, sizeof(url), key, sizeof(key));
}

// Text ask isn't used on the device (voice is the input), but keep the API.
bool frij_ai_ask(const char* question)
{
    (void)question;
    return false;
}

frij_ai_state_t frij_ai_state(void)
{
    return (frij_ai_state_t)s_state.load();
}

void frij_ai_take(char* q, size_t q_n, char* a, size_t a_n)
{
    if (q && q_n) {
        snprintf(q, q_n, "%s", s_res_q);
    }
    if (a && a_n) {
        snprintf(a, a_n, "%s", s_res_a);
    }
    s_state = FRIJ_AI_IDLE;
}

// ---- base64 (no dependency) ------------------------------------------------

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t base64_into(const uint8_t* in, size_t n, char* out)
{
    size_t o = 0;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = in[i] << 16;
        v |= (i + 1 < n ? in[i + 1] : 0) << 8;
        v |= (i + 2 < n ? in[i + 2] : 0);
        out[o++] = B64[(v >> 18) & 0x3F];
        out[o++] = B64[(v >> 12) & 0x3F];
        out[o++] = (i + 1 < n) ? B64[(v >> 6) & 0x3F] : '=';
        out[o++] = (i + 2 < n) ? B64[v & 0x3F] : '=';
    }
    out[o] = '\0';
    return o;
}

// ---- WAV (16-bit PCM mono) header ------------------------------------------

static void wav_header(uint8_t* h, uint32_t data_bytes)
{
    uint32_t rate = MIC_RATE, brate = MIC_RATE * 2;
    memcpy(h, "RIFF", 4);
    *(uint32_t*)(h + 4)  = 36 + data_bytes;
    memcpy(h + 8, "WAVEfmt ", 8);
    *(uint32_t*)(h + 16) = 16;
    *(uint16_t*)(h + 20) = 1;   // PCM
    *(uint16_t*)(h + 22) = 1;   // mono
    *(uint32_t*)(h + 24) = rate;
    *(uint32_t*)(h + 28) = brate;
    *(uint16_t*)(h + 32) = 2;   // block align
    *(uint16_t*)(h + 34) = 16;  // bits
    memcpy(h + 36, "data", 4);
    *(uint32_t*)(h + 40) = data_bytes;
}

// ---- the record + ask task -------------------------------------------------

static void fail(const char* msg)
{
    snprintf(s_res_q, sizeof(s_res_q), "%s", "");
    snprintf(s_res_a, sizeof(s_res_a), "%s", msg);
    s_state = FRIJ_AI_ERROR;
}

static void post_audio(void)
{
    char url[256], key[512];
    if (!frij_store_cloud_config(url, sizeof(url), key, sizeof(key))) {
        fail("Cloud isn't configured.");
        return;
    }

    uint32_t data_bytes = (uint32_t)(s_pcm_n * sizeof(int16_t));
    size_t   wav_bytes  = 44 + data_bytes;
    // base64 of the WAV (PSRAM): 4/3 + header + NUL
    char* b64 = (char*)heap_caps_malloc(((wav_bytes + 2) / 3) * 4 + 8, MALLOC_CAP_SPIRAM);
    if (!b64) {
        fail("Out of memory.");
        return;
    }
    // prepend the WAV header to the PCM (reuse the PCM buffer's leading room?
    // simplest: encode header then body into one base64 stream)
    uint8_t hdr[44];
    wav_header(hdr, data_bytes);
    // assemble header+pcm contiguously in a small PSRAM blob for one b64 pass
    uint8_t* wav = (uint8_t*)heap_caps_malloc(wav_bytes, MALLOC_CAP_SPIRAM);
    if (!wav) {
        free(b64);
        fail("Out of memory.");
        return;
    }
    memcpy(wav, hdr, 44);
    memcpy(wav + 44, s_pcm, data_bytes);
    base64_into(wav, wav_bytes, b64);
    free(wav);

    // request body {"audio": "<b64>", "mime": "audio/wav"}
    std::string body = "{\"audio\":\"";
    body += b64;
    body += "\",\"mime\":\"audio/wav\"}";
    free(b64);

    WiFiClientSecure client;
    client.setInsecure();  // TODO: pin the Supabase cert
    HTTPClient http;
    std::string endpoint = std::string(url) + "/functions/v1/ask";
    http.begin(client, endpoint.c_str());
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", key);
    http.addHeader("Authorization", (std::string("Bearer ") + key).c_str());
    http.setTimeout(30000);
    int code = http.POST((uint8_t*)body.data(), body.size());
    String resp = http.getString();
    http.end();

    JsonDocument out;
    if (code == 200 && deserializeJson(out, resp.c_str()) == DeserializationError::Ok &&
        out["a"].is<const char*>()) {
        snprintf(s_res_q, sizeof(s_res_q), "%s", out["q"] | "");
        snprintf(s_res_a, sizeof(s_res_a), "%s", (const char*)out["a"]);
        s_state = FRIJ_AI_DONE;
    } else {
        const char* err = out["error"] | "";
        fail(err[0] ? err : "Couldn't reach Frij AI. Check the connection.");
    }
}

static void capture_task(void*)
{
    M5.Speaker.end();  // mic + speaker share the I2S bus — free it first
    M5.Mic.begin();
    s_pcm_n = 0;
    // Fill the buffer in ~100ms chunks; M5.Mic.record() queues a chunk and
    // fills it asynchronously, so wait for it before advancing/stopping.
    const size_t chunk = MIC_RATE / 10;
    while (s_capturing.load() && s_pcm_n + chunk <= MAX_SAMPLES) {
        if (!M5.Mic.record(s_pcm + s_pcm_n, chunk, MIC_RATE)) {
            vTaskDelay(1);
            continue;
        }
        while (M5.Mic.isRecording()) {
            vTaskDelay(1);
        }
        s_pcm_n += chunk;
    }
    M5.Mic.end();
    M5.Speaker.begin();  // hand the bus back for click/tone feedback

    if (s_cancelled.load()) {
        // backed out — drop the clip, don't call the cloud
    } else if (s_pcm_n < MIC_RATE / 4) {  // < ~0.25s: too short to be a question
        fail("Didn't catch that. Hold the button and speak.");
    } else {
        post_audio();
    }
    s_task = nullptr;
    vTaskDelete(nullptr);
}

bool frij_ai_listen_start(void)
{
    if (!frij_ai_available() || s_state == FRIJ_AI_BUSY || s_task) {
        return false;
    }
    if (!s_pcm) {
        s_pcm = (int16_t*)heap_caps_malloc(MAX_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    }
    if (!s_pcm) {
        return false;  // no PSRAM — fall back to the mock path
    }
    s_state     = FRIJ_AI_BUSY;
    s_cancelled = false;
    s_capturing = true;
    // pinned to core 1 with a generous stack (HTTPS + JSON live here)
    xTaskCreatePinnedToCore(capture_task, "frij_ai", 8192, nullptr, 1, &s_task, 1);
    return s_task != nullptr;
}

bool frij_ai_listen_ask(void)
{
    if (!s_capturing.load()) {
        return false;
    }
    s_capturing = false;  // the task notices, stops recording, then POSTs
    return true;
}

void frij_ai_listen_cancel(void)
{
    if (!s_capturing.load()) {
        return;
    }
    s_cancelled = true;
    s_capturing = false;  // the task ends and skips the POST
    s_state     = FRIJ_AI_IDLE;
}

#endif
