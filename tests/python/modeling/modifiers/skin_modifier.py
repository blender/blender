# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "main",
)

import os
import sys

"""
blender -b --factory-startup tests/files/modeling/modifiers/skin_modifier.blend --python tests/python/modeling/modifiers/skin_modifier.py
"""

BASE_DIR = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(BASE_DIR, "..", ".."))
from modules.mesh_test import RunTest, ModifierSpec, SpecMeshTest


def main():
    tests = [
        # Holes in the result, see: #165789.
        SpecMeshTest("SkinHole", "testSkinHole", "expectedSkinHole",
                     [ModifierSpec('skin', 'SKIN', {})]),
        # Mesh separating at forks, see: #165312.
        SpecMeshTest("SkinIslands", "testSkinIslands", "expectedSkinIslands",
                     [ModifierSpec('skin', 'SKIN', {})]),
        # Loose vertices from frames inside the hull, see: !165946.
        SpecMeshTest("SkinLoose", "testSkinLoose", "expectedSkinLoose",
                     [ModifierSpec('skin', 'SKIN', {})]),
        # Degenerate geometry when the root is inside an open chain, see: #66963.
        SpecMeshTest("SkinOpenChainRoot", "testSkinOpenChainRoot", "expectedSkinOpenChainRoot",
                     [ModifierSpec('skin', 'SKIN', {})]),
        # Frame orientation resetting at root in open chains. see: !166215.
        SpecMeshTest("SkinFrameOrientation", "testSkinFrameOrientation", "expectedSkinFrameOrientation",
                     [ModifierSpec('skin', 'SKIN', {})]),
    ]

    modifiers_test = RunTest(tests)
    modifiers_test.main()


if __name__ == "__main__":
    main()
