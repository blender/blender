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

"""
Thumbnailer runs with python 2.6 and 3.x.
To run automatically with nautilus:
   gconftool --type boolean --set /desktop/gnome/thumbnailers/application@x-blender/enable true
   gconftool --type string --set /desktop/gnome/thumbnailers/application@x-blender/command "blender-thumbnailer.py %i %o"
"""

import os
import struct
import sys

def blend_extract_thumb(path):
    # def MAKE_ID(tag): ord(tag[0])<<24 | ord(tag[1])<<16 | ord(tag[2])<<8 | ord(tag[3])
    REND = 1145980242 # MAKE_ID(b'REND')
    TEST = 1414743380 # MAKE_ID(b'TEST')

    blendfile = open(path, 'rb')

    head = blendfile.read(7)

    if head[0:2] == b'\x1f\x8b': # gzip magic
        import gzip
        blendfile.close()
        blendfile = gzip.open(path, 'rb')
        head = blendfile.read(7)

    if head != b'BLENDER':
        blendfile.close()
        return None, 0, 0

    is_64_bit = (blendfile.read(1) == b'-')

    # true for PPC, false for X86
    is_big_endian = (blendfile.read(1) == b'V')

    # Now read the bhead chunk!!!
    blendfile.read(3) # skip the version

    sizeof_pointer = 8 if is_64_bit else 4

    sizeof_bhead = 24 if is_64_bit else 20
    
    int_endian = '>i' if is_big_endian else '<i'
    int_endian_pair = '>ii' if is_big_endian else '<ii'
    
    while True:
        try:
            code, length = struct.unpack(int_endian_pair, blendfile.read(8)) # 8 == sizeof(int) * 2
        except IOError:
            return None, 0, 0
        
        # finally read the rest of the bhead struct, pointer and 2 ints
        blendfile.seek(sizeof_bhead - 8, os.SEEK_CUR)

        if code == REND:
            blendfile.seek(length, os.SEEK_CUR)
        else:
            break
    
    if code != TEST:
        return None, 0, 0

    try:
        x, y = struct.unpack(int_endian_pair, blendfile.read(8)) # 8 == sizeof(int) * 2
    except struct.error:
        return None, 0, 0

    length -= 8 # sizeof(int) * 2
    
    if length != x * y * 4:
        return None, 0, 0
    
    image_buffer = blendfile.read(length)
    
    if len(image_buffer) != length:
        return None, 0, 0

    return image_buffer, x, y


def write_png(buf, width, height):
    import zlib

    # reverse the vertical line order and add null bytes at the start
    width_byte_4 = width * 4
    raw_data = b"".join([b'\x00' + buf[span:span + width_byte_4] for span in range((height - 1) * width * 4, -1, - width_byte_4)])
    
    def png_pack(png_tag, data):
        chunk_head = png_tag + data
        return struct.pack("!I", len(data)) + chunk_head + struct.pack("!I", 0xFFFFFFFF & zlib.crc32(chunk_head))

    return b"".join([
        b'\x89PNG\r\n\x1a\n',
        png_pack(b'IHDR', struct.pack("!2I5B", width, height, 8, 6, 0, 0, 0)),
        png_pack(b'IDAT', zlib.compress(raw_data, 9)),
        png_pack(b'IEND', b'')])


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Expected 2 arguments <input.blend> <output.png>")
    else:
        file_in = sys.argv[-2]

        buf, width, height = blend_extract_thumb(file_in)
        
        if buf:
            file_out = sys.argv[-1]

            f = open(file_out, "wb")
            f.write(write_png(buf, width, height))
            f.close()
