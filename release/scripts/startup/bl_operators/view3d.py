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

# <pep8-80 compliant>

import bpy
from bpy.types import Operator


class VIEW3D_OT_edit_mesh_extrude_individual_move(Operator):
    "Extrude individual elements and move"
    bl_label = "Extrude Individual and Move"
    bl_idname = "view3d.edit_mesh_extrude_individual_move"

    def execute(self, context):
        mesh = context.object.data
        select_mode = context.tool_settings.mesh_select_mode

        totface = mesh.total_face_sel
        totedge = mesh.total_edge_sel
        #~ totvert = mesh.total_vert_sel

        if select_mode[2] and totface == 1:
            bpy.ops.mesh.extrude_region_move('INVOKE_REGION_WIN',
                    TRANSFORM_OT_translate={
                        "constraint_orientation": 'NORMAL',
                        "constraint_axis": (False, False, True)})
        elif select_mode[2] and totface > 1:
            bpy.ops.mesh.extrude_faces_move('INVOKE_REGION_WIN')
        elif select_mode[1] and totedge >= 1:
            bpy.ops.mesh.extrude_edges_move('INVOKE_REGION_WIN')
        else:
            bpy.ops.mesh.extrude_vertices_move('INVOKE_REGION_WIN')

        # ignore return from operators above because they are 'RUNNING_MODAL',
        # and cause this one not to be freed. [#24671]
        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)


class VIEW3D_OT_edit_mesh_extrude_move(Operator):
    "Extrude and move along normals"
    bl_label = "Extrude and Move on Normals"
    bl_idname = "view3d.edit_mesh_extrude_move_normal"

    def execute(self, context):
        mesh = context.object.data

        totface = mesh.total_face_sel
        totedge = mesh.total_edge_sel
        #~ totvert = mesh.total_vert_sel

        if totface >= 1:
            bpy.ops.mesh.extrude_region_move('INVOKE_REGION_WIN',
                    TRANSFORM_OT_translate={
                        "constraint_orientation": 'NORMAL',
                        "constraint_axis": (False, False, True)})
        elif totedge == 1:
            bpy.ops.mesh.extrude_region_move('INVOKE_REGION_WIN',
                    TRANSFORM_OT_translate={
                        "constraint_orientation": 'NORMAL',
                        # not a popular choice, too restrictive for retopo.
                        #~ "constraint_axis": (True, True, False)})
                        "constraint_axis": (False, False, False)})
        else:
            bpy.ops.mesh.extrude_region_move('INVOKE_REGION_WIN')

        # ignore return from operators above because they are 'RUNNING_MODAL',
        # and cause this one not to be freed. [#24671]
        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)
