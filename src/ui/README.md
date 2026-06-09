# ui/

Shared, **app-agnostic** UI building blocks reused by apps and the launcher.
There's no design language yet — this is where it grows (styled buttons, a
theme, list rows, …) as patterns repeat.

## Now

- `theme.h` — design tokens: colors (`FRIJ_PRIMARY`, `FRIJ_SURFACE_*`, …),
  fonts (SF Pro Rounded — see `fonts/`), 4pt spacing, radius, motion.
- `fonts/` — SF Pro Rounded converted to LVGL fonts (see its README + license note).
- `components.*` — themed widgets:
  - `frij_col` / `frij_label` — layout + typed text.
  - `frij_page` — the standard page body: a **vertically scrollable** column.
    After adding children call `frij_page_settle` to finalize it (centers when
    the content fits, top-aligns when it overflows so the first row stays
    visible). The launcher turns a swipe past the scroll edge into Back.
  - `frij_page_under_header` — reusable "safe area": insets a page beneath a top
    bar so its centered content still lands at the screen's true center.
  - `frij_apply_bg` — the subtle page-background gradient; `frij_screen_min` —
    shorter screen side, for responsive sizing.
  - `frij_surface_row` — rounded row with a subtle gradient + press feedback.
  - `frij_check` / `frij_check_set` — circular check with a pop animation.
  - `frij_slider_row` — a full-width card that IS the slider: drag anywhere to
    set, accent fill shows the amount, label on the left and a live value
    readout (with a unit, e.g. `80%`) on the right.
  - `frij_toggle` — themed switch (takes an accent).
  - `frij_action_row` — a tappable card row (label + chevron) for actions.
  - `frij_confirm` — a modal confirmation dialog (dimmed backdrop + centered
    card with Cancel / accent-Confirm). Use it before destructive actions.
  - `frij_action_sheet` — a modal with a stacked list of options (first is the
    accent/primary) + Cancel; the callback gets the chosen index. Both modals
    animate in (dim fades, card rises) and dismiss on a backdrop tap; the
    launcher's Back closes the open one first (`frij_modal_close_top`).
  - `frij_section_label` — a small muted heading to group rows on a page.
  - `frij_toast` — a brief auto-dismissing snackbar near the bottom (fades in,
    holds, fades out, self-removes). For transient confirmations.
  - `frij_glow` — soft radial accent halo; **FLOATING** (doesn't scroll or grow
    the scroll area). Returns the object so it can be repositioned.
  - `frij_top_tint` — a restrained accent wash along the top edge that fades to
    nothing (the per-app color cue behind a header). FLOATING.
  - `frij_progress_ring` — thin arc gauge.
  - `frij_empty_state` — round "nothing here" placeholder.
  - `frij_header` — shared app top bar: back button (returns to the launcher) +
    centered title + optional right action button. Round-safe placement.
  - `frij_anim_enter` — fade + rise entrance (stagger lists with a delay).
  - `frij_stagger_in` — run `frij_anim_enter` across a container's children with
    an increasing delay; the shared staggered-list entrance.
- `carousel.*` — a horizontal, looping, finger-following pager. Input-free: the
  owner calls `drag(dx)` / `end(dx)`. No loop when `count <= 1`. Shows an
  auto-fading **page-dot indicator** at the bottom (active dot uses the accent
  passed to `init`; hidden entirely for a single page).

## Guideline

A component takes an `lv_obj_t* parent` (and plain data), never app specifics,
and styles itself from `theme.h`. If two apps would copy the same widget or
look, put it here. Motion stays subtle: ~200–250ms, ease-out.

**Responsive:** use `%` widths (`frij_page` is 86%) and `frij_screen_min()` for
pixel sizes that should scale, so layouts hold on the larger 466×466 device.
