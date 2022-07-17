# SPDX-License-Identifier: GPL-2.0-or-later

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
