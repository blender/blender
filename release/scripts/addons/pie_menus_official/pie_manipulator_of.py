
bl_info = {
    "name": "Manipulator Menu: Key: 'Ctrl Space'",
    "description": "Manipulator Modes",
    "author": "Antony Riakiotakis, Sebastian Koenig",
    "version": (0, 1, 1),
    "blender": (2, 77, 0),
    "location": "Ctrl Space",
    "warning": "",
    "wiki_url": "",
    "category": "3d View"
}

import bpy
from bpy.types import (
        Menu,
        Operator,
        )
from bpy.props import (
        EnumProperty,
        )


# Pie Manipulator Mode - Ctrl Space
class VIEW3D_manipulator_set_of(Operator):
    bl_label = "Set Manipulator"
    bl_idname = "view3d.manipulator_set"

    type = EnumProperty(
            name="Type",
            items=(('TRANSLATE', "Translate", "Use the manipulator for movement transformations"),
                   ('ROTATE', "Rotate", "Use the manipulator for rotation transformations"),
                   ('SCALE', "Scale", "Use the manipulator for scale transformations"),
                   ),
            )

    def execute(self, context):
        # show manipulator if user selects an option
        context.space_data.show_manipulator = True
        context.space_data.transform_manipulators = {self.type}

        return {'FINISHED'}


class VIEW3D_PIE_manipulator_of(Menu):
    bl_label = "Manipulator"
    bl_idname = "view3d.manipulator_of"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.operator("view3d.manipulator_set", icon='MAN_TRANS', text="Translate").type = 'TRANSLATE'
        pie.operator("view3d.manipulator_set", icon='MAN_ROT', text="Rotate").type = 'ROTATE'
        pie.operator("view3d.manipulator_set", icon='MAN_SCALE', text="Scale").type = 'SCALE'
        pie.prop(context.space_data, "show_manipulator")


classes = (
    VIEW3D_manipulator_set_of,
    VIEW3D_PIE_manipulator_of,
    )

addon_keymaps = []


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    wm = bpy.context.window_manager

    if wm.keyconfigs.addon:
        # Align
        km = wm.keyconfigs.addon.keymaps.new(name='Object Non-modal')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'SPACE', 'PRESS', ctrl=True)
        kmi.properties.name = "view3d.manipulator_of"
        addon_keymaps.append((km, kmi))


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)

    wm = bpy.context.window_manager
    kc = wm.keyconfigs.addon
    if kc:
        for km, kmi in addon_keymaps:
            km.keymap_items.remove(kmi)
    addon_keymaps.clear()


if __name__ == "__main__":
    register()
