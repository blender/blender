# SPDX-FileCopyrightText: 2021-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Pose Library - creation functions.
"""

import dataclasses
import functools
import re

from typing import Optional, FrozenSet, Set, Union, Iterable, cast

if "functions" not in locals():
    from . import asset_browser, functions
else:
    import importlib

    asset_browser = importlib.reload(asset_browser)
    functions = importlib.reload(functions)

import bpy
from bpy.types import (
    Action,
    ActionChannelbag,
    ActionSlot,
    Bone,
    Context,
    FCurve,
    Keyframe,
)

FCurveValue = Union[float, int]

pose_bone_re = re.compile(r'pose.bones\["([^"]+)"\]')
"""RegExp for matching FCurve data paths."""


@dataclasses.dataclass(unsafe_hash=True, frozen=True)
class PoseCreationParams:
    armature_ob: bpy.types.Object
    src_action: Optional[Action]
    src_action_slot: Optional[ActionSlot]
    src_frame_nr: float
    bone_names: FrozenSet[str]
    new_asset_name: str


class UnresolvablePathError(ValueError):
    """Raised when a data_path cannot be resolved to a current value."""


@dataclasses.dataclass(unsafe_hash=True)
class PoseActionCreator:
    """Create an Action that's suitable for marking as Asset.

    Does not mark as asset yet, nor does it add asset metadata.
    """

    params: PoseCreationParams

    # These were taken from Blender's Action baking code in `anim_utils.py`.
    # Items are (name, array_length) tuples.
    _bbone_props = [
        ("bbone_curveinx", None),
        ("bbone_curveoutx", None),
        ("bbone_curveinz", None),
        ("bbone_curveoutz", None),
        ("bbone_rollin", None),
        ("bbone_rollout", None),
        ("bbone_scalein", 3),
        ("bbone_scaleout", 3),
        ("bbone_easein", None),
        ("bbone_easeout", None),
    ]

    def create(self) -> Optional[Action]:
        """Create a single-frame Action containing only the given bones, or None if no anim data was found."""
        from bpy_extras import anim_utils

        try:
            dst_action = self._create_new_action()
            slot = dst_action.slots.new(self.params.armature_ob.id_type, self.params.armature_ob.name)
            channelbag = anim_utils.action_ensure_channelbag_for_slot(dst_action, slot)
            self._store_pose(channelbag)
        finally:
            # Prevent next instantiations of this class from reusing pointers to
            # bones. They may not be valid by then any more.
            self._find_bone.cache_clear()

        if len(channelbag.fcurves) == 0:
            bpy.data.actions.remove(dst_action)
            return None

        return dst_action

    def _create_new_action(self) -> Action:
        dst_action = bpy.data.actions.new(self.params.new_asset_name)
        dst_action.user_clear()  # actions.new() sets users=1, but marking as asset also increments user count.
        return dst_action

    def _store_pose(self, dst_channelbag: ActionChannelbag) -> None:
        """Store the current pose into the given action."""
        self._store_bone_pose_parameters(dst_channelbag)
        self._store_animated_parameters(dst_channelbag)

    def _store_bone_pose_parameters(self, dst_channelbag: ActionChannelbag) -> None:
        """Store loc/rot/scale/bbone values in the Action."""

        for bone_name in sorted(self.params.bone_names):
            self._store_location(dst_channelbag, bone_name)
            self._store_rotation(dst_channelbag, bone_name)
            self._store_scale(dst_channelbag, bone_name)
            self._store_bbone(dst_channelbag, bone_name)

    def _store_animated_parameters(self, dst_channelbag: ActionChannelbag) -> None:
        """Store the current value of any animated bone properties."""
        from bpy_extras import anim_utils

        if self.params.src_action is None:
            return
        src_channelbag = anim_utils.action_get_channelbag_for_slot(self.params.src_action, self.params.src_action_slot)
        if not src_channelbag:
            return

        armature_ob = self.params.armature_ob
        for fcurve in src_channelbag.fcurves:
            match = pose_bone_re.match(fcurve.data_path)
            if not match:
                # Not animating a bone property.
                continue

            bone_name = match.group(1)
            if bone_name not in self.params.bone_names:
                # Bone is not our export set.
                continue

            if dst_channelbag.fcurves.find(fcurve.data_path, index=fcurve.array_index):
                # This property is already handled by a previous _store_xxx() call.
                continue

            # Only include in the pose if there is a key on this frame.
            if not self._has_key_on_frame(fcurve):
                continue

            try:
                value = self._current_value(armature_ob, fcurve.data_path, fcurve.array_index)
            except UnresolvablePathError:
                # A once-animated property no longer exists.
                continue

            dst_fcurve = dst_channelbag.fcurves.new(fcurve.data_path, index=fcurve.array_index, group_name=bone_name)
            dst_fcurve.keyframe_points.insert(self.params.src_frame_nr, value=value)
            dst_fcurve.update()

    def _store_location(self, dst_channelbag: ActionChannelbag, bone_name: str) -> None:
        """Store bone location."""
        self._store_bone_array(dst_channelbag, bone_name, "location", 3)

    def _store_rotation(self, dst_channelbag: ActionChannelbag, bone_name: str) -> None:
        """Store bone rotation given current rotation mode."""
        bone = self._find_bone(bone_name)
        if bone.rotation_mode == "QUATERNION":
            self._store_bone_array(dst_channelbag, bone_name, "rotation_quaternion", 4)
        elif bone.rotation_mode == "AXIS_ANGLE":
            self._store_bone_array(dst_channelbag, bone_name, "rotation_axis_angle", 4)
        else:
            self._store_bone_array(dst_channelbag, bone_name, "rotation_euler", 3)

    def _store_scale(self, dst_channelbag: ActionChannelbag, bone_name: str) -> None:
        """Store bone scale."""
        self._store_bone_array(dst_channelbag, bone_name, "scale", 3)

    def _store_bbone(self, dst_channelbag: ActionChannelbag, bone_name: str) -> None:
        """Store bendy-bone parameters."""
        for prop_name, array_length in self._bbone_props:
            if array_length:
                self._store_bone_array(dst_channelbag, bone_name, prop_name, array_length)
            else:
                self._store_bone_property(dst_channelbag, bone_name, prop_name)

    def _store_bone_array(
            self,
            dst_channelbag: ActionChannelbag,
            bone_name: str,
            property_name: str,
            array_length: int) -> None:
        """Store all elements of an array property."""
        for array_index in range(array_length):
            self._store_bone_property(dst_channelbag, bone_name, property_name, array_index)

    def _store_bone_property(
        self,
        dst_channelbag: ActionChannelbag,
        bone_name: str,
        property_path: str,
        array_index: int = -1,
    ) -> None:
        """Store the current value of a single bone property."""

        bone = self._find_bone(bone_name)
        value = self._current_value(bone, property_path, array_index)

        # Get the full 'pose.bones["bone_name"].blablabla' path suitable for FCurves.
        rna_path = bone.path_from_id(property_path)

        fcurve: Optional[FCurve] = dst_channelbag.fcurves.find(rna_path, index=array_index)
        if fcurve is None:
            fcurve = dst_channelbag.fcurves.new(rna_path, index=array_index, group_name=bone_name)
            assert fcurve is not None

        fcurve.keyframe_points.insert(self.params.src_frame_nr, value=value)
        fcurve.update()

    @classmethod
    def _current_value(cls, datablock: bpy.types.ID, data_path: str, array_index: int) -> FCurveValue:
        """Resolve an RNA path + array index to an actual value."""
        value_or_array = cls._path_resolve(datablock, data_path)

        if isinstance(value_or_array, str):
            # Enums resolve to a string.
            bone_path, enum_property_name = data_path.rsplit("[", 1)
            unescaped_property_name = bpy.utils.unescape_identifier(enum_property_name)
            # unescaped_property_name still has the quotes and a bracket at the end hence the [1:-2].
            value = cls._path_resolve(datablock, bone_path).get(unescaped_property_name[1:-2])
            assert isinstance(value, (int, float))
            return cast(FCurveValue, value)

        # Both indices -1 and 0 are used for non-array properties.
        # -1 cannot be used in arrays, whereas 0 can be used in both arrays and non-arrays.

        if array_index == -1:
            return cast(FCurveValue, value_or_array)

        if array_index == 0:
            value_or_array = cls._path_resolve(datablock, data_path)
            try:
                # MyPy doesn't understand this try/except is to determine the type.
                value = value_or_array[array_index]  # type: ignore
            except TypeError:
                # Not an array after all.
                return cast(FCurveValue, value_or_array)
            return cast(FCurveValue, value)

        # MyPy doesn't understand that array_index>0 implies this is indexable.
        return cast(FCurveValue, value_or_array[array_index])  # type: ignore

    @staticmethod
    def _path_resolve(datablock: bpy.types.ID, data_path: str) -> Union[FCurveValue, Iterable[FCurveValue]]:
        """Wrapper for datablock.path_resolve(data_path).

        Raise UnresolvablePathError when the path cannot be resolved.
        This is easier to deal with upstream than the generic ValueError raised
        by Blender.
        """
        try:
            return datablock.path_resolve(data_path)  # type: ignore
        except ValueError as ex:
            raise UnresolvablePathError(str(ex)) from ex

    @functools.lru_cache(maxsize=1024)
    def _find_bone(self, bone_name: str) -> Bone:
        """Find a bone by name.

        Assumes the named bone exists, as the bones this class handles comes
        from the user's selection, and you can't select a non-existent bone.
        """

        bone: Bone = self.params.armature_ob.pose.bones[bone_name]
        return bone

    def _has_key_on_frame(self, fcurve: FCurve) -> bool:
        """Return True iff the FCurve has a key on the source frame."""

        points = fcurve.keyframe_points
        if not points:
            return False

        frame_to_find = self.params.src_frame_nr
        margin = 0.001
        high = len(points) - 1
        low = 0
        while low <= high:
            mid = (high + low) // 2
            diff = points[mid].co.x - frame_to_find
            if abs(diff) < margin:
                return True
            if diff < 0:
                # Frame to find is bigger than the current middle.
                low = mid + 1
            else:
                # Frame to find is smaller than the current middle
                high = mid - 1
        return False


def create_pose_asset(
    params: PoseCreationParams,
) -> Optional[Action]:
    """Create a single-frame Action containing only the pose of the given bones.

    DOES mark as asset, DOES NOT configure asset metadata.
    """

    creator = PoseActionCreator(params)
    pose_action = creator.create()
    if pose_action is None:
        return None

    pose_action.asset_mark()
    pose_action.asset_generate_preview()
    return pose_action


def create_pose_asset_from_context(context: Context, new_asset_name: str) -> Optional[Action]:
    """Create Action asset from active object & selected bones."""

    bones = context.selected_pose_bones_from_active_object
    bone_names = {bone.name for bone in bones}

    params = PoseCreationParams(
        context.object,
        getattr(context.object.animation_data, "action", None),
        getattr(context.object.animation_data, "action_slot", None),
        context.scene.frame_current,
        frozenset(bone_names),
        new_asset_name,
    )

    return create_pose_asset(params)


def create_single_key_fcurve(dst_channelbag: ActionChannelbag, src_fcurve: FCurve, src_keyframe: Keyframe) -> FCurve:
    """Create a copy of the source FCurve, but only for the given keyframe.

    Returns a new FCurve with just one keyframe.
    """

    dst_fcurve = copy_fcurve_without_keys(dst_channelbag, src_fcurve)
    copy_keyframe(dst_fcurve, src_keyframe)
    return dst_fcurve


def copy_fcurve_without_keys(dst_channelbag: ActionChannelbag, src_fcurve: FCurve) -> FCurve:
    """Create a new FCurve and copy some properties."""

    src_group_name = src_fcurve.group.name if src_fcurve.group else ""
    dst_fcurve = dst_channelbag.fcurves.new(
        src_fcurve.data_path,
        index=src_fcurve.array_index,
        group_name=src_group_name)
    for propname in {"auto_smoothing", "color", "color_mode", "extrapolation"}:
        setattr(dst_fcurve, propname, getattr(src_fcurve, propname))
    return dst_fcurve


def copy_keyframe(dst_fcurve: FCurve, src_keyframe: Keyframe) -> Keyframe:
    """Copy a keyframe from one FCurve to the other."""

    dst_keyframe = dst_fcurve.keyframe_points.insert(
        src_keyframe.co.x, src_keyframe.co.y, options={'FAST'}, keyframe_type=src_keyframe.type
    )

    for propname in {
        "amplitude",
        "back",
        "easing",
        "handle_left",
        "handle_left_type",
        "handle_right",
        "handle_right_type",
        "interpolation",
        "period",
    }:
        setattr(dst_keyframe, propname, getattr(src_keyframe, propname))
    dst_fcurve.update()
    return dst_keyframe


def find_keyframe(fcurve: FCurve, frame: float) -> Optional[Keyframe]:
    # Binary search adapted from https://pythonguides.com/python-binary-search/
    keyframes = fcurve.keyframe_points
    low = 0
    high = len(keyframes) - 1
    mid = 0

    # Accept any keyframe that's within 'epsilon' of the requested frame.
    # This should account for rounding errors and the likes.
    epsilon = 1e-4
    frame_lowerbound = frame - epsilon
    frame_upperbound = frame + epsilon
    while low <= high:
        mid = (high + low) // 2
        keyframe = keyframes[mid]
        if keyframe.co.x < frame_lowerbound:
            low = mid + 1
        elif keyframe.co.x > frame_upperbound:
            high = mid - 1
        else:
            return keyframe
    return None


def assign_from_asset_browser(asset: Action, asset_browser_area: bpy.types.Area) -> None:
    """Assign some things from the asset browser to the asset.

    This sets the current catalog ID, and in the future could include tags
    from the active dynamic catalog, etc.
    """

    cat_id = asset_browser.active_catalog_id(asset_browser_area)
    asset.asset_data.catalog_id = cat_id
