# gpl author: Stanislav Blinov

bl_info = {
    "name": "V/E/F Context Menu",
    "author": "Stanislav Blinov",
    "version": (1, 0, 1),
    "blender": (2, 78, 0),
    "description": "Vert Edge Face Double Right Click Edit Mode",
    "category": "Mesh",
}

import bpy
import bpy_extras
from bpy.types import (
        Menu,
        Operator,
        )


class MESH_MT_CombinedMenu(Menu):
    bl_idname = "mesh.addon_combined_component_menu"
    bl_label = "Components"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def draw(self, context):
        layout = self.layout

        mode = context.tool_settings.mesh_select_mode
        if mode[0]:
            layout.menu("VIEW3D_MT_edit_mesh_vertices")
        if mode[1]:
            layout.menu("VIEW3D_MT_edit_mesh_edges")
        if mode[2]:
            layout.menu("VIEW3D_MT_edit_mesh_faces")


class MESH_OT_CallContextMenu(Operator):
    bl_idname = "mesh.addon_call_context_menu"
    bl_label = "Context Menu"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def execute(self, context):
        mode = context.tool_settings.mesh_select_mode
        num = sum(int(m) for m in mode)
        if num == 1:
            if mode[0]:
                return bpy.ops.wm.call_menu(name="VIEW3D_MT_edit_mesh_vertices")
            if mode[1]:
                return bpy.ops.wm.call_menu(name="VIEW3D_MT_edit_mesh_edges")
            if mode[2]:
                return bpy.ops.wm.call_menu(name="VIEW3D_MT_edit_mesh_faces")
        else:
            return bpy.ops.wm.call_menu(name=MESH_MT_CombinedMenu.bl_idname)


classes = (
    MESH_MT_CombinedMenu,
    MESH_OT_CallContextMenu,
    )


KEYMAPS = (
    # First, keymap identifiers (last bool is True for modal km).
    (("3D View", "VIEW_3D", "WINDOW", False), (
    # Then a tuple of keymap items, defined by a dict of kwargs
    # for the km new func, and a tuple of tuples (name, val)
    # for ops properties, if needing non-default values.
        ({"idname": MESH_OT_CallContextMenu.bl_idname, "type": 'RIGHTMOUSE', "value": 'DOUBLE_CLICK'},
         ()),
    )),
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy_extras.keyconfig_utils.addon_keymap_register(bpy.context.window_manager, KEYMAPS)


def unregister():
    bpy_extras.keyconfig_utils.addon_keymap_unregister(bpy.context.window_manager, KEYMAPS)

    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
