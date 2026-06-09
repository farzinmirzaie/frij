# ui/fonts/

LVGL fonts for the UI, generated from **SF Pro Rounded** with `lv_font_conv`.

| File | Source | Size | Weight | Range |
| --- | --- | --- | --- | --- |
| `frij_sf_body.c` | SF-Pro-Rounded-Regular | 18 px | Regular | ASCII |
| `frij_sf_title.c` | SF-Pro-Rounded-Semibold | 26 px | Semibold | ASCII |
| `frij_sf_display.c` | SF-Pro-Rounded-Semibold | 34 px | Semibold | ASCII |
| `frij_sf_clock.c` | SF-Pro-Rounded-Semibold | 64 px | Semibold | digits + `:` |

Wired via `ui/theme.h` (`FRIJ_FONT_*`). ASCII range only — symbol glyphs
(✓ ± +) aren't included, so symbol labels use Montserrat (`FRIJ_FONT_SYMBOL`).

## ⚠️ License

SF Pro is **Apple-proprietary** (SF Pro license). These generated `.c` files
embed the glyph outlines. Fine for personal/on-device use; do **not** treat them
as freely redistributable. Swap in a freely-licensed rounded font here if this
ever needs to be truly open.

## Regenerate

The OTFs are **not** committed. Fetch them, then:

```sh
R=SF-Pro-Rounded-Regular.otf
S=SF-Pro-Rounded-Semibold.otf
npx -y lv_font_conv@latest --font "$R" --size 18 --bpp 4 --range 0x20-0x7F --no-compress --format lvgl -o frij_sf_body.c
npx -y lv_font_conv@latest --font "$S" --size 26 --bpp 4 --range 0x20-0x7F --no-compress --format lvgl -o frij_sf_title.c
npx -y lv_font_conv@latest --font "$S" --size 34 --bpp 4 --range 0x20-0x7F --no-compress --format lvgl -o frij_sf_display.c
npx -y lv_font_conv@latest --font "$S" --size 64 --bpp 4 --symbols "0123456789:" --no-compress --format lvgl -o frij_sf_clock.c
```
