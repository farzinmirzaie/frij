#ifndef FRIJ_REGISTRY_H
#define FRIJ_REGISTRY_H

#include "app.h"

/*
 * A tiny list of apps the launcher knows how to show.
 *
 * Apps are added once at startup (see src/apps/apps.cpp). The launcher only
 * reads this list — it never hardcodes which apps exist.
 */

#define FRIJ_MAX_APPS 16

// Register an app. `app` must stay alive for the whole program (use a static).
void frij_registry_add(const frij_app_t* app);

// How many apps are registered.
int frij_registry_count(void);

// Get app number `index` (0-based), or NULL if out of range.
const frij_app_t* frij_registry_get(int index);

#endif  // FRIJ_REGISTRY_H
