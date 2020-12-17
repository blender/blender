# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import os
import sys

import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import RunTest, ParticleSystemSpec, MeshTest


def main():
    test = [
        MeshTest("ParticleSystemTest", "testParticleSystem", "expParticleSystem",
         [ParticleSystemSpec('Particles', 'PARTICLE_SYSTEM', {'render_type': "OBJECT",
          'instance_object': bpy.data.objects['Cube']}, 20)], threshold=1e-3),

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
