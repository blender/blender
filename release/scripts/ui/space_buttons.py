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
import bpy


class Buttons_HT_header(bpy.types.Header):
    bl_space_type = 'PROPERTIES'

    def draw(self, context):
        layout = self.layout

        so = context.space_data
        scene = context.scene

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)
            sub.menu("Buttons_MT_view", text="View")

        row = layout.row()
        row.prop(so, "buttons_context", expand=True, text="")
        row.prop(scene, "current_frame")


class Buttons_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        so = context.space_data

        col = layout.column()
        col.prop(so, "panel_alignment", expand=True)


classes = [
    Buttons_HT_header,
    Buttons_MT_view]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)

def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)
