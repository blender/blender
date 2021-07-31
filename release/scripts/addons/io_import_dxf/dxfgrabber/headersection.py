# Purpose: handle header section
# Created: 21.07.2012, taken from my ezdxf project
# Copyright (C) 2012, Manfred Moitzi
# License: MIT License
from __future__ import unicode_literals
__author__ = "mozman <mozman@gmx.at>"

from .tags import TagGroups, DXFTag


class HeaderSection(dict):
    name = "header"

    def __init__(self):
        super(HeaderSection, self).__init__()
        self._create_default_vars()

    @staticmethod
    def from_tags(tags):
        header = HeaderSection()
        if tags[1] == DXFTag(2, 'HEADER'):  # DXF12 without a HEADER section is valid!
            header._build(tags)
        return header

    def _create_default_vars(self):
        self['$ACADVER'] = 'AC1009'
        self['$DWGCODEPAGE'] = 'ANSI_1252'

    def _build(self, tags):
        if len(tags) == 3:  # empty header section!
            return
        groups = TagGroups(tags[2:-1], split_code=9)
        for group in groups:
            self[group[0].value] = group[1].value
