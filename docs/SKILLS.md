# Claude skills & tools for this stack

Notes on Claude Code skills/tools worth using on an LVGL + M5GFX + ESP32 +
PlatformIO project like this one.

> ⚠️ **No `sa-*` skills here.** Those are StashAway-internal and irrelevant to
> this personal project. Ignore them in this repo.
>
> ⚠️ Community skills below are **third-party and unverified** — they can run
> shell commands. Read a skill before enabling it. None are official Anthropic.

## Use now (already available, high value)

- **context7 MCP** — pulls up-to-date **LVGL v9** and **M5GFX** docs on demand.
  Best fix for the #1 risk here: LVGL v8-vs-v9 API drift. Prefer it over guessing
  API. (Already connected in this environment.)
- **Built-in `/code-review` and `/simplify`** — a focused cleanup pass (remove
  duplication, flatten nesting, clarify logic). Good fit for keeping the code readable.

## Worth evaluating (community, vet first)

- **PlatformIO build/flash skill** — automates `pio` build, flash, and serial
  monitor from Claude. Closest match to our daily loop. ([mcpmarket][pio])
- **esp32-claude-workbench** — structured ESP32 firmware workflow (missions,
  debug playbooks, testing-first) with Claude Code skills. Good ideas to borrow
  even if we don't adopt wholesale. ([github][workbench])
- **ESP32 / LVGL skills** (e.g. "ESP32-AI-Agent", "Waveshare ESP32-S3 LCD") —
  carry LVGL v8–v9 references and board pinouts. Partial fit: they assume
  specific boards and often LVGL v8.4, so treat as **reference**, not gospel —
  we're v9 and target-agnostic. ([snyk roundup][snyk], [waveshare skill][wave])

## Discovery (awesome-lists)

- travisvn/awesome-claude-skills ([github][travis])
- ComposioHQ/awesome-claude-skills ([github][composio])
- rohitg00/awesome-claude-code-toolkit ([github][toolkit])

## Recommendation for Frij

1. Lean on **context7** for every LVGL/M5GFX API question.
2. Consider the **PlatformIO build/flash** skill once we start flashing real
   hardware (P5).
3. Prefer a tiny **custom local skill** later (e.g. "scaffold a new Frij app:
   folder + `app.h` descriptor + register line") over heavy third-party skills —
   it stays simple and matches our architecture.
4. Avoid board-locked skills as sources of truth; we're target-agnostic.

[pio]: https://mcpmarket.com/tools/skills/platformio-build-flash
[workbench]: https://github.com/agodianel/esp32-claude-workbench
[snyk]: https://snyk.io/articles/claude-skills-embedded-systems-engineers/
[wave]: https://mcpmarket.com/tools/skills/waveshare-esp32-s3-4-3-lcd-developer-kit
[travis]: https://github.com/travisvn/awesome-claude-skills
[composio]: https://github.com/ComposioHQ/awesome-claude-skills
[toolkit]: https://github.com/rohitg00/awesome-claude-code-toolkit
