#!/usr/bin/env python3
"""Offline test for the iCal -> device events JSON mapping. No network, but
needs the parse deps:  pip install -r requirements.txt

Run:  python3 bridge/test_calendar.py
"""
import datetime
import os

from calendar_to_frij import (MAX_EVENTS, merge_events, parse_calendars,
                              parse_color, payload, to_events_json)

TODAY = datetime.date(2026, 6, 11)

# Mirrors a real Google feed: X-WR-TIMEZONE on the calendar, timed events as
# UTC instants ("Z" — 2026-06-14T01:30Z is 09:30 in Singapore).
ICS = """BEGIN:VCALENDAR
VERSION:2.0
PRODID:-//Test//Test//EN
X-WR-TIMEZONE:Asia/Singapore
BEGIN:VEVENT
UID:past@test
SUMMARY:Old appointment
DTSTART:20260601T020000Z
DTEND:20260601T030000Z
END:VEVENT
BEGIN:VEVENT
UID:dentist@test
SUMMARY:Dentist 🦷
DTSTART:20260614T013000Z
DTEND:20260614T023000Z
LOCATION:Qualiteeth Dental, Serangoon
END:VEVENT
BEGIN:VEVENT
UID:trip@test
SUMMARY:Trip to Iran
DTSTART;VALUE=DATE:20260704
DTEND;VALUE=DATE:20260718
END:VEVENT
BEGIN:VEVENT
UID:gym@test
SUMMARY:Gym class
DTSTART;TZID=Asia/Singapore:20260612T180000
DTEND;TZID=Asia/Singapore:20260612T190000
RRULE:FREQ=WEEKLY;COUNT=3
END:VEVENT
END:VCALENDAR
"""

# A public-holidays style feed: all-day events only.
HOLIDAYS_ICS = """BEGIN:VCALENDAR
VERSION:2.0
PRODID:-//Test//Holidays//EN
X-WR-TIMEZONE:UTC
BEGIN:VEVENT
UID:raya@test
SUMMARY:Hari Raya Haji
DTSTART;VALUE=DATE:20260616
DTEND;VALUE=DATE:20260617
END:VEVENT
END:VCALENDAR
"""


def test_color_normalizes():
    assert parse_color("#38BDF8") == "38BDF8"
    assert parse_color("f472b6") == "F472B6"
    assert parse_color("nope") == "F472B6"   # malformed -> brand pink fallback
    assert parse_color("") == "F472B6"


def test_parse_calendars(monkeyenv=None):
    # Only GCALENDAR_* keys, in sorted order; name/color/holiday parsed; URL kept.
    saved = dict(os.environ)
    try:
        for k in list(os.environ):
            if k.startswith("GCALENDAR_"):
                del os.environ[k]
        os.environ["GCALENDAR_FAMILY"] = "https://x/basic.ics,Family,#F472B6"
        os.environ["GCALENDAR_HOLIDAYS"] = "https://y/public.ics,Holidays,6B6B74,holiday"
        os.environ["GCALENDAR_NONAME"] = "https://z/basic.ics"      # name defaults from key
        cals = parse_calendars()
        assert cals == [
            {"name": "Family", "color": "F472B6", "holiday": False, "url": "https://x/basic.ics"},
            {"name": "Holidays", "color": "6B6B74", "holiday": True, "url": "https://y/public.ics"},
            {"name": "Noname", "color": "F472B6", "holiday": False, "url": "https://z/basic.ics"},
        ], cals
    finally:
        os.environ.clear()
        os.environ.update(saved)


def test_mapping():
    out = to_events_json(ICS.encode(), TODAY, "Family")
    # past event dropped; soonest first; recurrence expanded to 3 occurrences;
    # emoji stripped; UTC instants rendered as Singapore wall-clock; same-day
    # ends become "te"; locations carried; multi-day all-day events get an
    # inclusive "de" (DTEND Jul 18 exclusive -> Jul 17); every item tagged "c"
    assert out == [
        {"t": "Gym class", "d": "2026-06-12", "tm": "18:00", "te": "19:00", "c": "Family"},
        {"t": "Dentist", "d": "2026-06-14", "tm": "09:30", "te": "10:30",
         "l": "Qualiteeth Dental, Serangoon", "c": "Family"},
        {"t": "Gym class", "d": "2026-06-19", "tm": "18:00", "te": "19:00", "c": "Family"},
        {"t": "Gym class", "d": "2026-06-26", "tm": "18:00", "te": "19:00", "c": "Family"},
        {"t": "Trip to Iran", "d": "2026-07-04", "de": "2026-07-17", "c": "Family"},
    ], out


def test_holidays_merge():
    family = to_events_json(ICS.encode(), TODAY, "Family")
    holidays = to_events_json(HOLIDAYS_ICS.encode(), TODAY, "Holidays", holiday=True)
    # holiday flagged with "h", tagged with its calendar, slots into date order
    assert holidays == [{"t": "Hari Raya Haji", "d": "2026-06-16",
                         "c": "Holidays", "h": True}], holidays
    merged = merge_events(family, holidays)
    titles = [e["t"] for e in merged]
    assert titles == ["Gym class", "Dentist", "Hari Raya Haji", "Gym class",
                      "Gym class", "Trip to Iran"], titles


def test_payload_shape():
    cals = [{"name": "Family", "color": "F472B6", "holiday": False, "url": "u"},
            {"name": "Holidays", "color": "6B6B74", "holiday": True, "url": "v"}]
    value = payload([{"t": "X", "d": "2026-06-12", "c": "Family"}], cals, at=1765400000.7)
    assert value == {
        "at": 1765400000,
        "cal": [{"n": "Family", "c": "F472B6"},
                {"n": "Holidays", "c": "6B6B74", "h": True}],
        "ev": [{"t": "X", "d": "2026-06-12", "c": "Family"}],
    }, value


def test_caps_to_device_limit():
    many = "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:-//T//T//EN\n" + "".join(
        f"BEGIN:VEVENT\nUID:e{i}@test\nSUMMARY:Event {i}\n"
        f"DTSTART;VALUE=DATE:202607{i + 1:02d}\nEND:VEVENT\n" for i in range(MAX_EVENTS + 4)
    ) + "END:VCALENDAR\n"
    assert len(merge_events(to_events_json(many.encode(), TODAY, "Family"))) == MAX_EVENTS


if __name__ == "__main__":
    test_color_normalizes()
    test_parse_calendars()
    test_mapping()
    test_holidays_merge()
    test_payload_shape()
    test_caps_to_device_limit()
    print("all calendar tests passed")
