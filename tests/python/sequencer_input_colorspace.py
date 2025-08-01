# SPDX-FileCopyrightText: 2015-2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --factory-startup \
#   --python tests/python/sequencer_input_colorspace.py -- --testdir tests/files/sequence_editing/

import bpy

import argparse
import sys
import unittest

from pathlib import Path

TEST_DIR: Path


class MovieInputTest(unittest.TestCase):
    def get_movie_colorspace(self, filepath: Path):
        scene = bpy.context.scene
        ed = scene.sequence_editor_create()
        strip = ed.strips.new_movie(name='input', filepath=str(filepath), channel=1, frame_start=1)
        colorspace = strip.colorspace_settings.name
        ed.strips.remove(strip)
        return colorspace


class FFmpegHDRColorspace(MovieInputTest):
    def test_pq(self):
        prefix = TEST_DIR / Path("ffmpeg") / "media"

        self.assertEqual(self.get_movie_colorspace(prefix / "hdr_simple_export_pq_12bit.mov"), "Rec.2100-PQ")

    def test_hlg(self):
        prefix = TEST_DIR / Path("ffmpeg") / "media"

        self.assertEqual(self.get_movie_colorspace(prefix / "hdr_simple_export_hlg_12bit.mov"), "Rec.2100-HLG")


def main():
    global TEST_DIR

    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=Path)

    args, remaining = parser.parse_known_args(argv)

    TEST_DIR = args.testdir
    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
