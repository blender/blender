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
    "name": "Hotkey: 'X'",
    "description": "Edit mode V/E/F Delete Modes",
    "author": "pitiwazou, meta-androcto",
    "version": (0, 1, 0),
    "blender": (2, 77, 0),
    "location": "Mesh Edit Mode",
    "warning": "",
    "wiki_url": "",
    "category": "Edit Delete Pie"
    }

import bpy
from bpy.types import Menu


# Pie Delete - X
class PieDelete(Menu):
    bl_idname = "pie.delete"
    bl_label = "Pie Delete"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # 4 - LEFT
        pie.operator("mesh.delete", text="Delete Vertices", icon='VERTEXSEL').type = 'VERT'
        # 6 - RIGHT
        pie.operator("mesh.delete", text="Delete Faces", icon='FACESEL').type = 'FACE'
        # 2 - BOTTOM
        pie.operator("mesh.delete", text="Delete Edges", icon='EDGESEL').type = 'EDGE'
        # 8 - TOP
        pie.operator("mesh.dissolve_edges", text="Dissolve Edges", icon='SNAP_EDGE')
        # 7 - TOP - LEFT
        pie.operator("mesh.dissolve_verts", text="Dissolve Vertices", icon='SNAP_VERTEX')
        # 9 - TOP - RIGHT
        pie.operator("mesh.dissolve_faces", text="Dissolve Faces", icon='SNAP_FACE')
        # 1 - BOTTOM - LEFT
        box = pie.split().column()
        box.operator("mesh.dissolve_limited", text="Limited Dissolve", icon='STICKY_UVS_LOC')
        box.operator("mesh.delete_edgeloop", text="Delete Edge Loops", icon='BORDER_LASSO')
        box.operator("mesh.edge_collapse", text="Edge Collapse", icon='UV_EDGESEL')
        # 3 - BOTTOM - RIGHT
        box = pie.split().column()
        box.operator("mesh.delete", text="Only Edge & Faces", icon='SPACE2').type = 'EDGE_FACE'
        box.operator("mesh.delete", text="Only Faces", icon='UV_FACESEL').type = 'ONLY_FACE'
        box.operator("mesh.remove_doubles", text="Remove Doubles", icon='ORTHO')


classes = (
    PieDelete,
    )


addon_keymaps = []


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    wm = bpy.context.window_manager
    if wm.keyconfigs.addon:
        # Delete
        km = wm.keyconfigs.addon.keymaps.new(name='Mesh')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'X', 'PRESS')
        kmi.properties.name = "pie.delete"
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
