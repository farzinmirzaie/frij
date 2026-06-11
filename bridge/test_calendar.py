#!/usr/bin/env python3
"""Offline test for the iCal -> device events JSON mapping. No network, but
needs the parse deps:  pip install -r requirements.txt

Run:  python3 bridge/test_calendar.py
"""
import datetime

from calendar_to_frij import MAX_EVENTS, to_events_json

TODAY = datetime.date(2026, 6, 11)

ICS = """BEGIN:VCALENDAR
VERSION:2.0
PRODID:-//Test//Test//EN
BEGIN:VEVENT
UID:past@test
SUMMARY:Old appointment
DTSTART;TZID=Asia/Singapore:20260601T100000
DTEND;TZID=Asia/Singapore:20260601T110000
END:VEVENT
BEGIN:VEVENT
UID:dentist@test
SUMMARY:Dentist 🦷
DTSTART;TZID=Asia/Singapore:20260614T093000
DTEND;TZID=Asia/Singapore:20260614T103000
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


def test_mapping():
    out = to_events_json(ICS.encode(), TODAY)
    # past event dropped; soonest first; recurrence expanded to 3 occurrences;
    # emoji stripped; all-day event has no "tm"
    assert out == [
        {"t": "Gym class", "d": "2026-06-12", "tm": "18:00"},
        {"t": "Dentist", "d": "2026-06-14", "tm": "09:30"},
        {"t": "Gym class", "d": "2026-06-19", "tm": "18:00"},
        {"t": "Gym class", "d": "2026-06-26", "tm": "18:00"},
        {"t": "Trip to Iran", "d": "2026-07-04"},
    ], out


def test_caps_to_device_limit():
    many = "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:-//T//T//EN\n" + "".join(
        f"BEGIN:VEVENT\nUID:e{i}@test\nSUMMARY:Event {i}\n"
        f"DTSTART;VALUE=DATE:202607{i + 1:02d}\nEND:VEVENT\n" for i in range(MAX_EVENTS + 4)
    ) + "END:VCALENDAR\n"
    assert len(to_events_json(many.encode(), TODAY)) == MAX_EVENTS


if __name__ == "__main__":
    test_mapping()
    test_caps_to_device_limit()
    print("all calendar tests passed")
