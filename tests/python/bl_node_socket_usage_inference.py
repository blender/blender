# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys
import tempfile
import bpy
import unittest

args = None


class StructureTypeInferenceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir

    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        "Test dir {0} should exist".format(self.testdir))

    def load_testfile(self):
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "socket_usage_inference.blend"))

    def assertUsed(self, socket):
        self.assertTrue(socket.is_icon_visible)
        self.assertFalse(socket.is_inactive)

    def assertGrayedOut(self, socket):
        self.assertTrue(socket.is_icon_visible)
        self.assertTrue(socket.is_inactive)

    def assertHidden(self, socket):
        self.assertFalse(socket.is_icon_visible)

    def test_geometry_nodes(self):
        self.load_testfile()
        tree = bpy.data.node_groups["Geometry Nodes"]

        node = tree.nodes["Math Group"]
        self.assertUsed(node.inputs["A"])
        self.assertUsed(node.inputs["B"])

        node = tree.nodes["Menu Switch Group"]
        self.assertUsed(node.inputs["Menu"])
        self.assertUsed(node.inputs["A"])
        self.assertHidden(node.inputs["B"])

        node = tree.nodes["Transform Matrix"]
        self.assertUsed(node.inputs["Mode"])
        self.assertUsed(node.inputs["Geometry"])
        self.assertUsed(node.inputs["Transform"])
        self.assertHidden(node.inputs["Translation"])
        self.assertHidden(node.inputs["Rotation"])
        self.assertHidden(node.inputs["Scale"])

        node = tree.nodes["Transform Components"]
        self.assertUsed(node.inputs["Mode"])
        self.assertUsed(node.inputs["Geometry"])
        self.assertUsed(node.inputs["Translation"])
        self.assertUsed(node.inputs["Rotation"])
        self.assertUsed(node.inputs["Scale"])
        self.assertHidden(node.inputs["Transform"])


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
