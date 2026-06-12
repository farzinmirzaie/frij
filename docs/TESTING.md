# Testing & validating changes

How to verify a change before calling it done. There's no live-window capture
in the agent environment (no GUI session → `screencapture` fails), so use the
**headless snapshot tool** to see the UI.

`pio` isn't on PATH: use `~/.platformio/penv/bin/pio`.

## 1. Build

```sh
~/.platformio/penv/bin/pio run            # default emulator_StopWatch (466 round)
```

Must end in `SUCCESS`. Fix all `error:` lines.

## 2. Smoke-run (no crash)

```sh
.pio/build/emulator_StopWatch/program &   # opens the live SDL window on a real Mac
sleep 8; pkill -f emulator_StopWatch/program
```

Check it launches without asserts. (On a headless agent the window won't show,
but a crash/assert still prints to the log — use the snapshot tool to actually see it.)

## 3. See the UI (headless snapshot)

```sh
~/.platformio/penv/bin/pio run -e snapshot
.pio/build/snapshot/program                       # default: launcher / home
FRIJ_SNAP=todo .pio/build/snapshot/program        # one screen — see set below
sips -s format png /tmp/frij_snapshot.bmp --out /tmp/frij_snapshot.png
```

`FRIJ_SNAP` values: `todo` `todo_progress` `todo_add` `todo_glance` `events`
`events_glance` `events_countdown` `assistant` `assistant_glance` `ai_listen`
`ai_answer` `ai_error` `ai_thinking` `counter` `stopwatch` `stopwatch_glance` `scoreboard`
`scoreboard_glance` `settings`
(General) `network` `netoff` `sheet` `confirm` `keyboard` (numpad) `result`
`about`. (unset = the launcher/home.) Exit codes: 0 ok, 1 capture failed,
2 unknown key — scriptable in CI.

Then open/Read `/tmp/frij_snapshot.png`. It renders the real UI offscreen at
466×466 with the round clip, so it's faithful to the device.

## 4. Checklist before done

- [ ] `pio run` SUCCESS (and `-e snapshot` if UI changed).
- [ ] Snapshot reviewed for any screen you touched (home / todo / counter / settings).
- [ ] No content clipped by the round edge; text legible at 466.
- [ ] `docs/CHANGELOG.md` updated.
- [ ] Round left uncommitted for review (see the commit/review loop in CLAUDE.md).
