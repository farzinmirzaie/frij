#!/usr/bin/env python3
"""One-way sync: Google Calendars (family + optional holidays) -> the Frij store.

Reads the family calendar's **secret iCal URL** (Google Calendar ▸ Settings ▸
"Integrate calendar" ▸ Secret address in iCal format) — a plain .ics feed, so
no OAuth/API project is needed. Recurring events (RRULEs like "every 6
months") are expanded to concrete occurrences with `recurring-ical-events`.
A second, optional feed (FRIJ_HOLIDAYS_ICS_URL — e.g. Google's public Malaysia
holidays calendar) merges in with `"h": true` so the device can render
holidays differently. Missing/failed holidays feed never blocks the family sync.

The payload lands in Supabase `store:events` as
    {"at": 1765400000,            # unix epoch of this sync ("Updated Xm ago")
     "ev": [{"t": "Dentist", "d": "2026-06-14", "tm": "09:30", "te": "10:30",
             "l": "Qualiteeth"}, {"t": "Hari Raya", "d": "...", "h": true}, ...]}
("tm"/"te" = start/end clock, omitted for all-day events; "de" = inclusive end
date for multi-day all-day events; "l" = location; "h" = holiday). Times are
rendered in FRIJ_TZ (or the calendar's own X-WR-TIMEZONE) — Google's feed
stores them as UTC instants, which would otherwise be hours off. The device's
Events app renders countdowns; it never talks to Google.

Like the Keep bridge, this runs off-device on the GitHub Actions cron — the
secret URL lives in a GitHub secret / the local .env, never in the repo.

Usage:
    python3 calendar_to_frij.py            # fetch + upsert store:events
    python3 calendar_to_frij.py --dry-run  # print the JSON, write nothing
"""
import datetime
import json
import sys
import time
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


def to_events_json(ics_bytes, today, holiday=False):
    """Expand one calendar feed into device items: every occurrence inside the
    window, soonest first (uncapped — merge_events applies the device cap)."""
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
        if holiday:
            item["h"] = True
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


def payload(events, at):
    """The store value: the events plus the sync time ("Updated Xm ago")."""
    return {"at": int(at), "ev": events}


def main():
    load_dotenv()
    write = "--dry-run" not in sys.argv
    today = datetime.date.today()

    family = to_events_json(fetch_ics(env("FRIJ_ICS_URL", required=True)), today)

    # Optional second feed (public holidays). Absent or broken never blocks
    # the family sync — holidays just drop out until the next run.
    holidays = []
    holidays_url = env("FRIJ_HOLIDAYS_ICS_URL")
    if holidays_url:
        try:
            holidays = to_events_json(fetch_ics(holidays_url), today, holiday=True)
        except Exception as e:  # noqa: BLE001 — any feed failure is non-fatal
            print(f"warning: holidays feed failed ({e}); syncing family only", file=sys.stderr)

    value = payload(merge_events(family, holidays), time.time())
    print(json.dumps(value, ensure_ascii=False))

    url = env("SUPABASE_URL", required=True)
    anon = env("SUPABASE_ANON_KEY", required=True)
    table = env("SUPABASE_TABLE") or "store"
    key = env("FRIJ_EVENTS_KEY") or "events"
    if write:
        upsert_supabase(url, anon, table, key, value)
        print(f"synced {len(value['ev'])} event(s) -> {table}:{key}", file=sys.stderr)
    else:
        print(f"dry-run: would write {len(value['ev'])} event(s) to {table}:{key}",
              file=sys.stderr)


if __name__ == "__main__":
    main()
