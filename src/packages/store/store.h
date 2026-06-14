#ifndef FRIJ_STORE_H
#define FRIJ_STORE_H

#include <stddef.h>

/*
 * Shared key -> JSON store used by all apps. The backend is swappable:
 *   emulator -> local files (.frij_store/<key>.json)
 *   device   -> Supabase over HTTPS (TODO)
 *
 * Apps choose their own serialization; the store just moves text.
 * See docs/STORAGE.md.
 */

void frij_store_init(void);

// Read from the local cache into `buf` (NUL-terminated). Fast, no network.
// False if absent.
bool frij_store_load(const char* key, char* buf, size_t buf_size);

// Write to the local cache AND push to the cloud (best effort). False if the
// cache write fails.
bool frij_store_save(const char* key, const char* json);

// Pull the latest value from the cloud into the local cache (blocking). Use at
// boot, not on the UI path. False if the cloud has no value or is unreachable.
bool frij_store_pull(const char* key);

// Like pull(), but runs on a background thread and returns immediately. The
// cache updates when it finishes (visible on the next read). Use this when
// opening a screen so the gesture stays smooth.
void frij_store_pull_async(const char* key);

// Wipe all locally-cached values (factory reset of on-device data). Does not
// touch the cloud; apps fall back to their defaults on the next read.
void frij_store_clear(void);

// The Supabase project config the store loaded (.env on the emulator). Other
// cloud services (the AI edge function) reuse it. False until configured.
bool frij_store_cloud_config(char* url, size_t url_n, char* key, size_t key_n);

// Typed convenience accessors built on load/save. Values are stored as plain
// text (a decimal for ints, "1"/"0" for bools); `def` is returned when the key
// is absent. Saves go through the same cloud-backed path as frij_store_save.
int  frij_store_load_int(const char* key, int def);
void frij_store_save_int(const char* key, int value);
bool frij_store_load_bool(const char* key, bool def);
void frij_store_save_bool(const char* key, bool value);

#endif  // FRIJ_STORE_H
