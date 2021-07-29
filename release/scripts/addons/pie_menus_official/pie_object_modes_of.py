
bl_info = {
    "name": "Mode Set: Key: 'Tab'",
    "description": "Object Modes",
    "author": "Antony Riakiotakis, Sebastian Koenig",
    "version": (0, 1, 1),
    "blender": (2, 77, 0),
    "location": "Tab key",
    "warning": "",
    "wiki_url": "",
    "category": "3d View"
    }

import bpy
from bpy.types import Menu


# Pie Object Mode - Tab
class VIEW3D_PIE_object_mode_of(Menu):
    bl_idname = "pie.object_mode_of"
    bl_label = "Mode"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.operator_enum("OBJECT_OT_mode_set", "mode")


classes = (
    VIEW3D_PIE_object_mode_of,
    )

addon_keymaps = []


def register():

    for cls in classes:
        bpy.utils.register_class(cls)
    wm = bpy.context.window_manager

    if wm.keyconfigs.addon:
        # Object Modes
        km = wm.keyconfigs.addon.keymaps.new(name='Object Non-modal')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'TAB', 'PRESS')
        kmi.properties.name = "pie.object_mode_of"
        addon_keymaps.append((km, kmi))

        # Grease Pencil Edit Modes
        km = wm.keyconfigs.addon.keymaps.new(name='Grease Pencil Stroke Edit Mode')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'TAB', 'PRESS')
        kmi.properties.name = "pie.object_mode_of"
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
