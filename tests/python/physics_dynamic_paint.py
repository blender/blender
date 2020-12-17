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
from modules.mesh_test import RunTest, ModifierSpec, MeshTest


def main():
    test = [

        MeshTest("DynamicPaintSimple", "testObjDynamicPaintPlane", "expObjDynamicPaintPlane",
                 [ModifierSpec('dynamic_paint', 'DYNAMIC_PAINT',
                               {'ui_type': 'CANVAS',
                                'canvas_settings': {'canvas_surfaces': {'surface_type': 'WAVE', 'frame_end': 15}}},
                               15)]),

    ]
    dynamic_paint_test = RunTest(test)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            dynamic_paint_test.apply_modifiers = True
            dynamic_paint_test.do_compare = True
            dynamic_paint_test.run_all_tests()
            break
        elif cmd == "--run-test":
            dynamic_paint_test.apply_modifiers = False
            dynamic_paint_test.do_compare = False
            name = command[i + 1]
            dynamic_paint_test.run_test(name)
            break


if __name__ == "__main__":
    main()
