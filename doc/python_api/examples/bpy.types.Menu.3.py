"""
Preset Menus
++++++++++++

Preset menus are simply a convention that uses a menu sub-class
to perform the common task of managing presets.

This example shows how you can add a preset menu.

This example uses the object display options,
however you can use properties defined by your own scripts too.
"""

import bpy
from bpy.types import Operator, Menu
from bl_operators.presets import AddPresetBase


class OBJECT_MT_display_presets(Menu):
    bl_label = "Object Display Presets"
    preset_subdir = "object/display"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class AddPresetObjectDisplay(AddPresetBase, Operator):
    '''Add a Object Display Preset'''
    bl_idname = "camera.object_display_preset_add"
    bl_label = "Add Object Display Preset"
    preset_menu = "OBJECT_MT_display_presets"

    # variable used for all preset values
    preset_defines = [
        "obj = bpy.context.object"
    ]

    # properties to store in the preset
    preset_values = [
        "obj.display_type",
        "obj.show_bounds",
        "obj.display_bounds_type",
        "obj.show_name",
        "obj.show_axis",
        "obj.show_wire",
    ]

    # where to store the preset
    preset_subdir = "object/display"


# Display into an existing panel
def panel_func(self, context):
    layout = self.layout

    row = layout.row(align=True)
    row.menu(OBJECT_MT_display_presets.__name__, text=OBJECT_MT_display_presets.bl_label)
    row.operator(AddPresetObjectDisplay.bl_idname, text="", icon='ZOOM_IN')
    row.operator(AddPresetObjectDisplay.bl_idname, text="", icon='ZOOM_OUT').remove_active = True


classes = (
    OBJECT_MT_display_presets,
    AddPresetObjectDisplay,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.OBJECT_PT_display.prepend(panel_func)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)
    bpy.types.OBJECT_PT_display.remove(panel_func)


if __name__ == "__main__":
    register()
