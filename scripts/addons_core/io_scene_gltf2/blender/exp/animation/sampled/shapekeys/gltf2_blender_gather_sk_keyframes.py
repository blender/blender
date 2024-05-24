# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
import numpy as np
from ......blender.com.gltf2_blender_data_path import get_sk_exported
from ....gltf2_blender_gather_cache import cached
from ...gltf2_blender_gather_keyframes import Keyframe
from ...fcurves.gltf2_blender_gather_fcurves_channels import get_channel_groups
from ...fcurves.gltf2_blender_gather_fcurves_keyframes import gather_non_keyed_values
from ..gltf2_blender_gather_animation_sampling_cache import get_cache_data


@cached
def gather_sk_sampled_keyframes(obj_uuid,
                                action_name,
                                export_settings):

    start_frame = export_settings['ranges'][obj_uuid][action_name]['start']
    end_frame = export_settings['ranges'][obj_uuid][action_name]['end']

    keyframes = []

    frame = start_frame
    step = export_settings['gltf_frame_step']
    blender_obj = export_settings['vtree'].nodes[obj_uuid].blender_object

    if export_settings['gltf_optimize_armature_disable_viewport'] is True:
        # Using this option, we miss the drivers :(
        # No solution exists for now. In the future, we should be able to copy a driver
        if action_name in bpy.data.actions:
            channel_group, _, _ = get_channel_groups(
                obj_uuid, bpy.data.actions[action_name], export_settings, no_sample_option=True)
        elif blender_obj.data.shape_keys.animation_data and blender_obj.data.shape_keys.animation_data.action:
            channel_group, _, _ = get_channel_groups(
                obj_uuid, blender_obj.data.shape_keys.animation_data.action, export_settings, no_sample_option=True)
        else:
            channel_group = {}
            channels = [None] * len(get_sk_exported(blender_obj.data.shape_keys.key_blocks))

            # One day, if we will be able to bake drivers or evaluate it the right
            # way, we can add here the driver fcurves

        for chan in channel_group.values():
            channels = chan['properties']['value']
            break

        non_keyed_values = gather_non_keyed_values(obj_uuid, channels, None, False, export_settings)

        while frame <= end_frame:
            key = Keyframe(channels, frame, None)
            key.value = [c.evaluate(frame) for c in channels if c is not None]
            # Complete key with non keyed values, if needed
            if len([c for c in channels if c is not None]) != key.get_target_len():
                complete_key(key, non_keyed_values)

            keyframes.append(key)
            frame += step

    else:
        # Full bake, we will go frame by frame. This can take time (more than using evaluate)

        while frame <= end_frame:
            key = Keyframe([None] * (len(get_sk_exported(blender_obj.data.shape_keys.key_blocks))), frame, 'value')
            key.value_total = get_cache_data(
                'sk',
                obj_uuid,
                None,
                action_name,
                frame,
                step,
                export_settings
            )

            keyframes.append(key)
            frame += step

    if len(keyframes) == 0:
        # For example, option CROP negative frames, but all are negatives
        return None

    # In case SK has only basis
    if any([len(k.value) == 0 for k in keyframes]):
        return None

    if not export_settings['gltf_optimize_animation']:
        return keyframes

    # For sk, if all values are the same, we keep only first and last
    cst = fcurve_is_constant(keyframes)
    return [keyframes[0], keyframes[-1]] if cst is True and len(keyframes) >= 2 else keyframes


def fcurve_is_constant(keyframes):
    return all([j < 0.0001 for j in np.ptp([[k.value[i] for i in range(len(keyframes[0].value))] for k in keyframes], axis=0)])

# TODO de-duplicate, but import issue???


def complete_key(key: Keyframe, non_keyed_values: typing.Tuple[typing.Optional[float]]):
    """
    Complete keyframe with non keyed values
    """
    for i in range(0, key.get_target_len()):
        if i in key.get_indices():
            continue  # this is a keyed array_index or a SK animated
        key.set_value_index(i, non_keyed_values[i])
