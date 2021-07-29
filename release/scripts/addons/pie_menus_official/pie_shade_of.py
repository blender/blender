
bl_info = {
    "name": "Shade Menu: Key: 'Z key'",
    "description": "View Modes",
    "author": "Antony Riakiotakis, Sebastian Koenig",
    "version": (0, 1, 1),
    "blender": (2, 77, 0),
    "location": "Z key",
    "warning": "",
    "wiki_url": "",
    "category": "3d View"
    }

import bpy
from bpy.types import Menu


# Pie Shade Mode - Z
class VIEW3D_PIE_shade_of(Menu):
    bl_label = "Shade"
    bl_idname = "pie.shade_of"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.prop(context.space_data, "viewport_shade", expand=True)

        if context.active_object:
            if(context.mode == 'EDIT_MESH'):
                pie.operator("MESH_OT_faces_shade_smooth")
                pie.operator("MESH_OT_faces_shade_flat")
            else:
                pie.operator("OBJECT_OT_shade_smooth")
                pie.operator("OBJECT_OT_shade_flat")


classes = (
    VIEW3D_PIE_shade_of,
    )

addon_keymaps = []


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    wm = bpy.context.window_manager

    if wm.keyconfigs.addon:
        # Align
        km = wm.keyconfigs.addon.keymaps.new(name='Object Non-modal')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'Z', 'PRESS')
        kmi.properties.name = "pie.shade_of"
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
