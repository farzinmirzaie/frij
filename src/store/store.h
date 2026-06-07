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

// Copy stored text for `key` into `buf` (NUL-terminated). False if absent.
bool frij_store_load(const char* key, char* buf, size_t buf_size);

// Persist `json` (any text) under `key`. False on failure.
bool frij_store_save(const char* key, const char* json);

#endif  // FRIJ_STORE_H
