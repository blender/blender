#!/usr/bin/env python

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
Thumbnailer runs with python 2.7 and 3.x.
To run automatically with a file manager such as Nautilus, save this file
in a directory that is listed in PATH environment variable, and create
blender.thumbnailer file in ${HOME}/.local/share/thumbnailers/ directory
with the following contents:

[Thumbnailer Entry]
TryExec=blender-thumbnailer.py
Exec=blender-thumbnailer.py %u %o
MimeType=application/x-blender;
"""

import struct


def open_wrapper_get():
    """ wrap OS spesific read functionality here, fallback to 'open()'
    """

    class GFileWrapper:
        __slots__ = ("mode", "g_file")
        def __init__(self, url, mode='r'):
            self.mode = mode # used in gzip module
            self.g_file = Gio.File.parse_name(url).read(None)

        def read(self, size):
            return self.g_file.read_bytes(size, None).get_data()

        def seek(self, offset, whence=0):
            self.g_file.seek(offset, [1, 0, 2][whence], None)
            return self.g_file.tell()

        def tell(self):
            return self.g_file.tell()

        def close(self):
            self.g_file.close(None)

    def open_local_url(url, mode='r'):
        o = urlparse(url)
        if o.scheme == '':
            path = o.path
        elif o.scheme == 'file':
            path = unquote(o.path)
        else:
            raise(IOError('URL scheme "%s" needs gi.repository.Gio module' % o.scheme))
        return open(path, mode)

    try:
        from gi.repository import Gio
        return GFileWrapper
    except ImportError:
        try:
            # Python 3
            from urllib.parse import urlparse, unquote
        except ImportError:
            # Python 2
            from urlparse import urlparse
            from urllib import unquote
        return open_local_url


def blend_extract_thumb(path):
    import os
    open_wrapper = open_wrapper_get()

    # def MAKE_ID(tag): ord(tag[0])<<24 | ord(tag[1])<<16 | ord(tag[2])<<8 | ord(tag[3])
    REND = 1145980242  # MAKE_ID(b'REND')
    TEST = 1414743380  # MAKE_ID(b'TEST')

    blendfile = open_wrapper(path, 'rb')

    head = blendfile.read(12)

    if head[0:2] == b'\x1f\x8b':  # gzip magic
        import gzip
        blendfile.close()
        blendfile = gzip.GzipFile('', 'rb', 0, open_wrapper(path, 'rb'))
        head = blendfile.read(12)

    if not head.startswith(b'BLENDER'):
        blendfile.close()
        return None, 0, 0

    is_64_bit = (head[7] == b'-'[0])

    # true for PPC, false for X86
    is_big_endian = (head[8] == b'V'[0])

    # blender pre 2.5 had no thumbs
    if head[9:11] <= b'24':
        return None, 0, 0

    sizeof_bhead = 24 if is_64_bit else 20
    int_endian_pair = '>ii' if is_big_endian else '<ii'

    while True:
        bhead = blendfile.read(sizeof_bhead)

        if len(bhead) < sizeof_bhead:
            return None, 0, 0

        code, length = struct.unpack(int_endian_pair, bhead[0:8])  # 8 == sizeof(int) * 2

        if code == REND:
            blendfile.seek(length, os.SEEK_CUR)
        else:
            break

    if code != TEST:
        return None, 0, 0

    try:
        x, y = struct.unpack(int_endian_pair, blendfile.read(8))  # 8 == sizeof(int) * 2
    except struct.error:
        return None, 0, 0

    length -= 8  # sizeof(int) * 2

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
    raw_data = b"".join(b'\x00' + buf[span:span + width_byte_4] for span in range((height - 1) * width * 4, -1, - width_byte_4))

    def png_pack(png_tag, data):
        chunk_head = png_tag + data
        return struct.pack("!I", len(data)) + chunk_head + struct.pack("!I", 0xFFFFFFFF & zlib.crc32(chunk_head))

    return b"".join([
        b'\x89PNG\r\n\x1a\n',
        png_pack(b'IHDR', struct.pack("!2I5B", width, height, 8, 6, 0, 0, 0)),
        png_pack(b'IDAT', zlib.compress(raw_data, 9)),
        png_pack(b'IEND', b'')])


if __name__ == '__main__':
    import sys

    if len(sys.argv) < 3:
        print("Expected 2 arguments <input.blend> <output.png>")
    else:
        file_in = sys.argv[-2]

        buf, width, height = blend_extract_thumb(file_in)

        if buf:
            file_out = sys.argv[-1]

            f = open(file_out, "wb")
            f.write(write_png(buf, width, height))
            f.close()
