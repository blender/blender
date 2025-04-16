"""
Submenus
++++++++

This menu demonstrates some different functions.
"""
import bpy


class SubMenu(bpy.types.Menu):
    bl_idname = "OBJECT_MT_select_submenu"
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.select_all", text="Select/Deselect All").action = 'TOGGLE'
        layout.operator("object.select_all", text="Inverse").action = 'INVERT'
        layout.operator("object.select_random", text="Random")

        # Access this operator as a sub-menu.
        layout.operator_menu_enum("object.select_by_type", "type", text="Select All by Type...")

        layout.separator()

        # Expand each operator option into this menu.
        layout.operator_enum("object.light_add", "type")

        layout.separator()

        # Use existing menu.
        layout.menu("VIEW3D_MT_transform")


bpy.utils.register_class(SubMenu)

# Test call to display immediately.
bpy.ops.wm.call_menu(name="OBJECT_MT_select_submenu")
