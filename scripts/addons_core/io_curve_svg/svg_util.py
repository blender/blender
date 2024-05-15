# SPDX-FileCopyrightText: 2010-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import re


units = {"": 1.0,
         "px": 1.0,
         "in": 90.0,
         "mm": 90.0 / 25.4,
         "cm": 90.0 / 2.54,
         "pt": 1.25,
         "pc": 15.0,
         "em": 1.0,
         "ex": 1.0,
         "INVALID": 1.0,  # some DocBook files contain this
         }


def srgb_to_linearrgb(c):
    if c < 0.04045:
        return 0.0 if c < 0.0 else c * (1.0 / 12.92)
    else:
        return pow((c + 0.055) * (1.0 / 1.055), 2.4)


def check_points_equal(point_a, point_b):
    return (abs(point_a[0] - point_b[0]) < 1e-6 and
            abs(point_a[1] - point_b[1]) < 1e-6)


match_number = r"-?\d+(\.\d+)?([eE][-+]?\d+)?"
match_first_comma = r"^\s*(?=,)"
match_comma_pair = r",\s*(?=,)"
match_last_comma = r",\s*$"

match_number_optional_parts = r"(-?\d+(\.\d*)?([eE][-+]?\d+)?)|(-?\.\d+([eE][-+]?\d+)?)"
re_match_number_optional_parts = re.compile(match_number_optional_parts)

array_of_floats_pattern = f"({match_number_optional_parts})|{match_first_comma}|{match_comma_pair}|{match_last_comma}"
re_array_of_floats_pattern = re.compile(array_of_floats_pattern)


def parse_array_of_floats(text):
    """
    Accepts comma or space separated list of floats (without units) and returns an array
    of floating point values.
    """
    elements = re_array_of_floats_pattern.findall(text)
    return [value_to_float(v[0]) for v in elements]


def read_float(text: str, start_index: int = 0):
    """
    Reads floating point value from a string. Parsing starts at the given index.

    Returns the value itself (as a string) and index of first character after the value.
    """

    n = len(text)

    # Skip leading whitespace characters and characters which we consider ignorable for float
    # (like values separator).
    while start_index < n and (text[start_index].isspace() or text[start_index] == ','):
        start_index += 1
    if start_index == n:
        return "0", start_index

    text_part = text[start_index:]
    match = re_match_number_optional_parts.match(text_part)

    if match is None:
        raise Exception('Invalid float value near ' + text[start_index:start_index + 10])

    token = match.group(0)
    endptr = start_index + match.end(0)

    return token, endptr


def parse_coord(coord, size):
    """
    Parse coordinate component to common basis

    Needed to handle coordinates set in cm, mm, inches.
    """

    token, last_char = read_float(coord)
    val = float(token)
    unit = coord[last_char:].strip()  # strip() in case there is a space

    if unit == '%':
        return float(size) / 100.0 * val
    else:
        return val * units[unit]

    return val


def value_to_float(value_encoded: str):
    """
    A simple wrapper around float() which supports empty strings (which are converted to 0).
    """
    if len(value_encoded) == 0:
        return 0
    return float(value_encoded)
