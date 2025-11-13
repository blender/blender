# SPDX-FileCopyrightText: 2025 Blender Authors & Khronos Group contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later
import pathlib
import sys
import unittest
import tempfile

import bpy

sys.path.append(str(pathlib.Path(__file__).parent.absolute()))

from io_gltf_utils import gltf_generate_descr

args = None


def do_gltf_export(filepath, output_filepath, params_import, params_export):
    bpy.ops.wm.open_mainfile(filepath=str(filepath))
    bpy.ops.export_scene.gltf(
        filepath=output_filepath,
        export_format='GLTF_SEPARATE',
        **params_export)


class GLTFExportTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir
        cls.output_dir = args.outdir

    def test_export_gltf(self):
        input_files = sorted(pathlib.Path(self.testdir).glob("*.blend"))
        self.passed_tests = []
        self.failed_tests = []
        self.updated_tests = []

        from modules import io_report
        report = io_report.Report(
            "glTF Export",
            self.output_dir,
            self.testdir,
            self.testdir.joinpath("reference"),
            gltf_generate_descr)

        with tempfile.TemporaryDirectory() as tmp_dir:
            for input_file in input_files:
                with self.subTest(input_file.stem):
                    output_filepath = (pathlib.Path(tmp_dir) / input_file.name).with_suffix('.gltf')
                    bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "../../empty.blend"))
                    ok = report.generate_and_check(
                        input_file,
                        do_gltf_export,
                        output_filepath=output_filepath)
                    if not ok:
                        self.fail(f"{input_file.stem} import result does not match expectations")

        report.finish("io_gltf_roundtrip")


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
