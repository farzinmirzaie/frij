#!/usr/bin/env python3
"""One-way sync: a Google Calendar (family calendar) -> the Frij store.

Reads the calendar's **secret iCal URL** (Google Calendar ▸ Settings ▸
"Integrate calendar" ▸ Secret address in iCal format) — a plain .ics feed, so
no OAuth/API project is needed. Recurring events (RRULEs like "every 6
months") are expanded to concrete occurrences with `recurring-ical-events`.

The next N upcoming events land in Supabase `store:events` as
    [{"t": "Dentist", "d": "2026-06-14", "tm": "09:30"}, ...]
("tm" is omitted for all-day events). The device's Events app renders them as
countdowns; it never talks to Google.

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


def to_events_json(ics_bytes, today):
    """Expand the calendar into the device's JSON: the next MAX_EVENTS
    occurrences on/after `today` (a date), soonest first."""
    import icalendar
    import recurring_ical_events

    cal = icalendar.Calendar.from_ical(ics_bytes)
    window = (today, today + datetime.timedelta(days=WINDOW_DAYS))
    out = []
    for ev in recurring_ical_events.of(cal).between(*window):
        start = ev.get("DTSTART")
        if start is None:
            continue
        start = start.dt
        title = clean_text(str(ev.get("SUMMARY", "")))
        if not title:
            continue
        item = {"t": title[:TEXT_MAX]}
        if isinstance(start, datetime.datetime):
            item["d"] = start.date().isoformat()
            item["tm"] = start.strftime("%H:%M")
        else:  # all-day event: a plain date, no time
            item["d"] = start.isoformat()
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
