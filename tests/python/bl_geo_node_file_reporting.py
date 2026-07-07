# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# blender -b -P tests/python/bl_geo_node_file_reporting.py -- --verbose --testdir tests/files
import unittest
from pathlib import Path

import bpy


class FileReportingTest(unittest.TestCase):
    testdir: Path

    @classmethod
    def setUpClass(cls) -> None:
        cls.testdir = args.testdir

    def test_obj_import_node(self):
        """Test that the imported OBJ file is reported via bpy.data.file_path_map()."""
        blend_path = self.testdir / "io_tests/blend_scene/geonodes_import_obj.blend"

        bpy.ops.wm.open_mainfile(filepath=str(blend_path))

        node_tree = bpy.data.node_groups['Import OBJ']
        file_path_map: dict[bpy.types.ID, set[str]] = bpy.data.file_path_map()

        # Go through Path(...) to ensure platform-native slashes.
        relative_path = f"//{Path('../obj/all_tris.obj')!s}"

        self.assertIn(node_tree, file_path_map)
        self.assertEqual({relative_path}, file_path_map[node_tree],
                         "The path to the OBJ file should be reported, as relative path")

    def test_geonodes_sim_cache(self):
        """Test that the simulation caches are reported via bpy.data.foreach_path()."""
        blend_path = self.testdir / "modeling/geometry_nodes/155953-sim-cache-foreach-path/geonodes-sim-cache-bugreport.blend"

        bpy.ops.wm.open_mainfile(filepath=str(blend_path))

        # Filter out the entries with an empty value.
        file_path_map: dict[bpy.types.ID, set[str]] = {
            k: v for (k, v) in bpy.data.file_path_map().items() if v}

        # Expect only the directories to be reported, and not each individual file within them.
        expect_map = {
            bpy.data.objects['Custom Bake Path']: {'//bakePath'},
            bpy.data.objects['Custom Bake Path.001']: {'//set-on-node'},
            bpy.data.objects['Default Bake Path']: {'//config-on-sim-node'},
            bpy.data.objects['Default Bake Path.001']: {'//only-set-on-modifier'},
        }
        self.maxDiff = None

        # Compare the reported data-blocks first, because if those don't match the shown diff is rather hard to read.
        self.assertEqual(set(expect_map.keys()), set(file_path_map.keys()))
        self.assertEqual(expect_map, file_path_map)


def main():
    global args
    import argparse
    import sys

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining, verbosity=0)


if __name__ == "__main__":
    main()
