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
| Home | prev / next app | open app | open settings | — |
| App | prev / next screen | (back to home) | — | return to home |
| Settings | prev / next screen | — | (back to home) | return to home |

All four directions **follow the finger and snap** on release. Back is a
**hardware button** (not a gesture), so it never fights app content.

## Code map

| File | Role |
| --- | --- |
| `src/app.h` | App contract: `name`, `color`, `build_glance`, `screen_count`, `build_screen` |
| `src/launcher/carousel.*` | Input-free horizontal pager: the launcher calls `drag(dx)`/`end(dx)`; follows finger, snaps, loops (no loop when `count <= 1`) |
| `src/launcher/launcher.*` | The nav: one root input handler, three vertical layers, vertical follow-transitions, `frij_back()` |
| `src/launcher/settings.*` | Settings screens (a multi-screen carousel like an app) |
| `src/launcher/input.*` | Back input: Backspace key in the emulator; button GPIO on device (TODO) |
| `src/launcher/registry.*` | The list of registered apps |

**Input routing.** The launcher owns the single input handler on the root. It
decides the axis from the first movement: horizontal drags drive the active
layer's carousel (`drag`/`end`); vertical drags slide whole layers. Home sits in
the middle of a vertical stack — app below (swipe up), settings above (swipe
down); both follow the finger and snap. Each carousel page paints its app's
`color`, so colors slide in. Any layer with `count <= 1` simply doesn't page.

## Testing in the emulator

- Swipes: click-drag in the SDL window (all four directions follow the cursor).
- Back: press **Backspace** (`input.cpp` maps it to `frij_back()`).

## Status

- **Done:** 4-direction finger-follow nav; home/app/settings layers; per-app
  colors; looping carousels (suppressed for single-screen layers). Todo has 3
  screens, Settings has 2, Counter has 1.
- **Next:** real settings content (brightness; wifi is device-only); glance data
  refresh (`on_show` hook).
