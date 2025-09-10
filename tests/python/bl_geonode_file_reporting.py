# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# blender -b -P tests/python/bl_geonode_file_reporting.py -- --verbose --testdir tests/files/io_tests
import unittest
from pathlib import Path

import bpy


class TestFileImportNodes(unittest.TestCase):
    """Test that the imported OBJ file is reported via bpy.data.file_path_map()."""

    testdir: Path

    @classmethod
    def setUpClass(cls) -> None:
        cls.testdir = args.testdir

    def test_obj_import_node(self):
        blend_path = self.testdir / "blend_scene/geonodes_import_obj.blend"

        bpy.ops.wm.open_mainfile(filepath=str(blend_path))

        node_tree = bpy.data.node_groups['Import OBJ']
        file_path_map: dict[bpy.types.ID, set[str]] = bpy.data.file_path_map()

        # Go through Path(...) to ensure platform-native slashes.
        relative_path = f"//{Path('../obj/all_tris.obj')!s}"

        self.assertIn(node_tree, file_path_map)
        self.assertEqual({relative_path}, file_path_map[node_tree],
                         "The path to the OBJ file should be reported, as relative path")


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
