#!/usr/bin/env python3
"""One-way sync: a Google Calendar (family calendar) -> the Frij store.

Reads the calendar's **secret iCal URL** (Google Calendar ▸ Settings ▸
"Integrate calendar" ▸ Secret address in iCal format) — a plain .ics feed, so
no OAuth/API project is needed. Recurring events (RRULEs like "every 6
months") are expanded to concrete occurrences with `recurring-ical-events`.

The next N upcoming events land in Supabase `store:events` as
    [{"t": "Dentist", "d": "2026-06-14", "tm": "09:30", "te": "10:30",
      "l": "Qualiteeth"}, ...]
("tm"/"te" = start/end clock, omitted for all-day events; "de" = inclusive end
date for multi-day all-day events; "l" = location). Times are rendered in the
calendar's own timezone (X-WR-TIMEZONE, or the FRIJ_TZ env override) — Google's
feed stores them as UTC instants, which would otherwise be hours off. The
device's Events app renders them as countdowns; it never talks to Google.

Like the Keep bridge, this runs off-device on the GitHub Actions cron — the
secret URL lives in a GitHub secret / the local .env, never in the repo.

Usage:
    python3 calendar_to_frij.py            # fetch + upsert store:events
    python3 calendar_to_frij.py --dry-run  # print the JSON, write nothing
"""
import datetime
import json
import sys
import urllib.request

# Shared helpers (dotenv, env, Supabase upsert, emoji strip) live in the Keep
# bridge; both scripts run from this directory.
from keep_to_frij import clean_text, env, load_dotenv, upsert_supabase

MAX_EVENTS = 10   # matches the device's cap (src/apps/events/events.cpp)
TEXT_MAX   = 63   # matches the device's TEXT_LEN - 1
WINDOW_DAYS = 365  # how far ahead to look (covers anniversaries/birthdays)


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


def to_events_json(ics_bytes, today):
    """Expand the calendar into the device's JSON: the next MAX_EVENTS
    occurrences on/after `today` (a date), soonest first."""
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
        out.append(item)
    # soonest first; all-day events sort before timed ones on the same day
    out.sort(key=lambda e: (e["d"], e.get("tm", "")))
    if len(out) > MAX_EVENTS:
        print(f"note: {len(out)} upcoming events, capping to {MAX_EVENTS} (device limit)",
              file=sys.stderr)
    return out[:MAX_EVENTS]


def main():
    load_dotenv()
    write = "--dry-run" not in sys.argv

    ics = fetch_ics(env("FRIJ_ICS_URL", required=True))
    events = to_events_json(ics, datetime.date.today())
    print(json.dumps(events, ensure_ascii=False))

    url = env("SUPABASE_URL", required=True)
    anon = env("SUPABASE_ANON_KEY", required=True)
    table = env("SUPABASE_TABLE") or "store"
    key = env("FRIJ_EVENTS_KEY") or "events"
    if write:
        upsert_supabase(url, anon, table, key, events)
        print(f"synced {len(events)} event(s) -> {table}:{key}", file=sys.stderr)
    else:
        print(f"dry-run: would write {len(events)} event(s) to {table}:{key}", file=sys.stderr)


if __name__ == "__main__":
    main()
