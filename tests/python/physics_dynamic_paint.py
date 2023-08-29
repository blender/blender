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

        SpecMeshTest("DynamicPaintSimple", "testObjDynamicPaintPlane", "expObjDynamicPaintPlane",
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
