# Purpose: handle text styles
# Created: 06.01.2014
# Copyright (C) 2014, Manfred Moitzi
# License: MIT License
from __future__ import unicode_literals
__author__ = "mozman <mozman@gmx.at>"

from .layers import Table
from .tags import Tags


class Style(object):
    def __init__(self, tags):
        self.name = ""
        self.height = 1.0
        self.width = 1.0
        self.oblique = 0.
        self.is_backwards = False
        self.is_upside_down = False
        self.font = ""
        self.big_font = ""
        for code, value in tags.plain_tags():
            if code == 2:
                self.name = value
            elif code == 70:
                self.flags = value
            elif code == 40:
                self.height = value
            elif code == 41:
                self.width = value
            elif code == 50:
                self.oblique = value
            elif code == 71:
                self.is_backwards = bool(value & 2)
                self.is_upside_down = bool(value & 4)
                self.oblique = value
            elif code == 3:
                self.font = value
            elif code == 4:
                self.big_font = value


class StyleTable(Table):
    name = 'styles'

    @staticmethod
    def from_tags(tags):
        styles = StyleTable()
        for entry_tags in styles.entry_tags(tags):
            style = Style(entry_tags)
            styles._table_entries[style.name] = style
        return styles


DEFAULT_STYLE = """  0
STYLE
  2
STANDARD
 70
0
 40
0.0
 41
1.0
 50
0.0
 71
0
 42
1.0
  3
Arial
  4

"""

default_text_style = Style(Tags.from_text(DEFAULT_STYLE))
