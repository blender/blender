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

narrowui = 180


class DataButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    def poll(self, context):
        return (context.object and context.object.type == 'EMPTY')


class DATA_PT_empty(DataButtonsPanel):
    bl_label = "Empty"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.prop(ob, "empty_draw_type", text="Display")
        else:
            layout.prop(ob, "empty_draw_type", text="")

        layout.prop(ob, "empty_draw_size", text="Size")

bpy.types.register(DATA_PT_empty)
