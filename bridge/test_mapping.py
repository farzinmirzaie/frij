#!/usr/bin/env python3
"""Offline test for the Keep -> device JSON mapping. No network/credentials.

Run:  python3 bridge/test_mapping.py
"""
from keep_to_frij import MAX_ITEMS, TEXT_MAX, to_todo_json


def test_basic_mapping():
    items = [("Milk", False), ("Eggs", True)]
    assert to_todo_json(items) == [{"t": "Milk", "d": False}, {"t": "Eggs", "d": True}]


def test_trims_blank_and_long():
    items = [("  ", False), ("   Bread  ", False), ("x" * 60, True)]
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


if __name__ == "__main__":
    test_basic_mapping()
    test_trims_blank_and_long()
    test_strips_emoji()
    test_caps_to_device_limit()
    print("all mapping tests passed")
