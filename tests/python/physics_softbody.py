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
from modules.mesh_test import ModifierTest, PhysicsSpec


def main():
    test = [
      ["testSoftBody", "expectedSoftBody",
       [PhysicsSpec('Softbody', 'SOFT_BODY', {'use_goal': False, 'bend': 8, 'pull': 0.8, 'push': 0.8}, 45)]],
    ]
    softBody_test = ModifierTest(test)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            softBody_test.apply_modifiers = True
            softBody_test.run_all_tests()
            break
        elif cmd == "--run-test":
            softBody_test.apply_modifiers = False
            index = int(command[i + 1])
            softBody_test.run_test(index)
            break


if __name__ == "__main__":
    main()
