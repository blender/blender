# SPDX-FileCopyrightText: 2009-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import sys
import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import RunTest, ParticleSystemSpec, SpecMeshTest


def main():
    test = [
        SpecMeshTest(
            "ParticleSystemTest", "testParticleSystem", "expParticleSystem", [
                ParticleSystemSpec(
                    'Particles',
                    'PARTICLE_SYSTEM',
                    {'render_type': "OBJECT", 'instance_object': bpy.data.objects['Cube']}, 20)
            ],
            threshold=1e-3,
        ),
    ]
    particle_test = RunTest(test)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            particle_test.apply_modifiers = True
            particle_test.do_compare = True
            particle_test.run_all_tests()
            break
        elif cmd == "--run-test":
            particle_test.apply_modifiers = False
            particle_test.do_compare = False
            name = command[i + 1]
            particle_test.run_test(name)
            break


if __name__ == "__main__":
    main()
