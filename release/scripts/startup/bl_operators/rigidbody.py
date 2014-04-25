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
    bl_label = "Copy Rigid Body Settings"
    bl_options = {'REGISTER', 'UNDO'}

    _attrs = (
        "type",
        "kinematic",
        "mass",
        "collision_shape",
        "use_margin",
        "collision_margin",
        "friction",
        "restitution",
        "use_deactivation",
        "use_start_deactivated",
        "deactivate_linear_velocity",
        "deactivate_angular_velocity",
        "linear_damping",
        "angular_damping",
        "collision_groups",
        "mesh_source",
        "use_deform",
        "enabled",
        )

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.rigid_body)

    def execute(self, context):
        obj_act = context.object
        scene = context.scene

        # deselect all but mesh objects
        for o in context.selected_objects:
            if o.type != 'MESH':
                o.select = False

        objects = context.selected_objects
        if objects:
            # add selected objects to active one groups and recalculate
            bpy.ops.group.objects_add_active()
            scene.frame_set(scene.frame_current)
            rb_from = obj_act.rigid_body
            # copy settings
            for o in objects:
                rb_to = o.rigid_body
                if (o == obj_act) or (rb_to is None):
                    continue
                for attr in self._attrs:
                    setattr(rb_to, attr, getattr(rb_from, attr))

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
        obj = context.object
        return (obj and obj.rigid_body)

    def execute(self, context):
        bake = []
        objects = []
        scene = context.scene
        frame_orig = scene.frame_current
        frames_step = range(self.frame_start, self.frame_end + 1, self.step)
        frames_full = range(self.frame_start, self.frame_end + 1)

        # filter objects selection
        for obj in context.selected_objects:
            if not obj.rigid_body or obj.rigid_body.type != 'ACTIVE':
                obj.select = False

        objects = context.selected_objects

        if objects:
            # store transformation data
            # need to start at scene start frame so simulation is run from the beginning
            for f in frames_full:
                scene.frame_set(f)
                if f in frames_step:
                    mat = {}
                    for i, obj in enumerate(objects):
                        mat[i] = obj.matrix_world.copy()
                    bake.append(mat)

            # apply transformations as keyframes
            for i, f in enumerate(frames_step):
                scene.frame_set(f)
                for j, obj in enumerate(objects):
                    mat = bake[i][j]
                    # convert world space transform to parent space, so parented objects don't get offset after baking
                    if (obj.parent):
                        mat = obj.matrix_parent_inverse.inverted() * obj.parent.matrix_world.inverted() * mat

                    obj.location = mat.to_translation()

                    rot_mode = obj.rotation_mode
                    if rot_mode == 'QUATERNION':
                        q1 = obj.rotation_quaternion
                        q2 = mat.to_quaternion()
                        # make quaternion compatible with the previous one
                        if q1.dot(q2) < 0.0:
                            obj.rotation_quaternion = -q2
                        else:
                            obj.rotation_quaternion = q2
                    elif rot_mode == 'AXIS_ANGLE':
                        # this is a little roundabout but there's no better way right now
                        aa = mat.to_quaternion().to_axis_angle()
                        obj.rotation_axis_angle = (aa[1], ) + aa[0][:]
                    else:  # euler
                        # make sure euler rotation is compatible to previous frame
                        # NOTE: assume that on first frame, the starting rotation is appropriate
                        obj.rotation_euler = mat.to_euler(rot_mode, obj.rotation_euler)

                bpy.ops.anim.keyframe_insert(type='BUILTIN_KSI_LocRot', confirm_success=False)

            # remove baked objects from simulation
            bpy.ops.rigidbody.objects_remove()

            # clean up keyframes
            for obj in objects:
                action = obj.animation_data.action
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


class ConnectRigidBodies(Operator):
    '''Create rigid body constraints between selected rigid bodies'''
    bl_idname = "rigidbody.connect"
    bl_label = "Connect Rigid Bodies"
    bl_options = {'REGISTER', 'UNDO'}

    con_type = EnumProperty(
            name="Type",
            description="Type of generated constraint",
            # XXX Would be nice to get icons too, but currently not possible ;)
            items=tuple((e.identifier, e.name, e.description, e. value)
                        for e in bpy.types.RigidBodyConstraint.bl_rna.properties["type"].enum_items),
            default='FIXED',
            )
    pivot_type = EnumProperty(
            name="Location",
            description="Constraint pivot location",
            items=(('CENTER', "Center", "Pivot location is between the constrained rigid bodies"),
                   ('ACTIVE', "Active", "Pivot location is at the active object position"),
                   ('SELECTED', "Selected", "Pivot location is at the selected object position")),
            default='CENTER',
            )
    connection_pattern = EnumProperty(
            name="Connection Pattern",
            description="Pattern used to connect objects",
            items=(('SELECTED_TO_ACTIVE', "Selected to Active", "Connect selected objects to the active object"),
                   ('CHAIN_DISTANCE', "Chain by Distance", "Connect objects as a chain based on distance, starting at the active object")),
            default='SELECTED_TO_ACTIVE',
            )

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.rigid_body)

    def _add_constraint(self, context, object1, object2):
        if object1 == object2:
            return

        if self.pivot_type == 'ACTIVE':
            loc = object1.location
        elif self.pivot_type == 'SELECTED':
            loc = object2.location
        else:
            loc = (object1.location + object2.location) / 2.0

        ob = bpy.data.objects.new("Constraint", object_data=None)
        ob.location = loc
        context.scene.objects.link(ob)
        context.scene.objects.active = ob
        ob.select = True

        bpy.ops.rigidbody.constraint_add()
        con_obj = context.active_object
        con_obj.empty_draw_type = 'ARROWS'
        con = con_obj.rigid_body_constraint
        con.type = self.con_type

        con.object1 = object1
        con.object2 = object2

    def execute(self, context):
        scene = context.scene
        objects = context.selected_objects
        obj_act = context.active_object
        change = False

        if self.connection_pattern == 'CHAIN_DISTANCE':
            objs_sorted = [obj_act]
            objects_tmp = context.selected_objects
            try:
                objects_tmp.remove(obj_act)
            except ValueError:
                pass

            last_obj = obj_act

            while objects_tmp:
                objects_tmp.sort(key=lambda o: (last_obj.location - o.location).length)
                last_obj = objects_tmp.pop(0)
                objs_sorted.append(last_obj)

            for i in range(1, len(objs_sorted)):
                self._add_constraint(context, objs_sorted[i - 1], objs_sorted[i])
                change = True

        else:  # SELECTED_TO_ACTIVE
            for obj in objects:
                self._add_constraint(context, obj_act, obj)
                change = True

        if change:
            # restore selection
            bpy.ops.object.select_all(action='DESELECT')
            for obj in objects:
                obj.select = True
            scene.objects.active = obj_act
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "No other objects selected")
            return {'CANCELLED'}
