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
blender -b --factory-startup tests/files/modeling/modifiers/armature_modifier --python tests/python/modeling/modifiers/armature_modifier.py
"""

BASE_DIR = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(BASE_DIR, "..", ".."))
from modules.mesh_test import RunTest, ModifierSpec, MultiModifierSpec, SpecMeshTest


def main():
    tests = [
        SpecMeshTest("ArmatureDefault", "testMonkeyArmatureDefault", "expectedMonkeyArmatureDefault",
                     [ModifierSpec('armature', 'ARMATURE',
                                   {'object': bpy.data.objects['testArmatureDefault'],
                                    'use_vertex_groups': True})]),
        SpecMeshTest("ArmatureEnvelope", "testMonkeyArmatureEnvelope", "expectedMonkeyArmatureEnvelope",
                     [ModifierSpec('armature', 'ARMATURE',
                                   {'object': bpy.data.objects['testArmatureEnvelope'],
                                    'use_vertex_groups': False,
                                    'use_bone_envelopes': True})]),
        SpecMeshTest("ArmatureVGroupEnvelope", "testMonkeyArmatureVGroupEnvelope", "expectedMonkeyArmatureVGroupEnvelope",
                     [ModifierSpec('armature', 'ARMATURE',
                                   {'object': bpy.data.objects['testArmatureVGroupEnvelope'],
                                    'use_vertex_groups': True,
                                    'use_bone_envelopes': True})]),
        SpecMeshTest("ArmaturePreserveVolume", "testMonkeyArmaturePreserveVolume", "expectedMonkeyArmaturePreserveVolume",
                     [ModifierSpec('armature', 'ARMATURE',
                                   {'object': bpy.data.objects['testArmaturePreserveVolume'],
                                    'use_vertex_groups': True,
                                    'use_deform_preserve_volume': True})]),
        SpecMeshTest("ArmatureMasked", "testMonkeyArmatureMasked", "expectedMonkeyArmatureMasked",
                     [ModifierSpec('armature', 'ARMATURE',
                                   {'object': bpy.data.objects['testArmatureMasked'],
                                    'use_vertex_groups': True,
                                    'vertex_group': "Mask"})]),
        SpecMeshTest("ArmatureMaskedInverse", "testMonkeyArmatureMaskedInverse", "expectedMonkeyArmatureMaskedInverse",
                     [ModifierSpec('armature', 'ARMATURE',
                                   {'object': bpy.data.objects['testArmatureMaskedInverse'],
                                    'use_vertex_groups': True,
                                    'vertex_group': "Mask",
                                    'invert_vertex_group': True})]),
        SpecMeshTest("ArmatureMultiModifier", "testMonkeyArmatureMultiModifier", "expectedMonkeyArmatureMultiModifier",
                     [MultiModifierSpec(
                         [ModifierSpec('armature1', 'ARMATURE',
                                       {'object': bpy.data.objects['testArmatureMultiModifier1'],
                                        'use_vertex_groups': True}),
                          ModifierSpec('armature2', 'ARMATURE',
                                       {'object': bpy.data.objects['testArmatureMultiModifier2'],
                                        'use_vertex_groups': True,
                                        'use_multi_modifier': True})])]),
        SpecMeshTest("ArmatureMultiModifierMasked", "testMonkeyArmatureMultiModifierMasked", "expectedMonkeyArmatureMultiModifierMasked",
                     [MultiModifierSpec(
                         [ModifierSpec('armature1', 'ARMATURE',
                                       {'object': bpy.data.objects['testArmatureMultiModifierMasked1'],
                                        'use_vertex_groups': True,
                                        'vertex_group': "Mask"}),
                          ModifierSpec('armature2', 'ARMATURE',
                                       {'object': bpy.data.objects['testArmatureMultiModifierMasked2'],
                                        'use_vertex_groups': True,
                                        'vertex_group': "Mask",
                                        'use_multi_modifier': True})])]),
        SpecMeshTest("ArmatureLatticeEnvelope", "testLatticeArmatureEnvelope", "expectedLatticeArmatureEnvelope",
                     [ModifierSpec('armature', 'ARMATURE',
                                   {'object': bpy.data.objects['testArmatureLatticeEnvelope'],
                                    'use_vertex_groups': False,
                                    'use_bone_envelopes': True})]),
        SpecMeshTest("ArmatureLatticeVGroup", "testLatticeArmatureVGroup", "expectedLatticeArmatureVGroup",
                     [ModifierSpec('armature', 'ARMATURE',
                                   {'object': bpy.data.objects['testArmatureLatticeVGroup'],
                                    'use_vertex_groups': True,
                                    'use_bone_envelopes': False})]),
        SpecMeshTest("ArmatureLatticeNoVGroup", "testLatticeArmatureNoVGroup", "expectedLatticeArmatureNoVGroup",
                     [ModifierSpec('armature', 'ARMATURE',
                                   {'object': bpy.data.objects['testArmatureLatticeNoVGroup'],
                                    'use_vertex_groups': True,
                                    'use_bone_envelopes': False})]),
    ]

    modifiers_test = RunTest(tests)
    modifiers_test.main()


if __name__ == "__main__":
    main()
