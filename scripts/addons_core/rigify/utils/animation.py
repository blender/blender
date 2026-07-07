# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy  # noqa
import math  # noqa
from mathutils import Matrix, Vector  # noqa

from typing import TYPE_CHECKING, Callable, Any, Collection, Iterator, Optional, Sequence
from bpy.types import Action, bpy_struct, FCurve
from bpy_extras import anim_utils

import json

if TYPE_CHECKING:
    from ..rig_ui_template import PanelLayout


rig_id = None


##############################################
# Keyframing functions
##############################################

def _get_channelbag_for_rig(rig: bpy.types.Object) -> bpy.types.ActionChannelbag | None:
    assert isinstance(rig, bpy.types.Object)
    if not rig.animation_data:
        return None

    action = rig.animation_data.action
    action_slot = rig.animation_data.action_slot
    return anim_utils.action_get_channelbag_for_slot(action, action_slot)


def get_keyed_frames_in_range(context: bpy.types.Context, rig: bpy.types.Object) -> list[float]:
    channelbag = _get_channelbag_for_rig(rig)
    if not channelbag:
        return []

    frame_range = RIGIFY_OT_get_frame_range.get_range(context)
    return sorted(get_curve_frame_set(channelbag.fcurves, frame_range))


def bones_in_frame(f, rig, *args):
    """
    True if one of the bones listed in args is animated at frame f
    :param f: the frame
    :param rig: the rig
    :param args: bone names
    :return:
    """

    channelbag = _get_channelbag_for_rig(rig)
    if not channelbag:
        return False

    for fc in channelbag.fcurves:
        animated_frames = [kp.co[0] for kp in fc.keyframe_points]
        for bone in args:
            if bone in fc.data_path.split('"') and f in animated_frames:
                return True

    return False


def overwrite_prop_animation(rig, bone, prop_name, value, frames):
    channelbag = _get_channelbag_for_rig(rig)
    if not channelbag:
        return

    bone_name = bone.name
    curve = None

    for fcu in channelbag.fcurves:
        words = fcu.data_path.split('"')
        if words[0] == "pose.bones[" and words[1] == bone_name and words[-2] == prop_name:
            curve = fcu
            break

    if not curve:
        return

    for kp in curve.keyframe_points:
        if kp.co[0] in frames:
            kp.co[1] = value


################################################################
# Utilities for inserting keyframes and/or setting transforms ##
################################################################

SCRIPT_UTILITIES_KEYING = ['''
######################
## Keyframing tools ##
######################

def get_keying_flags(context):
    "Retrieve the general keyframing flags from user preferences."
    prefs = context.preferences
    ts = context.scene.tool_settings
    flags = set()
    # Not adding INSERTKEY_VISUAL
    if prefs.edit.use_keyframe_insert_needed:
        flags.add('INSERTKEY_NEEDED')
    if ts.use_keyframe_cycle_aware:
        flags.add('INSERTKEY_CYCLE_AWARE')
    return flags

def get_autokey_flags(context, ignore_keyingset=False):
    "Retrieve the Auto Keyframe flags, or None if disabled."
    ts = context.scene.tool_settings
    if ts.use_keyframe_insert_auto and (ignore_keyingset or not ts.use_keyframe_insert_keyingset):
        flags = get_keying_flags(context)
        if context.preferences.edit.use_keyframe_insert_available:
            flags.add('INSERTKEY_AVAILABLE')
        if ts.auto_keying_mode == 'REPLACE_KEYS':
            flags.add('INSERTKEY_REPLACE')
        return flags
    else:
        return None

def add_flags_if_set(base, new_flags):
    "Add more flags if base is not None."
    if base is None:
        return None
    else:
        return base | new_flags

def get_4d_rot_lock(bone):
    "Retrieve the lock status for 4D rotation."
    if bone.lock_rotations_4d:
        return [bone.lock_rotation_w, *bone.lock_rotation]
    else:
        return [all(bone.lock_rotation)] * 4

def keyframe_transform_properties(obj, bone_name, keyflags, *,
                                  ignore_locks=False, no_loc=False, no_rot=False, no_scale=False):
    "Keyframe transformation properties, taking flags and mode into account, and avoiding keying locked channels."
    bone = obj.pose.bones[bone_name]

    def keyframe_channels(prop, locks):
        if ignore_locks or not all(locks):
            if ignore_locks or not any(locks):
                bone.keyframe_insert(prop, group=bone_name, options=keyflags)
            else:
                for i, lock in enumerate(locks):
                    if not lock:
                        bone.keyframe_insert(prop, index=i, group=bone_name, options=keyflags)

    if not (no_loc or bone.bone.use_connect):
        keyframe_channels('location', bone.lock_location)

    if not no_rot:
        if bone.rotation_mode == 'QUATERNION':
            keyframe_channels('rotation_quaternion', get_4d_rot_lock(bone))
        elif bone.rotation_mode == 'AXIS_ANGLE':
            keyframe_channels('rotation_axis_angle', get_4d_rot_lock(bone))
        else:
            keyframe_channels('rotation_euler', bone.lock_rotation)

    if not no_scale:
        keyframe_channels('scale', bone.lock_scale)

######################
## Constraint tools ##
######################

def get_constraint_target_matrix(con):
    target = con.target
    if target:
        if target.type == 'ARMATURE' and con.subtarget:
            if con.subtarget in target.pose.bones:
                bone = target.pose.bones[con.subtarget]
                return target.convert_space(
                    pose_bone=bone, matrix=bone.matrix, from_space='POSE', to_space=con.target_space)
        else:
            return target.convert_space(matrix=target.matrix_world, from_space='WORLD', to_space=con.target_space)
    return Matrix.Identity(4)

def undo_copy_scale_with_offset(obj, bone, con, old_matrix):
    "Undo the effects of Copy Scale with Offset constraint on a bone matrix."
    inf = con.influence

    if con.mute or inf == 0 or not con.is_valid or not con.use_offset or con.use_add:
        return old_matrix

    tgt_matrix = get_constraint_target_matrix(con)
    tgt_scale = tgt_matrix.to_scale()
    use = [con.use_x, con.use_y, con.use_z]

    if con.use_make_uniform:
        if con.use_x and con.use_y and con.use_z:
            total = tgt_matrix.determinant()
        else:
            total = 1
            for i, use in enumerate(use):
                if use:
                    total *= tgt_scale[i]

        tgt_scale = [abs(total)**(1./3.)]*3
    else:
        for i, use in enumerate(use):
            if not use:
                tgt_scale[i] = 1

    scale_delta = [
        1 / (1 + (math.pow(x, con.power) - 1) * inf)
        for x in tgt_scale
    ]

    return old_matrix @ Matrix.Diagonal([*scale_delta, 1])

def undo_copy_scale_constraints(obj, bone, matrix):
    "Undo the effects of all Copy Scale with Offset constraints on a bone matrix."
    for con in reversed(bone.constraints):
        if con.type == 'COPY_SCALE':
            matrix = undo_copy_scale_with_offset(obj, bone, con, matrix)
    return matrix

###############################
## Assign and keyframe tools ##
###############################

def set_custom_property_value(obj, bone_name, prop, value, *, keyflags=None):
    "Assign the value of a custom property, and optionally keyframe it."
    from rna_prop_ui import rna_idprop_ui_prop_update
    bone = obj.pose.bones[bone_name]
    bone[prop] = value
    rna_idprop_ui_prop_update(bone, prop)
    if keyflags is not None:
        bone.keyframe_insert(rna_idprop_quote_path(prop), group=bone.name, options=keyflags)

def get_transform_matrix(obj, bone_name, *, space='POSE', with_constraints=True):
    "Retrieve the matrix of the bone before or after constraints in the given space."
    bone = obj.pose.bones[bone_name]
    if with_constraints:
        return obj.convert_space(pose_bone=bone, matrix=bone.matrix, from_space='POSE', to_space=space)
    else:
        return obj.convert_space(pose_bone=bone, matrix=bone.matrix_basis, from_space='LOCAL', to_space=space)

def get_chain_transform_matrices(obj, bone_names, **options):
    return [get_transform_matrix(obj, name, **options) for name in bone_names]

def set_transform_from_matrix(obj, bone_name, matrix, *, space='POSE', undo_copy_scale=False,
                              ignore_locks=False, no_loc=False, no_rot=False, no_scale=False, keyflags=None):
    """Apply the matrix to the transformation of the bone, taking locked channels, mode and certain
    constraints into account, and optionally keyframe it."""
    bone = obj.pose.bones[bone_name]

    def restore_channels(prop, old_vec, locks, extra_lock):
        if extra_lock or (not ignore_locks and all(locks)):
            setattr(bone, prop, old_vec)
        else:
            if not ignore_locks and any(locks):
                new_vec = Vector(getattr(bone, prop))

                for i, lock in enumerate(locks):
                    if lock:
                        new_vec[i] = old_vec[i]

                setattr(bone, prop, new_vec)

    # Save the old values of the properties
    old_loc = Vector(bone.location)
    old_rot_euler = Vector(bone.rotation_euler)
    old_rot_quat = Vector(bone.rotation_quaternion)
    old_rot_axis = Vector(bone.rotation_axis_angle)
    old_scale = Vector(bone.scale)

    # Compute and assign the local matrix
    if space != 'LOCAL':
        matrix = obj.convert_space(pose_bone=bone, matrix=matrix, from_space=space, to_space='LOCAL')

    if undo_copy_scale:
        matrix = undo_copy_scale_constraints(obj, bone, matrix)

    bone.matrix_basis = matrix

    # Restore locked properties
    restore_channels('location', old_loc, bone.lock_location, no_loc or bone.bone.use_connect)

    if bone.rotation_mode == 'QUATERNION':
        restore_channels('rotation_quaternion', old_rot_quat, get_4d_rot_lock(bone), no_rot)
        bone.rotation_axis_angle = old_rot_axis
        bone.rotation_euler = old_rot_euler
    elif bone.rotation_mode == 'AXIS_ANGLE':
        bone.rotation_quaternion = old_rot_quat
        restore_channels('rotation_axis_angle', old_rot_axis, get_4d_rot_lock(bone), no_rot)
        bone.rotation_euler = old_rot_euler
    else:
        bone.rotation_quaternion = old_rot_quat
        bone.rotation_axis_angle = old_rot_axis
        restore_channels('rotation_euler', old_rot_euler, bone.lock_rotation, no_rot)

    restore_channels('scale', old_scale, bone.lock_scale, no_scale)

    # Keyframe properties
    if keyflags is not None:
        keyframe_transform_properties(
            obj, bone_name, keyflags, ignore_locks=ignore_locks,
            no_loc=no_loc, no_rot=no_rot, no_scale=no_scale
        )

def set_chain_transforms_from_matrices(context, obj, bone_names, matrices, **options):
    for bone, matrix in zip(bone_names, matrices):
        set_transform_from_matrix(obj, bone, matrix, **options)
        context.view_layer.update()
''']

exec(SCRIPT_UTILITIES_KEYING[-1])

############################################
# Utilities for managing animation curves ##
############################################

SCRIPT_UTILITIES_CURVES = ['''
###########################
## Animation curve tools ##
###########################

from bpy_extras import anim_utils

def flatten_curve_set(curves):
    "Iterate over all FCurves inside a set of nested lists and dictionaries."
    if curves is None:
        pass
    elif isinstance(curves, bpy.types.FCurve):
        yield curves
    elif isinstance(curves, dict):
        for sub in curves.values():
            yield from flatten_curve_set(sub)
    else:
        for sub in curves:
            yield from flatten_curve_set(sub)

def flatten_curve_key_set(curves, key_range=None):
    "Iterate over all keys of the given fcurves in the specified range."
    for curve in flatten_curve_set(curves):
        for key in curve.keyframe_points:
            if key_range is None or key_range[0] <= key.co[0] <= key_range[1]:
                yield key

def get_curve_frame_set(curves, key_range=None):
    "Compute a set of all time values with existing keys in the given curves and range."
    return set(key.co[0] for key in flatten_curve_key_set(curves, key_range))

def set_curve_key_interpolation(curves, ipo, key_range=None):
    "Assign the given interpolation value to all curve keys in range."
    for key in flatten_curve_key_set(curves, key_range):
        key.interpolation = ipo

def delete_curve_keys_in_range(curves, key_range=None):
    "Delete all keys of the given curves within the given range."
    for curve in flatten_curve_set(curves):
        points = curve.keyframe_points
        for i in range(len(points), 0, -1):
            key = points[i - 1]
            if key_range is None or key_range[0] <= key.co[0] <= key_range[1]:
                points.remove(key, fast=True)
        curve.update()

def nla_tweak_to_scene(anim_data, frames, invert=False):
    "Convert a frame value or list between scene and tweaked NLA strip time."
    if frames is None:
        return None
    elif anim_data is None or not anim_data.use_tweak_mode:
        return frames
    elif isinstance(frames, (int, float)):
        return anim_data.nla_tweak_strip_time_to_scene(frames, invert=invert)
    else:
        return type(frames)(
            anim_data.nla_tweak_strip_time_to_scene(v, invert=invert) for v in frames
        )

def find_action(action):
    if isinstance(action, bpy.types.Object):
        action = action.animation_data
    if isinstance(action, bpy.types.AnimData):
        action = action.action
    if isinstance(action, bpy.types.Action):
        return action
    else:
        return None

def clean_action_empty_curves(action):
    "Delete completely empty curves from the given action."
    action = find_action(action)
    for layer in action.layers:
        for strip in layer.strips:
            for channelbag in strip.channelbags:
                for curve in channelbag.fcurves[:]:
                    if curve.is_empty:
                        channelbag.fcurves.remove(curve)
    action.update_tag()

TRANSFORM_PROPS_LOCATION = frozenset(['location'])
TRANSFORM_PROPS_ROTATION = frozenset(['rotation_euler', 'rotation_quaternion', 'rotation_axis_angle'])
TRANSFORM_PROPS_SCALE = frozenset(['scale'])
TRANSFORM_PROPS_ALL = frozenset(TRANSFORM_PROPS_LOCATION | TRANSFORM_PROPS_ROTATION | TRANSFORM_PROPS_SCALE)

def transform_props_with_locks(lock_location, lock_rotation, lock_scale):
    props = set()
    if not lock_location:
        props |= TRANSFORM_PROPS_LOCATION
    if not lock_rotation:
        props |= TRANSFORM_PROPS_ROTATION
    if not lock_scale:
        props |= TRANSFORM_PROPS_SCALE
    return props

class FCurveTable(object):
    "Table for efficient lookup of FCurves by properties."

    def __init__(self):
        self.curve_map = collections.defaultdict(dict)

    def index_curves(self, curves):
        for curve in curves:
            index = curve.array_index
            if index < 0:
                index = 0
            self.curve_map[curve.data_path][index] = curve

    def get_prop_curves(self, ptr, prop_path):
        "Returns a dictionary from array index to curve for the given property, or Null."
        return self.curve_map.get(ptr.path_from_id(prop_path))

    def list_all_prop_curves(self, ptr_set, path_set):
        "Iterates over all FCurves matching the given object(s) and properties."
        if isinstance(ptr_set, bpy.types.bpy_struct):
            ptr_set = [ptr_set]
        for ptr in ptr_set:
            for path in path_set:
                curves = self.get_prop_curves(ptr, path)
                if curves:
                    yield from curves.values()

    def get_custom_prop_curves(self, ptr, prop):
        return self.get_prop_curves(ptr, rna_idprop_quote_path(prop))

class ActionCurveTable(FCurveTable):
    "Table for efficient lookup of Action FCurves by properties."

    def __init__(self, rig: bpy.types.Object) -> None:
        super().__init__()
        assert isinstance(rig, bpy.types.Object)

        if not rig.animation_data:
            return
        action = rig.animation_data.action
        action_slot = rig.animation_data.action_slot

        channelbag = anim_utils.action_get_channelbag_for_slot(action, action_slot)
        if not channelbag:
            return
        self.index_curves(channelbag.fcurves)

class DriverCurveTable(FCurveTable):
    "Table for efficient lookup of Driver FCurves by properties."

    def __init__(self, object):
        super().__init__()
        self.anim_data = object.animation_data
        if self.anim_data:
            self.index_curves(self.anim_data.drivers)
''']

AnyCurveSet = None | FCurve | dict | Collection
flatten_curve_set: Callable[[AnyCurveSet], Iterator[FCurve]]
flatten_curve_key_set: Callable[..., set[float]]
get_curve_frame_set: Callable[..., set[float]]
set_curve_key_interpolation: Callable[..., None]
delete_curve_keys_in_range: Callable[..., None]
nla_tweak_to_scene: Callable
find_action: Callable[[bpy_struct], Action]
clean_action_empty_curves: Callable[[bpy_struct], None]
TRANSFORM_PROPS_LOCATION: frozenset[str]
TRANSFORM_PROPS_ROTATION = frozenset[str]
TRANSFORM_PROPS_SCALE = frozenset[str]
TRANSFORM_PROPS_ALL = frozenset[str]
transform_props_with_locks: Callable[[bool, bool, bool], set[str]]
FCurveTable: Any
ActionCurveTable: Any
DriverCurveTable: Any

exec(SCRIPT_UTILITIES_CURVES[-1])

################################################
# Utilities for operators that bake keyframes ##
################################################

_SCRIPT_REGISTER_WM_PROPS = '''
bpy.types.WindowManager.rigify_transfer_use_all_keys = bpy.props.BoolProperty(
    name="Bake All Keyed Frames",
    description="Bake on every frame that has a key for any of the bones, as opposed to just the relevant ones",
    default=False
)
bpy.types.WindowManager.rigify_transfer_use_frame_range = bpy.props.BoolProperty(
    name="Limit Frame Range", description="Only bake keyframes in a certain frame range", default=False
)
bpy.types.WindowManager.rigify_transfer_start_frame = bpy.props.IntProperty(
    name="Start", description="First frame to transfer", default=0, min=0
)
bpy.types.WindowManager.rigify_transfer_end_frame = bpy.props.IntProperty(
    name="End", description="Last frame to transfer", default=0, min=0
)
'''

_SCRIPT_UNREGISTER_WM_PROPS = '''
del bpy.types.WindowManager.rigify_transfer_use_all_keys
del bpy.types.WindowManager.rigify_transfer_use_frame_range
del bpy.types.WindowManager.rigify_transfer_start_frame
del bpy.types.WindowManager.rigify_transfer_end_frame
'''

_SCRIPT_UTILITIES_BAKE_OPS = '''
class RIGIFY_OT_get_frame_range(bpy.types.Operator):
    bl_idname = "rigify.get_frame_range" + ('_'+rig_id if rig_id else '')
    bl_label = "Get Frame Range"
    bl_description = "Set start and end frame from scene"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        scn = context.scene
        id_store = context.window_manager
        id_store.rigify_transfer_start_frame = scn.frame_start
        id_store.rigify_transfer_end_frame = scn.frame_end
        return {'FINISHED'}

    @staticmethod
    def get_range(context):
        id_store = context.window_manager
        if not id_store.rigify_transfer_use_frame_range:
            return None
        else:
            return (id_store.rigify_transfer_start_frame, id_store.rigify_transfer_end_frame)

    @classmethod
    def draw_range_ui(self, context, layout):
        id_store = context.window_manager

        row = layout.row(align=True)
        row.prop(id_store, 'rigify_transfer_use_frame_range', icon='PREVIEW_RANGE', text='')

        row = row.row(align=True)
        row.active = id_store.rigify_transfer_use_frame_range
        row.prop(id_store, 'rigify_transfer_start_frame')
        row.prop(id_store, 'rigify_transfer_end_frame')
        row.operator(self.bl_idname, icon='TIME', text='')
'''

RIGIFY_OT_get_frame_range: Any

exec(_SCRIPT_UTILITIES_BAKE_OPS)

################################################
# Framework for operators that bake keyframes ##
################################################

SCRIPT_REGISTER_BAKE = ['RIGIFY_OT_get_frame_range']

SCRIPT_UTILITIES_BAKE = SCRIPT_UTILITIES_KEYING + SCRIPT_UTILITIES_CURVES + ['''
##################################
# Common bake operator settings ##
##################################
''' + _SCRIPT_REGISTER_WM_PROPS + _SCRIPT_UTILITIES_BAKE_OPS + '''
#######################################
# Keyframe baking operator framework ##
#######################################

class RigifyOperatorMixinBase:
    bl_options = {'UNDO', 'INTERNAL'}

    def init_invoke(self, context):
        "Override to initialize the operator before invoke."

    def init_execute(self, context):
        "Override to initialize the operator before execute."

    def before_save_state(self, context, rig):
        "Override to prepare for saving state."

    def after_save_state(self, context, rig):
        "Override to undo before_save_state."


class RigifyBakeKeyframesMixin(RigifyOperatorMixinBase):
    """Basic framework for an operator that updates a set of keyed frames."""

    # Utilities
    def nla_from_raw(self, frames):
        "Convert frame(s) from inner action time to scene time."
        return nla_tweak_to_scene(self.bake_anim, frames)

    def nla_to_raw(self, frames):
        "Convert frame(s) from scene time to inner action time."
        return nla_tweak_to_scene(self.bake_anim, frames, invert=True)

    def bake_get_bone(self, bone_name):
        "Get pose bone by name."
        return self.bake_rig.pose.bones[bone_name]

    def bake_get_bones(self, bone_names):
        "Get multiple pose bones by name."
        if isinstance(bone_names, (list, set)):
            return [self.bake_get_bone(name) for name in bone_names]
        else:
            return self.bake_get_bone(bone_names)

    def bake_get_all_bone_curves(self, bone_names, props):
        "Get a list of all curves for the specified properties of the specified bones."
        return list(self.bake_curve_table.list_all_prop_curves(self.bake_get_bones(bone_names), props))

    def bake_get_all_bone_custom_prop_curves(self, bone_names, props):
        "Get a list of all curves for the specified custom properties of the specified bones."
        return self.bake_get_all_bone_curves(bone_names, [rna_idprop_quote_path(p) for p in props])

    def bake_get_bone_prop_curves(self, bone_name, prop):
        "Get an index to curve dict for the specified property of the specified bone."
        return self.bake_curve_table.get_prop_curves(self.bake_get_bone(bone_name), prop)

    def bake_get_bone_custom_prop_curves(self, bone_name, prop):
        "Get an index to curve dict for the specified custom property of the specified bone."
        return self.bake_curve_table.get_custom_prop_curves(self.bake_get_bone(bone_name), prop)

    def bake_add_curve_frames(self, curves):
        "Register frames keyed in the specified curves for baking."
        self.bake_frames_raw |= get_curve_frame_set(curves, self.bake_frame_range_raw)

    def bake_add_bone_frames(self, bone_names, props):
        "Register frames keyed for the specified properties of the specified bones for baking."
        curves = self.bake_get_all_bone_curves(bone_names, props)
        self.bake_add_curve_frames(curves)
        return curves

    def bake_replace_custom_prop_keys_constant(self, bone, prop, new_value):
        "If the property is keyframed, delete keys in bake range and re-key as Constant."
        prop_curves = self.bake_get_bone_custom_prop_curves(bone, prop)

        if prop_curves and 0 in prop_curves:
            range_raw = self.nla_to_raw(self.get_bake_range())
            delete_curve_keys_in_range(prop_curves, range_raw)
            set_custom_property_value(self.bake_rig, bone, prop, new_value, keyflags={'INSERTKEY_AVAILABLE'})
            set_curve_key_interpolation(prop_curves, 'CONSTANT', range_raw)

    # Default behavior implementation
    def bake_init(self, context):
        self.bake_rig = context.active_object
        self.bake_anim = self.bake_rig.animation_data
        self.bake_frame_range = RIGIFY_OT_get_frame_range.get_range(context)
        self.bake_frame_range_raw = self.nla_to_raw(self.bake_frame_range)
        self.bake_curve_table = ActionCurveTable(self.bake_rig)
        self.bake_current_frame = context.scene.frame_current
        self.bake_frames_raw = set()
        self.bake_state = dict()

        self.keyflags = get_keying_flags(context)
        self.keyflags_switch = None

        if context.window_manager.rigify_transfer_use_all_keys:
            self.bake_add_curve_frames(self.bake_curve_table.curve_map)

    def bake_add_frames_done(self):
        "Computes and sets the final set of frames to bake."
        frames = self.nla_from_raw(self.bake_frames_raw)
        self.bake_frames = sorted(set(map(round, frames)))

    def is_bake_empty(self):
        return len(self.bake_frames_raw) == 0

    def report_bake_empty(self):
        self.bake_add_frames_done()
        if self.is_bake_empty():
            self.report({'WARNING'}, 'No keys to bake.')
            return True
        return False

    def get_bake_range(self):
        "Returns the frame range that is being baked."
        if self.bake_frame_range:
            return self.bake_frame_range
        else:
            frames = self.bake_frames
            return (frames[0], frames[-1])

    def get_bake_range_pair(self):
        "Returns the frame range that is being baked, both in scene and action time."
        range = self.get_bake_range()
        return range, self.nla_to_raw(range)

    def bake_save_state(self, context):
        "Scans frames and collects data for baking before changing anything."
        rig = self.bake_rig
        scene = context.scene
        saved_state = self.bake_state

        try:
            self.before_save_state(context, rig)

            for frame in self.bake_frames:
                scene.frame_set(frame)
                saved_state[frame] = self.save_frame_state(context, rig)

        finally:
            self.after_save_state(context, rig)

    def bake_clean_curves_in_range(self, context, curves):
        "Deletes all keys from the given curves in the bake range."
        range, range_raw = self.get_bake_range_pair()

        context.scene.frame_set(range[0])
        delete_curve_keys_in_range(curves, range_raw)

        return range, range_raw

    def bake_apply_state(self, context):
        "Scans frames and applies the baking operation."
        rig = self.bake_rig
        scene = context.scene
        saved_state = self.bake_state

        for frame in self.bake_frames:
            scene.frame_set(frame)
            self.apply_frame_state(context, rig, saved_state.get(frame))

        clean_action_empty_curves(self.bake_rig)
        scene.frame_set(self.bake_current_frame)

    @staticmethod
    def draw_common_bake_ui(context, layout):
        layout.prop(context.window_manager, 'rigify_transfer_use_all_keys')

        RIGIFY_OT_get_frame_range.draw_range_ui(context, layout)

    @classmethod
    def poll(cls, context):
        return find_action(context.active_object) is not None

    def execute_scan_curves(self, context, obj):
        "Override to register frames to be baked, and return curves that should be cleared."
        raise NotImplementedError()

    def execute_before_apply(self, context, obj, range, range_raw):
        "Override to execute code one time before the bake apply frame scan."
        pass

    def execute(self, context):
        self.init_execute(context)
        self.bake_init(context)

        curves = self.execute_scan_curves(context, self.bake_rig)

        if self.report_bake_empty():
            return {'CANCELLED'}

        try:
            self.bake_save_state(context)

            range, range_raw = self.bake_clean_curves_in_range(context, curves)

            self.execute_before_apply(context, self.bake_rig, range, range_raw)

            self.bake_apply_state(context)

        except Exception as e:
            traceback.print_exc()
            self.report({'ERROR'}, 'Exception: ' + str(e))

        return {'FINISHED'}

    def invoke(self, context, event):
        self.init_invoke(context)

        if hasattr(self, 'draw'):
            return context.window_manager.invoke_props_dialog(self)
        else:
            return context.window_manager.invoke_confirm(self, event)


class RigifySingleUpdateMixin(RigifyOperatorMixinBase):
    """Basic framework for an operator that updates only the current frame."""

    def execute(self, context):
        self.init_execute(context)
        obj = context.active_object
        self.keyflags = get_autokey_flags(context, ignore_keyingset=True)
        self.keyflags_switch = add_flags_if_set(self.keyflags, {'INSERTKEY_AVAILABLE'})

        try:
            try:
                self.before_save_state(context, obj)
                state = self.save_frame_state(context, obj)
            finally:
                self.after_save_state(context, obj)

            self.apply_frame_state(context, obj, state)

        except Exception as e:
            traceback.print_exc()
            self.report({'ERROR'}, 'Exception: ' + str(e))

        return {'FINISHED'}

    def invoke(self, context, event):
        self.init_invoke(context)

        if hasattr(self, 'draw'):
            return context.window_manager.invoke_props_popup(self, event)
        else:
            return self.execute(context)
''']

RigifyOperatorMixinBase: Any
RigifyBakeKeyframesMixin: Any
RigifySingleUpdateMixin: Any

exec(SCRIPT_UTILITIES_BAKE[-1])

#####################################
# Generic Clear Keyframes operator ##
#####################################

SCRIPT_REGISTER_OP_CLEAR_KEYS = ['POSE_OT_rigify_clear_keyframes']

SCRIPT_UTILITIES_OP_CLEAR_KEYS = ['''
#############################
## Generic Clear Keyframes ##
#############################

class POSE_OT_rigify_clear_keyframes(bpy.types.Operator):
    bl_idname = "pose.rigify_clear_keyframes_" + rig_id
    bl_label = "Clear Keyframes And Transformation"
    bl_options = {'UNDO', 'INTERNAL'}
    bl_description = "Remove all keyframes for the relevant bones and reset transformation"

    bones: StringProperty(name="Bone List")

    @classmethod
    def poll(cls, context):
        return find_action(context.active_object) is not None

    def invoke(self, context, event):
        return context.window_manager.invoke_confirm(self, event)

    def execute(self, context):
        obj = context.active_object
        bone_list = [ obj.pose.bones[name] for name in json.loads(self.bones) ]

        curve_table = ActionCurveTable(context.active_object)
        curves = list(curve_table.list_all_prop_curves(bone_list, TRANSFORM_PROPS_ALL))

        key_range = RIGIFY_OT_get_frame_range.get_range(context)
        range_raw = nla_tweak_to_scene(obj.animation_data, key_range, invert=True)
        delete_curve_keys_in_range(curves, range_raw)

        for bone in bone_list:
            bone.location = bone.rotation_euler = (0,0,0)
            bone.rotation_quaternion = (1,0,0,0)
            bone.rotation_axis_angle = (0,0,1,0)
            bone.scale = (1,1,1)

        clean_action_empty_curves(obj)
        obj.update_tag(refresh={'TIME'})
        return {'FINISHED'}
''']


def add_clear_keyframes_button(panel: 'PanelLayout', *,
                               bones: Sequence[str] = (), text=''):
    panel.use_bake_settings()
    panel.script.add_utilities(SCRIPT_UTILITIES_OP_CLEAR_KEYS)
    panel.script.register_classes(SCRIPT_REGISTER_OP_CLEAR_KEYS)

    op_props = {'bones': json.dumps(bones)}

    panel.operator('pose.rigify_clear_keyframes_{rig_id}', text=text, icon='CANCEL',
                   properties=op_props)


###################################
# Generic Snap FK to IK operator ##
###################################

SCRIPT_REGISTER_OP_SNAP = ['POSE_OT_rigify_generic_snap', 'POSE_OT_rigify_generic_snap_bake']

SCRIPT_UTILITIES_OP_SNAP = ['''
#############################
## Generic Snap (FK to IK) ##
#############################

class RigifyGenericSnapBase:
    input_bones:   StringProperty(name="Input Chain")
    output_bones:  StringProperty(name="Output Chain")
    ctrl_bones:    StringProperty(name="Input Controls")

    tooltip:         StringProperty(name="Tooltip", default="FK to IK")
    locks:           bpy.props.BoolVectorProperty(name="Locked", size=3, default=[False,False,False])
    undo_copy_scale: bpy.props.BoolProperty(name="Undo Copy Scale", default=False)

    def init_execute(self, context):
        self.input_bone_list = json.loads(self.input_bones)
        self.output_bone_list = json.loads(self.output_bones)
        self.ctrl_bone_list = json.loads(self.ctrl_bones)

    def save_frame_state(self, context, obj):
        return get_chain_transform_matrices(obj, self.input_bone_list)

    def apply_frame_state(self, context, obj, matrices):
        set_chain_transforms_from_matrices(
            context, obj, self.output_bone_list, matrices,
            undo_copy_scale=self.undo_copy_scale, keyflags=self.keyflags,
            no_loc=self.locks[0], no_rot=self.locks[1], no_scale=self.locks[2],
        )

class POSE_OT_rigify_generic_snap(RigifyGenericSnapBase, RigifySingleUpdateMixin, bpy.types.Operator):
    bl_idname = "pose.rigify_generic_snap_" + rig_id
    bl_label = "Snap Bones"
    bl_description = "Snap on the current frame"

    @classmethod
    def description(cls, context, props):
        return "Snap " + props.tooltip + " on the current frame"

class POSE_OT_rigify_generic_snap_bake(RigifyGenericSnapBase, RigifyBakeKeyframesMixin, bpy.types.Operator):
    bl_idname = "pose.rigify_generic_snap_bake_" + rig_id
    bl_label = "Apply Snap To Keyframes"
    bl_description = "Apply snap to keyframes"

    @classmethod
    def description(cls, context, props):
        return "Apply snap " + props.tooltip + " to keyframes"

    def execute_scan_curves(self, context, obj):
        props = transform_props_with_locks(*self.locks)
        self.bake_add_bone_frames(self.ctrl_bone_list, TRANSFORM_PROPS_ALL)
        return self.bake_get_all_bone_curves(self.output_bone_list, props)
''']


def add_fk_ik_snap_buttons(panel: 'PanelLayout', op_single: str, op_bake: str, *,
                           label, rig_name='', properties: dict[str, Any],
                           clear_bones: Optional[list[str]] = None,
                           compact: Optional[bool] = None):
    assert label and properties

    if rig_name:
        label += ' (%s)' % rig_name

    if compact or not clear_bones:
        row = panel.row(align=True)
        row.operator(op_single, text=label, icon='SNAP_ON', properties=properties)
        row.operator(op_bake, text='', icon='ACTION_TWEAK', properties=properties)

        if clear_bones:
            add_clear_keyframes_button(row, bones=clear_bones)
    else:
        col = panel.column(align=True)
        col.operator(op_single, text=label, icon='SNAP_ON', properties=properties)
        row = col.row(align=True)
        row.operator(op_bake, text='Action', icon='ACTION_TWEAK', properties=properties)
        add_clear_keyframes_button(row, bones=clear_bones, text='Clear')


def add_generic_snap(panel: 'PanelLayout', *,
                     output_bones: Sequence[str] = (), input_bones: Sequence[str] = (),
                     input_ctrl_bones: Sequence[str] = (), label='Snap',
                     rig_name='', undo_copy_scale=False, compact: Optional[bool] = None,
                     clear=True, locks: Optional[Sequence[bool]] = None,
                     tooltip: Optional[str] = None):
    panel.use_bake_settings()
    panel.script.add_utilities(SCRIPT_UTILITIES_OP_SNAP)
    panel.script.register_classes(SCRIPT_REGISTER_OP_SNAP)

    op_props = {
        'output_bones': json.dumps(output_bones),
        'input_bones': json.dumps(input_bones),
        'ctrl_bones': json.dumps(input_ctrl_bones or input_bones),
    }

    if undo_copy_scale:
        op_props['undo_copy_scale'] = undo_copy_scale
    if locks is not None:
        op_props['locks'] = tuple(locks[0:3])
    if tooltip is not None:
        op_props['tooltip'] = tooltip

    clear_bones = output_bones if clear else None

    add_fk_ik_snap_buttons(
        panel, 'pose.rigify_generic_snap_{rig_id}', 'pose.rigify_generic_snap_bake_{rig_id}',
        label=label, rig_name=rig_name, properties=op_props, clear_bones=clear_bones, compact=compact,
    )


def add_generic_snap_fk_to_ik(panel: 'PanelLayout', *,
                              fk_bones: Sequence[str] = (), ik_bones: Sequence[str] = (),
                              ik_ctrl_bones: Sequence[str] = (), label='FK->IK',
                              rig_name='', undo_copy_scale=False,
                              compact: Optional[bool] = None, clear=True):
    add_generic_snap(
        panel, output_bones=fk_bones, input_bones=ik_bones, input_ctrl_bones=ik_ctrl_bones,
        label=label, rig_name=rig_name, undo_copy_scale=undo_copy_scale, compact=compact, clear=clear
    )


###############################
# Module register/unregister ##
###############################

def register():
    from bpy.utils import register_class

    exec(_SCRIPT_REGISTER_WM_PROPS)

    register_class(RIGIFY_OT_get_frame_range)


def unregister():
    from bpy.utils import unregister_class

    exec(_SCRIPT_UNREGISTER_WM_PROPS)

    unregister_class(RIGIFY_OT_get_frame_range)
