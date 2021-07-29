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

"""Replace default list-style menu for transform orientations with a pie."""

bl_info = {
    "name": "Hotkey: 'Alt + Spacebar'",
    "author": "Italic_",
    "version": (1, 1, 0),
    "blender": (2, 77, 0),
    "description": "Set Transform Orientations",
    "location": "3D View",
    "category": "Orientation Pie"}

import bpy
from bpy.types import (
        Menu,
        Operator,
        )
from bpy.props import (
        StringProperty,
        )


class OrientPoll(Operator):
    bl_idname = "pie.orientation"
    bl_label = "Orientation Poll"
    bl_options = {'INTERNAL'}

    space = StringProperty()

    @classmethod
    def poll(cls, context):
        return context.space_data.type == "VIEW_3D"

    def execute(self, context):
        context.space_data.transform_orientation = self.space
        return {'FINISHED'}


class OrientPie(Menu):
    bl_label = "Transform Orientation"
    bl_idname = "pie.orient"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        view = context.space_data

        pie.operator("pie.orientation", text="Global").space = 'GLOBAL'
        pie.operator("pie.orientation", text="Local").space = 'LOCAL'
        pie.operator("pie.orientation", text="Gimbal").space = 'GIMBAL'

        # XXX: Display only custom orientations
        pie = pie.box()
        pie.prop(view, "transform_orientation", text="")
        pie = layout.menu_pie()

        pie.operator("pie.orientation", text="Normal").space = 'NORMAL'
        pie.operator("pie.orientation", text="View").space = 'VIEW'


addon_keymaps = []

classes = (
    OrientPie,
    OrientPoll
    )


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    wm = bpy.context.window_manager
    if wm.keyconfigs.addon:
        # Manipulators
        km = wm.keyconfigs.addon.keymaps.new(name='3D View Generic', space_type='VIEW_3D')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'SPACE', 'PRESS', alt=True)
        kmi.properties.name = "pie.orient"
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
