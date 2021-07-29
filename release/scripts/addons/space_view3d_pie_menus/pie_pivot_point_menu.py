# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

bl_info = {
    "name": "Hotkey: ' . key' ",
    "description": "Set Pivot Point Menu",
    "author": "seb_k, meta-androcto",
    "version": (0, 1, 1),
    "blender": (2, 77, 0),
    "location": "3D View",
    "warning": "",
    "wiki_url": "",
    "category": "Pivot Point Pie"
    }

import bpy
from bpy.types import Menu


class VIEW3D_PIE_pivot(Menu):
    bl_label = "Pivot"
    bl_idname = "pie.pivot"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.prop(context.space_data, "pivot_point", expand=True)
        if context.active_object.mode == 'OBJECT':
            pie.prop(context.space_data, "use_pivot_point_align", text="Center Points")


classes = (
    VIEW3D_PIE_pivot,
    )

addon_keymaps = []


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    wm = bpy.context.window_manager
    if wm.keyconfigs.addon:
        # Pivot Point
        km = wm.keyconfigs.addon.keymaps.new(name='3D View Generic', space_type='VIEW_3D')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'PERIOD', 'PRESS')
        kmi.properties.name = "pie.pivot"
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
