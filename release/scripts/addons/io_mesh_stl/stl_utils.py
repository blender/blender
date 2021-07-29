# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

"""
Import and export STL files

Used as a blender script, it load all the stl files in the scene:

blender --python stl_utils.py -- file1.stl file2.stl file3.stl ...
"""

import os
import struct
import contextlib
import itertools
from mathutils.geometry import normal

# TODO: endien

class ListDict(dict):
    """
    Set struct with order.

    You can:
       - insert data into without doubles
       - get the list of data in insertion order with self.list

    Like collections.OrderedDict, but quicker, can be replaced if
    ODict is optimised.
    """

    def __init__(self):
        dict.__init__(self)
        self.list = []
        self._len = 0

    def add(self, item):
        """
        Add a value to the Set, return its position in it.
        """
        value = self.setdefault(item, self._len)
        if value == self._len:
            self.list.append(item)
            self._len += 1

        return value


# an stl binary file is
# - 80 bytes of description
# - 4 bytes of size (unsigned int)
# - size triangles :
#
#   - 12 bytes of normal
#   - 9 * 4 bytes of coordinate (3*3 floats)
#   - 2 bytes of garbage (usually 0)
BINARY_HEADER = 80
BINARY_STRIDE = 12 * 4 + 2


def _header_version():
    import bpy
    return "Exported from Blender-" + bpy.app.version_string


def _is_ascii_file(data):
    """
    This function returns True if the data represents an ASCII file.

    Please note that a False value does not necessary means that the data
    represents a binary file. It can be a (very *RARE* in real life, but
    can easily be forged) ascii file.
    """
    # Skip header...
    data.seek(BINARY_HEADER)
    size = struct.unpack('<I', data.read(4))[0]
    # Use seek() method to get size of the file.
    data.seek(0, os.SEEK_END)
    file_size = data.tell()
    # Reset to the start of the file.
    data.seek(0)

    if size == 0:  # Odds to get that result from an ASCII file are null...
        print("WARNING! Reported size (facet number) is 0, assuming invalid binary STL file.")
        return False  # Assume binary in this case.

    return (file_size != BINARY_HEADER + 4 + BINARY_STRIDE * size)


def _binary_read(data):
    # Skip header...
    data.seek(BINARY_HEADER)
    size = struct.unpack('<I', data.read(4))[0]

    if size == 0:
        # Workaround invalid crap.
        data.seek(0, os.SEEK_END)
        file_size = data.tell()
        # Reset to after-the-size in the file.
        data.seek(BINARY_HEADER + 4)

        file_size -= BINARY_HEADER + 4
        size = file_size // BINARY_STRIDE
        print("WARNING! Reported size (facet number) is 0, inferring %d facets from file size." % size)

    # We read 4096 elements at once, avoids too much calls to read()!
    CHUNK_LEN = 4096
    chunks = [CHUNK_LEN] * (size // CHUNK_LEN)
    chunks.append(size % CHUNK_LEN)

    unpack = struct.Struct('<12f').unpack_from
    for chunk_len in chunks:
        if chunk_len == 0:
            continue
        buf = data.read(BINARY_STRIDE * chunk_len)
        for i in range(chunk_len):
            # read the normal and points coordinates of each triangle
            pt = unpack(buf, BINARY_STRIDE * i)
            yield pt[:3], (pt[3:6], pt[6:9], pt[9:])


def _ascii_read(data):
    # an stl ascii file is like
    # HEADER: solid some name
    # for each face:
    #
    #     facet normal x y z
    #     outerloop
    #          vertex x y z
    #          vertex x y z
    #          vertex x y z
    #     endloop
    #     endfacet

    # strip header
    data.readline()

    curr_nor = None

    for l in data:
        l = l.lstrip()
        if l.startswith(b'facet'):
            curr_nor = tuple(map(float, l.split()[2:]))
        # if we encounter a vertex, read next 2
        if l.startswith(b'vertex'):
            yield curr_nor, [tuple(map(float, l_item.split()[1:])) for l_item in (l, data.readline(), data.readline())]


def _binary_write(filepath, faces):
    with open(filepath, 'wb') as data:
        fw = data.write
        # header
        # we write padding at header beginning to avoid to
        # call len(list(faces)) which may be expensive
        fw(struct.calcsize('<80sI') * b'\0')

        # 3 vertex == 9f
        pack = struct.Struct('<9f').pack

        # number of vertices written
        nb = 0

        for face in faces:
            # calculate face normal
            # write normal + vertexes + pad as attributes
            fw(struct.pack('<3f', *normal(*face)) + pack(*itertools.chain.from_iterable(face)))
            # attribute byte count (unused)
            fw(b'\0\0')
            nb += 1

        # header, with correct value now
        data.seek(0)
        fw(struct.pack('<80sI', _header_version().encode('ascii'), nb))


def _ascii_write(filepath, faces):
    with open(filepath, 'w') as data:
        fw = data.write
        header = _header_version()
        fw('solid %s\n' % header)

        for face in faces:
            # calculate face normal
            fw('facet normal %f %f %f\nouter loop\n' % normal(*face)[:])
            for vert in face:
                fw('vertex %f %f %f\n' % vert[:])
            fw('endloop\nendfacet\n')

        fw('endsolid %s\n' % header)


def write_stl(filepath="",
              faces=(),
              ascii=False,
              ):
    """
    Write a stl file from faces,

    filepath
       output filepath

    faces
       iterable of tuple of 3 vertex, vertex is tuple of 3 coordinates as float

    ascii
       save the file in ascii format (very huge)
    """
    (_ascii_write if ascii else _binary_write)(filepath, faces)


def read_stl(filepath):
    """
    Return the triangles and points of an stl binary file.

    Please note that this process can take lot of time if the file is
    huge (~1m30 for a 1 Go stl file on an quad core i7).

    - returns a tuple(triangles, triangles' normals, points).

      triangles
          A list of triangles, each triangle as a tuple of 3 index of
          point in *points*.

      triangles' normals
          A list of vectors3 (tuples, xyz).

      points
          An indexed list of points, each point is a tuple of 3 float
          (xyz).

    Example of use:

       >>> tris, tri_nors, pts = read_stl(filepath)
       >>> pts = list(pts)
       >>>
       >>> # print the coordinate of the triangle n
       >>> print(pts[i] for i in tris[n])
    """
    import time
    start_time = time.process_time()

    tris, tri_nors, pts = [], [], ListDict()

    with open(filepath, 'rb') as data:
        # check for ascii or binary
        gen = _ascii_read if _is_ascii_file(data) else _binary_read

        for nor, pt in gen(data):
            # Add the triangle and the point.
            # If the point is allready in the list of points, the
            # index returned by pts.add() will be the one from the
            # first equal point inserted.
            tris.append([pts.add(p) for p in pt])
            tri_nors.append(nor)

    print('Import finished in %.4f sec.' % (time.process_time() - start_time))

    return tris, tri_nors, pts.list


if __name__ == '__main__':
    import sys
    import bpy
    from io_mesh_stl import blender_utils

    filepaths = sys.argv[sys.argv.index('--') + 1:]

    for filepath in filepaths:
        objName = bpy.path.display_name(filepath)
        tris, pts = read_stl(filepath)

        blender_utils.create_and_link_mesh(objName, tris, pts)
