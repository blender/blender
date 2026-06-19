# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --factory-startup --python tests/python/sequencer_strip_naming.py

import bpy

import sys
import unittest


class StripNamingTest(unittest.TestCase):
    def setUp(self):
        bpy.context.scene.sequence_editor_create()
        self.strips = bpy.context.scene.sequence_editor.strips

    def tearDown(self):
        bpy.context.scene.sequence_editor_clear()

    def test_duplicate_name_gets_numeric_suffix(self):
        # identical names get .001, .002, ...
        s1 = self.strips.new_meta("Meta", 1, 1)
        s2 = self.strips.new_meta("Meta", 2, 1)
        s3 = self.strips.new_meta("Meta", 3, 1)
        self.assertEqual(s1.name, "Meta")
        self.assertEqual(s2.name, "Meta.001")
        self.assertEqual(s3.name, "Meta.002")

    def test_purely_numeric_suffix_increments(self):
        # strip name already ends in .###: the number should be increased
        s1 = self.strips.new_meta("Clip.012", 1, 1)
        s2 = self.strips.new_meta("Clip.012", 2, 1)
        self.assertEqual(s1.name, "Clip.012")
        self.assertEqual(s2.name, "Clip.013")

    def test_dot_number_nonnumeric_suffix_not_stripped(self):
        # a ".###" followed by more characters should not be stripped
        name = "name.123_anything_456"
        s1 = self.strips.new_meta(name, 1, 1)
        s2 = self.strips.new_meta(name, 2, 1)
        s3 = self.strips.new_meta(name, 3, 1)
        self.assertEqual(s1.name, "name.123_anything_456")
        self.assertEqual(s2.name, "name.123_anything_456.001")
        self.assertEqual(s3.name, "name.123_anything_456.002")

    def test_isoformat_name_not_stripped(self):
        # naming case from #160144:
        # datetime.isoformat() produces names like YYYY-MM-DDTHH:MM:SS.ffffff+HH:MM
        # The ".ffffff+HH:MM" part must be preserved, not stripped
        name = "2024-01-15T12:30:45.123456+02:00"
        s1 = self.strips.new_meta(name, 1, 1)
        s2 = self.strips.new_meta(name, 2, 1)
        self.assertEqual(s1.name, name)
        self.assertEqual(s2.name, name + ".001")

    def test_multiple_dots_last_numeric_increments(self):
        # "a.b.002": last suffix is purely numeric, so it should increment
        s1 = self.strips.new_meta("a.b.002", 1, 1)
        s2 = self.strips.new_meta("a.b.002", 2, 1)
        self.assertEqual(s1.name, "a.b.002")
        self.assertEqual(s2.name, "a.b.003")

    def test_multiple_dots_last_nonnumeric_appends(self):
        # "a.b.c": last suffix is non-numeric, whole name is the base
        s1 = self.strips.new_meta("a.b.c", 1, 1)
        s2 = self.strips.new_meta("a.b.c", 2, 1)
        self.assertEqual(s1.name, "a.b.c")
        self.assertEqual(s2.name, "a.b.c.001")


def main():
    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    unittest.main(argv=argv)


if __name__ == "__main__":
    main()
