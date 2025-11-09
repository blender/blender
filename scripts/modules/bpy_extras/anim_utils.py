# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "AutoKeying",

    "bake_action",
    "bake_action_objects",

    "bake_action_iter",
    "bake_action_objects_iter",

    "BakeOptions",
)

import contextlib
from dataclasses import dataclass
from typing import Iterable, Optional, Union, Iterator
from collections.abc import (
    Mapping,
    Sequence,
)

import bpy
from bpy.types import (
    Context, Action, ActionSlot, ActionChannelbag,
    Object, PoseBone, KeyingSet,
)

from rna_prop_ui import (
    rna_idprop_value_to_python,
)

FCurveKey = tuple[
    # `fcurve.data_path`.
    str,
    # `fcurve.array_index`.
    int,
]

# List of `[frame0, value0, frame1, value1, ...]` pairs.
ListKeyframes = list[float]


@dataclass
class BakeOptions:
    only_selected: bool
    """Only bake selected bones."""

    do_pose: bool
    """Bake pose channels"""

    do_object: bool
    """Bake objects."""

    do_visual_keying: bool
    """Use the final transformations for baking ('visual keying')."""

    do_constraint_clear: bool
    """Remove constraints after baking."""

    do_parents_clear: bool
    """Unparent after baking objects."""

    do_clean: bool
    """Remove redundant keyframes after baking."""

    do_location: bool
    """Bake location channels"""

    do_rotation: bool
    """Bake rotation channels"""

    do_scale: bool
    """Bake scale channels"""

    do_bbone: bool
    """Bake b-bone channels"""

    do_custom_props: bool
    """Bake custom properties."""


def action_get_channelbag_for_slot(action: Action | None, slot: ActionSlot | None) -> ActionChannelbag | None:
    """
    Returns the first channelbag found for the slot.
    In case there are multiple layers or strips they are iterated until a
    channelbag for that slot is found. In case no matching channelbag is found, returns None.
    """
    if not action or not slot:
        # This is just for convenience so that you can call
        # action_get_channelbag_for_slot(adt.action, adt.action_slot) and check
        # the return value for None, without having to also check the action and
        # the slot for None.
        return None

    for layer in action.layers:
        for strip in layer.strips:
            channelbag = strip.channelbag(slot)
            if channelbag:
                return channelbag
    return None


def action_get_first_suitable_slot(action: Action | None, target_id_type: str) -> ActionSlot | None:
    """Return the first Slot of the given Action that's suitable for the given ID type.

    Typically you should not need this function; when an Action is assigned to a
    data-block, just use the slot that was assigned along with it.
    """

    if not action:
        return None

    slot_types = ('UNSPECIFIED', target_id_type)
    for slot in action.slots:
        if slot.target_id_type in slot_types:
            return slot
    return None


def action_ensure_channelbag_for_slot(action: Action, slot: ActionSlot) -> ActionChannelbag:
    """Ensure a layer and a keyframe strip exists, then ensure that strip has a channelbag for the slot."""

    try:
        layer = action.layers[0]
    except IndexError:
        layer = action.layers.new("Layer")

    try:
        strip = layer.strips[0]
    except IndexError:
        strip = layer.strips.new(type='KEYFRAME')

    return strip.channelbag(slot, ensure=True)


def animdata_get_channelbag_for_assigned_slot(anim_data) -> ActionChannelbag:
    """Return the channelbag used in the given anim_data or None if there is no Action
    + Slot combination defined."""
    if not anim_data:
        return None
    if not anim_data.action or not anim_data.action_slot:
        return None
    return action_get_channelbag_for_slot(anim_data.action, anim_data.action_slot)


def bake_action(
        obj,
        *,
        action,
        frames,
        bake_options,
):
    """
    :arg obj: Object to bake.
    :type obj: :class:`bpy.types.Object`
    :arg action: An action to bake the data into, or None for a new action
       to be created.
    :type action: :class:`bpy.types.Action` | None
    :arg frames: Frames to bake.
    :type frames: int
    :arg bake_options: Options for baking.
    :type bake_options: :class:`anim_utils.BakeOptions`
    :return: Action or None.
    :rtype: :class:`bpy.types.Action` | None
    """
    if not (bake_options.do_pose or bake_options.do_object):
        return None

    action, = bake_action_objects(
        [(obj, action)],
        frames=frames,
        bake_options=bake_options
    )
    return action


def bake_action_objects(
        object_action_pairs,
        *,
        frames,
        bake_options,
):
    """
    A version of :func:`bake_action_objects_iter` that takes frames and returns the output.

    :arg frames: Frames to bake.
    :type frames: iterable of int
    :arg bake_options: Options for baking.
    :type bake_options: :class:`anim_utils.BakeOptions`

    :return: A sequence of Action or None types (aligned with ``object_action_pairs``)
    :rtype: Sequence[:class:`bpy.types.Action`]
    """
    if not (bake_options.do_pose or bake_options.do_object):
        return []

    iter = bake_action_objects_iter(object_action_pairs, bake_options=bake_options)
    iter.send(None)
    for frame in frames:
        iter.send(frame)
    return iter.send(None)


def bake_action_objects_iter(
        object_action_pairs,
        bake_options,
):
    """
    An coroutine that bakes actions for multiple objects.

    :arg object_action_pairs: Sequence of object action tuples,
       action is the destination for the baked data. When None a new action will be created.
    :type object_action_pairs: Sequence of (:class:`bpy.types.Object`, :class:`bpy.types.Action`)
    :arg bake_options: Options for baking.
    :type bake_options: :class:`anim_utils.BakeOptions`
    """
    scene = bpy.context.scene
    frame_back = scene.frame_current
    iter_all = tuple(
        bake_action_iter(obj, action=action, bake_options=bake_options)
        for (obj, action) in object_action_pairs
    )
    for iter in iter_all:
        iter.send(None)
    while True:
        frame = yield None
        if frame is None:
            break
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        for iter in iter_all:
            iter.send(frame)
    scene.frame_set(frame_back)
    yield tuple(iter.send(None) for iter in iter_all)


# XXX visual keying is actually always considered as True in this code...
def bake_action_iter(
        obj,
        *,
        action,
        bake_options,
):
    """
    An coroutine that bakes action for a single object.

    :arg obj: Object to bake.
    :type obj: :class:`bpy.types.Object`
    :arg action: An action to bake the data into, or None for a new action
       to be created.
    :type action: :class:`bpy.types.Action` | None
    :arg bake_options: Boolean options of what to include into the action bake.
    :type bake_options: :class:`anim_utils.BakeOptions`

    :return: an action or None
    :rtype: :class:`bpy.types.Action`
    """
    # -------------------------------------------------------------------------
    # Helper Functions and vars

    # Note: BBONE_PROPS is a list so we can preserve the ordering
    BBONE_PROPS = [
        "bbone_curveinx", "bbone_curveoutx",
        "bbone_curveinz", "bbone_curveoutz",
        "bbone_rollin", "bbone_rollout",
        "bbone_scalein", "bbone_scaleout",
        "bbone_easein", "bbone_easeout",
    ]
    BBONE_PROPS_LENGTHS = {
        "bbone_curveinx": 1,
        "bbone_curveoutx": 1,
        "bbone_curveinz": 1,
        "bbone_curveoutz": 1,
        "bbone_rollin": 1,
        "bbone_rollout": 1,
        "bbone_scalein": 3,
        "bbone_scaleout": 3,
        "bbone_easein": 1,
        "bbone_easeout": 1,
    }

    def can_be_keyed(value):
        """Returns a tri-state boolean.

        - True: known to be keyable.
        - False: known to not be keyable.
        - None: unknown, might be an enum property for which RNA uses a string to
            indicate a specific item (keyable) or an actual string property (not
            keyable).
        """
        if isinstance(value, (int, float, bool)):
            # These types are certainly keyable.
            return True
        if isinstance(value, (list, tuple, set, dict)):
            # These types are certainly not keyable.
            return False
        # Maybe this could be made stricter, as also ID pointer properties and
        # some other types cannot be keyed. However, the above checks are enough
        # to fix the crash that this code was written for (#117988).
        return None

    # Convert rna_prop types (IDPropertyArray, etc) to python types.
    def clean_custom_properties(obj):
        if not bake_options.do_custom_props:
            # Don't bother remembering any custom properties when they're not
            # going to be baked anyway.
            return {}

        # Be careful about which properties to actually consider for baking, as
        # keeping references to complex Blender data-structures around for too long
        # can cause crashes. See #117988.
        clean_props = {
            key: rna_idprop_value_to_python(value)
            for key, value in obj.items()
            if can_be_keyed(value) is not False
        }
        return clean_props

    def bake_custom_properties(obj, *, custom_props, frame, group_name=""):
        import idprop
        if frame is None or not custom_props:
            return
        for key, value in custom_props.items():
            if key in obj.bl_rna.properties and not obj.bl_rna.properties[key].is_animatable:
                continue
            if isinstance(obj[key], idprop.types.IDPropertyGroup):
                continue
            obj[key] = value
            # The check for `is_runtime` is needed in case the custom property has the same
            # name as a built in property, e.g. `scale`. In that case the simple check
            # `key in ...` would be true and the square brackets would never get added.
            if key in obj.bl_rna.properties and obj.bl_rna.properties[key].is_runtime:
                rna_path = key
            else:
                rna_path = "[\"{:s}\"]".format(bpy.utils.escape_identifier(key))
            try:
                obj.keyframe_insert(rna_path, frame=frame, group=group_name)
            except TypeError:
                # The is_animatable check above is per property. A property in isolation
                # may be considered animatable, but it could be owned by a data-block that
                # itself cannot be animated.
                continue

    def pose_frame_info(obj):
        matrix = {}
        bbones = {}
        custom_props = {}
        for name, pbone in obj.pose.bones.items():
            if bake_options.do_visual_keying:
                # Get the final transform of the bone in its own local space...
                matrix[name] = obj.convert_space(
                    pose_bone=pbone, matrix=pbone.matrix,
                    from_space='POSE', to_space='LOCAL',
                )
            else:
                matrix[name] = pbone.matrix_basis.copy()

            # Bendy Bones
            if pbone.bone.bbone_segments > 1:
                bbones[name] = {bb_prop: getattr(pbone, bb_prop) for bb_prop in BBONE_PROPS}

            # Custom Properties
            custom_props[name] = clean_custom_properties(pbone)

        return matrix, bbones, custom_props

    def armature_frame_info(obj):
        if obj.type != 'ARMATURE':
            return {}
        return clean_custom_properties(obj)

    if bake_options.do_parents_clear:
        if bake_options.do_visual_keying:
            def obj_frame_info(obj):
                return obj.matrix_world.copy(), clean_custom_properties(obj)
        else:
            def obj_frame_info(obj):
                parent = obj.parent
                matrix = obj.matrix_basis
                if parent:
                    return parent.matrix_world @ matrix, clean_custom_properties(obj)
                else:
                    return matrix.copy(), clean_custom_properties(obj)
    else:
        if bake_options.do_visual_keying:
            def obj_frame_info(obj):
                parent = obj.parent
                matrix = obj.matrix_world
                if parent:
                    return parent.matrix_world.inverted_safe() @ matrix, clean_custom_properties(obj)
                else:
                    return matrix.copy(), clean_custom_properties(obj)
        else:
            def obj_frame_info(obj):
                return obj.matrix_basis.copy(), clean_custom_properties(obj)

    # -------------------------------------------------------------------------
    # Setup the Context

    if obj.pose is None:
        bake_options.do_pose = False

    if not (bake_options.do_pose or bake_options.do_object):
        raise Exception("Pose and object baking is disabled, no action needed")

    pose_info = []
    armature_info = []
    obj_info = []

    # -------------------------------------------------------------------------
    # Collect transformations

    while True:
        # Caller is responsible for setting the frame and updating the scene.
        frame = yield None

        # Signal we're done!
        if frame is None:
            break
        if bake_options.do_pose:
            pose_info.append((frame, *pose_frame_info(obj)))
            armature_info.append((frame, armature_frame_info(obj)))
        if bake_options.do_object:
            obj_info.append((frame, *obj_frame_info(obj)))

    # -------------------------------------------------------------------------
    # Create action

    # in case animation data hasn't been created
    atd = obj.animation_data_create()
    old_slot_name = atd.last_slot_identifier[2:]
    is_new_action = action is None
    if is_new_action:
        action = bpy.data.actions.new("Action")

    # Only leave tweak mode if we actually need to modify the action (#57159)
    if action != atd.action:
        # Leave tweak mode before trying to modify the action (#48397)
        if atd.use_tweak_mode:
            atd.use_tweak_mode = False
        atd.action = action

    # A slot needs to be assigned.
    if not atd.action_slot:
        slot = action.slots.new(obj.id_type, old_slot_name or obj.name)
        atd.action_slot = slot

    # Baking the action only makes sense in Replace mode, so force it (#69105)
    if not atd.use_tweak_mode:
        atd.action_blend_type = 'REPLACE'

    # If any data is going to be baked, there will be a channelbag created, so
    # might just as well create it now and have a clear, unambiguous reference
    # to it. If it is created here, it will have no F-Curves, and so certain
    # loops below will just be no-ops.
    channelbag: ActionChannelbag = action_ensure_channelbag_for_slot(atd.action, atd.action_slot)

    # -------------------------------------------------------------------------
    # Clean (store initial data)
    if bake_options.do_clean:
        clean_orig_data = {fcu: {p.co[1] for p in fcu.keyframe_points} for fcu in channelbag.fcurves}
    else:
        clean_orig_data = {}

    # -------------------------------------------------------------------------
    # Apply transformations to action

    # pose
    lookup_fcurves = {(fcurve.data_path, fcurve.array_index): fcurve for fcurve in channelbag.fcurves}

    if bake_options.do_pose:
        for f, armature_custom_properties in armature_info:
            bake_custom_properties(
                obj,
                custom_props=armature_custom_properties,
                frame=f,
                group_name="Armature Custom Properties"
            )

        for name, pbone in obj.pose.bones.items():
            if bake_options.only_selected and not pbone.select:
                continue

            if bake_options.do_constraint_clear:
                while pbone.constraints:
                    pbone.constraints.remove(pbone.constraints[0])

            # Create compatible euler & quaternion rotation values.
            euler_prev = None
            quat_prev = None

            base_fcurve_path = pbone.path_from_id() + "."
            path_location = base_fcurve_path + "location"
            path_quaternion = base_fcurve_path + "rotation_quaternion"
            path_axis_angle = base_fcurve_path + "rotation_axis_angle"
            path_euler = base_fcurve_path + "rotation_euler"
            path_scale = base_fcurve_path + "scale"
            paths_bbprops = [(base_fcurve_path + bbprop) for bbprop in BBONE_PROPS]

            keyframes = KeyframesCo()

            if bake_options.do_location:
                keyframes.add_paths(path_location, 3)
            if bake_options.do_rotation:
                keyframes.add_paths(path_quaternion, 4)
                keyframes.add_paths(path_axis_angle, 4)
                keyframes.add_paths(path_euler, 3)
            if bake_options.do_scale:
                keyframes.add_paths(path_scale, 3)

            if bake_options.do_bbone and pbone.bone.bbone_segments > 1:
                for prop_name, path in zip(BBONE_PROPS, paths_bbprops):
                    keyframes.add_paths(path, BBONE_PROPS_LENGTHS[prop_name])

            rotation_mode = pbone.rotation_mode
            total_new_keys = len(pose_info)
            for (f, matrix, bbones, custom_props) in pose_info:
                pbone.matrix_basis = matrix[name].copy()

                if bake_options.do_location:
                    keyframes.extend_co_values(path_location, 3, f, pbone.location)

                if bake_options.do_rotation:
                    if rotation_mode == 'QUATERNION':
                        if quat_prev is not None:
                            quat = pbone.rotation_quaternion.copy()
                            quat.make_compatible(quat_prev)
                            pbone.rotation_quaternion = quat
                            quat_prev = quat
                            del quat
                        else:
                            quat_prev = pbone.rotation_quaternion.copy()
                        keyframes.extend_co_values(path_quaternion, 4, f, pbone.rotation_quaternion)
                    elif rotation_mode == 'AXIS_ANGLE':
                        keyframes.extend_co_values(path_axis_angle, 4, f, pbone.rotation_axis_angle)
                    else:  # euler, XYZ, ZXY etc
                        if euler_prev is not None:
                            euler = pbone.matrix_basis.to_euler(pbone.rotation_mode, euler_prev)
                            pbone.rotation_euler = euler
                            del euler
                        euler_prev = pbone.rotation_euler.copy()
                        keyframes.extend_co_values(path_euler, 3, f, pbone.rotation_euler)

                if bake_options.do_scale:
                    keyframes.extend_co_values(path_scale, 3, f, pbone.scale)

                # Bendy Bones
                if bake_options.do_bbone and pbone.bone.bbone_segments > 1:
                    bbone_shape = bbones[name]
                    for prop_index, prop_name in enumerate(BBONE_PROPS):
                        prop_len = BBONE_PROPS_LENGTHS[prop_name]
                        if prop_len > 1:
                            keyframes.extend_co_values(
                                paths_bbprops[prop_index], prop_len, f, bbone_shape[prop_name]
                            )
                        else:
                            keyframes.extend_co_value(
                                paths_bbprops[prop_index], f, bbone_shape[prop_name]
                            )
                # Custom Properties
                if bake_options.do_custom_props:
                    bake_custom_properties(pbone, custom_props=custom_props[name], frame=f, group_name=name)

            if is_new_action:
                keyframes.insert_keyframes_into_new_action(total_new_keys, channelbag, name)
            else:
                keyframes.insert_keyframes_into_existing_action(
                    lookup_fcurves, total_new_keys, channelbag)

    # object. TODO. multiple objects
    if bake_options.do_object:
        if bake_options.do_constraint_clear:
            while obj.constraints:
                obj.constraints.remove(obj.constraints[0])

        # Create compatible euler & quaternion rotations.
        euler_prev = None
        quat_prev = None

        path_location = "location"
        path_quaternion = "rotation_quaternion"
        path_axis_angle = "rotation_axis_angle"
        path_euler = "rotation_euler"
        path_scale = "scale"

        keyframes = KeyframesCo()
        if bake_options.do_location:
            keyframes.add_paths(path_location, 3)
        if bake_options.do_rotation:
            keyframes.add_paths(path_quaternion, 4)
            keyframes.add_paths(path_axis_angle, 4)
            keyframes.add_paths(path_euler, 3)
        if bake_options.do_scale:
            keyframes.add_paths(path_scale, 3)

        rotation_mode = obj.rotation_mode
        total_new_keys = len(obj_info)
        for (f, matrix, custom_props) in obj_info:
            name = "Action Bake"  # XXX: placeholder
            obj.matrix_basis = matrix

            if bake_options.do_location:
                keyframes.extend_co_values(path_location, 3, f, obj.location)

            if bake_options.do_rotation:
                if rotation_mode == 'QUATERNION':
                    if quat_prev is not None:
                        quat = obj.rotation_quaternion.copy()
                        quat.make_compatible(quat_prev)
                        obj.rotation_quaternion = quat
                        quat_prev = quat
                        del quat
                    else:
                        quat_prev = obj.rotation_quaternion.copy()
                    keyframes.extend_co_values(path_quaternion, 4, f, obj.rotation_quaternion)

                elif rotation_mode == 'AXIS_ANGLE':
                    keyframes.extend_co_values(path_axis_angle, 4, f, obj.rotation_axis_angle)
                else:  # euler, XYZ, ZXY etc
                    if euler_prev is not None:
                        obj.rotation_euler = matrix.to_euler(obj.rotation_mode, euler_prev)
                    euler_prev = obj.rotation_euler.copy()
                    keyframes.extend_co_values(path_euler, 3, f, obj.rotation_euler)

            if bake_options.do_scale:
                keyframes.extend_co_values(path_scale, 3, f, obj.scale)

            if bake_options.do_custom_props:
                bake_custom_properties(obj, custom_props=custom_props, frame=f, group_name=name)

        if is_new_action:
            keyframes.insert_keyframes_into_new_action(total_new_keys, channelbag, name)
        else:
            keyframes.insert_keyframes_into_existing_action(lookup_fcurves, total_new_keys, channelbag)

        if bake_options.do_parents_clear:
            obj.parent = None

    # -------------------------------------------------------------------------
    # Clean

    if bake_options.do_clean:
        for fcu in channelbag.fcurves:
            fcu_orig_data = clean_orig_data.get(fcu, set())

            keyframe_points = fcu.keyframe_points
            i = 1
            while i < len(keyframe_points) - 1:
                val = keyframe_points[i].co[1]

                if val in fcu_orig_data:
                    i += 1
                    continue

                val_prev = keyframe_points[i - 1].co[1]
                val_next = keyframe_points[i + 1].co[1]

                if abs(val - val_prev) + abs(val - val_next) < 0.0001:
                    keyframe_points.remove(keyframe_points[i])
                else:
                    i += 1

    yield action


class KeyframesCo:
    """
    A buffer for keyframe Co unpacked values per ``FCurveKey``. ``FCurveKeys`` are added using
    ``add_paths()``, Co values stored using extend_co_values(), then finally use
    ``insert_keyframes_into_*_action()`` for efficiently inserting keys into the F-curves.

    Users are limited to one Action Group per instance.
    """
    __slots__ = (
        "keyframes_from_fcurve",
    )

    # `keyframes[(rna_path, array_index)] = list(time0,value0, time1,value1,...)`.
    keyframes_from_fcurve: Mapping[FCurveKey, ListKeyframes]

    def __init__(self):
        self.keyframes_from_fcurve = {}

    def add_paths(
        self,
        rna_path: str,
        total_indices: int,
    ) -> None:
        keyframes_from_fcurve = self.keyframes_from_fcurve
        for array_index in range(0, total_indices):
            keyframes_from_fcurve[(rna_path, array_index)] = []

    def extend_co_values(
        self,
        rna_path: str,
        total_indices: int,
        frame: float,
        values: Sequence[float],
    ) -> None:
        keyframes_from_fcurve = self.keyframes_from_fcurve
        for array_index in range(0, total_indices):
            keyframes_from_fcurve[(rna_path, array_index)].extend((frame, values[array_index]))

    def extend_co_value(
        self,
        rna_path: str,
        frame: float,
        value: float,
    ) -> None:
        self.keyframes_from_fcurve[(rna_path, 0)].extend((frame, value))

    def insert_keyframes_into_new_action(
        self,
        total_new_keys: int,
        channelbag: ActionChannelbag,
        group_name: str,
    ) -> None:
        """
        Assumes the action is new, that it has no F-curves. Otherwise, the only difference between versions is
        performance and implementation simplicity.

        :arg group_name: Name of the Group that F-curves are added to.
        """
        linear_enum_values = [
            bpy.types.Keyframe.bl_rna.properties["interpolation"].enum_items["LINEAR"].value
        ] * total_new_keys

        for fc_key, key_values in self.keyframes_from_fcurve.items():
            if len(key_values) == 0:
                continue

            data_path, array_index = fc_key
            keyframe_points = channelbag.fcurves.new(
                data_path, index=array_index, group_name=group_name
            ).keyframe_points

            keyframe_points.add(total_new_keys)
            keyframe_points.foreach_set("co", key_values)
            keyframe_points.foreach_set("interpolation", linear_enum_values)

            # There's no need to do fcurve.update() because the keys are already ordered, have
            # no duplicates and all handles are Linear.

    def insert_keyframes_into_existing_action(
        self,
        lookup_fcurves: Mapping[FCurveKey, bpy.types.FCurve],
        total_new_keys: int,
        channelbag: ActionChannelbag,
    ) -> None:
        """
        Assumes the action already exists, that it might already have F-curves. Otherwise, the
        only difference between versions is performance and implementation simplicity.

        :arg lookup_fcurves: : This is only used for efficiency.
           It's a substitute for ``channelbag.fcurves.find()`` which is a potentially expensive linear search.
        """
        linear_enum_values = [
            bpy.types.Keyframe.bl_rna.properties["interpolation"].enum_items["LINEAR"].value
        ] * total_new_keys

        for fc_key, key_values in self.keyframes_from_fcurve.items():
            if len(key_values) == 0:
                continue

            fcurve = lookup_fcurves.get(fc_key, None)
            if fcurve is None:
                data_path, array_index = fc_key
                fcurve = channelbag.fcurves.new(data_path, index=array_index)

            keyframe_points = fcurve.keyframe_points

            co_buffer = [0] * (2 * len(keyframe_points))
            keyframe_points.foreach_get("co", co_buffer)
            co_buffer.extend(key_values)

            ipo_buffer = [None] * len(keyframe_points)
            keyframe_points.foreach_get("interpolation", ipo_buffer)
            ipo_buffer.extend(linear_enum_values)

            # XXX: Currently baking inserts the same number of keys for all baked properties.
            # This block of code breaks if that's no longer true since we then will not be properly
            # initializing all the data.
            keyframe_points.add(total_new_keys)
            keyframe_points.foreach_set("co", co_buffer)
            keyframe_points.foreach_set("interpolation", ipo_buffer)

            # This also deduplicates keys where baked keys were inserted on the
            # same frame as existing ones.
            fcurve.update()


class AutoKeying:
    """Auto-keying support."""

    # Use AutoKeying.keytype() or Authkeying.options() context to change those.
    _keytype = 'KEYFRAME'
    _force_autokey = False  # Allow use without the user activating auto-keying.
    _use_loc = True
    _use_rot = True
    _use_scale = True

    @classmethod
    @contextlib.contextmanager
    def keytype(cls, the_keytype: str) -> Iterator[None]:
        """Context manager to set the key type that's inserted."""
        default_keytype = cls._keytype
        try:
            cls._keytype = the_keytype
            yield
        finally:
            cls._keytype = default_keytype

    @classmethod
    @contextlib.contextmanager
    def options(
            cls,
            *,
            keytype: str = "",
            use_loc: bool = True,
            use_rot: bool = True,
            use_scale: bool = True,
            force_autokey: bool = False) -> Iterator[None]:
        """Context manager to set various keyframing options."""
        default_keytype = cls._keytype
        default_use_loc = cls._use_loc
        default_use_rot = cls._use_rot
        default_use_scale = cls._use_scale
        default_force_autokey = cls._force_autokey
        try:
            cls._keytype = keytype
            cls._use_loc = use_loc
            cls._use_rot = use_rot
            cls._use_scale = use_scale
            cls._force_autokey = force_autokey
            yield
        finally:
            cls._keytype = default_keytype
            cls._use_loc = default_use_loc
            cls._use_rot = default_use_rot
            cls._use_scale = default_use_scale
            cls._force_autokey = default_force_autokey

    @classmethod
    def keying_options(cls, context: Context) -> set[str]:
        """Retrieve the general keyframing options from user preferences."""

        prefs = context.preferences
        ts = context.scene.tool_settings
        options = set()

        if prefs.edit.use_visual_keying:
            options.add('INSERTKEY_VISUAL')
        if prefs.edit.use_keyframe_insert_needed:
            options.add('INSERTKEY_NEEDED')
        if ts.use_keyframe_cycle_aware:
            options.add('INSERTKEY_CYCLE_AWARE')
        return options

    @classmethod
    def keying_options_from_keyingset(cls, context: Context, keyingset: KeyingSet) -> set[str]:
        """Retrieve the general keyframing options from user preferences."""

        ts = context.scene.tool_settings
        options = set()

        if keyingset.use_insertkey_visual:
            options.add('INSERTKEY_VISUAL')
        if keyingset.use_insertkey_needed:
            options.add('INSERTKEY_NEEDED')
        if ts.use_keyframe_cycle_aware:
            options.add('INSERTKEY_CYCLE_AWARE')
        return options

    @classmethod
    def autokeying_options(cls, context: Context) -> Optional[set[str]]:
        """Retrieve the Auto Keyframe options, or None if disabled."""

        ts = context.scene.tool_settings

        if not (cls._force_autokey or ts.use_keyframe_insert_auto):
            return None

        active_keyingset = context.scene.keying_sets_all.active
        if ts.use_keyframe_insert_keyingset and active_keyingset:
            # No support for keying sets in this function
            raise RuntimeError("This function should not be called when there is an active keying set")

        prefs = context.preferences
        options = cls.keying_options(context)

        if prefs.edit.use_keyframe_insert_available:
            options.add('INSERTKEY_AVAILABLE')
        if ts.auto_keying_mode == 'REPLACE_KEYS':
            options.add('INSERTKEY_REPLACE')
        return options

    @staticmethod
    def get_4d_rotlock(bone: PoseBone) -> Iterable[bool]:
        "Retrieve the lock status for 4D rotation."
        if bone.lock_rotations_4d:
            return [bone.lock_rotation_w, *bone.lock_rotation]
        return [all(bone.lock_rotation)] * 4

    @classmethod
    def keyframe_channels(
        cls,
        target: Union[Object, PoseBone],
        options: set[str],
        data_path: str,
        group: str,
        locks: Iterable[bool],
    ) -> None:
        """Keyframe channels, avoiding keying locked channels."""
        if all(locks):
            return

        if not any(locks):
            target.keyframe_insert(data_path, group=group, options=options, keytype=cls._keytype)
            return

        for index, lock in enumerate(locks):
            if lock:
                continue
            target.keyframe_insert(data_path, index=index, group=group, options=options, keytype=cls._keytype)

    @classmethod
    def key_transformation(
        cls,
        target: Union[Object, PoseBone],
        options: set[str],
    ) -> None:
        """Keyframe transformation properties, avoiding keying locked channels."""

        is_bone = isinstance(target, PoseBone)
        if is_bone:
            group = target.name
        else:
            group = "Object Transforms"

        def keyframe(data_path: str, locks: Iterable[bool]) -> None:
            cls.keyframe_channels(target, options, data_path, group, locks)

        if cls._use_loc and not (is_bone and target.bone.use_connect):
            keyframe("location", target.lock_location)

        if cls._use_rot:
            if target.rotation_mode == 'QUATERNION':
                keyframe("rotation_quaternion", cls.get_4d_rotlock(target))
            elif target.rotation_mode == 'AXIS_ANGLE':
                keyframe("rotation_axis_angle", cls.get_4d_rotlock(target))
            else:
                keyframe("rotation_euler", target.lock_rotation)

        if cls._use_scale:
            keyframe("scale", target.lock_scale)

    @classmethod
    def key_transformation_via_keyingset(cls,
                                         context: Context,
                                         target: Union[Object, PoseBone],
                                         keyingset: KeyingSet) -> None:
        """Auto-key transformation properties with the given keying set."""

        keyingset.refresh()

        is_bone = isinstance(target, PoseBone)
        options = cls.keying_options_from_keyingset(context, keyingset)

        paths_to_key = {keysetpath.data_path: keysetpath for keysetpath in keyingset.paths}

        def keyframe(data_path: str, locks: Iterable[bool]) -> None:
            # Keying sets are relative to the ID.
            full_data_path = target.path_from_id(data_path)
            try:
                keysetpath = paths_to_key[full_data_path]
            except KeyError:
                # No biggie, just means this property shouldn't be keyed.
                return

            match keysetpath.group_method:
                case 'NAMED':
                    group = keysetpath.group
                case 'KEYINGSET':
                    group = keyingset.name
                case 'NONE', _:
                    group = ""

            cls.keyframe_channels(target, options, data_path, group, locks)

        if cls._use_loc and not (is_bone and target.bone.use_connect):
            keyframe("location", target.lock_location)

        if cls._use_rot:
            if target.rotation_mode == 'QUATERNION':
                keyframe("rotation_quaternion", cls.get_4d_rotlock(target))
            elif target.rotation_mode == 'AXIS_ANGLE':
                keyframe("rotation_axis_angle", cls.get_4d_rotlock(target))
            else:
                keyframe("rotation_euler", target.lock_rotation)

        if cls._use_scale:
            keyframe("scale", target.lock_scale)

    @classmethod
    def active_keyingset(cls, context: Context) -> KeyingSet | None:
        """Return the active keying set, if it should be used.

        Only returns the active keying set when the auto-key settings indicate
        it should be used, and when it is not using absolute paths (because
        that's not supported by the Copy Global Transform add-on).
        """
        ts = context.scene.tool_settings
        if not ts.use_keyframe_insert_keyingset:
            return None

        active_keyingset = context.scene.keying_sets_all.active
        if not active_keyingset:
            return None

        active_keyingset.refresh()
        if active_keyingset.is_path_absolute:
            # Absolute-path keying sets are not supported (yet?).
            return None

        return active_keyingset

    @classmethod
    def autokey_transformation(cls, context: Context, target: Union[Object, PoseBone]) -> None:
        """Auto-key transformation properties."""

        # See if the active keying set should be used.
        keyingset = cls.active_keyingset(context)
        if keyingset:
            cls.key_transformation_via_keyingset(context, target, keyingset)
            return

        # Use regular autokeying options.
        options = cls.autokeying_options(context)
        if options is None:
            return
        cls.key_transformation(target, options)
