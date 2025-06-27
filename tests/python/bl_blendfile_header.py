# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import _blendfile_header  # Not part of the Public API, allow use for testing.
import blend_render_info
import bpy
import pathlib
import sys
import unittest
import gzip
import tempfile

args = None


class BlendFileHeaderTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir

    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        "Test dir {0} should exist".format(self.testdir))

    def test_small_bhead_8(self):
        path = self.testdir / "SmallBHead8.blend"
        with gzip.open(path, "rb") as f:
            header = _blendfile_header.BlendFileHeader(f)
            self.assertEqual(header.magic, b"BLENDER")
            self.assertEqual(header.file_format_version, 0)
            self.assertEqual(header.pointer_size, 8)
            self.assertTrue(header.is_little_endian)
            self.assertEqual(header.version, 300)

            header_struct = header.create_block_header_struct()
            self.assertIs(header_struct.type, _blendfile_header.SmallBHead8)

            buffer = f.read(header_struct.struct.size)
            block = header_struct.parse(buffer)
            self.assertEqual(block.code, b"REND")
            self.assertEqual(block.len, 72)
            self.assertEqual(block.old, 140732920740000)
            self.assertEqual(block.SDNAnr, 0)
            self.assertEqual(block.nr, 1)

        self.assertEqual(blend_render_info.read_blend_rend_chunk(path), [(1, 250, "Scene")])

    def test_large_bhead_8(self):
        path = self.testdir / "LargeBHead8.blend"
        with open(path, "rb") as f:
            header = _blendfile_header.BlendFileHeader(f)
            self.assertEqual(header.magic, b"BLENDER")
            self.assertEqual(header.file_format_version, 1)
            self.assertEqual(header.pointer_size, 8)
            self.assertTrue(header.is_little_endian)
            self.assertEqual(header.version, 500)

            header_struct = header.create_block_header_struct()
            self.assertIs(header_struct.type, _blendfile_header.LargeBHead8)

            buffer = f.read(header_struct.struct.size)
            block = header_struct.parse(buffer)
            self.assertEqual(block.code, b"REND")
            self.assertEqual(block.len, 72)
            self.assertEqual(block.old, 140737488337232)
            self.assertEqual(block.SDNAnr, 0)
            self.assertEqual(block.nr, 1)

        self.assertEqual(blend_render_info.read_blend_rend_chunk(path), [(1, 250, "Scene")])

    def test_bhead_4(self):
        path = self.testdir / "BHead4.blend"
        with gzip.open(path, "rb") as f:
            header = _blendfile_header.BlendFileHeader(f)
            self.assertEqual(header.magic, b"BLENDER")
            self.assertEqual(header.file_format_version, 0)
            self.assertEqual(header.pointer_size, 4)
            self.assertTrue(header.is_little_endian)
            self.assertEqual(header.version, 260)

            header_struct = header.create_block_header_struct()
            self.assertIs(header_struct.type, _blendfile_header.BHead4)

            buffer = f.read(header_struct.struct.size)
            block = header_struct.parse(buffer)
            self.assertEqual(block.code, b"REND")
            self.assertEqual(block.len, 32)
            self.assertEqual(block.old, 2684488)
            self.assertEqual(block.SDNAnr, 0)
            self.assertEqual(block.nr, 1)

        self.assertEqual(blend_render_info.read_blend_rend_chunk(path), [(1, 250, "Space types")])

    def test_bhead_4_big_endian(self):
        path = self.testdir / "BHead4_big_endian.blend"
        with gzip.open(path, "rb") as f:
            header = _blendfile_header.BlendFileHeader(f)
            self.assertEqual(header.magic, b"BLENDER")
            self.assertEqual(header.file_format_version, 0)
            self.assertEqual(header.pointer_size, 4)
            self.assertFalse(header.is_little_endian)
            self.assertEqual(header.version, 170)

            header_struct = header.create_block_header_struct()
            self.assertIs(header_struct.type, _blendfile_header.BHead4)

            buffer = f.read(header_struct.struct.size)
            block = header_struct.parse(buffer)
            self.assertEqual(block.code, b"REND")
            self.assertEqual(block.len, 32)
            self.assertEqual(block.old, 2147428916)
            self.assertEqual(block.SDNAnr, 0)
            self.assertEqual(block.nr, 1)

        self.assertEqual(blend_render_info.read_blend_rend_chunk(path), [(1, 150, "1")])

    def test_current(self):
        directory = tempfile.mkdtemp()
        path = pathlib.Path(directory) / "test.blend"

        bpy.ops.wm.read_factory_settings(use_empty=True)

        scene = bpy.data.scenes[0]
        scene.name = "Test Scene"
        scene.frame_start = 10
        scene.frame_end = 20
        bpy.ops.wm.save_as_mainfile(filepath=str(path), compress=False, copy=True)

        version = bpy.app.version
        version_int = version[0] * 100 + version[1]

        with open(path, "rb") as f:
            header = _blendfile_header.BlendFileHeader(f)
            self.assertEqual(header.magic, b"BLENDER")
            self.assertEqual(header.file_format_version, 1)
            self.assertEqual(header.pointer_size, 8)
            self.assertTrue(header.is_little_endian)
            self.assertEqual(header.version, version_int)

            header_struct = header.create_block_header_struct()
            self.assertIs(header_struct.type, _blendfile_header.LargeBHead8)

            buffer = f.read(header_struct.struct.size)
            block = header_struct.parse(buffer)
            self.assertEqual(block.code, b"REND")

        self.assertEqual(blend_render_info.read_blend_rend_chunk(path), [(10, 20, "Test Scene")])


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
