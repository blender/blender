# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from .....blender.com.gltf2_blender_data_path import get_sk_exported
from ....com.gltf2_blender_data_path import get_target_object_path
from ...gltf2_blender_gather_cache import cached
from ..gltf2_blender_gather_keyframes import Keyframe


@cached
def gather_fcurve_keyframes(
        obj_uuid: str,
        channel_group: typing.Tuple[bpy.types.FCurve],
        bone: typing.Optional[str],
        custom_range: typing.Optional[set],
        extra_mode: bool,
        export_settings):

    keyframes = []

    non_keyed_values = gather_non_keyed_values(obj_uuid, channel_group, bone, extra_mode, export_settings)

    # Just use the keyframes as they are specified in blender
    # Note: channels has some None items only for SK if some SK are not animated
    frames = [keyframe.co[0] for keyframe in [c for c in channel_group if c is not None][0].keyframe_points]
    # some weird files have duplicate frame at same time, removed them
    frames = sorted(set(frames))

    if export_settings['gltf_negative_frames'] == "CROP":
        frames = [f for f in frames if f >= 0]

    if export_settings['gltf_frame_range'] is True:
        frames = [f for f in frames if f >= bpy.context.scene.frame_start and f <= bpy.context.scene.frame_end]

    if custom_range is not None:
        frames = [f for f in frames if f >= custom_range[0] and f <= custom_range[1]]

    if len(frames) == 0:
        return None

    for i, frame in enumerate(frames):
        key = Keyframe(channel_group, frame, None)
        key.value = [c.evaluate(frame) for c in channel_group if c is not None]
        # Complete key with non keyed values, if needed
        if len([c for c in channel_group if c is not None]) != key.get_target_len():
            complete_key(key, non_keyed_values)

        # compute tangents for cubic spline interpolation
        if [c for c in channel_group if c is not None][0].keyframe_points[0].interpolation == "BEZIER":
            # Construct the in tangent
            if frame == frames[0]:
                # start in-tangent should become all zero
                key.set_first_tangent()
            else:
                # otherwise construct an in tangent coordinate from the keyframes control points. We intermediately
                # use a point at t+1 to define the tangent. This allows the tangent control point to be transformed
                # normally, but only works for locally linear transformation. The more non-linear a transform, the
                # more imprecise this method is.
                # We could use any other (v1, t1) for which (v1 - v0) / (t1 - t0) equals the tangent. By using t+1
                # for both in and out tangents, we guarantee that (even if there are errors or numerical imprecisions)
                # symmetrical control points translate to symmetrical tangents.
                # Note: I am not sure that linearity is never broken with quaternions and their normalization.
                # Especially at sign swap it might occur that the value gets negated but the control point not.
                # I have however not once encountered an issue with this.
                key.in_tangent = [c.keyframe_points[i].co[1] +
                                  (c.keyframe_points[i].handle_left[1] -
                                   c.keyframe_points[i].co[1]) /
                                  (c.keyframe_points[i].handle_left[0] -
                                   c.keyframe_points[i].co[0]) for c in channel_group if c is not None]
            # Construct the out tangent
            if frame == frames[-1]:
                # end out-tangent should become all zero
                key.set_last_tangent()
            else:
                # otherwise construct an in tangent coordinate from the keyframes control points.
                # This happens the same way how in tangents are handled above.
                key.out_tangent = [c.keyframe_points[i].co[1] +
                                   (c.keyframe_points[i].handle_right[1] -
                                    c.keyframe_points[i].co[1]) /
                                   (c.keyframe_points[i].handle_right[0] -
                                    c.keyframe_points[i].co[0]) for c in channel_group if c is not None]

            __complete_key_tangents(key, non_keyed_values)

        keyframes.append(key)

    return keyframes


def gather_non_keyed_values(
        obj_uuid: str,
        channel_group: typing.Tuple[bpy.types.FCurve],
        bone: typing.Optional[str],
        extra_mode: bool,
        export_settings
) -> typing.Tuple[typing.Optional[float]]:

    if extra_mode is True:
        # No need to check if there are non non keyed values, as we export fcurve independently
        return [None]

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object

    non_keyed_values = []

    # Note: channels has some None items only for SK if some SK are not animated
    if None not in channel_group:
        # classic case for object TRS or bone TRS
        # Or if all morph target are animated

        target = channel_group[0].data_path.split('.')[-1]
        if target == "value":
            # All morph targets are animated
            return tuple([None] * len(channel_group))

        indices = [c.array_index for c in channel_group]
        indices.sort()
        length = {
            "delta_location": 3,
            "delta_rotation_euler": 3,
            "delta_rotation_quaternion": 4,
            "delta_scale": 3,
            "location": 3,
            "rotation_axis_angle": 4,
            "rotation_euler": 3,
            "rotation_quaternion": 4,
            "scale": 3,
            "value": len(channel_group)
        }.get(target)

        if length is None:
            # This is not a known target
            return ()

        for i in range(0, length):
            if i in indices:
                non_keyed_values.append(None)
            else:
                if bone is None:
                    non_keyed_values.append({
                        "delta_location": blender_object.delta_location,
                        "delta_rotation_euler": blender_object.delta_rotation_euler,
                        "delta_rotation_quaternion": blender_object.delta_rotation_quaternion,
                        "delta_scale": blender_object.delta_scale,
                        "location": blender_object.location,
                        "rotation_axis_angle": blender_object.rotation_axis_angle,
                        "rotation_euler": blender_object.rotation_euler,
                        "rotation_quaternion": blender_object.rotation_quaternion,
                        "scale": blender_object.scale
                    }[target][i])
                else:
                    # TODO, this is not working if the action is not active (NLA case for example) ?
                    trans, rot, scale = blender_object.pose.bones[bone].matrix_basis.decompose()
                    non_keyed_values.append({
                        "location": trans,
                        "rotation_axis_angle": rot,
                        "rotation_euler": rot,
                        "rotation_quaternion": rot,
                        "scale": scale
                    }[target][i])

        return tuple(non_keyed_values)

    else:
        # We are in case of morph target, where all targets are not animated
        # So channels has some None items
        first_channel = [c for c in channel_group if c is not None][0]
        object_path = get_target_object_path(first_channel.data_path)
        if object_path:
            shapekeys_idx = {}
            cpt_sk = 0
            for sk in get_sk_exported(blender_object.data.shape_keys.key_blocks):
                shapekeys_idx[cpt_sk] = sk.name
                cpt_sk += 1

            for idx_c, channel in enumerate(channel_group):
                if channel is None:
                    non_keyed_values.append(blender_object.data.shape_keys.key_blocks[shapekeys_idx[idx_c]].value)
                else:
                    non_keyed_values.append(None)

        return tuple(non_keyed_values)


def complete_key(key: Keyframe, non_keyed_values: typing.Tuple[typing.Optional[float]]):
    """
    Complete keyframe with non keyed values
    """
    for i in range(0, key.get_target_len()):
        if i in key.get_indices():
            continue  # this is a keyed array_index or a SK animated
        key.set_value_index(i, non_keyed_values[i])


def __complete_key_tangents(key: Keyframe, non_keyed_values: typing.Tuple[typing.Optional[float]]):
    """
    Complete keyframe with non keyed values for tangents
    """
    for i in range(0, key.get_target_len()):
        if i in key.get_indices():
            continue  # this is a keyed array_index or a SK animated
        if key.in_tangent is not None:
            key.set_value_index_in(i, non_keyed_values[i])
        if key.out_tangent is not None:
            key.set_value_index_out(i, non_keyed_values[i])
