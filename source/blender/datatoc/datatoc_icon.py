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

# <pep8 compliant>
_IS_BIG_ENDIAN = (__import__("sys").byteorder != 'little')


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


def icon_decode_head(f_src):
    import struct

    # 2 ints
    temp_data = f_src.read(4 * 2)
    icon_w, icon_h = struct.unpack('<2I', temp_data)

    temp_data = f_src.read(4 * 2)
    orig_x, orig_y = struct.unpack('<2I', temp_data)

    temp_data = f_src.read(4 * 2)
    canvas_w, canvas_h = struct.unpack('<2I', temp_data)

    return (icon_w, icon_h,
            orig_x, orig_y,
            canvas_w, canvas_h)


def icon_decode(f_src):
    head = icon_decode_head(f_src)

    (icon_w, icon_h,
     orig_x, orig_y,
     canvas_w, canvas_h) = head

    # pixels
    import array

    pixels = f_src.read(icon_w * icon_h * 4)
    pixels = array.array('I', pixels)
    if _IS_BIG_ENDIAN:
        pixels.byteswap()

    return head, pixels


def icon_read(file_src):
    with open(file_src, 'rb') as f_src:
        head, pixels = icon_decode(f_src)
    return head, pixels


def icon_merge(file_src, pixels_canvas, canvas_w, canvas_h):
    """ Takes an icon filepath and merges into a pixel array
    """
    head, pixels = icon_read(file_src)

    (icon_w, icon_h,
     orig_x, orig_y,
     w_canvas_test, h_canvas_test) = head

    assert(w_canvas_test == canvas_w)
    assert(h_canvas_test == canvas_h)

    for x in range(icon_w):
        for y in range(icon_h):
            # get pixel
            pixel = pixels[(y * icon_w) + x]

            # set pixel
            dst_x = orig_x + x
            dst_y = orig_y + y
            pixels_canvas[(dst_y * canvas_w) + dst_x] = pixel


def icondir_to_png(path_src, file_dst):
    """ Takes a path full of 'dat' files and writes out
    """
    import os
    import array

    files = [os.path.join(path_src, f) for f in os.listdir(path_src) if f.endswith(".dat")]

    # First check if we need to bother.
    if os.path.exists(file_dst):
        dst_time = os.path.getmtime(file_dst)
        has_newer = False
        for f in files:
            if os.path.getmtime(f) > dst_time:
                has_newer = True
                break
        if not has_newer:
            return

    with open(files[0], 'rb') as f_src:
        (icon_w, icon_h,
         orig_x, orig_y,
         canvas_w, canvas_h) = icon_decode_head(f_src)

    # load in pixel data
    pixels_canvas = array.array('I', [0]) * (canvas_w * canvas_h)
    for f in files:
        icon_merge(f, pixels_canvas, canvas_w, canvas_h)

    # write pixels
    with open(file_dst, 'wb') as f_dst:
        import sys
        # py2/3 compat
        if sys.version.startswith("2"):
            pixels_data = pixels_canvas.tostring()
        else:
            pixels_data = pixels_canvas.tobytes()

        image_data = write_png(pixels_data, canvas_w, canvas_h)
        f_dst.write(image_data)


def main_ex(argv):
    import os

    path_src = argv[-2].rstrip(os.sep)
    file_dst = argv[-1]

    icondir_to_png(path_src, file_dst)


def main():
    import sys
    main_ex(sys.argv)


if __name__ == "__main__":
    main()
