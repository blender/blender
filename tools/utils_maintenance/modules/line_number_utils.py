# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
When writing text checking utilities, it's not always straightforward
to find line numbers and ranges from an offset within the text.

This module provides helpers to efficiently do this.

The main utility is ``finditer_with_line_numbers_and_bounds``,
an alternative to ``re.finditer`` which yields line numbers and offsets
for the line bounds - useful for scanning files and reporting errors that include the line contents.
"""

__all__ = (
    "finditer_newline_cache_compute",
    "finditer_with_line_numbers_and_bounds",
    "line_to_offset_range",
)

from collections.abc import (
    Iterator,
)

import re as _re


def finditer_newline_cache_compute(text: str) -> tuple[dict[int, int], list[int]]:
    """
    Return a tuple containing:
    Offset to
    """
    # Offset to line lookup.
    offset_to_line_cache: dict[int, int] = {}
    # Line to offset lookup.
    line_to_offset_cache: list[int] = [0]

    for i, m in enumerate(_re.finditer("\\n", text), 1):
        ofs = m.start()
        offset_to_line_cache[ofs] = i
        line_to_offset_cache.append(ofs)

    return offset_to_line_cache, line_to_offset_cache


def finditer_with_line_numbers_and_bounds(
        pattern: str,
        text: str,
        *,
        offset_to_line_cache: dict[int, int] | None = None,
        flags: int = 0,
) -> Iterator[tuple[_re.Match[str], int, tuple[int, int]]]:
    """
    A version of ``re.finditer`` that returns ``(match, line_number, line_bounds)``.

    Note that ``offset_to_line_cache`` is the first return value from
    ``finditer_newline_cache_compute``.
    This should be passed in if the iterator is called multiple times
    on the same buffer, to avoid calculating this every time.
    """
    if offset_to_line_cache is None:
        offset_to_line_cache, line_to_offset_cache = finditer_newline_cache_compute(text)
        del line_to_offset_cache

    text_len = len(text)
    for m in _re.finditer(pattern, text, flags):

        if (beg := text.rfind("\n", 0, m.start())) == -1:
            beg = 0
            line_number = 0
        else:
            line_number = offset_to_line_cache[beg]

        if (end := text.find("\n", m.end(), text_len)) == -1:
            end = text_len

        yield m, line_number, (beg, end)


def line_to_offset_range(line: int, offset_limit: int, line_to_offset_cache: list[int]) -> tuple[int, int]:
    """
    Given an offset, return line bounds.
    """
    assert line >= 0
    beg = line_to_offset_cache[line]
    end = line_to_offset_cache[line + 1] if (line + 1 < len(line_to_offset_cache)) else offset_limit
    return beg, end
