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


class GreasePencilPanel():
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    # bl_region_type = 'TOOLS'
    bl_label = "Grease Pencil"

    @staticmethod
    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)

        row = col.row(align=True)
        row.operator("gpencil.draw", text="Draw").mode = 'DRAW'
        row.operator("gpencil.draw", text="Line").mode = 'DRAW_STRAIGHT'

        row = col.row(align=True)
        row.operator("gpencil.draw", text="Poly").mode = 'DRAW_POLY'
        row.operator("gpencil.draw", text="Erase").mode = 'ERASER'

        row = col.row(align=True)
        row.prop(context.tool_settings, "use_grease_pencil_sessions")

