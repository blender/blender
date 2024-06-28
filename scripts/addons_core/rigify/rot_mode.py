# SPDX-FileCopyrightText: 2017-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Quaternion/Euler Rotation Mode Converter v0.1

This script/addon:
    - Changes (pose) bone rotation mode
    - Converts keyframes from one rotation mode to another
    - Creates fcurves/keyframes in target rotation mode
    - Deletes previous fcurves/keyframes.
    - Converts multiple bones
    - Converts multiple Actions

TO-DO:
    - To convert object's rotation mode (already done in Mutant Bob script,
        but not done in this one.)
    - To understand "EnumProperty" and write it well.
    - Code clean
    - ...

GitHub: https://github.com/MarioMey/rotation_mode_addon/
BlenderArtist thread: https://blenderartists.org/forum/showthread.php?388197-Quat-Euler-Rotation-Mode-Converter

Mutant Bob did the "hard code" of this script. Thanks him!
blender.stackexchange.com/questions/40711/how-to-convert-quaternions-keyframes-to-euler-ones-in-several-actions


"""

# bl_info = {
#     "name": "Rotation Mode Converter",
#     "author": "Mario Mey / Mutant Bob",
#     "version": (0, 2),
#     "blender": (2, 91, 0),
#     'location': 'Pose Mode -> Header -> Pose -> Convert Rotation Modes',
#     "description": "Converts Animation between different rotation orders",
#     "warning": "",
#     "doc_url": "",
#     "tracker_url": "https://github.com/MarioMey/rotation_mode_addon/",
#     "category": "Animation",
# }

import bpy
from bpy.props import (
    EnumProperty,
    StringProperty,
)


def get_or_create_fcurve(action, data_path, array_index=-1, group=None):
    for fc in action.fcurves:
        if fc.data_path == data_path and (array_index < 0 or fc.array_index == array_index):
            return fc

    fc = action.fcurves.new(data_path, index=array_index)
    fc.group = group
    return fc


def add_keyframe_quat(action, quat, frame, bone_prefix, group):
    for i in range(len(quat)):
        fc = get_or_create_fcurve(action, bone_prefix + "rotation_quaternion", i, group)
        pos = len(fc.keyframe_points)
        fc.keyframe_points.add(1)
        fc.keyframe_points[pos].co = [frame, quat[i]]
        fc.update()


def add_keyframe_euler(action, euler, frame, bone_prefix, group):
    for i in range(len(euler)):
        fc = get_or_create_fcurve(action, bone_prefix + "rotation_euler", i, group)
        pos = len(fc.keyframe_points)
        fc.keyframe_points.add(1)
        fc.keyframe_points[pos].co = [frame, euler[i]]
        fc.update()


def frames_matching(action, data_path):
    frames = set()
    for fc in action.fcurves:
        if fc.data_path == data_path:
            fri = [kp.co[0] for kp in fc.keyframe_points]
            frames.update(fri)
    return frames


def group_qe(_obj, action, bone, bone_prefix, order):
    """Converts only one group/bone in one action - Quaternion to euler."""
    # pose_bone = bone
    data_path = bone_prefix + "rotation_quaternion"
    frames = frames_matching(action, data_path)
    group = action.groups[bone.name]

    for fr in frames:
        quat = bone.rotation_quaternion.copy()
        for fc in action.fcurves:
            if fc.data_path == data_path:
                quat[fc.array_index] = fc.evaluate(fr)
        euler = quat.to_euler(order)

        add_keyframe_euler(action, euler, fr, bone_prefix, group)
        bone.rotation_mode = order


def group_eq(_obj, action, bone, bone_prefix, order):
    """Converts only one group/bone in one action - Euler to Quaternion."""
    # pose_bone = bone
    data_path = bone_prefix + "rotation_euler"
    frames = frames_matching(action, data_path)
    group = action.groups[bone.name]

    for fr in frames:
        euler = bone.rotation_euler.copy()
        for fc in action.fcurves:
            if fc.data_path == data_path:
                euler[fc.array_index] = fc.evaluate(fr)
        quat = euler.to_quaternion()

        add_keyframe_quat(action, quat, fr, bone_prefix, group)
        bone.rotation_mode = order


def convert_curves_of_bone_in_action(obj, action, bone, order):
    """Convert given bone's curves in given action to given rotation order."""
    to_euler = False
    bone_prefix = ''

    for fcurve in action.fcurves:
        if fcurve.group.name == bone.name:

            # If To-Euler conversion
            if order != 'QUATERNION':
                if fcurve.data_path.endswith('rotation_quaternion'):
                    to_euler = True
                    bone_prefix = fcurve.data_path[:-len('rotation_quaternion')]
                    break

            # If To-Quaternion conversion
            else:
                if fcurve.data_path.endswith('rotation_euler'):
                    to_euler = True
                    bone_prefix = fcurve.data_path[:-len('rotation_euler')]
                    break

    # If To-Euler conversion
    if to_euler and order != 'QUATERNION':
        # Converts the group/bone from Quaternion to Euler
        group_qe(obj, action, bone, bone_prefix, order)

        # Removes quaternion fcurves
        for key in action.fcurves:
            if key.data_path == 'pose.bones["' + bone.name + '"].rotation_quaternion':
                action.fcurves.remove(key)

    # If To-Quaternion conversion
    elif to_euler:
        # Converts the group/bone from Euler to Quaternion
        group_eq(obj, action, bone, bone_prefix, order)

        # Removes euler fcurves
        for key in action.fcurves:
            if key.data_path == 'pose.bones["' + bone.name + '"].rotation_euler':
                action.fcurves.remove(key)

    # Changes rotation mode to new one
    bone.rotation_mode = order


# noinspection PyPep8Naming
class POSE_OT_convert_rotation(bpy.types.Operator):
    bl_label = 'Convert Rotation Modes'
    bl_idname = 'pose.convert_rotation'
    bl_description = 'Convert animation from any rotation mode to any other'
    bl_options = {'REGISTER', 'UNDO'}

    # Properties.
    target_rotation_mode: EnumProperty(
        items=[
            ('QUATERNION', 'Quaternion', 'Quaternion'),
            ('XYZ', 'XYZ', 'XYZ Euler'),
            ('XZY', 'XZY', 'XZY Euler'),
            ('YXZ', 'YXZ', 'YXZ Euler'),
            ('YZX', 'YZX', 'YZX Euler'),
            ('ZXY', 'ZXY', 'ZXY Euler'),
            ('ZYX', 'ZYX', 'ZYX Euler')
        ],
        name='Convert To',
        description="The target rotation mode",
        default='QUATERNION',
    )
    affected_bones: EnumProperty(
        name="Affected Bones",
        items=[
            ('SELECT', 'Selected', 'Selected'),
            ('ALL', 'All', 'All'),
        ],
        description="Which bones to affect",
        default='SELECT',
    )
    affected_actions: EnumProperty(
        name="Affected Action",
        items=[
            ('SINGLE', 'Single', 'Single'),
            ('ALL', 'All', 'All'),
        ],
        description="Which Actions to affect",
        default='SINGLE',
    )
    selected_action: StringProperty(name="Action")

    def invoke(self, context, event):
        ob = context.object
        if ob and ob.type == 'ARMATURE' and ob.animation_data and ob.animation_data.action:
            self.selected_action = context.object.animation_data.action.name
        else:
            self.affected_actions = 'ALL'

        wm = context.window_manager
        return wm.invoke_props_dialog(self)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        layout.row().prop(self, 'affected_bones', expand=True)
        layout.row().prop(self, 'affected_actions', expand=True)
        if self.affected_actions == 'SINGLE':
            layout.prop_search(self, 'selected_action', bpy.data, 'actions')
        layout.prop(self, 'target_rotation_mode')

    def execute(self, context):
        obj = context.active_object

        actions = [bpy.data.actions.get(self.selected_action)]
        pose_bones = context.selected_pose_bones
        if self.affected_bones == 'ALL':
            pose_bones = obj.pose.bones
        if self.affected_actions == 'ALL':
            actions = bpy.data.actions

        for action in actions:
            for pb in pose_bones:
                convert_curves_of_bone_in_action(obj, action, pb, self.target_rotation_mode)

        return {'FINISHED'}


def draw_convert_rotation(self, _context):
    self.layout.separator()
    self.layout.operator(POSE_OT_convert_rotation.bl_idname)


classes = [
    POSE_OT_convert_rotation
]


def register():
    from bpy.utils import register_class

    # Classes.
    for cls in classes:
        register_class(cls)

    bpy.types.VIEW3D_MT_pose.append(draw_convert_rotation)


def unregister():
    from bpy.utils import unregister_class

    # Classes.
    for cls in classes:
        unregister_class(cls)

    bpy.types.VIEW3D_MT_pose.remove(draw_convert_rotation)
