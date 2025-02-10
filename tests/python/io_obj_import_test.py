# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later
import pathlib
import sys
import unittest

import bpy

sys.path.append(str(pathlib.Path(__file__).parent.absolute()))

args = None


class OBJImportTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir
        cls.output_dir = args.outdir

    def test_import_obj(self):
        input_files = sorted(pathlib.Path(self.testdir).glob("*.obj"))
        self.passed_tests = []
        self.failed_tests = []
        self.updated_tests = []

        from modules import io_report
        report = io_report.Report("OBJ Import", self.output_dir, self.testdir, self.testdir.joinpath("reference"))

        for input_file in input_files:
            with self.subTest(pathlib.Path(input_file).stem):
                bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "../empty.blend"))
                ok = report.import_and_check(
                    input_file, lambda filepath, params: bpy.ops.wm.obj_import(filepath=str(input_file), **params))
                if not ok:
                    self.fail(f"{input_file.stem} import result does not match expectations")

        report.finish("io_obj")


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    parser.add_argument('--outdir', required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
