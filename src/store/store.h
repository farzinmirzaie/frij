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

// Pull the latest value from the cloud into the local cache. Call before
// load() when you want fresh data (e.g. when opening a screen). False if the
// cloud has no value or is unreachable.
bool frij_store_pull(const char* key);

#endif  // FRIJ_STORE_H
