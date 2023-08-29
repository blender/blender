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

        SpecMeshTest("ParticleInstanceSimple", "testParticleInstance", "expectedParticleInstance",
                     [ModifierSpec('ParticleInstance', 'PARTICLE_INSTANCE', {'object': bpy.data.objects['Cube']})],
                     threshold=1e-3),

    ]
    particle_instance_test = RunTest(test)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            particle_instance_test.apply_modifiers = True
            particle_instance_test.do_compare = True
            particle_instance_test.run_all_tests()
            break
        elif cmd == "--run-test":
            particle_instance_test.apply_modifiers = False
            particle_instance_test.do_compare = False
            name = command[i + 1]
            particle_instance_test.run_test(name)
            break


if __name__ == "__main__":
    main()
