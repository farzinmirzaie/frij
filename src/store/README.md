# store/

A shared key→JSON store all apps use. The backend is swappable.

## API (`store.h`)

```c
frij_store_init();
frij_store_load(key, buf, size);  // local cache — fast, no network
frij_store_save(key, json);       // cache + push to the cloud (best effort)
frij_store_pull(key);             // cloud -> cache (call when opening a screen)
```

Key = app name; one JSON blob per app. Apps choose their own serialization
(Counter stores a number; Todo a JSON array via ArduinoJson).

## Backends

- **Emulator:** local files `.frij_store/<key>.json` **and** Supabase over HTTPS
  (libcurl). Config is read from `.env`.
- **Device:** TODO — Supabase over `WiFiClientSecure`, an NVS/LittleFS cache,
  async + an offline queue.

## Notes

- HTTP calls are synchronous, so `save`/`pull` briefly block the UI. Fine on the
  emulator; the device phase needs async.
- Cloud setup (Supabase table, env vars, keys, RLS): see
  [`docs/STORAGE.md`](../../docs/STORAGE.md).
