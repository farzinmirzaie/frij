# ui/

Shared, **app-agnostic** UI building blocks reused by apps and the launcher.
There's no design language yet — this is where it grows (styled buttons, a
theme, list rows, …) as patterns repeat.

## Now

- `carousel.*` — a horizontal, looping, finger-following pager. Input-free: the
  owner calls `drag(dx)` during a drag and `end(dx)` on release. No loop when
  `count <= 1`.

## Guideline

A component here takes an `lv_obj_t* parent` (and plain data), never app
specifics. If two apps would copy the same widget, put it here instead.
