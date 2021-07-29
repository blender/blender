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

# By CoDEmanX

import bpy
from bpy.types import Operator
from bpy.props import (
        EnumProperty,
        BoolProperty,
        )


class DATA_OP_facetype_select(Operator):
    bl_idname = "data.facetype_select"
    bl_label = "Select by face type"
    bl_description = "Select all faces of a certain type"
    bl_options = {'REGISTER', 'UNDO'}

    face_type = EnumProperty(
            name="Select faces:",
            items=(("3", "Triangles", "Faces made up of 3 vertices"),
                   ("4", "Quads", "Faces made up of 4 vertices"),
                   ("5", "Ngons", "Faces made up of 5 and more vertices")),
            default="5"
            )
    extend = BoolProperty(
            name="Extend",
            description="Extend Selection",
            default=False
            )

    @classmethod
    def poll(cls, context):
        return context.active_object is not None and context.active_object.type == 'MESH'

    def execute(self, context):
        try:
            bpy.ops.object.mode_set(mode='EDIT')

            if not self.extend:
                bpy.ops.mesh.select_all(action='DESELECT')

            context.tool_settings.mesh_select_mode = (False, False, True)

            if self.face_type == "3":
                bpy.ops.mesh.select_face_by_sides(number=3, type='EQUAL')
            elif self.face_type == "4":
                bpy.ops.mesh.select_face_by_sides(number=4, type='EQUAL')
            else:
                bpy.ops.mesh.select_face_by_sides(number=4, type='GREATER')

            return {'FINISHED'}

        except Exception as e:
            print("\n[Select by face type]\nOperator: data.facetype_select\nERROR: %s\n" % e)
            self.report({'WARNING'},
                        "Face selection could not be performed (Check the console for more info)")

            return {'CANCELLED'}
