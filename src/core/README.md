# core/

App-agnostic, **non-UI** shared helpers — pure logic that several apps need but
that isn't a widget (so it doesn't belong in `ui/`) and isn't board-specific (so
it doesn't belong in `system/` or `utility/`).

## Now

- `datetime.*` — `frij_clock_is_24h()` (the single source of truth for the
  "24-hour time" setting) + `frij_format_time()` (`14:30` / `2:30 PM`). Use these
  so the watch face, Last sync, and any future time display all agree and react
  to the toggle.

## Guideline

Keep it dependency-light: standard C/LVGL utilities + the `store`. No LVGL
widgets here, no hardware. If a helper starts drawing, it belongs in `ui/`.
