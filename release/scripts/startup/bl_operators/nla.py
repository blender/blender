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


def pose_frame_info(obj):
    from mathutils import Matrix

    info = {}

    pose = obj.pose

    pose_items = pose.bones.items()

    for name, pbone in pose_items:
        binfo = {}
        bone = pbone.bone

        binfo["parent"] = getattr(bone.parent, "name", None)
        binfo["bone"] = bone
        binfo["pbone"] = pbone
        binfo["matrix_local"] = bone.matrix_local.copy()
        try:
            binfo["matrix_local_inv"] = binfo["matrix_local"].inverted()
        except:
            binfo["matrix_local_inv"] = Matrix()

        binfo["matrix"] = bone.matrix.copy()
        binfo["matrix_pose"] = pbone.matrix.copy()
        try:
            binfo["matrix_pose_inv"] = binfo["matrix_pose"].inverted()
        except:
            binfo["matrix_pose_inv"] = Matrix()

        info[name] = binfo

    for name, pbone in pose_items:
        binfo = info[name]
        binfo_parent = binfo.get("parent", None)
        if binfo_parent:
            binfo_parent = info[binfo_parent]

        matrix = binfo["matrix_pose"]
        rest_matrix = binfo["matrix_local"]

        if binfo_parent:
            matrix = binfo_parent["matrix_pose_inv"] * matrix
            rest_matrix = binfo_parent["matrix_local_inv"] * rest_matrix

        binfo["matrix_key"] = rest_matrix.inverted() * matrix

    return info


def obj_frame_info(obj):
    info = {}
    # parent = obj.parent
    info["matrix_key"] = obj.matrix_local.copy()
    return info


def bake(frame_start,
         frame_end, step=1,
         only_selected=False,
         do_pose=True,
         do_object=True,
         do_constraint_clear=False,
         ):

    scene = bpy.context.scene
    obj = bpy.context.object
    pose = obj.pose
    frame_back = scene.frame_current

    if pose is None:
        do_pose = False

    if do_pose is None and do_object is None:
        return None

    pose_info = []
    obj_info = []

    frame_range = range(frame_start, frame_end + 1, step)

    # -------------------------------------------------------------------------
    # Collect transformations
    
    # could speed this up by applying steps here too...
    for f in frame_range:
        scene.frame_set(f)

        if do_pose:
            pose_info.append(pose_frame_info(obj))
        if do_object:
            obj_info.append(obj_frame_info(obj))

        f += 1

    # -------------------------------------------------------------------------
    # Create action

    # incase animation data hassnt been created
    atd = obj.animation_data_create()
    action = bpy.data.actions.new("Action")
    atd.action = action

    if do_pose:
        pose_items = pose.bones.items()
    else:
        pose_items = []  # skip

    # -------------------------------------------------------------------------
    # Apply transformations to action

    # pose
    for name, pbone in (pose_items if do_pose else ()):
        if only_selected and not pbone.bone.select:
            continue

        if do_constraint_clear:
            while pbone.constraints:
                pbone.constraints.remove(pbone.constraints[0])

        for f in frame_range:
            matrix = pose_info[(f - frame_start) // step][name]["matrix_key"]

            # pbone.location = matrix.to_translation()
            # pbone.rotation_quaternion = matrix.to_quaternion()
            pbone.matrix_basis = matrix

            pbone.keyframe_insert("location", -1, f, name)

            rotation_mode = pbone.rotation_mode

            if rotation_mode == 'QUATERNION':
                pbone.keyframe_insert("rotation_quaternion", -1, f, name)
            elif rotation_mode == 'AXIS_ANGLE':
                pbone.keyframe_insert("rotation_axis_angle", -1, f, name)
            else:  # euler, XYZ, ZXY etc
                pbone.keyframe_insert("rotation_euler", -1, f, name)

            pbone.keyframe_insert("scale", -1, f, name)

    # object. TODO. multiple objects
    if do_object:
        if do_constraint_clear:
            while obj.constraints:
                obj.constraints.remove(obj.constraints[0])

        for f in frame_range:
            matrix = obj_info[(f - frame_start) // step]["matrix_key"]
            obj.matrix_local = matrix

            obj.keyframe_insert("location", -1, f)

            rotation_mode = obj.rotation_mode

            if rotation_mode == 'QUATERNION':
                obj.keyframe_insert("rotation_quaternion", -1, f)
            elif rotation_mode == 'AXIS_ANGLE':
                obj.keyframe_insert("rotation_axis_angle", -1, f)
            else:  # euler, XYZ, ZXY etc
                obj.keyframe_insert("rotation_euler", -1, f)

            obj.keyframe_insert("scale", -1, f)

    scene.frame_set(frame_back)

    return action


from bpy.props import IntProperty, BoolProperty, EnumProperty


class BakeAction(bpy.types.Operator):
    '''Bake animation to an Action'''
    bl_idname = "nla.bake"
    bl_label = "Bake Action"
    bl_options = {'REGISTER', 'UNDO'}

    frame_start = IntProperty(name="Start Frame",
            description="Start frame for baking",
            default=1, min=1, max=300000)
    frame_end = IntProperty(name="End Frame",
            description="End frame for baking",
            default=250, min=1, max=300000)
    step = IntProperty(name="Frame Step",
            description="Frame Step",
            default=1, min=1, max=120)
    only_selected = BoolProperty(name="Only Selected",
            default=True)
    clear_consraints = BoolProperty(name="Clear Constraints",
            default=False)
    bake_types = EnumProperty(
            name="Bake Data",
            options={'ENUM_FLAG'},
            items=(('POSE', "Pose", ""),
                   ('OBJECT', "Object", ""),
                   ),
            default={'POSE'},
            )

    def execute(self, context):

        action = bake(self.frame_start,
                      self.frame_end,
                      self.step,
                      self.only_selected,
                      'POSE' in self.bake_types,
                      'OBJECT' in self.bake_types,
                      self.clear_consraints,
                      )

        if action is None:
            self.report({'INFO'}, "Nothing to bake")
            return {'CANCELLED'}

        # basic cleanup, could move elsewhere
        for fcu in action.fcurves:
            keyframe_points = fcu.keyframe_points
            i = 1
            while i < len(fcu.keyframe_points) - 1:
                val_prev = keyframe_points[i - 1].co[1]
                val_next = keyframe_points[i + 1].co[1]
                val = keyframe_points[i].co[1]

                if abs(val - val_prev) + abs(val - val_next) < 0.0001:
                    keyframe_points.remove(keyframe_points[i])
                else:
                    i += 1

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
