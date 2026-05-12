# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later
"""
blender -b --factory-startup tests/files/sculpting/voxel_remesh_compare --python tests/python/sculpt_paint/voxel_remesh_compare_test.py
"""

__all__ = (
    "main",
)

import os
import sys

import bpy

BASE_DIR = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(BASE_DIR, ".."))
from modules.mesh_test import RunTest, SpecMeshTest, OperatorSpec


def main():
    tests = [
        SpecMeshTest("Color Interpolation", "testCube", "expectedCube",
                     [
                         OperatorSpec('SCULPT', 'ed.undo_push', {}),
                         OperatorSpec('SCULPT', 'object.voxel_remesh', {}),
                     ]),
    ]

    modifiers_test = RunTest(tests)
    modifiers_test.main()


if __name__ == "__main__":
    main()
