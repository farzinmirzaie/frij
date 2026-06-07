#include "registry.h"

// Fixed-size list — no dynamic memory, easy to reason about.
static const frij_app_t* s_apps[FRIJ_MAX_APPS];
static int s_count = 0;

void frij_registry_add(const frij_app_t* app)
{
    if (app == NULL || s_count >= FRIJ_MAX_APPS) {
        return;  // full or invalid: ignore rather than crash
    }
    s_apps[s_count++] = app;
}

int frij_registry_count(void)
{
    return s_count;
}

const frij_app_t* frij_registry_get(int index)
{
    if (index < 0 || index >= s_count) {
        return NULL;
    }
    return s_apps[index];
}
