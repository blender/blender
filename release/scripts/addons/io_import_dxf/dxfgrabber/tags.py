# Purpose: tag reader
# Created: 21.07.2012, taken from my ezdxf project
# Copyright (C) 2012, Manfred Moitzi
# License: MIT License
from __future__ import unicode_literals
__author__ = "mozman <mozman@gmx.at>"

import sys
from .codepage import toencoding
from .const import acadrelease
from array import array

from io import StringIO
from collections import namedtuple
from itertools import chain, islice
from . import tostr


DXFTag = namedtuple('DXFTag', 'code value')
NONE_TAG = DXFTag(999999, 'NONE')
APP_DATA_MARKER = 102
SUBCLASS_MARKER = 100
XDATA_MARKER = 1001


class DXFStructureError(Exception):
    pass


def point_tuple(value):
    return tuple(float(f) for f in value)


POINT_CODES = frozenset(chain(range(10, 20), (210, ), range(110, 113), range(1010, 1020)))


def is_point_tag(tag):
    return tag[0] in POINT_CODES


infinite = float('inf')
neg_infinite = float('-inf')


def to_float_with_infinite(value):
    try:
        return float(value)
    except ValueError:
        value = value.lower().strip()
        if value.startswith('inf'):
            return infinite
        if value.startswith('-inf'):
            return neg_infinite
        else:
            raise


class TagCaster:
    def __init__(self):
        self._cast = self._build()

    def _build(self):
        table = {}
        for caster, codes in TYPES:
            for code in codes:
                table[code] = caster
        return table

    def cast(self, tag):
        code, value = tag
        typecaster = self._cast.get(code, tostr)
        try:
            value = typecaster(value)
        except ValueError:
            if typecaster is int:  # convert float to int
                value = int(float(value))
            else:
                raise
        return DXFTag(code, value)

    def cast_value(self, code, value):
        typecaster = self._cast.get(code, tostr)
        try:
            return typecaster(value)
        except ValueError:
            if typecaster is int:  # convert float to int
                return int(float(value))
            else:
                raise

TYPES = [
    (tostr, range(0, 10)),
    (point_tuple, range(10, 20)),
    (to_float_with_infinite, range(20, 60)),
    (int, range(60, 100)),
    (tostr, range(100, 106)),
    (point_tuple, range(110, 113)),
    (to_float_with_infinite, range(113, 150)),
    (int, range(170, 180)),
    (point_tuple, [210]),
    (to_float_with_infinite, range(211, 240)),
    (int, range(270, 290)),
    (int, range(290, 300)),  # bool 1=True 0=False
    (tostr, range(300, 370)),
    (int, range(370, 390)),
    (tostr, range(390, 400)),
    (int, range(400, 410)),
    (tostr, range(410, 420)),
    (int, range(420, 430)),
    (tostr, range(430, 440)),
    (int, range(440, 460)),
    (to_float_with_infinite, range(460, 470)),
    (tostr, range(470, 480)),
    (tostr, range(480, 482)),
    (tostr, range(999, 1010)),
    (point_tuple, range(1010, 1020)),
    (to_float_with_infinite, range(1020, 1060)),
    (int, range(1060, 1072)),
]

_TagCaster = TagCaster()
cast_tag = _TagCaster.cast
cast_tag_value = _TagCaster.cast_value


def stream_tagger(stream, assure_3d_coords=False):
    """ Generates DXFTag() from a stream (untrusted external source). Does not skip comment tags 999.
    """
    class Counter:
        def __init__(self):
            self.counter = 0

    undo_tag = None
    line = Counter()  # writeable line counter for next_tag(), Python 2.7 does not support the nonlocal statement

    def next_tag():
        code = stream.readline()
        value = stream.readline()
        line.counter += 2
        if code and value:  # StringIO(): empty strings indicates EOF
            return DXFTag(int(code.rstrip('\r\n')), value.rstrip('\r\n'))  # without line ending
        else:  # StringIO(): empty lines indicates EOF
            raise EOFError()

    while True:
        try:
            if undo_tag is not None:
                x = undo_tag
                undo_tag = None
            else:
                x = next_tag()
            code = x.code
            if code == 999:  # skip comments
                continue
            if code in POINT_CODES:
                y = next_tag()  # y coordinate is mandatory
                if y.code != code + 10:
                    raise DXFStructureError("Missing required y coordinate near line: {}.".format(line.counter))
                z = next_tag()  # z coordinate just for 3d points
                try:
                    if z.code == code + 20:
                        point = (float(x.value), float(y.value), float(z.value))
                    else:
                        if assure_3d_coords:
                            point = (float(x.value), float(y.value), 0.)
                        else:
                            point = (float(x.value), float(y.value))
                        undo_tag = z
                except ValueError:
                    raise DXFStructureError('Invalid floating point values near line: {}.'.format(line.counter))
                yield DXFTag(code, point)
            else:  # just a single tag
                try:
                    yield cast_tag(x)
                except ValueError:
                    raise DXFStructureError('Invalid tag (code={code}, value="{value}") near line: {line}.'.format(
                        line=line.counter,
                        code=x.code,
                        value=x.value,
                    ))
        except EOFError:
            return


def string_tagger(s):
    return stream_tagger(StringIO(s))


class Tags(list):
    """ DXFTag() chunk as flat list. """
    def find_all(self, code):
        """ Returns a list of DXFTag(code, ...). """
        return [tag for tag in self if tag.code == code]

    def tag_index(self, code, start=0, end=None):
        """Return first index of DXFTag(code, value).
        """
        if end is None:
            end = len(self)
        index = start
        while index < end:
            if self[index].code == code:
                return index
            index += 1
        raise ValueError(code)

    def get_value(self, code):
        for tag in self:
            if tag.code == code:
                return tag.value
        raise ValueError(code)

    @staticmethod
    def from_text(text):
        return Tags(string_tagger(text))

    def get_type(self):
        return self.__getitem__(0).value

    def plain_tags(self):  # yield no app data and no xdata
        is_app_data = False
        for tag in self:
            if tag.code >= 1000:  # skip xdata
                continue
            if tag.code == APP_DATA_MARKER:
                if is_app_data:  # tag.value == '}'
                    is_app_data = False  # end of app data
                else:  # value == '{APPID'
                    is_app_data = True  # start of app data
                continue
            if not is_app_data:
                yield tag

    def xdata(self):
        index = 0
        end = len(self)
        while index < end:
            if self[index].code > 999:  # all xdata tag codes are >= 1000
                return self[index:]  # xdata is always at the end of the DXF entity
            index += 1
        return []

    def app_data(self):
        app_data = {}
        app_tags = None
        for tag in self:
            if tag.code == APP_DATA_MARKER:
                if tag.value == '}':  # end of app data
                    app_tags.append(tag)
                    app_data[app_tags[0].value] = app_tags
                    app_tags = None
                else:
                    app_tags = [tag]
            else:
                if app_tags is not None:  # collection app data
                    app_tags.append(tag)
        return app_data

    def subclasses(self):
        classes = {}
        name = 'noname'
        tags = []
        for tag in self.plain_tags():
            if tag.code == SUBCLASS_MARKER:
                classes[name] = tags
                tags = []
                name = tag.value
            else:
                tags.append(tag)
        classes[name] = tags
        return classes

    def get_subclass(self, name):
        classes = self.subclasses()
        return classes.get(name, 'noname')


class TagGroups(list):
    """
    Group of tags starting with a SplitTag and ending before the next SplitTag.

    A SplitTag is a tag with code == splitcode, like (0, 'SECTION') for splitcode=0.

    """
    def __init__(self, tags, split_code=0):
        super(TagGroups, self).__init__()
        self._build_groups(tags, split_code)

    def _build_groups(self, tags, splitcode):
        def append(tag):  # first do nothing, skip tags in front of the first split tag
            pass
        group = None
        for tag in tags:  # has to work with iterators/generators
            if tag.code == splitcode:
                if group is not None:
                    self.append(group)
                group = Tags([tag])
                append = group.append  # redefine append: add tags to this group
            else:
                append(tag)
        if group is not None:
            self.append(group)

    def get_name(self, index):
        return self[index][0].value

    @staticmethod
    def from_text(text, split_code=0):
        return TagGroups(Tags.from_text(text), split_code)


class ClassifiedTags:
    """ Manage Subclasses, AppData and Extended Data """

    def __init__(self, iterable=None):
        self.appdata = list()  # code == 102, keys are "{<arbitrary name>", values are Tags()
        self.subclasses = list()  # code == 100, keys are "subclassname", values are Tags()
        self.xdata = list()  # code >= 1000, keys are "APPNAME", values are Tags()
        if iterable is not None:
            self._setup(iterable)

    @property
    def noclass(self):
        return self.subclasses[0]

    def _setup(self, iterable):
        tagstream = iter(iterable)

        def collect_subclass(start_tag):
            """ a subclass can contain appdata, but not xdata, ends with
            SUBCLASSMARKER or XDATACODE.
            """
            data = Tags() if start_tag is None else Tags([start_tag])
            try:
                while True:
                    tag = next(tagstream)
                    if tag.code == APP_DATA_MARKER and tag.value[0] == '{':
                        app_data_pos = len(self.appdata)
                        data.append(DXFTag(tag.code, app_data_pos))
                        collect_appdata(tag)
                    elif tag.code in (SUBCLASS_MARKER, XDATA_MARKER):
                        self.subclasses.append(data)
                        return tag
                    else:
                        data.append(tag)
            except StopIteration:
                pass
            self.subclasses.append(data)
            return NONE_TAG

        def collect_appdata(starttag):
            """ appdata, can not contain xdata or subclasses """
            data = Tags([starttag])
            while True:
                try:
                    tag = next(tagstream)
                except StopIteration:
                    raise DXFStructureError("Missing closing DXFTag(102, '}') for appdata structure.")
                data.append(tag)
                if tag.code == APP_DATA_MARKER:
                    break
            self.appdata.append(data)

        def collect_xdata(starttag):
            """ xdata are always at the end of the entity and can not contain
            appdata or subclasses
            """
            data = Tags([starttag])
            try:
                while True:
                    tag = next(tagstream)
                    if tag.code == XDATA_MARKER:
                        self.xdata.append(data)
                        return tag
                    else:
                        data.append(tag)
            except StopIteration:
                pass
            self.xdata.append(data)
            return NONE_TAG

        tag = collect_subclass(None)  # preceding tags without a subclass
        while tag.code == SUBCLASS_MARKER:
            tag = collect_subclass(tag)
        while tag.code == XDATA_MARKER:
            tag = collect_xdata(tag)

        if tag is not NONE_TAG:
            raise DXFStructureError("Unexpected tag '%r' at end of entity." % tag)

    def __iter__(self):
        for subclass in self.subclasses:
            for tag in subclass:
                if tag.code == APP_DATA_MARKER and isinstance(tag.value, int):
                    for subtag in self.appdata[tag.value]:
                        yield subtag
                else:
                    yield tag

        for xdata in self.xdata:
            for tag in xdata:
                yield tag

    def get_subclass(self, name):
        for subclass in self.subclasses:
            if len(subclass) and subclass[0].value == name:
                return subclass
        raise KeyError("Subclass '%s' does not exist." % name)

    def get_xdata(self, appid):
        for xdata in self.xdata:
            if xdata[0].value == appid:
                return xdata
        raise ValueError("No extended data for APPID '%s'" % appid)

    def get_appdata(self, name):
        for appdata in self.appdata:
            if appdata[0].value == name:
                return appdata
        raise ValueError("Application defined group '%s' does not exist." % name)

    def get_type(self):
        return self.noclass[0].value

    @staticmethod
    def from_text(text):
        return ClassifiedTags(string_tagger(text))


class DXFInfo(object):
    def __init__(self):
        self.release = 'R12'
        self.version = 'AC1009'
        self.encoding = 'cp1252'
        self.handseed = '0'

    def DWGCODEPAGE(self, value):
        self.encoding = toencoding(value)

    def ACADVER(self, value):
        self.version = value
        self.release = acadrelease.get(value, 'R12')

    def HANDSEED(self, value):
        self.handseed = value


def dxfinfo(stream):
    info = DXFInfo()
    tag = DXFTag(999999, '')
    tagreader = stream_tagger(stream)
    while tag != DXFTag(0, 'ENDSEC'):
        tag = next(tagreader)
        if tag.code != 9:
            continue
        name = tag.value[1:]
        method = getattr(info, name, None)
        if method is not None:
            method(next(tagreader).value)
    return info


def binary_encoded_data_to_bytes(data):
    PY3 = sys.version_info[0] >= 3
    byte_array = array('B' if PY3 else b'B')
    for text in data:
        byte_array.extend(int(text[index:index+2], 16) for index in range(0, len(text), 2))
    return byte_array.tobytes() if PY3 else byte_array.tostring()
