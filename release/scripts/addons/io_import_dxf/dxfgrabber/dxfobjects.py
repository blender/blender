# encoding: utf-8
# Purpose: objects classes
# Created: 17.04.2016
# Copyright (C) 2016, Manfred Moitzi
# License: MIT License
from __future__ import unicode_literals
__author__ = "mozman <mozman@gmx.at>"

from datetime import datetime
from .juliandate import calendar_date


class DXFObject(object):
    def __init__(self):
        self.dxftype = 'ENTITY'
        self.handle = None
        self.owner = None

    def setup_attributes(self, tags):
        self.dxftype = tags.get_type()
        for code, value in tags.plain_tags():
            if code == 5:   # special case 105 handled by STYLE TABLE
                self.handle = value
            elif code == 330:
                self.owner = value
            else:
                yield code, value  # chain of generators


def unpack_seconds(seconds):
    seconds = int(seconds / 1000)  # remove 1/1000 part
    hours = int(seconds / 3600)
    seconds = int(seconds % 3600)
    minutes = int(seconds / 60)
    seconds = int(seconds % 60)
    return hours, minutes, seconds


class Sun(DXFObject):
    def __init__(self):
        super(Sun, self).__init__()
        self.version = 1
        self.status = False
        self.sun_color = None
        self.intensity = 0.
        self.shadows = False
        self.date = datetime.now()
        self.daylight_savings_time = False
        self.shadow_type = 0
        self.shadow_map_size = 0
        self.shadow_softness = 0.

    def setup_attributes(self, tags):
        date = 0
        time = 0
        for code, value in super(Sun, self).setup_attributes(tags):
            if code == 40:
                self.intensity = value
            elif code == 63:
                self.sun_color = value
            elif code == 70:
                self.shadow_type = value
            elif code == 71:
                self.shadow_map_size = value
            elif code == 90:
                self.version = value
            elif code == 91:
                date = value
            elif code == 92:
                time = value
            elif code == 280:
                self.shadow_softness = value
            elif code == 290:
                self.status = bool(value)
            elif code == 291:
                self.shadows = bool(value)
            elif code == 292:
                self.daylight_savings_time = bool(value)
            else:
                yield code, value  # chain of generators

        if date > 0:
            date = calendar_date(date)
        else:
            date = datetime.now()
        hours, minutes, seconds = unpack_seconds(time)
        self.date = datetime(date.year, date.month, date.day, hours, minutes, seconds)

ObjectsTable = {
    'SUN': Sun,
}


def objects_factory(tags):
    dxftype = tags.get_type()
    cls = ObjectsTable.get(dxftype, DXFObject)  # get object class
    entity = cls()  # call constructor
    list(entity.setup_attributes(tags))  # setup dxf attributes - chain of generators
    return entity
