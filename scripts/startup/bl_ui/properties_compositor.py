# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import (
    Panel, Menu
)


class SCENE_MT_add_compositor_effect(Menu):
    bl_label = "Add Effect"
    bl_options = {'SEARCH_ON_KEY_PRESS'}

    def draw(self, context):
        layout = self.layout

        if layout.operator_context == 'EXEC_REGION_WIN':
            layout.operator_context = 'INVOKE_REGION_WIN'
            layout.operator(
                "WM_OT_search_single_menu",
                text="Search...",
                icon='VIEWZOOM',
            ).menu_idname = "SCENE_MT_add_compositor_effect"
            layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.menu_contents("SCENE_MT_add_compositor_effect_root_catalogs")
        layout.separator()
        layout.operator("scene.add_compositor_effect", text="Effect", icon='NODE_COMPOSITING')


class SCENE_PT_compositor_effects(Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "compositor"
    bl_label = "Effects"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout
        layout.operator("wm.call_menu", text="Add Effect", icon='ADD').name = "SCENE_MT_add_compositor_effect"
        layout.template_scene_compositor_effects()


classes = (
    SCENE_PT_compositor_effects,
    SCENE_MT_add_compositor_effect,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
