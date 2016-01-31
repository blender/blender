"""
Preset Menus
++++++++++++

Preset menus are simply a convention that uses a menu sub-class
to perform the common task of managing presets.

This example shows how you can add a preset menu.

This example uses the object draw options,
however you can use properties defined by your own scripts too.
"""

import bpy
from bpy.types import Operator, Menu
from bl_operators.presets import AddPresetBase


class OBJECT_MT_draw_presets(Menu):
    bl_label = "Object Draw Presets"
    preset_subdir = "object/draw"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class AddPresetObjectDraw(AddPresetBase, Operator):
    '''Add a Object Draw Preset'''
    bl_idname = "camera.object_draw_preset_add"
    bl_label = "Add Object Draw Preset"
    preset_menu = "OBJECT_MT_draw_presets"

    # variable used for all preset values
    preset_defines = [
        "obj = bpy.context.object"
        ]

    # properties to store in the preset
    preset_values = [
        "obj.draw_type",
        "obj.show_bounds",
        "obj.draw_bounds_type",
        "obj.show_name",
        "obj.show_axis",
        "obj.show_wire",
        ]

    # where to store the preset
    preset_subdir = "object/draw"


# Draw into an existing panel
def panel_func(self, context):
    layout = self.layout

    row = layout.row(align=True)
    row.menu(OBJECT_MT_draw_presets.__name__, text=OBJECT_MT_draw_presets.bl_label)
    row.operator(AddPresetObjectDraw.bl_idname, text="", icon='ZOOMIN')
    row.operator(AddPresetObjectDraw.bl_idname, text="", icon='ZOOMOUT').remove_active = True


classes = (
    OBJECT_MT_draw_presets,
    AddPresetObjectDraw,
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
