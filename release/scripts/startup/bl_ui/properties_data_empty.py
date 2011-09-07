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
from bpy.types import Panel


class DataButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return (context.object and context.object.type == 'EMPTY')


class DATA_PT_empty(DataButtonsPanel, Panel):
    bl_label = "Empty"

    def draw(self, context):
        layout = self.layout

        ob = context.object

        layout.prop(ob, "empty_draw_type", text="Display")

        if ob.empty_draw_type == 'IMAGE':
            layout.template_ID(ob, "data", open="image.open", unlink="image.unlink")

            layout.prop(ob, "color", text="Transparency", index=3, slider=True)
            row = layout.row(align=True)
            row.prop(ob, "empty_image_offset", text="Offset X", index=0)
            row.prop(ob, "empty_image_offset", text="Offset Y", index=1)

        layout.prop(ob, "empty_draw_size", text="Size")

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
