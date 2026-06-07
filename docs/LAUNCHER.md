# Launcher design

Frij is a looping carousel of apps. Each app shows a full-screen **glance**
(informational) on the launcher; swiping up opens the app's own left/right
carousel of interactive **screens**.

## Layers & gestures

```
                       ┌──────────────┐
              swipe ↓  │   SETTINGS   │   brightness, wifi (Phase B)
                       └──────┬───────┘
                              │ Back
   ← loop ────────────────────┴──────────────────── loop →
   ┌──────────┐   ┌──────────┐   ┌──────────┐
   │  glance  │ ↔ │  glance  │ ↔ │  glance  │        LAUNCHER (home)
   └──────────┘   └────┬─────┘   └──────────┘
                       │ swipe ↑ (open)   │ Back
                  ┌────┴─────┐
   ← loop ────────┤  screen  ├──────── loop →        APP (app owns L/R)
                  └──────────┘
```

| Layer | left / right | up | down | Back |
| --- | --- | --- | --- | --- |
| Launcher | prev / next app | open app | settings | — |
| App | prev / next screen | — | — | return to launcher |
| Settings | — | — | — | return to launcher |

Back is a **hardware button** (not a gesture), so it never fights app content.

## Code map

| File | Role |
| --- | --- |
| `src/app.h` | App contract: `name`, `build_glance`, `screen_count`, `build_screen` |
| `src/launcher/carousel.*` | Generic looping pager (`next`/`prev`, rebuilds the page) |
| `src/launcher/launcher.*` | Layer state machine + one screen-level gesture handler + `frij_back()` |
| `src/launcher/input.*` | Back input: Backspace key in the emulator; button GPIO on device (TODO) |
| `src/launcher/registry.*` | The list of registered apps |

One gesture handler on the screen routes by the current layer. Gestures bubble
up from the carousel pages (`LV_OBJ_FLAG_EVENT_BUBBLE`); non-clickable glance
content lets the swipe reach the page.

## Testing in the emulator

- Swipes: click-drag in the SDL window (left/right/up/down).
- Back: press **Backspace** (`input.cpp` maps it to `frij_back()`).

## Status

- **Phase A (done):** carousel, state machine, swipe-up to open, Back. Todo and
  Counter expose a glance + one screen.
- **Phase B:** real settings (brightness; wifi is device-only).
- **Phase C:** multi-screen apps + glance data refresh (`on_show` hook).
