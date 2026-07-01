# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""
import modules.ui_test_utils as ui


def _test_panel():
    from bpy.types import Panel

    class TEST_PT_panel(Panel):
        bl_label = "Test Panel"
        bl_idname = "TEST_PT_panel"
        bl_category = 'Test Panel'
        bl_space_type = 'TEXT_EDITOR'
        bl_region_type = 'UI'

        def draw(self, context):
            self.layout.operator("test.operator")

    return TEST_PT_panel


def _test_operator():
    from bpy.types import Operator

    class TEST_PT_operator(Operator):
        bl_label = "Test Operator"
        bl_idname = "test.operator"
        bl_category = 'Test Operator'

        def execute(self, context):
            return {'FINISHED'}

    return TEST_PT_operator


def unregister_referenced_type():
    e, _t, _window = ui.test_window()

    test_panel = _test_panel()
    test_operator = _test_operator()
    import bpy

    bpy.utils.register_class(test_panel)
    bpy.utils.register_class(test_operator)

    yield

    # Show the popup with a 'test.operator' reference
    bpy.ops.wm.call_panel(name=test_panel.bl_idname, keep_open=True)

    yield

    bpy.utils.unregister_class(test_operator)

    # Let popup be refreshed
    yield

    # If the reference is not removed activating the button should crash
    yield e.ret()
