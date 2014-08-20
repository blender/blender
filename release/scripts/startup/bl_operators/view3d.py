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
from bpy.props import BoolProperty


class VIEW3D_OT_edit_mesh_extrude_individual_move(Operator):
    "Extrude individual elements and move"
    bl_label = "Extrude Individual and Move"
    bl_idname = "view3d.edit_mesh_extrude_individual_move"

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj is not None and obj.mode == 'EDIT')

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

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj is not None and obj.mode == 'EDIT')

    @staticmethod
    def extrude_region(context, use_vert_normals):
        mesh = context.object.data

        totface = mesh.total_face_sel
        totedge = mesh.total_edge_sel
        #~ totvert = mesh.total_vert_sel

        if totface >= 1:
            if use_vert_normals:
                bpy.ops.mesh.extrude_region_shrink_fatten('INVOKE_REGION_WIN',
                        TRANSFORM_OT_shrink_fatten={})
            else:
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

    def execute(self, context):
        return VIEW3D_OT_edit_mesh_extrude_move.extrude_region(context, False)

    def invoke(self, context, event):
        return self.execute(context)


class VIEW3D_OT_edit_mesh_extrude_shrink_fatten(Operator):
    "Extrude and move along individual normals"
    bl_label = "Extrude and Move on Individual Normals"
    bl_idname = "view3d.edit_mesh_extrude_move_shrink_fatten"

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj is not None and obj.mode == 'EDIT')

    def execute(self, context):
        return VIEW3D_OT_edit_mesh_extrude_move.extrude_region(context, True)

    def invoke(self, context, event):
        return self.execute(context)


class VIEW3D_OT_select_or_deselect_all(Operator):
    "Select element under the mouse, deselect everything is there's nothing under the mouse"
    bl_label = "Select or Deselect All"
    bl_idname = "view3d.select_or_deselect_all"
    bl_options = {'UNDO'}

    extend = BoolProperty(
            name="Extend",
            description="Extend selection instead of deselecting everything first",
            default=False,
            )

    toggle = BoolProperty(
            name="Toggle",
            description="Toggle the selection",
            default=False,
            )

    deselect = BoolProperty(
            name="Deselect",
            description="Remove from selection",
            default=False,
            )

    center = BoolProperty(
            name="Center",
            description="Use the object center when selecting, in editmode used to extend object selection",
            default=False,
            )

    enumerate = BoolProperty(
            name="Enumerate",
            description="List objects under the mouse (object mode only)",
            default=False,
            )

    object = BoolProperty(
            name="Object",
            description="Use object selection (editmode only)",
            default=False,
            )

    @classmethod
    def poll(cls, context):
        active_object = context.active_object
        if active_object:
            return active_object.mode in {'EDIT', 'OBJECT', 'POSE'}
        return True

    def invoke(self, context, event):
        x = event.mouse_region_x
        y = event.mouse_region_y

        if self.extend is False and self.toggle is False and self.deselect is False:
            active_object = context.active_object

            if active_object:
                if active_object.mode == 'EDIT':
                    if active_object.type == 'MESH':
                        bpy.ops.mesh.select_all(action='DESELECT')
                    elif active_object.type == 'CURVE':
                        bpy.ops.curve.select_all(action='DESELECT')
                    elif active_object.type == 'SURFACE':
                        bpy.ops.curve.select_all(action='DESELECT')
                    elif active_object.type == 'LATTICE':
                        bpy.ops.lattice.select_all(action='DESELECT')
                    elif active_object.type == 'META':
                        bpy.ops.mball.select_all(action='DESELECT')
                    elif active_object.type == 'ARMATURE':
                        bpy.ops.armature.select_all(action='DESELECT')
                elif active_object.mode == 'POSE':
                    bpy.ops.pose.select_all(action='DESELECT')
                elif active_object.mode == 'PARTICLE_EDIT':
                    bpy.ops.particle.select_all(action='DESELECT')
                else:
                    bpy.ops.object.select_all(action='DESELECT')
            else:
                bpy.ops.object.select_all(action='DESELECT')

        return bpy.ops.view3d.select(extend=self.extend,
                                     deselect=self.deselect,
                                     toggle=self.toggle,
                                     center=self.center,
                                     enumerate=self.enumerate,
                                     object=self.object,
                                     location=(x, y))
