
bl_info = {
    "name": "Pivot Menu: Key: '. key'",
    "description": "Manipulator Modes",
    "author": "Antony Riakiotakis, Sebastian Koenig",
    "version": (0, 1, 1),
    "blender": (2, 77, 0),
    "location": ". key",
    "warning": "",
    "wiki_url": "",
    "category": "3d View"
    }

import bpy
from bpy.types import Menu


# Pie Pivot Mode - . key
class VIEW3D_PIE_pivot_of(Menu):
    bl_label = "Pivot"
    bl_idname = "view3d.pivot_of"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.prop(context.space_data, "pivot_point", expand=True)
        if context.active_object and context.active_object.mode == 'OBJECT':
            pie.prop(context.space_data, "use_pivot_point_align", text="Center Points")


classes = (
    VIEW3D_PIE_pivot_of,
    )

addon_keymaps = []


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    wm = bpy.context.window_manager

    if wm.keyconfigs.addon:
        # Align
        km = wm.keyconfigs.addon.keymaps.new(name='Object Non-modal')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'PERIOD', 'PRESS')
        kmi.properties.name = "view3d.pivot_of"
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
