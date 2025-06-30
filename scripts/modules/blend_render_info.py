#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2010-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# This module can get render info without running from inside blender.


__all__ = (
    "read_blend_rend_chunk",
)

import _blendfile_header


class RawBlendFileReader:
    """
    Return a file handle to the raw blend file data (abstracting compressed formats).
    """
    __slots__ = (
        # The path to load.
        "_filepath",
        # The file base file handler or None (only set for compressed formats).
        "_blendfile_base",
        # The file handler to return to the caller (always uncompressed data).
        "_blendfile",
    )

    def __init__(self, filepath):
        self._filepath = filepath
        self._blendfile_base = None
        self._blendfile = None

    def __enter__(self):
        blendfile = open(self._filepath, "rb")
        blendfile_base = None
        head = blendfile.read(4)
        blendfile.seek(0)
        if head[0:2] == b'\x1f\x8b':  # GZIP magic.
            import gzip
            blendfile_base = blendfile
            blendfile = gzip.open(blendfile, "rb")
        elif head[0:4] == b'\x28\xb5\x2f\xfd':  # Z-standard magic.
            import zstandard
            blendfile_base = blendfile
            blendfile = zstandard.open(blendfile, "rb")

        self._blendfile_base = blendfile_base
        self._blendfile = blendfile

        return self._blendfile

    def __exit__(self, _exc_type, _exc_value, _exc_traceback):
        self._blendfile.close()
        if self._blendfile_base is not None:
            self._blendfile_base.close()

        return False


def get_render_info_structure(endian_str, size):
    import struct
    # The maximum size of the scene name changed over time, so create a different
    # structure depending on the size of the entire block.
    if size == 2 * 4 + 24:
        return struct.Struct(endian_str + b'ii24s')
    if size == 2 * 4 + 64:
        return struct.Struct(endian_str + b'ii64s')
    if size == 2 * 4 + 256:
        return struct.Struct(endian_str + b'ii256s')
    raise ValueError("Unknown REND chunk size: {:d}".format(size))


def _read_blend_rend_chunk_from_file(blendfile, filepath):
    import sys

    from os import SEEK_CUR

    try:
        blender_header = _blendfile_header.BlendFileHeader(blendfile)
    except _blendfile_header.BlendHeaderError:
        sys.stderr.write("Not a blend file: {:s}\n".format(filepath))
        return []

    scenes = []

    endian_str = b'<' if blender_header.is_little_endian else b'>'

    block_header_struct = blender_header.create_block_header_struct()
    while bhead := _blendfile_header.BlockHeader(blendfile, block_header_struct):
        if bhead.code == b'ENDB':
            break
        remaining_bytes = bhead.size
        if bhead.code == b'REND':
            rend_block_struct = get_render_info_structure(endian_str, bhead.size)
            start_frame, end_frame, scene_name = rend_block_struct.unpack(blendfile.read(rend_block_struct.size))
            remaining_bytes -= rend_block_struct.size

            scene_name = scene_name[:scene_name.index(b'\0')]
            # It's possible old blend files are not UTF8 compliant, use `surrogateescape`.
            scene_name = scene_name.decode("utf8", errors="surrogateescape")
            scenes.append((start_frame, end_frame, scene_name))

        blendfile.seek(remaining_bytes, SEEK_CUR)

    return scenes


def read_blend_rend_chunk(filepath):
    with RawBlendFileReader(filepath) as blendfile:
        return _read_blend_rend_chunk_from_file(blendfile, filepath)


def main():
    import sys

    for filepath in sys.argv[1:]:
        for value in read_blend_rend_chunk(filepath):
            print("{:d} {:d} {:s}".format(*value))


if __name__ == '__main__':
    main()
