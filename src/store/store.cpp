#include "store.h"

#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>)
#    define FRIJ_NATIVE 1
#  endif
#endif

#ifdef FRIJ_NATIVE
// ---- Emulator backend: one file per key under .frij_store/ ----------------
#include <sys/stat.h>

#define STORE_DIR ".frij_store"

static void path_for(const char* key, char* out, size_t n)
{
    snprintf(out, n, "%s/%s.json", STORE_DIR, key);
}

void frij_store_init(void)
{
    mkdir(STORE_DIR, 0755);  // no-op if it already exists
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

bool frij_store_save(const char* key, const char* json)
{
    char path[128];
    path_for(key, path, sizeof(path));
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return false;
    }
    fputs(json, f);
    fclose(f);
    return true;
}

#else
// ---- Device backend: Supabase over HTTPS (TODO) ---------------------------
// Will POST/GET rows via REST with a scoped key, backed by a local cache and an
// offline queue. See docs/STORAGE.md.
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
#endif
