#!/usr/bin/env python3
"""Offline test for the iCal -> device events JSON mapping. No network, but
needs the parse deps:  pip install -r requirements.txt

Run:  python3 bridge/test_calendar.py
"""
import datetime

from calendar_to_frij import MAX_EVENTS, merge_events, payload, to_events_json

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


def test_mapping():
    out = to_events_json(ICS.encode(), TODAY)
    # past event dropped; soonest first; recurrence expanded to 3 occurrences;
    # emoji stripped; UTC instants rendered as Singapore wall-clock; same-day
    # ends become "te"; locations carried; multi-day all-day events get an
    # inclusive "de" (DTEND Jul 18 exclusive -> Jul 17)
    assert out == [
        {"t": "Gym class", "d": "2026-06-12", "tm": "18:00", "te": "19:00"},
        {"t": "Dentist", "d": "2026-06-14", "tm": "09:30", "te": "10:30",
         "l": "Qualiteeth Dental, Serangoon"},
        {"t": "Gym class", "d": "2026-06-19", "tm": "18:00", "te": "19:00"},
        {"t": "Gym class", "d": "2026-06-26", "tm": "18:00", "te": "19:00"},
        {"t": "Trip to Iran", "d": "2026-07-04", "de": "2026-07-17"},
    ], out


def test_holidays_merge():
    family = to_events_json(ICS.encode(), TODAY)
    holidays = to_events_json(HOLIDAYS_ICS.encode(), TODAY, holiday=True)
    # holiday flagged with "h" and slots into date order between family events
    assert holidays == [{"t": "Hari Raya Haji", "d": "2026-06-16", "h": True}], holidays
    merged = merge_events(family, holidays)
    titles = [e["t"] for e in merged]
    assert titles == ["Gym class", "Dentist", "Hari Raya Haji", "Gym class",
                      "Gym class", "Trip to Iran"], titles


def test_payload_shape():
    value = payload([{"t": "X", "d": "2026-06-12"}], at=1765400000.7)
    assert value == {"at": 1765400000, "ev": [{"t": "X", "d": "2026-06-12"}]}, value


def test_caps_to_device_limit():
    many = "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:-//T//T//EN\n" + "".join(
        f"BEGIN:VEVENT\nUID:e{i}@test\nSUMMARY:Event {i}\n"
        f"DTSTART;VALUE=DATE:202607{i + 1:02d}\nEND:VEVENT\n" for i in range(MAX_EVENTS + 4)
    ) + "END:VCALENDAR\n"
    assert len(merge_events(to_events_json(many.encode(), TODAY))) == MAX_EVENTS


if __name__ == "__main__":
    test_mapping()
    test_holidays_merge()
    test_payload_shape()
    test_caps_to_device_limit()
    print("all calendar tests passed")
