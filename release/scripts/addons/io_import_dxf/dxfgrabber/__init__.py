# dxfgrabber - copyright (C) 2012 by Manfred Moitzi (mozman)
# Purpose: grab information from DXF drawings - all DXF versions supported
# Created: 21.07.2012
# License: MIT License

version = (0, 8, 4)
VERSION = "%d.%d.%d" % version

__author__ = "mozman <mozman@gmx.at>"
__doc__ = """A Python library to grab information from DXF drawings - all DXF versions supported."""


# Python27/3x support should be done here
import sys

PYTHON3 = sys.version_info.major > 2

if PYTHON3:
    tostr = str
else:  # PYTHON27
    tostr = unicode

# end of Python 2/3 adaption
# if tostr does not work, look at package 'dxfwrite' for escaping unicode chars

from .const import BYBLOCK, BYLAYER

import io
from .tags import dxfinfo
from .color import aci_to_true_color


def read(stream, options=None):
    if hasattr(stream, 'readline'):
        from .drawing import Drawing
        return Drawing(stream, options)
    else:
        raise AttributeError('stream object requires a readline() method.')


def readfile(filename, options=None):
    try:  # is it ascii code-page encoded?
        return readfile_as_asc(filename, options)
    except UnicodeDecodeError:  # try unicode and ignore errors
        return readfile_as_utf8(filename, options, errors='ignore')


def readfile_as_utf8(filename, options=None, errors='strict'):
    return _read_encoded_file(filename, options, encoding='utf-8', errors=errors)


def readfile_as_asc(filename, options=None):
    def get_encoding():
        with io.open(filename) as fp:
            info = dxfinfo(fp)
        return info.encoding

    return _read_encoded_file(filename, options, encoding=get_encoding())


def _read_encoded_file(filename, options=None, encoding='utf-8', errors='strict'):
    from .drawing import Drawing

    with io.open(filename, encoding=encoding, errors=errors) as fp:
        dwg = Drawing(fp, options)
    dwg.filename = filename
    return dwg
