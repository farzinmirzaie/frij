#!/usr/bin/env python3
"""One-way sync: one or more Google Calendars -> the Frij store.

Each calendar is declared in the environment as a `GCALENDAR_*` variable whose
value is comma-separated `url,name,color`:

    GCALENDAR_FAMILY=https://calendar.google.com/calendar/ical/.../basic.ics,Family,F472B6
    GCALENDAR_WORK=https://.../basic.ics,Work,38BDF8
    GCALENDAR_HOLIDAYS=https://.../public/basic.ics,Holidays,6B6B74

  - url    the calendar's **secret iCal URL** (Google Calendar > Settings >
           "Integrate calendar" > Secret address in iCal format) — a plain .ics
           feed, so no OAuth/API project is needed. Treat it like a password.
           (URLs never contain commas, so the split is unambiguous.)
  - name   short display name; also the per-event tag the device filters on.
  - color  6-hex RRGGBB (leading "#" optional) — the calendar's badge color.
           (A "holidays" calendar is just one given a gray color, e.g. 6B6B74 —
           there's no special holiday handling; every calendar is treated alike.)

Recurring events (RRULEs like "every 6 months") are expanded to concrete
occurrences with `recurring-ical-events`. The payload lands in Supabase
`store:events` as

    {"at": 1765400000,                       # unix epoch of this sync
     "cal": [{"n": "Family", "c": "F472B6"},  # every declared calendar, so the
             {"n": "Holidays", "c": "6B6B74"}],   # device can list + toggle each
     "ev": [{"t": "Dentist", "d": "2026-06-14", "tm": "09:30", "te": "10:30",
             "l": "Qualiteeth", "c": "Family"},
            {"t": "Hari Raya", "d": "...", "c": "Holidays"}, ...]}

("tm"/"te" = start/end clock, omitted for all-day events; "de" = inclusive end
date for multi-day all-day events; "l" = location; "c" = calendar name). Times
are rendered in FRIJ_TZ (or the calendar's own X-WR-TIMEZONE) — Google's feed
stores them as UTC instants, which would otherwise be hours off. The device's
Events app renders countdowns and lets the user toggle calendars on/off; it
never talks to Google.

A single broken/unreachable feed is non-fatal: its calendar is still listed (so
its toggle persists) but contributes no events until the next run.

Like the Keep bridge, this runs off-device on the GitHub Actions cron — the
secret URLs live in GitHub secrets / the local .env, never in the repo.

Usage:
    python3 calendar_to_frij.py            # fetch + upsert store:events
    python3 calendar_to_frij.py --dry-run  # print the JSON, write nothing
"""
import datetime
import json
import os
import sys
import time
import urllib.request

# Shared helpers (dotenv, env, Supabase upsert, emoji strip) live in the Keep
# bridge; both scripts run from this directory.
from keep_to_frij import clean_text, env, load_dotenv, upsert_supabase

MAX_EVENTS = 10   # matches the device's cap (src/packages/data/events.h FRIJ_EVENTS_MAX)
MAX_CALS = 8      # matches the device's cap (FRIJ_CAL_MAX)
TEXT_MAX = 63     # matches the device's TEXT_LEN - 1
WINDOW_DAYS = 365  # how far ahead to look (covers anniversaries/birthdays)


def parse_color(raw):
    """Normalize a calendar color to bare 6-hex RRGGBB (uppercase). Falls back
    to the brand pink (F472B6) when missing or malformed."""
    s = (raw or "").strip().lstrip("#").upper()
    if len(s) == 6 and all(c in "0123456789ABCDEF" for c in s):
        return s
    if raw:
        print(f"warning: bad color '{raw}', using F472B6", file=sys.stderr)
    return "F472B6"


def parse_calendars():
    """Read every GCALENDAR_* env var into [{name, color, url}], in sorted
    env-key order (stable across runs). Exits if none are set."""
    cals = []
    for key in sorted(os.environ):
        if not key.startswith("GCALENDAR_"):
            continue
        # split into at most 3 fields; any extra trailing token is ignored
        parts = [p.strip() for p in os.environ[key].split(",", 3)]
        url = parts[0] if parts else ""
        if not url:
            print(f"warning: {key} has no URL, skipping", file=sys.stderr)
            continue
        name = (parts[1] if len(parts) > 1 and parts[1] else key[len("GCALENDAR_"):].title())
        color = parse_color(parts[2] if len(parts) > 2 else "")
        cals.append({"name": name[:TEXT_MAX], "color": color, "url": url})
    if not cals:
        sys.exit("error: no GCALENDAR_* calendars configured (see bridge/README.md)")
    if len(cals) > MAX_CALS:
        print(f"note: {len(cals)} calendars, capping to {MAX_CALS} (device limit)", file=sys.stderr)
    return cals[:MAX_CALS]


def fetch_ics(url):
    with urllib.request.urlopen(url, timeout=30) as resp:
        return resp.read()


def calendar_tz(cal):
    """The timezone to render clock times in: FRIJ_TZ env override, else the
    calendar's own X-WR-TIMEZONE, else leave instants as they came."""
    from zoneinfo import ZoneInfo

    name = env("FRIJ_TZ") or str(cal.get("X-WR-TIMEZONE", "") or "")
    try:
        return ZoneInfo(name) if name else None
    except KeyError:
        print(f"warning: unknown timezone '{name}', leaving times as-is", file=sys.stderr)
        return None


def to_events_json(ics_bytes, today, name):
    """Expand one calendar feed into device items tagged with `name`: every
    occurrence inside the window, soonest first (uncapped — merge_events applies
    the device cap)."""
    import icalendar
    import recurring_ical_events

    cal = icalendar.Calendar.from_ical(ics_bytes)
    tz = calendar_tz(cal)
    window = (today, today + datetime.timedelta(days=WINDOW_DAYS))
    out = []
    for ev in recurring_ical_events.of(cal).between(*window):
        start = ev.get("DTSTART")
        if start is None:
            continue
        start = start.dt
        end = ev.get("DTEND")
        end = end.dt if end is not None else None
        title = clean_text(str(ev.get("SUMMARY", "")))
        if not title:
            continue
        item = {"t": title[:TEXT_MAX]}
        if isinstance(start, datetime.datetime):
            # Google's feed stores timed events as UTC instants — render the
            # wall-clock time in the calendar's timezone, not UTC.
            if tz and start.tzinfo is not None:
                start = start.astimezone(tz)
            item["d"] = start.date().isoformat()
            item["tm"] = start.strftime("%H:%M")
            if isinstance(end, datetime.datetime):
                if tz and end.tzinfo is not None:
                    end = end.astimezone(tz)
                # only a same-day end is useful as a "12:00 - 13:00" range
                if end > start and end.date() == start.date():
                    item["te"] = end.strftime("%H:%M")
        else:  # all-day event: a plain date, no time
            item["d"] = start.isoformat()
            # DTEND is exclusive; expose the inclusive last day when it spans
            # more than one day ("Trip" Jul 4-18 -> de = Jul 17)
            if isinstance(end, datetime.date) and not isinstance(end, datetime.datetime):
                last = end - datetime.timedelta(days=1)
                if last > start:
                    item["de"] = last.isoformat()
        location = clean_text(str(ev.get("LOCATION", "")))
        if location:
            item["l"] = location[:TEXT_MAX]
        item["c"] = name        # the calendar this event belongs to (device filter + color)
        out.append(item)
    # soonest first; all-day events sort before timed ones on the same day
    out.sort(key=lambda e: (e["d"], e.get("tm", "")))
    return out


def merge_events(*lists):
    """Merge per-feed lists, soonest first, capped to the device limit."""
    out = sorted((e for lst in lists for e in lst),
                 key=lambda e: (e["d"], e.get("tm", "")))
    if len(out) > MAX_EVENTS:
        print(f"note: {len(out)} upcoming events, capping to {MAX_EVENTS} (device limit)",
              file=sys.stderr)
    return out[:MAX_EVENTS]


def payload(events, cals, at):
    """The store value: the calendars (name/color, for the device's toggle
    screen), the events, and the sync time ("Updated Xm ago")."""
    cal_meta = [{"n": c["name"], "c": c["color"]} for c in cals]
    return {"at": int(at), "cal": cal_meta, "ev": events}


def main():
    load_dotenv()
    write = "--dry-run" not in sys.argv
    today = datetime.date.today()
    cals = parse_calendars()

    # Sync each feed independently. A broken feed warns and contributes nothing,
    # but its calendar still rides in "cal" so its toggle persists on the device.
    per_feed = []
    for c in cals:
        try:
            per_feed.append(to_events_json(fetch_ics(c["url"]), today, c["name"]))
        except Exception as e:  # noqa: BLE001 — one feed must not block the rest
            print(f"warning: calendar '{c['name']}' failed ({e}); skipping it this run",
                  file=sys.stderr)

    value = payload(merge_events(*per_feed), cals, time.time())
    print(json.dumps(value, ensure_ascii=False))

    url = env("SUPABASE_URL", required=True)
    anon = env("SUPABASE_ANON_KEY", required=True)
    table = env("SUPABASE_TABLE") or "store"
    key = env("FRIJ_EVENTS_KEY") or "events"
    if write:
        upsert_supabase(url, anon, table, key, value)
        print(f"synced {len(value['ev'])} event(s) from {len(cals)} calendar(s) -> {table}:{key}",
              file=sys.stderr)
    else:
        print(f"dry-run: would write {len(value['ev'])} event(s) to {table}:{key}",
              file=sys.stderr)


if __name__ == "__main__":
    main()
