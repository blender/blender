# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "bake_action",
    "bake_action_objects",

    "bake_action_iter",
    "bake_action_objects_iter",
)

import bpy
from bpy.types import Action
from dataclasses import dataclass

from typing import (
    List,
    Mapping,
    Sequence,
    Tuple,
)

from rna_prop_ui import (
    rna_idprop_value_to_python,
)

FCurveKey = Tuple[
    # `fcurve.data_path`.
    str,
    # `fcurve.array_index`.
    int,
]

# List of `[frame0, value0, frame1, value1, ...]` pairs.
ListKeyframes = List[float]


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


def bake_action(
        obj,
        *,
        action, frames,
        bake_options: BakeOptions
):
    """
    :arg obj: Object to bake.
    :type obj: :class:`bpy.types.Object`
    :arg action: An action to bake the data into, or None for a new action
       to be created.
    :type action: :class:`bpy.types.Action` or None
    :arg frames: Frames to bake.
    :type frames: iterable of int

    :return: an action or None
    :rtype: :class:`bpy.types.Action`
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
        bake_options: BakeOptions
):
    """
    A version of :func:`bake_action_objects_iter` that takes frames and returns the output.

    :arg frames: Frames to bake.
    :type frames: iterable of int

    :return: A sequence of Action or None types (aligned with `object_action_pairs`)
    :rtype: sequence of :class:`bpy.types.Action`
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
        bake_options: BakeOptions
):
    """
    An coroutine that bakes actions for multiple objects.

    :arg object_action_pairs: Sequence of object action tuples,
       action is the destination for the baked data. When None a new action will be created.
    :type object_action_pairs: Sequence of (:class:`bpy.types.Object`, :class:`bpy.types.Action`)
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
        bake_options: BakeOptions
):
    """
    An coroutine that bakes action for a single object.

    :arg obj: Object to bake.
    :type obj: :class:`bpy.types.Object`
    :arg action: An action to bake the data into, or None for a new action
       to be created.
    :type action: :class:`bpy.types.Action` or None
    :arg bake_options: Boolean options of what to include into the action bake.
    :type bake_options: :class: `anim_utils.BakeOptions`

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

    # Convert rna_prop types (IDPropertyArray, etc) to python types.
    def clean_custom_properties(obj):
        clean_props = {
            key: rna_idprop_value_to_python(value)
            for key, value in obj.items()
        }
        return clean_props

    def bake_custom_properties(obj, *, custom_props, frame, group_name=""):
        if frame is None or not custom_props:
            return
        for key, value in custom_props.items():
            obj[key] = value
            try:
                obj.keyframe_insert(f'["{bpy.utils.escape_identifier(key)}"]', frame=frame, group=group_name)
            except TypeError:
                # Non animatable properties (datablocks, etc) cannot be keyed.
                continue

    def pose_frame_info(obj):
        matrix = {}
        bbones = {}
        custom_props = {}
        for name, pbone in obj.pose.bones.items():
            if bake_options.do_visual_keying:
                # Get the final transform of the bone in its own local space...
                matrix[name] = obj.convert_space(pose_bone=pbone, matrix=pbone.matrix,
                                                 from_space='POSE', to_space='LOCAL')
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
    # Clean (store initial data)
    if bake_options.do_clean and action is not None:
        clean_orig_data = {fcu: {p.co[1] for p in fcu.keyframe_points} for fcu in action.fcurves}
    else:
        clean_orig_data = {}

    # -------------------------------------------------------------------------
    # Create action

    # in case animation data hasn't been created
    atd = obj.animation_data_create()
    is_new_action = action is None
    if is_new_action:
        action = bpy.data.actions.new("Action")

    # Only leave tweak mode if we actually need to modify the action (#57159)
    if action != atd.action:
        # Leave tweak mode before trying to modify the action (#48397)
        if atd.use_tweak_mode:
            atd.use_tweak_mode = False

        atd.action = action

    # Baking the action only makes sense in Replace mode, so force it (#69105)
    if not atd.use_tweak_mode:
        atd.action_blend_type = 'REPLACE'

    # -------------------------------------------------------------------------
    # Apply transformations to action

    # pose
    lookup_fcurves = {(fcurve.data_path, fcurve.array_index): fcurve for fcurve in action.fcurves}
    if bake_options.do_pose:
        for f, armature_custom_properties in armature_info:
            bake_custom_properties(obj, custom_props=armature_custom_properties, frame=f)

        for name, pbone in obj.pose.bones.items():
            if bake_options.only_selected and not pbone.bone.select:
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
                keyframes.insert_keyframes_into_new_action(total_new_keys, action, name)
            else:
                keyframes.insert_keyframes_into_existing_action(lookup_fcurves, total_new_keys, action, name)

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
            keyframes.insert_keyframes_into_new_action(total_new_keys, action, name)
        else:
            keyframes.insert_keyframes_into_existing_action(lookup_fcurves, total_new_keys, action, name)

        if bake_options.do_parents_clear:
            obj.parent = None

    # -------------------------------------------------------------------------
    # Clean

    if bake_options.do_clean:
        for fcu in action.fcurves:
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
        action: Action,
        action_group_name: str,
    ) -> None:
        """
        Assumes the action is new, that it has no F-curves. Otherwise, the only difference between versions is
        performance and implementation simplicity.

        :arg action_group_name: Name of Action Group that F-curves are added to.
        :type action_group_name: str
        """
        linear_enum_values = [
            bpy.types.Keyframe.bl_rna.properties["interpolation"].enum_items["LINEAR"].value
        ] * total_new_keys

        for fc_key, key_values in self.keyframes_from_fcurve.items():
            if len(key_values) == 0:
                continue

            data_path, array_index = fc_key
            keyframe_points = action.fcurves.new(
                data_path, index=array_index, action_group=action_group_name
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
        action: Action,
        action_group_name: str,
    ) -> None:
        """
        Assumes the action already exists, that it might already have F-curves. Otherwise, the
        only difference between versions is performance and implementation simplicity.

        :arg lookup_fcurves: : This is only used for efficiency.
           It's a substitute for ``action.fcurves.find()`` which is a potentially expensive linear search.
        :type lookup_fcurves: ``Mapping[FCurveKey, bpy.types.FCurve]``
        :arg action_group_name: Name of Action Group that F-curves are added to.
        :type action_group_name: str
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
                fcurve = action.fcurves.new(
                    data_path, index=array_index, action_group=action_group_name
                )

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
