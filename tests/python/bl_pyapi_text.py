# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background -noaudio --python tests/python/bl_pyapi_text.py -- --verbose
import bpy
import unittest


class TestText(unittest.TestCase):

    def setUp(self):
        self.text = bpy.data.texts.new("test_text")

    def tearDown(self):
        bpy.data.texts.remove(self.text)
        del self.text

    def test_text_new(self):
        self.assertEqual(len(bpy.data.texts), 1)
        self.assertEqual(self.text.name, "test_text")
        self.assertEqual(self.text.as_string(), "")

    def test_text_clear(self):
        self.text.clear()
        self.assertEqual(self.text.as_string(), "")

    def test_text_fill(self):
        tmp_text = (
            "Line 1: Test line 1\n"
            "Line 2: test line 2\n"
            "Line 3: test line 3"
        )
        self.text.write(tmp_text)
        self.assertEqual(self.text.as_string(), tmp_text)

    def test_text_region_as_string(self):
        tmp_text = (
            "Line 1: Test line 1\n"
            "Line 2: test line 2\n"
            "Line 3: test line 3"
        )
        self.text.write(tmp_text)
        # Get string in the middle of the text.
        self.assertEqual(self.text.region_as_string(range=((1, 0), (1, -1))), "Line 2: test line 2")
        # Big range test.
        self.assertEqual(self.text.region_as_string(range=((-10000, -10000), (10000, 10000))), tmp_text)

    def test_text_region_from_string(self):
        tmp_text = (
            "Line 1: Test line 1\n"
            "Line 2: test line 2\n"
            "Line 3: test line 3"
        )
        self.text.write(tmp_text)
        # Set string in the middle of the text.
        self.text.region_from_string("line 2", range=((1, 0), (1, -1)))
        self.assertEqual(self.text.as_string(), tmp_text.replace("Line 2: test line 2", "line 2"))
        # Large range test.
        self.text.region_from_string("New Text", range=((-10000, -10000), (10000, 10000)))
        self.assertEqual(self.text.as_string(), "New Text")


if __name__ == "__main__":
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
