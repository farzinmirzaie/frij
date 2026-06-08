# src/

- `main.cpp` / `user_app.cpp` — boot; `user_app()` inits the store, registers
  apps, and starts the launcher.
- `app.h` — the app contract (`frij_app_t`).

Each package has its own README:

| Folder | What |
| --- | --- |
| `apps/` | the mini-apps (+ settings) and where they register |
| `launcher/` | the navigation shell (layers, gestures, registry) |
| `ui/` | shared, app-agnostic UI building blocks |
| `store/` | shared key→JSON storage (local cache + Supabase, off-thread) |
| `system/` | neutral board-service interfaces (brightness, …) |
| `utility/` | the board-specific LVGL ↔ M5GFX bridge |
