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

import pathlib
import sys
import unittest

import bpy

args = None

class AbstractUSDTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir

    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        'Test dir {0} should exist'.format(self.testdir))

        # Make sure we always start with a known-empty file.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))

class USDImportTest(AbstractUSDTest):

    def test_import_prim_hierarchy(self):
        """Test importing a simple object hierarchy from a USDA file."""

        infile = str(self.testdir / "prim-hierarchy.usda")

        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res)

        objects = bpy.context.scene.collection.objects
        self.assertEqual(5, len(objects))

        # Test the hierarchy.
        self.assertIsNone(objects['World'].parent)
        self.assertEqual(objects['World'], objects['Plane'].parent)
        self.assertEqual(objects['World'], objects['Plane_001'].parent)
        self.assertEqual(objects['World'], objects['Empty'].parent)
        self.assertEqual(objects['Empty'], objects['Plane_002'].parent)

def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
