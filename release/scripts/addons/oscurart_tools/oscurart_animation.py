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
from mathutils import Matrix

# ---------------------------QUICK PARENT------------------


def DefQuickParent(inf, out):
    ob = bpy.context.object
    if ob.type == "ARMATURE":
        target = [object for object in bpy.context.selected_objects if object != ob][0]
        ob = bpy.context.active_pose_bone if bpy.context.object.type == 'ARMATURE' else bpy.context.object
        target.select = False
        bpy.context.scene.frame_set(frame=bpy.context.scene.quick_animation_in)
        a = Matrix(target.matrix_world)
        a.invert()
        i = Matrix(ob.matrix)
        for frame in range(inf, out):
            bpy.context.scene.frame_set(frame=frame)
            ob.matrix = target.matrix_world * a * i
            bpy.ops.anim.keyframe_insert(type="LocRotScale")
    else:
        target = [object for object in bpy.context.selected_objects if object != ob][0]
        ob = bpy.context.active_pose_bone if bpy.context.object.type == 'ARMATURE' else bpy.context.object
        target.select = False
        bpy.context.scene.frame_set(frame=bpy.context.scene.quick_animation_in)
        a = Matrix(target.matrix_world)
        a.invert()
        i = Matrix(ob.matrix_world)
        for frame in range(inf, out):
            bpy.context.scene.frame_set(frame=frame)
            ob.matrix_world = target.matrix_world * a * i
            bpy.ops.anim.keyframe_insert(type="LocRotScale")


class QuickParent(bpy.types.Operator):
    """Creates a parent from one object to other in a selected frame range"""
    bl_idname = "anim.quick_parent_osc"
    bl_label = "Quick Parent"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        DefQuickParent(
            bpy.context.scene.quick_animation_in,
            bpy.context.scene.quick_animation_out,
        )
        return {'FINISHED'}
