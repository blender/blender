# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import math
import pathlib
import sys
import tempfile
import unittest

import bpy

args = None


class GPencilSVGSceneUnitScaleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir
        cls.output_dir = args.outdir

    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir.parent / 'empty.blend'))
        scene = bpy.context.scene
        scene.unit_settings.system = 'METRIC'
        scene.unit_settings.scale_length = 0.01

    def test_import_svg_respects_scene_unit_scale(self):
        self.output_dir.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=self.output_dir) as tmpdir:
            svg_path = pathlib.Path(tmpdir) / 'unit_scale.svg'
            svg_path.write_text(
                '<?xml version="1.0" encoding="UTF-8"?>\n'
                '<svg xmlns="http://www.w3.org/2000/svg" width="100mm" height="20mm" '
                'viewBox="0 0 100 20">\n'
                '  <path d="M 0 10 L 100 10" fill="none" stroke="#000000" stroke-width="1" />\n'
                '</svg>\n',
                encoding='utf-8',
            )

            bpy.ops.wm.grease_pencil_import_svg(
                filepath=str(svg_path),
                use_scene_unit=True,
                scale=1.0,
                recenter_bounds=True,
            )

            gp_objects = [obj for obj in bpy.data.objects if obj.type == 'GREASEPENCIL']
            self.assertTrue(gp_objects, 'No Grease Pencil object was created')

            gp_obj = gp_objects[-1]
            bpy.context.view_layer.update()
            width = gp_obj.dimensions.x

            self.assertTrue(
                math.isclose(width, 10.0, rel_tol=0.05, abs_tol=0.05),
                f'Expected SVG width to respect scene unit scale (~10.0 units), got {width!r}',
            )


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


if __name__ == '__main__':
    main()
