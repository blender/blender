#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2010-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# This module can get render info without running from inside blender.
#
# This struct won't change according to Ton.
# Note that the size differs on 32/64bit
#
# typedef struct BHead {
#     int code, len;
#     void *old;
#     int SDNAnr, nr;
# } BHead;

__all__ = (
    "read_blend_rend_chunk",
)


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

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self._blendfile.close()
        if self._blendfile_base is not None:
            self._blendfile_base.close()

        return False


def _read_blend_rend_chunk_from_file(blendfile, filepath):
    import struct
    import sys

    from os import SEEK_CUR

    head = blendfile.read(7)
    if head != b'BLENDER':
        sys.stderr.write("Not a blend file: {:s}\n".format(filepath))
        return []

    is_64_bit = (blendfile.read(1) == b'-')

    # true for PPC, false for X86
    is_big_endian = (blendfile.read(1) == b'V')

    # Now read the bhead chunk!
    blendfile.seek(3, SEEK_CUR)  # Skip the version.

    scenes = []

    sizeof_bhead = 24 if is_64_bit else 20

    # Should always be 4, but a malformed/corrupt file may be less.
    while (bhead_id := blendfile.read(4)) != b'ENDB':

        if len(bhead_id) != 4:
            sys.stderr.write("Unable to read until ENDB block (corrupt file): {:s}\n".format(filepath))
            break

        sizeof_data_left = struct.unpack('>i' if is_big_endian else '<i', blendfile.read(4))[0]
        if sizeof_data_left < 0:
            # Very unlikely, but prevent other errors.
            sys.stderr.write("Negative block size found (corrupt file): {:s}\n".format(filepath))
            break

        # 4 from the `head_id`, another 4 for the size of the BHEAD.
        sizeof_bhead_left = sizeof_bhead - 8

        # The remainder of the BHEAD struct is not used.
        blendfile.seek(sizeof_bhead_left, SEEK_CUR)

        if bhead_id == b'REND':
            # Now we want the scene name, start and end frame. this is 32bits long.
            start_frame, end_frame = struct.unpack('>2i' if is_big_endian else '<2i', blendfile.read(8))
            sizeof_data_left -= 8

            scene_name = blendfile.read(64)
            sizeof_data_left -= 64

            scene_name = scene_name[:scene_name.index(b'\0')]
            # It's possible old blend files are not UTF8 compliant, use `surrogateescape`.
            scene_name = scene_name.decode("utf8", errors='surrogateescape')

            scenes.append((start_frame, end_frame, scene_name))

        if sizeof_data_left > 0:
            blendfile.seek(sizeof_data_left, SEEK_CUR)
        elif sizeof_data_left < 0:
            # Very unlikely, but prevent attempting to further parse corrupt data.
            sys.stderr.write("Error calculating next block (corrupt file): {:s}\n".format(filepath))
            break

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
