# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import typing
import numpy as np
from ....cache import cached
from ...keyframes import Keyframe
from ..sampling_cache import get_cache_data


@cached
def gather_bone_sampled_keyframes(
        armature_uuid: str,
        bone: str,
        channel: str,
        action_name: str,
        slot_identifier: str,
        node_channel_is_animated: bool,
        export_settings
) -> typing.List[Keyframe]:

    start_frame = export_settings['ranges'][armature_uuid][action_name]['start']
    end_frame = export_settings['ranges'][armature_uuid][action_name]['end']

    keyframes = []

    frame = start_frame
    step = export_settings['gltf_frame_step']

    while frame <= end_frame:
        key = Keyframe(None, frame, channel)

        mat = get_cache_data(
            'bone',
            armature_uuid,
            bone,
            action_name,
            frame,
            step,
            slot_identifier,
            export_settings)

        trans, rot, scale = mat.decompose()

        key.value = {
            "location": trans,
            "rotation_quaternion": rot,
            "scale": scale
        }[channel]

        keyframes.append(key)
        frame += step

    if len(keyframes) == 0:
        # For example, option CROP negative frames, but all are negatives
        return None

    if not export_settings['gltf_optimize_animation']:
        # For bones, if all values are the same, keeping only if changing values, or if user want to keep data
        if node_channel_is_animated is True:
            return keyframes  # Always keeping
        else:
            # baked bones
            if export_settings['gltf_optimize_animation_keep_armature'] is False:
                # Not keeping if not changing property
                cst = fcurve_is_constant(keyframes)
                return None if cst is True else keyframes
            else:
                # Keep data, as requested by user. We keep all samples, as user don't want to optimize
                return keyframes

    else:

        # For armatures
        # Check if all values are the same
        # In that case, if there is no real keyframe on this channel for this given bone,
        # We can ignore these keyframes
        # if there are some fcurve, we can keep only 2 keyframes, first and last
        cst = fcurve_is_constant(keyframes)

        if node_channel_is_animated is True:  # fcurve on this bone for this property
            # Keep animation, but keep only 2 keyframes if data are not changing
            return [keyframes[0], keyframes[-1]] if cst is True and len(keyframes) >= 2 else keyframes
        else:  # bone is not animated (no fcurve)
            # Not keeping if not changing property if user decided to not keep
            if export_settings['gltf_optimize_animation_keep_armature'] is False:
                return None if cst is True else keyframes
            else:
                # Keep at least 2 keyframes if data are not changing
                return [keyframes[0], keyframes[-1]] if cst is True and len(keyframes) >= 2 else keyframes


def fcurve_is_constant(keyframes):
    return all([j < 0.0001 for j in np.ptp([[k.value[i] for i in range(len(keyframes[0].value))] for k in keyframes], axis=0)])
