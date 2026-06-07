# launcher/

The navigation shell: a vertical stack of layers, each a horizontal carousel.

```
        [ SETTINGS ]   swipe down
        [  HOME    ]   app glances
        [  APP     ]   swipe up
```

| Layer | left / right | up | down | Back |
| --- | --- | --- | --- | --- |
| Home | prev / next app | open app | open settings | — |
| App | prev / next screen | (back to home) | — | return to home |
| Settings | prev / next screen | — | (back to home) | return to home |

All four directions **follow the finger and snap** on release.

## Files

- `launcher.*` — the nav. One root input handler decides the axis from the first
  move: horizontal drags drive the active layer's carousel; vertical drags slide
  whole layers (home center, app below, settings above). Owns `frij_back()`.
- `registry.*` — the app list + the settings slot. The launcher hardcodes no app
  names; it reads what `apps.cpp` registered.
- `input.*` — the Back source: Backspace in the emulator; a button GPIO on the
  device (TODO).

The carousel itself is a shared widget in [`../ui/`](../ui/README.md). Apps and
settings come from the registry (see [`../apps/`](../apps/README.md)).

## Notes

- Layers with a single screen don't loop.
- Home is persistent; app/settings layers are built on entry and destroyed on
  the way back.
- Back is a hardware button, never a gesture, so it can't fight app content.
