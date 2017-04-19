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

"""
./blender.bin --background -noaudio --factory-startup --python tests/python/bl_alembic_import_test.py -- --testdir /path/to/lib/tests/alembic
"""

import pathlib
import sys
import unittest

import bpy

args = None


class SimpleImportTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir

    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        'Test dir %s should exist' % self.testdir)

        # Make sure we always start with a known-empty file.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))

    def test_import_cube_hierarchy(self):
        res = bpy.ops.wm.alembic_import(
            filepath=str(self.testdir / "cubes-hierarchy.abc"),
            as_background_job=False)
        self.assertEqual({'FINISHED'}, res)

        # The objects should be linked to scene_collection in Blender 2.8,
        # and to scene in Blender 2.7x.
        objects = bpy.context.scene.objects
        self.assertEqual(13, len(objects))

        # Test the hierarchy.
        self.assertIsNone(objects['Cube'].parent)
        self.assertEqual(objects['Cube'], objects['Cube_001'].parent)
        self.assertEqual(objects['Cube'], objects['Cube_002'].parent)
        self.assertEqual(objects['Cube'], objects['Cube_003'].parent)
        self.assertEqual(objects['Cube_003'], objects['Cube_004'].parent)
        self.assertEqual(objects['Cube_003'], objects['Cube_005'].parent)
        self.assertEqual(objects['Cube_003'], objects['Cube_006'].parent)


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--')+1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    import traceback
    # So a python error exits Blender itself too
    try:
        main()
    except SystemExit:
        raise
    except:
        traceback.print_exc()
        sys.exit(1)
