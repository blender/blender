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
    - Properly support slotted Actions.
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
from bpy.types import (
    ActionChannelbag,
)

from bpy_extras import anim_utils


def add_keyframe_quat(
        channelbag: ActionChannelbag,
        quat: list[float],
        frame: float,
        bone_prefix: str,
        group_name: str) -> None:
    for i in range(len(quat)):
        fc = channelbag.fcurves.ensure(bone_prefix + "rotation_quaternion", index=i, group_name=group_name)
        pos = len(fc.keyframe_points)
        fc.keyframe_points.add(1)
        fc.keyframe_points[pos].co = [frame, quat[i]]
        fc.update()


def add_keyframe_euler(
        channelbag: ActionChannelbag,
        euler: list[float],
        frame: float,
        bone_prefix: str,
        group_name: str) -> None:
    for i in range(len(euler)):
        fc = channelbag.fcurves.ensure(bone_prefix + "rotation_euler", index=i, group_name=group_name)
        pos = len(fc.keyframe_points)
        fc.keyframe_points.add(1)
        fc.keyframe_points[pos].co = [frame, euler[i]]
        fc.update()


def frames_matching(channelbag, data_path):
    frames = set()
    for fc in channelbag.fcurves:
        if fc.data_path == data_path:
            fri = [kp.co[0] for kp in fc.keyframe_points]
            frames.update(fri)
    return frames


def group_qe(_obj, channelbag, bone, bone_prefix, order):
    """Converts only one group/bone in one channelbag - Quaternion to euler."""
    # pose_bone = bone
    data_path = bone_prefix + "rotation_quaternion"
    frames = frames_matching(channelbag, data_path)

    for fr in frames:
        quat = bone.rotation_quaternion.copy()
        for fc in channelbag.fcurves:
            if fc.data_path == data_path:
                quat[fc.array_index] = fc.evaluate(fr)
        euler = quat.to_euler(order)

        add_keyframe_euler(channelbag, euler, fr, bone_prefix, bone.name)
        bone.rotation_mode = order


def group_eq(obj, channelbag, bone, bone_prefix, order):
    """Converts only one group/bone in one channelbag - Euler to Quaternion."""
    # pose_bone = bone
    data_path = bone_prefix + "rotation_euler"
    frames = frames_matching(channelbag, data_path)

    for fr in frames:
        euler = bone.rotation_euler.copy()
        for fc in channelbag.fcurves:
            if fc.data_path == data_path:
                euler[fc.array_index] = fc.evaluate(fr)
        quat = euler.to_quaternion()

        add_keyframe_quat(channelbag, quat, fr, bone_prefix, bone.name)
        bone.rotation_mode = order


def convert_curves_of_bone(obj, channelbag, bone, order):
    """Convert given bone's curves in given channelbag to given rotation order."""
    to_euler = False
    bone_prefix = ''

    for fcurve in channelbag.fcurves:
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

    fcurves_to_remove = []

    # If To-Euler conversion
    if to_euler and order != 'QUATERNION':
        # Converts the group/bone from Quaternion to Euler
        group_qe(obj, channelbag, bone, bone_prefix, order)

        # Removes quaternion fcurves
        for key in channelbag.fcurves:
            if key.data_path == 'pose.bones["' + bone.name + '"].rotation_quaternion':
                fcurves_to_remove.append(key)

    # If To-Quaternion conversion
    elif to_euler:
        # Converts the group/bone from Euler to Quaternion
        group_eq(obj, channelbag, bone, bone_prefix, order)

        # Removes euler fcurves
        for key in channelbag.fcurves:
            if key.data_path == 'pose.bones["' + bone.name + '"].rotation_euler':
                fcurves_to_remove.append(key)

    for fcurve in fcurves_to_remove:
        channelbag.fcurves.remove(fcurve)

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
        name="Affected Actions",
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

        assigned_action = obj.animation_data and obj.animation_data.action
        assigned_slot = obj.animation_data and obj.animation_data.action_slot

        if self.affected_bones == 'ALL':
            pose_bones = obj.pose.bones
        else:
            pose_bones = context.selected_pose_bones

        if self.affected_actions == 'ALL':
            actions = bpy.data.actions
        else:
            actions = [bpy.data.actions.get(self.selected_action)]

        for action in actions:
            if action == assigned_action:
                # On the assigned Action, use the assigned slot.
                action_slot = assigned_slot
            else:
                # Otherwise find a suitable slot to process.
                #
                # NOTE: this may not pick the right slot if they are not
                # consistently named. Also, if there are multiple suitable
                # slots, only the first one is converted.
                if assigned_slot.identifier in action.slots:
                    action_slot = action.slots[assigned_slot.identifier]
                else:
                    action_slot = anim_utils.action_get_first_suitable_slot(action, 'OBJECT')

            channelbag = anim_utils.action_get_channelbag_for_slot(action, action_slot)
            if not channelbag:
                continue

            for pb in pose_bones:
                convert_curves_of_bone(obj, channelbag, pb, self.target_rotation_mode)

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
