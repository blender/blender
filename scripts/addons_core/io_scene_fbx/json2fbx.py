#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2014-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Usage
=====

   json2fbx [FILES]...

This script will write a binary FBX file for each JSON argument given.


Input
======

The JSON data is formatted into a list of nested lists of 4 items:

   ``[id, [data, ...], "data_types", [subtree, ...]]``

Where each list may be empty, and the items in
the subtree are formatted the same way.

data_types is a string, aligned with data that spesifies a type
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


def elem_empty(elem, name):
    import encode_bin
    sub_elem = encode_bin.FBXElem(name)
    if elem is not None:
        elem.elems.append(sub_elem)
    return sub_elem


def parse_json_rec(fbx_root, json_node):
    name, data, data_types, children = json_node
    ver = 0

    assert(len(data_types) == len(data))

    e = elem_empty(fbx_root, name.encode())
    for d, dt in zip(data, data_types):
        if dt == "B":
            e.add_bool(d)
        elif dt == "C":
            d = eval('b"""' + d + '"""')
            e.add_char(d)
        elif dt == "Z":
            e.add_int8(d)
        elif dt == "Y":
            e.add_int16(d)
        elif dt == "I":
            e.add_int32(d)
        elif dt == "L":
            e.add_int64(d)
        elif dt == "F":
            e.add_float32(d)
        elif dt == "D":
            e.add_float64(d)
        elif dt == "R":
            d = eval('b"""' + d + '"""')
            e.add_bytes(d)
        elif dt == "S":
            d = d.encode().replace(b"::", b"\x00\x01")
            e.add_string(d)
        elif dt == "i":
            e.add_int32_array(d)
        elif dt == "l":
            e.add_int64_array(d)
        elif dt == "f":
            e.add_float32_array(d)
        elif dt == "d":
            e.add_float64_array(d)
        elif dt == "b":
            e.add_bool_array(d)
        elif dt == "c":
            e.add_byte_array(d)

    if name == "FBXVersion":
        assert(data_types == "I")
        ver = int(data[0])

    for child in children:
        _ver = parse_json_rec(e, child)
        if _ver:
            ver = _ver

    return ver


def parse_json(json_root):
    root = elem_empty(None, b"")
    ver = 0

    for n in json_root:
        _ver = parse_json_rec(root, n)
        if _ver:
            ver = _ver

    return root, ver


def json2fbx(fn):
    import os
    import json

    import encode_bin

    fn_fbx = "%s.fbx" % os.path.splitext(fn)[0]
    print("Writing: %r " % fn_fbx, end="")
    with open(fn) as f_json:
        json_root = json.load(f_json)
    with encode_bin.FBXElem.enable_multithreading_cm():
        fbx_root, fbx_version = parse_json(json_root)
    print("(Version %d) ..." % fbx_version)
    encode_bin.write(fn_fbx, fbx_root, fbx_version)


# ----------------------------------------------------------------------------
# Command Line

def main():
    import sys

    if "--help" in sys.argv:
        print(__doc__)
        return

    for arg in sys.argv[1:]:
        try:
            json2fbx(arg)
        except:
            print("Failed to convert %r, error:" % arg)

            import traceback
            traceback.print_exc()


if __name__ == "__main__":
    main()
