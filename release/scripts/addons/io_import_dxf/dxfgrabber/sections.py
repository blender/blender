# Purpose: handle dxf sections
# Created: 21.07.2012, taken from my ezdxf project
# Copyright (C) 2012, Manfred Moitzi
# License: MIT License
from __future__ import unicode_literals
__author__ = "mozman <mozman@gmx.at>"

from .codepage import toencoding
from .defaultchunk import DefaultChunk, iterchunks
from .headersection import HeaderSection
from .tablessection import TablesSection
from .entitysection import EntitySection, ObjectsSection
from .blockssection import BlocksSection
from .acdsdata import AcDsDataSection


class Sections(object):
    def __init__(self, tagreader, drawing):
        self._sections = {}
        self._create_default_sections()
        self._setup_sections(tagreader, drawing)

    def __contains__(self, name):
        return name in self._sections

    def _create_default_sections(self):
        self._sections['header'] = HeaderSection()
        for cls in SECTIONMAP.values():
            section = cls()
            self._sections[section.name] = section

    def _setup_sections(self, tagreader, drawing):
        def name(section):
            return section[1].value

        for section in iterchunks(tagreader, stoptag='EOF', endofchunk='ENDSEC'):
            if name(section) == 'HEADER':
                new_section = HeaderSection.from_tags(section)
                drawing.dxfversion = new_section.get('$ACADVER', 'AC1009')
                codepage = new_section.get('$DWGCODEPAGE', 'ANSI_1252')
                drawing.encoding = toencoding(codepage)
            else:
                section_name = name(section)
                if section_name in SECTIONMAP:
                    section_class = get_section_class(section_name)
                    new_section = section_class.from_tags(section, drawing)
                else:
                    new_section = None
            if new_section is not None:
                self._sections[new_section.name] = new_section

    def __getattr__(self, key):
        try:
            return self._sections[key]
        except KeyError:
            raise AttributeError(key)

SECTIONMAP = {
    'TABLES': TablesSection,
    'ENTITIES': EntitySection,
    'OBJECTS': ObjectsSection,
    'BLOCKS': BlocksSection,
    'ACDSDATA': AcDsDataSection,
}


def get_section_class(name):
    return SECTIONMAP.get(name, DefaultChunk)
