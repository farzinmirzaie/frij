# store/

A shared key→JSON store all apps use. The backend is swappable.

## API (`store.h`)

```c
frij_store_init();
frij_store_load(key, buf, size);  // local cache — fast, no network
frij_store_save(key, json);       // cache + push to the cloud (best effort)
frij_store_pull(key);             // cloud -> cache (call when opening a screen)

frij_store_load_int(key, def);    // typed helpers over load/save: values are
frij_store_save_int(key, value);  // stored as plain text ("42", "1"/"0").
frij_store_load_bool(key, def);   // `def` is returned when the key is absent.
frij_store_save_bool(key, value);
```

Key = app name; one JSON blob per app. Apps choose their own serialization
(Counter stores a number; Todo a JSON array via ArduinoJson).

## Backends

- **Emulator:** local files `.frij_store/<key>.json` **and** Supabase over HTTPS
  (libcurl). Config is read from `.env`.
- **Device:** TODO — Supabase over `WiFiClientSecure`, an NVS/LittleFS cache,
  async + an offline queue.

## Notes

- **Cloud I/O never blocks the UI.** `save` writes the cache then pushes on a
  background thread; `pull_async` fetches on a thread. The cache is written
  atomically (temp + rename), so UI reads never see a half-written file. Use
  `pull_async` when opening a screen; the blocking `pull` is for boot only.
- Cloud setup (Supabase table, env vars, keys, RLS): see
  [`docs/STORAGE.md`](../../docs/STORAGE.md).
