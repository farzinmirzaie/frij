#!/usr/bin/env python3
"""Offline test for the Keep -> device JSON mapping. No network/credentials.

Run:  python3 bridge/test_mapping.py
"""
import os

from keep_to_frij import MAX_ITEMS, TEXT_MAX, parse_notes, to_todo_json


def test_basic_mapping():
    items = [("Milk", False), ("Eggs", True)]
    assert to_todo_json(items) == [{"t": "Milk", "d": False}, {"t": "Eggs", "d": True}]


def test_trims_blank_and_long():
    items = [("  ", False), ("   Bread  ", False), ("x" * 80, True)]
    out = to_todo_json(items)
    assert out == [{"t": "Bread", "d": False}, {"t": "x" * TEXT_MAX, "d": True}]


def test_strips_emoji():
    items = [("Service the car 🚗", False), ("Clean fridge ❄️", True),
             ("Buy comforter 🛏️", False)]
    out = to_todo_json(items)
    assert out == [{"t": "Service the car", "d": False},
                   {"t": "Clean fridge", "d": True},
                   {"t": "Buy comforter", "d": False}]


def test_caps_to_device_limit():
    items = [(f"item {i}", False) for i in range(MAX_ITEMS + 5)]
    assert len(to_todo_json(items)) == MAX_ITEMS


def test_parse_notes():
    # GKEEP_NOTE_* only, sorted by env-key; value = storeKey,noteTitle (title may
    # contain commas — split on the first comma only); blank/malformed skipped.
    saved = dict(os.environ)
    try:
        for k in list(os.environ):
            if k.startswith("GKEEP_NOTE_"):
                del os.environ[k]
        os.environ["GKEEP_NOTE_TODO"] = "todo,Todos"
        os.environ["GKEEP_NOTE_SHOP"] = "groceries,Groceries, weekly"  # comma in title
        os.environ["GKEEP_NOTE_BAD"] = "noTitle"                       # skipped
        notes = parse_notes()
        assert notes == [
            {"key": "groceries", "title": "Groceries, weekly"},  # SHOP < TODO
            {"key": "todo", "title": "Todos"},
        ], notes
    finally:
        os.environ.clear()
        os.environ.update(saved)


if __name__ == "__main__":
    test_basic_mapping()
    test_trims_blank_and_long()
    test_strips_emoji()
    test_caps_to_device_limit()
    test_parse_notes()
    print("all mapping tests passed")
