# Purpose: handle linetypes table
# Created: 06.01.2014
# Copyright (C) 2014, Manfred Moitzi
# License: MIT License

__author__ = "mozman <mozman@gmx.at>"

from .layers import Table


class Linetype(object):
    def __init__(self, tags):
        self.name = ""
        self.description = ""
        self.length = 0  # overall length of the pattern
        self.pattern = []  # list of floats: value>0: line, value<0: gap, value=0: dot
        for code, value in tags.plain_tags():
            if code == 2:
                self.name = value
            elif code == 3:
                self.description = value
            elif code == 40:
                self.length = value
            elif code == 49:
                self.pattern.append(value)


class LinetypeTable(Table):
    name = 'linetypes'

    @staticmethod
    def from_tags(tags):
        styles = LinetypeTable()
        for entry_tags in styles.entry_tags(tags):
            style = Linetype(entry_tags)
            styles._table_entries[style.name] = style
        return styles

