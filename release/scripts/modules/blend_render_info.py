#!/usr/bin/python

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

# This module can get render info without running from inside blender.
#
# This struct wont change according to Ton.
# Note that the size differs on 32/64bit
#
# typedef struct BHead {
#     int code, len;
#     void *old;
#     int SDNAnr, nr;
# } BHead;

def read_blend_rend_chunk(path):

    import struct

    file = open(path, 'rb')
    
    head = file.read(7)

    if head[0:2] == b'\x1f\x8b': # gzip magic
        import gzip
        file.close()
        file = gzip.open(path, 'rb')
        head = file.read(7)

    if head != b'BLENDER':
        print("not a blend file:", path)
        file.close()
        return []

    is_64_bit = (file.read(1) == b'-')

    # true for PPC, false for X86
    is_big_endian = (file.read(1) == b'V')

    # Now read the bhead chunk!!!
    file.read(3) # skip the version

    scenes = []
    
    sizeof_bhead = 24 if is_64_bit else 20

    while file.read(4) == b'REND':
        sizeof_bhead_left = sizeof_bhead - 4
        
        rend_length = struct.unpack('>i' if is_big_endian else '<i', file.read(4))[0]
        sizeof_bhead_left -= 4

        # We dont care about the rest of the bhead struct
        file.read(sizeof_bhead_left)
        
        # Now we want the scene name, start and end frame. this is 32bites long
        start_frame, end_frame = struct.unpack('>2i' if is_big_endian else '<2i', file.read(8))

        scene_name = file.read(24)

        scene_name = scene_name[:scene_name.index(b'\0')]

        try:
            scene_name = str(scene_name, 'utf8')
        except TypeError:
            pass

        scenes.append((start_frame, end_frame, scene_name))

    return scenes


def main():
    import sys
    for arg in sys.argv[1:]:
        if arg.lower().endswith('.blend'):
            for value in read_blend_rend_chunk(arg):
                print("%d %d %s" % value)

if __name__ == '__main__':
    main()
