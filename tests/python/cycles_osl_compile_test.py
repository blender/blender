# SPDX-FileCopyrightText: 2015-2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import sys
import tempfile
import unittest

from pathlib import Path

import bpy
import _cycles

args = None


class AbstractOSLCompileTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tempdir = tempfile.TemporaryDirectory()
        cls.testdir = args.testdir
        cls.tempdir = Path(cls._tempdir.name)

    def setUp(self):
        self.assertTrue(self.testdir.exists(), "Test dir {0} should exist".format(self.testdir))

    def tearDown(self):
        self._tempdir.cleanup()


class OSLCompileTest(AbstractOSLCompileTest):
    def test_compile_simple(self):
        input_file = self.tempdir / "input.osl"
        output_file = self.tempdir / "output.oso"

        input_file.write_text("""
shader basic_shader(
    float in_float = 1.0,
    color in_color = color(1.0, 1.0, 1.0),
    output float out_float = 0.0,
    output color out_color = color(0.0, 0.0, 0.0)
    )
{
    out_float = in_float * 2.0;
    out_color = in_color * 2.0;
}""")

        self.assertTrue(_cycles.osl_compile(str(input_file), str(output_file)))


def main():
    global args
    import argparse

    if "--" in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index("--") + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument("--testdir", required=True, type=Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining, verbosity=0)


if __name__ == "__main__":
    main()
