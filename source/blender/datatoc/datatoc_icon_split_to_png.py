#!/usr/bin/env python3

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

# This script is just to view the icons


def write_png(buf, width, height):
    import zlib
    import struct
    # reverse the vertical line order and add null bytes at the start
    width_byte_4 = width * 4
    raw_data = b"".join(b'\x00' + buf[span:span + width_byte_4] for span in range((height - 1) * width * 4, -1, - width_byte_4))

    def png_pack(png_tag, data):
        chunk_head = png_tag + data
        return struct.pack("!I", len(data)) + chunk_head + struct.pack("!I", 0xFFFFFFFF & zlib.crc32(chunk_head))

    return b"".join([
        b'\x89PNG\r\n\x1a\n',
        png_pack(b'IHDR', struct.pack("!2I5B", width, height, 8, 6, 0, 0, 0)),
        png_pack(b'IDAT', zlib.compress(raw_data, 9)),
        png_pack(b'IEND', b'')])


def icondata_to_png(file_src, file_dst):
    import struct

    with open(file_src, 'rb') as f_src:
        # 2 ints
        temp_data = f_src.read(4 * 2)
        w, h = struct.unpack('<2I', temp_data)
        temp_data = f_src.read(4 * 2)  # (x, y)         - ignored
        temp_data = f_src.read(4 * 2)  # (xfrom, yfrom) - ignored
        # pixels
        temp_data = f_src.read(w * h * 4)

    buf = write_png(temp_data, w, h)

    with open(file_dst, 'wb') as f_dst:
        f_dst.write(buf)


def main():
    import sys
    import os

    for arg in sys.argv[1:]:
        file_src = arg
        file_dst = os.path.splitext(arg)[0] + ".png"

        icondata_to_png(file_src, file_dst)

if __name__ == "__main__":
    main()
