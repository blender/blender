#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2006-2012 assimp team
# SPDX-FileCopyrightText: 2013 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Usage
=====

   fbx2json [FILES]...

This script will write a JSON file for each FBX argument given.


Output
======

The JSON data is formatted into a list of nested lists of 4 items:

   ``[id, [data, ...], "data_types", [subtree, ...]]``

Where each list may be empty, and the items in
the subtree are formatted the same way.

data_types is a string, aligned with data that specifies a type
for each property.

The types are as follows:

* 'Z': - INT8
* 'Y': - INT16
* 'B': - BOOL
* 'C': - CHAR
* 'I': - INT32
* 'F': - FLOAT32
* 'D': - FLOAT64
* 'L': - INT64
* 'R': - BYTES
* 'S': - STRING
* 'f': - FLOAT32_ARRAY
* 'i': - INT32_ARRAY
* 'd': - FLOAT64_ARRAY
* 'l': - INT64_ARRAY
* 'b': - BOOL ARRAY
* 'c': - BYTE ARRAY

Note that key:value pairs aren't used since the id's are not
ensured to be unique.
"""


# ----------------------------------------------------------------------------
# FBX Binary Parser

from struct import unpack
import array
import zlib

# at the end of each nested block, there is a NUL record to indicate
# that the sub-scope exists (i.e. to distinguish between P: and P : {})
_BLOCK_SENTINEL_LENGTH = ...
_BLOCK_SENTINEL_DATA = ...
read_fbx_elem_uint = ...
_IS_BIG_ENDIAN = (__import__("sys").byteorder != 'little')
_HEAD_MAGIC = b'Kaydara FBX Binary\x20\x20\x00\x1a\x00'
from collections import namedtuple
FBXElem = namedtuple("FBXElem", ("id", "props", "props_type", "elems"))
del namedtuple


def read_uint(read):
    return unpack(b'<I', read(4))[0]


def read_uint64(read):
    return unpack(b'<Q', read(8))[0]


def read_ubyte(read):
    return unpack(b'B', read(1))[0]


def read_string_ubyte(read):
    size = read_ubyte(read)
    data = read(size)
    return data


def unpack_array(read, array_type, array_stride, array_byteswap):
    length = read_uint(read)
    encoding = read_uint(read)
    comp_len = read_uint(read)

    data = read(comp_len)

    if encoding == 0:
        pass
    elif encoding == 1:
        data = zlib.decompress(data)

    assert(length * array_stride == len(data))

    data_array = array.array(array_type, data)
    if array_byteswap and _IS_BIG_ENDIAN:
        data_array.byteswap()
    return data_array


read_data_dict = {
    b'Z'[0]: lambda read: unpack(b'<b', read(1))[0],  # 8 bit int
    b'Y'[0]: lambda read: unpack(b'<h', read(2))[0],  # 16 bit int
    b'B'[0]: lambda read: unpack(b'?', read(1))[0],   # 1 bit bool (yes/no)
    b'C'[0]: lambda read: unpack(b'<c', read(1))[0],  # char
    b'I'[0]: lambda read: unpack(b'<i', read(4))[0],  # 32 bit int
    b'F'[0]: lambda read: unpack(b'<f', read(4))[0],  # 32 bit float
    b'D'[0]: lambda read: unpack(b'<d', read(8))[0],  # 64 bit float
    b'L'[0]: lambda read: unpack(b'<q', read(8))[0],  # 64 bit int
    b'R'[0]: lambda read: read(read_uint(read)),      # binary data
    b'S'[0]: lambda read: read(read_uint(read)),      # string data
    b'f'[0]: lambda read: unpack_array(read, 'f', 4, False),  # array (float)
    b'i'[0]: lambda read: unpack_array(read, 'i', 4, True),   # array (int)
    b'd'[0]: lambda read: unpack_array(read, 'd', 8, False),  # array (double)
    b'l'[0]: lambda read: unpack_array(read, 'q', 8, True),   # array (long)
    b'b'[0]: lambda read: unpack_array(read, 'b', 1, False),  # array (bool)
    b'c'[0]: lambda read: unpack_array(read, 'B', 1, False),  # array (ubyte)
}


# FBX 7500 (aka FBX2016) introduces incompatible changes at binary level:
#   * The NULL block marking end of nested stuff switches from 13 bytes long to 25 bytes long.
#   * The FBX element metadata (end_offset, prop_count and prop_length) switch from uint32 to uint64.
def init_version(fbx_version):
    global _BLOCK_SENTINEL_LENGTH, _BLOCK_SENTINEL_DATA, read_fbx_elem_uint

    assert(_BLOCK_SENTINEL_LENGTH == ...)
    assert(_BLOCK_SENTINEL_DATA == ...)

    if fbx_version < 7500:
        _BLOCK_SENTINEL_LENGTH = 13
        read_fbx_elem_uint = read_uint
    else:
        _BLOCK_SENTINEL_LENGTH = 25
        read_fbx_elem_uint = read_uint64
    _BLOCK_SENTINEL_DATA = (b'\0' * _BLOCK_SENTINEL_LENGTH)


def read_elem(read, tell, use_namedtuple):
    # [0] the offset at which this block ends
    # [1] the number of properties in the scope
    # [2] the length of the property list
    end_offset = read_fbx_elem_uint(read)
    if end_offset == 0:
        return None

    prop_count = read_fbx_elem_uint(read)
    prop_length = read_fbx_elem_uint(read)

    elem_id = read_string_ubyte(read)        # elem name of the scope/key
    elem_props_type = bytearray(prop_count)  # elem property types
    elem_props_data = [None] * prop_count    # elem properties (if any)
    elem_subtree = []                        # elem children (if any)

    for i in range(prop_count):
        data_type = read(1)[0]
        elem_props_data[i] = read_data_dict[data_type](read)
        elem_props_type[i] = data_type

    if tell() < end_offset:
        while tell() < (end_offset - _BLOCK_SENTINEL_LENGTH):
            elem_subtree.append(read_elem(read, tell, use_namedtuple))

        if read(_BLOCK_SENTINEL_LENGTH) != _BLOCK_SENTINEL_DATA:
            raise IOError("failed to read nested block sentinel, "
                          "expected all bytes to be 0")

    if tell() != end_offset:
        raise IOError("scope length not reached, something is wrong")

    args = (elem_id, elem_props_data, elem_props_type, elem_subtree)
    return FBXElem(*args) if use_namedtuple else args


def parse_version(fn):
    """
    Return the FBX version,
    if the file isn't a binary FBX return zero.
    """
    with open(fn, 'rb') as f:
        read = f.read

        if read(len(_HEAD_MAGIC)) != _HEAD_MAGIC:
            return 0

        return read_uint(read)


def parse(fn, use_namedtuple=True):
    root_elems = []

    with open(fn, 'rb') as f:
        read = f.read
        tell = f.tell

        if read(len(_HEAD_MAGIC)) != _HEAD_MAGIC:
            raise IOError("Invalid header")

        fbx_version = read_uint(read)
        init_version(fbx_version)

        while True:
            elem = read_elem(read, tell, use_namedtuple)
            if elem is None:
                break
            root_elems.append(elem)

    args = (b'', [], bytearray(0), root_elems)
    return FBXElem(*args) if use_namedtuple else args, fbx_version


# ----------------------------------------------------------------------------
# Inline Modules

# pyfbx.data_types
data_types = type(array)("data_types")
data_types.__dict__.update(
    dict(
        INT8=b'Z'[0],
        INT16=b'Y'[0],
        BOOL=b'B'[0],
        CHAR=b'C'[0],
        INT32=b'I'[0],
        FLOAT32=b'F'[0],
        FLOAT64=b'D'[0],
        INT64=b'L'[0],
        BYTES=b'R'[0],
        STRING=b'S'[0],
        FLOAT32_ARRAY=b'f'[0],
        INT32_ARRAY=b'i'[0],
        FLOAT64_ARRAY=b'd'[0],
        INT64_ARRAY=b'l'[0],
        BOOL_ARRAY=b'b'[0],
        BYTE_ARRAY=b'c'[0],
    ))

# pyfbx.parse_bin
parse_bin = type(array)("parse_bin")
parse_bin.__dict__.update(
    dict(
        parse=parse
    ))


# ----------------------------------------------------------------------------
# JSON Converter
# from pyfbx import parse_bin, data_types
import json
import array


def fbx2json_property_as_string(prop, prop_type):
    if prop_type == data_types.STRING:
        prop_str = prop.decode('utf-8')
        prop_str = prop_str.replace('\x00\x01', '::')
        return json.dumps(prop_str)
    else:
        prop_py_type = type(prop)
        if prop_py_type == bytes:
            return json.dumps(repr(prop)[2:-1])
        elif prop_py_type == bool:
            return json.dumps(prop)
        elif prop_py_type == array.array:
            return repr(list(prop))

    return repr(prop)


def fbx2json_properties_as_string(fbx_elem):
    return ", ".join(fbx2json_property_as_string(*prop_item)
                     for prop_item in zip(fbx_elem.props,
                                          fbx_elem.props_type))


def fbx2json_recurse(fw, fbx_elem, ident, is_last):
    fbx_elem_id = fbx_elem.id.decode('utf-8')
    fw('%s["%s", ' % (ident, fbx_elem_id))
    fw('[%s], ' % fbx2json_properties_as_string(fbx_elem))
    fw('"%s", ' % (fbx_elem.props_type.decode('ascii')))

    fw('[')
    if fbx_elem.elems:
        fw('\n')
        ident_sub = ident + "    "
        for fbx_elem_sub in fbx_elem.elems:
            fbx2json_recurse(fw, fbx_elem_sub, ident_sub,
                             fbx_elem_sub is fbx_elem.elems[-1])
    fw(']')

    fw(']%s' % ('' if is_last else ',\n'))


def fbx2json(fn):
    import os

    fn_json = "%s.json" % os.path.splitext(fn)[0]
    print("Writing: %r " % fn_json, end="")
    fbx_root_elem, fbx_version = parse(fn, use_namedtuple=True)
    print("(Version %d) ..." % fbx_version)

    with open(fn_json, 'w', encoding="ascii", errors='xmlcharrefreplace') as f:
        fw = f.write
        fw('[\n')
        ident_sub = "    "
        for fbx_elem_sub in fbx_root_elem.elems:
            fbx2json_recurse(f.write, fbx_elem_sub, ident_sub,
                             fbx_elem_sub is fbx_root_elem.elems[-1])
        fw(']\n')


# ----------------------------------------------------------------------------
# Command Line

def main():
    import sys

    if "--help" in sys.argv:
        print(__doc__)
        return

    for arg in sys.argv[1:]:
        try:
            fbx2json(arg)
        except:
            print("Failed to convert %r, error:" % arg)

            import traceback
            traceback.print_exc()


if __name__ == "__main__":
    main()
