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
from bpy.props import IntProperty
from bpy.props import EnumProperty


class CopyRigidbodySettings(Operator):
    '''Copy Rigid Body settings from active object to selected'''
    bl_idname = "rigidbody.object_settings_copy"
    bl_label = "Copy Rigidbody Settings"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = bpy.context.object
        return (obj and obj.rigid_body)

    def execute(self, context):
        obj = context.object
        scn = context.scene

        # deselect all but mesh objects
        for o in context.selected_objects:
            if o.type != 'MESH':
                o.select = False

        sel = context.selected_objects
        if sel:
            # add selected objects to active one groups and recalculate
            bpy.ops.group.objects_add_active()
            scn.frame_set(scn.frame_current)

            # copy settings
            for o in sel:
                if o.rigid_body is None:
                    continue
                
                o.rigid_body.type = obj.rigid_body.type
                o.rigid_body.kinematic = obj.rigid_body.kinematic
                o.rigid_body.mass = obj.rigid_body.mass
                o.rigid_body.collision_shape = obj.rigid_body.collision_shape
                o.rigid_body.use_margin = obj.rigid_body.use_margin
                o.rigid_body.collision_margin = obj.rigid_body.collision_margin
                o.rigid_body.friction = obj.rigid_body.friction
                o.rigid_body.restitution = obj.rigid_body.restitution
                o.rigid_body.use_deactivation = obj.rigid_body.use_deactivation
                o.rigid_body.start_deactivated = obj.rigid_body.start_deactivated
                o.rigid_body.deactivate_linear_velocity = obj.rigid_body.deactivate_linear_velocity
                o.rigid_body.deactivate_angular_velocity = obj.rigid_body.deactivate_angular_velocity
                o.rigid_body.linear_damping = obj.rigid_body.linear_damping
                o.rigid_body.angular_damping = obj.rigid_body.angular_damping
                o.rigid_body.collision_groups = obj.rigid_body.collision_groups

        return {'FINISHED'}


class BakeToKeyframes(Operator):
    '''Bake rigid body transformations of selected objects to keyframes'''
    bl_idname = "rigidbody.bake_to_keyframes"
    bl_label = "Bake To Keyframes"
    bl_options = {'REGISTER', 'UNDO'}

    frame_start = IntProperty(
            name="Start Frame",
            description="Start frame for baking",
            min=0, max=300000,
            default=1,
            )
    frame_end = IntProperty(
            name="End Frame",
            description="End frame for baking",
            min=1, max=300000,
            default=250,
            )
    step = IntProperty(
            name="Frame Step",
            description="Frame Step",
            min=1, max=120,
            default=1,
            )

    @classmethod
    def poll(cls, context):
        obj = bpy.context.object
        return (obj and obj.rigid_body)

    def execute(self, context):
        bake = []
        objs = []
        scene = bpy.context.scene
        frame_orig = scene.frame_current
        frames = list(range(self.frame_start, self.frame_end + 1, self.step))

        # filter objects selection
        for ob in bpy.context.selected_objects:
            if not ob.rigid_body or ob.rigid_body.type != 'ACTIVE':
                ob.select = False

        objs = bpy.context.selected_objects

        if objs:
            # store transformation data
            for f in list(range(self.frame_start, self.frame_end + 1)):
                scene.frame_set(f)
                if f in frames:
                    mat = {}
                    for i, ob in enumerate(objs):
                        mat[i] = ob.matrix_world.copy()
                    bake.append(mat)

            # apply transformations as keyframes
            for i, f in enumerate(frames):
                scene.frame_set(f)
                ob_prev = objs[0]
                for j, ob in enumerate(objs):
                    mat = bake[i][j]

                    ob.location = mat.to_translation()

                    rot_mode = ob.rotation_mode
                    if rot_mode == 'QUATERNION':
                        ob.rotation_quaternion = mat.to_quaternion()
                    elif rot_mode == 'AXIS_ANGLE':
                        # this is a little roundabout but there's no better way right now
                        aa = mat.to_quaternion().to_axis_angle()
                        ob.rotation_axis_angle = (aa[1], ) + aa[0][:]
                    else: # euler
                        # make sure euler rotation is compatible to previous frame
                        ob.rotation_euler = mat.to_euler(rot_mode, ob_prev.rotation_euler)

                    ob_prev = ob

                bpy.ops.anim.keyframe_insert(type='BUILTIN_KSI_LocRot', confirm_success=False)

            # remove baked objects from simulation
            bpy.ops.rigidbody.objects_remove()

            # clean up keyframes
            for ob in objs:
                action = ob.animation_data.action
                for fcu in action.fcurves:
                    keyframe_points = fcu.keyframe_points
                    i = 1
                    # remove unneeded keyframes
                    while i < len(keyframe_points) - 1:
                        val_prev = keyframe_points[i - 1].co[1]
                        val_next = keyframe_points[i + 1].co[1]
                        val = keyframe_points[i].co[1]

                        if abs(val - val_prev) + abs(val - val_next) < 0.0001:
                            keyframe_points.remove(keyframe_points[i])
                        else:
                            i += 1
                    # use linear interpolation for better visual results
                    for keyframe in keyframe_points:
                        keyframe.interpolation = 'LINEAR'

            # return to the frame we started on
            scene.frame_set(frame_orig)

        return {'FINISHED'}

    def invoke(self, context, event):
        scene = context.scene
        self.frame_start = scene.frame_start
        self.frame_end = scene.frame_end

        wm = context.window_manager
        return wm.invoke_props_dialog(self)

