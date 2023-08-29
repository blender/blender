#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2018-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

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
            "print(f'fps:{strip.fps}'); " \
            "print(f'duration:{strip.frame_final_duration}'); " % movie.as_posix()

    def get_movie_file_field(self, filename: pathlib.Path, field: str) -> str:
        script = self.get_script_for_file(filename)
        output = self.run_blender('', script)
        prefix = field + ":"
        for line in output.splitlines():
            if line.startswith(prefix):
                return line.split(':')[1]
        return ""

    def get_movie_file_field_float(self, filename: pathlib.Path, field: str) -> float:
        return float(self.get_movie_file_field(filename, field))

    def get_movie_file_field_int(self, filename: pathlib.Path, field: str) -> float:
        return int(self.get_movie_file_field(filename, field))

    def get_movie_file_fps(self, filename: pathlib.Path) -> float:
        return self.get_movie_file_field_float(filename, "fps")

    def get_movie_file_duration(self, filename: pathlib.Path) -> float:
        return self.get_movie_file_field_int(filename, "duration")


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

    def test_T68091(self):
        self.assertAlmostEqual(
            self.get_movie_file_fps('T68091-invalid-nb_frames-at-10fps.mp4'),
            10.0,
            places=2)
        self.assertEqual(
            self.get_movie_file_duration('T68091-invalid-nb_frames-at-10fps.mp4'),
            10)

    def test_T54834(self):
        self.assertEqual(
            self.get_movie_file_duration('T54834.ogg'),
            50)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--blender', required=True)
    parser.add_argument('--testdir', required=True)
    args, remaining = parser.parse_known_args()

    unittest.main(argv=sys.argv[0:1] + remaining)
