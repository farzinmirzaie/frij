# ui/fonts/

LVGL fonts for the UI, generated from **SF Rounded** with `lv_font_conv`.

| File | Weight | Size | Range |
| --- | --- | --- | --- |
| `frij_sf_body.c` | Regular | 18 px | ASCII |
| `frij_sf_header.c` | Semibold | 22 px | ASCII (app header title) |
| `frij_sf_title.c` | Semibold | 26 px | ASCII |
| `frij_sf_display.c` | Semibold | 34 px | ASCII |
| `frij_sf_logo.c` | Semibold | 56 px | `Frij` only (the wordmark) |
| `frij_sf_clock.c` | Semibold | 64 px | digits + `:` |

Wired via `ui/theme.h` (`FRIJ_FONT_*`). ASCII range only — symbol glyphs
(✓ ± +) aren't included, so symbol labels use Montserrat (`FRIJ_FONT_SYMBOL`,
`FRIJ_FONT_SYMBOL_L`).

## ⚠️ License

SF is **Apple-proprietary**. These generated `.c` files embed the glyph
outlines. Fine for personal/on-device use; do **not** treat them as freely
redistributable. Swap in a freely-licensed rounded font here if this ever needs
to be truly open.

## Regenerate

The current cuts come from macOS's system variable font, instanced to static
weights with fontTools (the repo's `.venv` has it):

```sh
.venv/bin/pip install fonttools
.venv/bin/fonttools varLib.instancer /System/Library/Fonts/SFNSRounded.ttf wght=400 -o /tmp/sfr-regular.ttf
.venv/bin/fonttools varLib.instancer /System/Library/Fonts/SFNSRounded.ttf wght=600 -o /tmp/sfr-semibold.ttf

cd src/ui/fonts
R=/tmp/sfr-regular.ttf
S=/tmp/sfr-semibold.ttf
npx -y lv_font_conv@latest --font "$R" --size 18 --bpp 4 --range 0x20-0x7F --no-compress --format lvgl -o frij_sf_body.c
npx -y lv_font_conv@latest --font "$S" --size 22 --bpp 4 --range 0x20-0x7F --no-compress --format lvgl -o frij_sf_header.c
npx -y lv_font_conv@latest --font "$S" --size 26 --bpp 4 --range 0x20-0x7F --no-compress --format lvgl -o frij_sf_title.c
npx -y lv_font_conv@latest --font "$S" --size 34 --bpp 4 --range 0x20-0x7F --no-compress --format lvgl -o frij_sf_display.c
npx -y lv_font_conv@latest --font "$S" --size 56 --bpp 4 --symbols "Frij" --no-compress --format lvgl -o frij_sf_logo.c
npx -y lv_font_conv@latest --font "$S" --size 64 --bpp 4 --symbols "0123456789:" --no-compress --format lvgl -o frij_sf_clock.c
```

(Apple's official static OTFs from developer.apple.com/fonts also work — point
`R`/`S` at `SF-Pro-Rounded-Regular.otf` / `SF-Pro-Rounded-Semibold.otf`.)
