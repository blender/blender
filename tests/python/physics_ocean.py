# SPDX-FileCopyrightText: 2009-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import sys

import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import RunTest, ModifierSpec, SpecMeshTest


def main():
    test = [
        # World coordinates of test and expected object should be same.
        SpecMeshTest("PlaneOcean", "testObjPlaneOcean", "expObjPlaneOcean",
                     [ModifierSpec('Ocean', 'OCEAN', {})]),
    ]
    ocean_test = RunTest(test)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            ocean_test.apply_modifiers = True
            ocean_test.do_compare = True
            ocean_test.run_all_tests()
            break
        elif cmd == "--run-test":
            ocean_test.apply_modifiers = False
            ocean_test.do_compare = False
            name = command[i + 1]
            ocean_test.run_test(name)
            break


if __name__ == "__main__":
    main()
