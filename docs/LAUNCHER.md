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
| `src/launcher/carousel.*` | Looping pager: drag follows the finger, snaps on release; reports vertical swipes via a callback |
| `src/launcher/launcher.*` | Layer state machine + one screen-level gesture handler + `frij_back()` |
| `src/launcher/input.*` | Back input: Backspace key in the emulator; button GPIO on device (TODO) |
| `src/launcher/registry.*` | The list of registered apps |

The carousel owns horizontal drags directly (press/pressing/release on the
viewport, with pages set to `LV_OBJ_FLAG_EVENT_BUBBLE`): the current and
incoming pages translate with the finger and snap on release. A vertical drag is
reported to the launcher via the `vswipe` callback. Each page paints its app's
`color` background, so colors slide in during a swipe. An app may declare
`screen_count > 1` to get its own looping screen carousel.

## Testing in the emulator

- Swipes: click-drag in the SDL window (left/right/up/down).
- Back: press **Backspace** (`input.cpp` maps it to `frij_back()`).

## Status

- **Phase A (done):** drag-following looping carousel, state machine, swipe-up
  to open, Back, per-app colors. Todo has 3 screens; Counter has 1.
- **Phase B:** real settings (brightness; wifi is device-only).
- **Phase C:** multi-screen apps + glance data refresh (`on_show` hook).
