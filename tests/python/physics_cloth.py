# SPDX-FileCopyrightText: 2009-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import sys

import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import RunTest, ModifierSpec, SpecMeshTest


def main():
    test = [

        SpecMeshTest("ClothSimple", "testClothPlane", "expectedClothPlane",
                     [ModifierSpec('Cloth', 'CLOTH', {'settings': {'quality': 5}}, 15)], threshold=1e-3),

        # Not reproducible
        # SpecMeshTest("ClothPressure", "testObjClothPressure", "expObjClothPressure",
        #           [ModifierSpec('Cloth2', 'CLOTH', {'settings': {'use_pressure': True,
        #           'uniform_pressure_force': 1}}, 16)]),

        # Not reproducible
        # SpecMeshTest("ClothSelfCollision", "testClothCollision", "expClothCollision",
        #           [ModifierSpec('Cloth', 'CLOTH', {'collision_settings': {'use_self_collision': True}}, 67)]),

        SpecMeshTest("ClothSpring", "testTorusClothSpring", "expTorusClothSpring",
                     [ModifierSpec('Cloth2', 'CLOTH', {'settings': {'use_internal_springs': True}}, 10)], threshold=1e-3),

    ]
    cloth_test = RunTest(test)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            cloth_test.apply_modifiers = True
            cloth_test.do_compare = True
            cloth_test.run_all_tests()
            break
        elif cmd == "--run-test":
            cloth_test.apply_modifiers = False
            cloth_test.do_compare = False
            name = command[i + 1]
            cloth_test.run_test(name)
            break


if __name__ == "__main__":
    main()
