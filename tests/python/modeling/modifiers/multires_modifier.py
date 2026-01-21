# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "main",
)

import os
import sys

import bpy

"""
blender -b --factory-startup tests/files/modeling/modifiers/multires_modifier --python tests/python/modeling/modifiers/multires_modifier.py
"""

BASE_DIR = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(BASE_DIR, "..", ".."))
from modules.mesh_test import RunTest, ModifierSpec, SpecMeshTest, OperatorSpecObjectMode


def main():
    tests = [
        SpecMeshTest("CatmullClarkSubdivide", "testCubeMultires", "expectedCubeMultires",
                     [
                         ModifierSpec('multires', 'MULTIRES', {}),
                         OperatorSpecObjectMode('multires_subdivide', {'modifier': 'multires'}),
                         OperatorSpecObjectMode('modifier_apply', {'modifier': 'multires'})
                     ], apply_modifier=False),
        SpecMeshTest("SimpleSubdivide", "testSuzanneSimple", "expectedSuzanneSimple",
                     [
                         ModifierSpec('multires', 'MULTIRES', {}),
                         OperatorSpecObjectMode('multires_subdivide', {'modifier': 'multires', 'mode': 'SIMPLE'}),
                         OperatorSpecObjectMode('modifier_apply', {'modifier': 'multires'})
                     ], apply_modifier=False),
        SpecMeshTest("LinearSubdivide", "testSuzanneLinear", "expectedSuzanneLinear",
                     [
                         ModifierSpec('multires', 'MULTIRES', {}),
                         OperatorSpecObjectMode('multires_subdivide', {'modifier': 'multires', 'mode': 'LINEAR'}),
                         OperatorSpecObjectMode('modifier_apply', {'modifier': 'multires'})
                     ], apply_modifier=False),
    ]

    modifiers_test = RunTest(tests)
    modifiers_test.main()


if __name__ == "__main__":
    main()
