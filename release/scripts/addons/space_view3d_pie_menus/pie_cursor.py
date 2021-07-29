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
    "name": "Hotkey: 'Shift S'",
    "description": "Cursor Menu",
    "author": "pitiwazou, meta-androcto",
    "version": (0, 1, 0),
    "blender": (2, 77, 0),
    "location": "3D View",
    "warning": "",
    "wiki_url": "",
    "category": "Cursor Pie"
    }

import bpy
from bpy.types import (
        Menu,
        Operator,
        )


# SnapCursSelToCenter1 thanks to Isaac Weaver (wisaac) D1963
class Snap_CursSelToCenter1(Operator):
    bl_idname = "view3d.snap_cursor_selected_to_center1"
    bl_label = "Snap Cursor & Selection to Center"
    bl_description = ("Snap 3D cursor and selected objects to the center \n"
                     "Works only in Object Mode")

    @classmethod
    def poll(cls, context):
        return (context.area.type == "VIEW_3D" and context.mode == "OBJECT")

    def execute(self, context):
        context.space_data.cursor_location = (0, 0, 0)
        for obj in context.selected_objects:
            obj.location = (0, 0, 0)

        return {'FINISHED'}


# Origin/Pivot menu1  - Shift + S
class Snap_CursorMenu(Menu):
    bl_idname = "snap.cursormenu"
    bl_label = "Cursor Menu"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # 4 - LEFT
        pie.operator("view3d.snap_selected_to_cursor", text="Selection to Cursor",
                     icon='CLIPUV_HLT').use_offset = False
        # 6 - RIGHT
        pie.operator("view3d.snap_selected_to_cursor",
                    text="Selection to Cursor (Offset)", icon='CURSOR').use_offset = True
        # 2 - BOTTOM
        pie.operator("view3d.snap_cursor_selected_to_center1",
                    text="Selected & Cursor to Center", icon='ALIGN')
        # 8 - TOP
        pie.operator("view3d.snap_cursor_to_center", text="Cursor to Center", icon='CLIPUV_DEHLT')
        # 7 - TOP - LEFT
        pie.operator("view3d.snap_cursor_to_selected", text="Cursor to Selected", icon='ROTACTIVE')
        # 9 - TOP - RIGHT
        pie.operator("view3d.snap_cursor_to_active", text="Cursor to Active", icon='BBOX')
        # 1 - BOTTOM - LEFT
        pie.operator("view3d.snap_selected_to_grid", text="Selection to Grid", icon='GRID')
        # 3 - BOTTOM - RIGHT
        pie.operator("view3d.snap_cursor_to_grid", text="Cursor to Grid", icon='GRID')


classes = (
    Snap_CursorMenu,
    Snap_CursSelToCenter1,
    )

addon_keymaps = []


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    wm = bpy.context.window_manager
    if wm.keyconfigs.addon:
        # Origin/Pivot
        km = wm.keyconfigs.addon.keymaps.new(name='3D View Generic', space_type='VIEW_3D')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'S', 'PRESS', shift=True)
        kmi.properties.name = "snap.cursormenu"
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
