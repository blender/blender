# Purpose: handle default chunk
# Created: 21.07.2012, taken from my ezdxf project
# Copyright (C) 2012, Manfred Moitzi
# License: MIT License
from __future__ import unicode_literals
__author__ = "mozman <mozman@gmx.at>"

from .tags import Tags, DXFTag


class DefaultChunk(object):
    def __init__(self, tags):
        assert isinstance(tags, Tags)
        self.tags = tags

    @staticmethod
    def from_tags(tags, drawing):
        return DefaultChunk(tags)

    @property
    def name(self):
        return self.tags[1].value.lower()


def iterchunks(tagreader, stoptag='EOF', endofchunk='ENDSEC'):
    while True:
        tag = next(tagreader)
        if tag == DXFTag(0, stoptag):
            return

        tags = Tags([tag])
        append = tags.append
        end_tag = DXFTag(0, endofchunk)
        while tag != end_tag:
            tag = next(tagreader)
            append(tag)
        yield tags
