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

        SpecMeshTest("SoftBodySimple", "testSoftBody", "expectedSoftBody",
                     [ModifierSpec('Softbody', 'SOFT_BODY',
                                   {'settings': {'use_goal': False, 'bend': 8, 'pull': 0.8, 'push': 0.8}},
                                   45)]),
    ]
    soft_body_test = RunTest(test)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            soft_body_test.apply_modifiers = True
            soft_body_test.do_compare = True
            soft_body_test.run_all_tests()
            break
        elif cmd == "--run-test":
            soft_body_test.apply_modifiers = False
            soft_body_test.do_compare = False
            name = command[i + 1]
            soft_body_test.run_test(name)
            break


if __name__ == "__main__":
    main()
