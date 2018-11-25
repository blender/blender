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

import argparse
import pathlib
import sys
import unittest

from modules.test_utils import AbstractBlenderRunnerTest


class AbstractFFmpegTest(AbstractBlenderRunnerTest):
    @classmethod
    def setUpClass(cls):
        cls.blender = args.blender
        cls.testdir = pathlib.Path(args.testdir)


class AbstractFFmpegSequencerTest(AbstractFFmpegTest):
    def get_script_for_file(self, filename: pathlib.Path) -> str:
        movie = self.testdir / filename
        return \
            "import bpy; " \
            "bpy.context.scene.sequence_editor_create(); " \
            "strip = bpy.context.scene.sequence_editor.sequences.new_movie(" \
            "'test_movie', %r, channel=1, frame_start=1); " \
            "print(f'fps:{strip.fps}')" % movie.as_posix()

    def get_movie_file_fps(self, filename: pathlib.Path) -> float:
        script = self.get_script_for_file(filename)
        output = self.run_blender('', script)
        for line in output.splitlines():
            if line.startswith('fps:'):
                return float(line.split(':')[1])
        return 0.0


class FPSDetectionTest(AbstractFFmpegSequencerTest):
    def test_T51153(self):
        self.assertAlmostEqual(
            self.get_movie_file_fps('T51153_bad_clip_2.mts'),
            29.97,
            places=2)

    def test_T53857(self):
        self.assertAlmostEqual(
            self.get_movie_file_fps('T53857_2018-01-22_15-30-49.mkv'),
            30.0,
            places=2)

    def test_T54148(self):
        self.assertAlmostEqual(
            self.get_movie_file_fps('T54148_magn_0.mkv'),
            1.0,
            places=2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--blender', required=True)
    parser.add_argument('--testdir', required=True)
    args, remaining = parser.parse_known_args()

    unittest.main(argv=sys.argv[0:1] + remaining)
