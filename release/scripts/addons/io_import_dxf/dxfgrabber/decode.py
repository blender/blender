# Purpose: decode DXF proprietary data
# Created: 01.05.2014
# Copyright (C) 2014, Manfred Moitzi
# License: MIT License
from __future__ import unicode_literals
__author__ = "mozman <mozman@gmx.at>"

from . import PYTHON3

_replacement_table = {
    0x20: ' ',
    0x40: '_',
    0x5F: '@',
}
for c in range(0x41, 0x5F):
    _replacement_table[c] = chr(0x41 + (0x5E - c))  # 0x5E -> 'A', 0x5D->'B', ...


def decode(text_lines):
    def _decode(text):
        s = []
        skip = False
        if PYTHON3:
            text = bytes(text, 'ascii')
        else:
            text = map(ord, text)

        for c in text:
            if skip:
                skip = False
                continue
            if c in _replacement_table:
                s += _replacement_table[c]
                skip = (c == 0x5E)  # skip space after 'A'
            else:
                s += chr(c ^ 0x5F)
        return ''.join(s)
    return [_decode(line) for line in text_lines]
