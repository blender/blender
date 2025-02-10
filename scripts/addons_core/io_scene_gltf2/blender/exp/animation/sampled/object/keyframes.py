# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import numpy as np
from ....tree import VExportNode
from ....cache import cached
from ...keyframes import Keyframe
from ..sampling_cache import get_cache_data


@cached
def gather_object_sampled_keyframes(
        obj_uuid: str,
        channel: str,
        action_name: str,
        slot_identifier: str,
        node_channel_is_animated: bool,
        export_settings
):

    start_frame = export_settings['ranges'][obj_uuid][action_name]['start']
    end_frame = export_settings['ranges'][obj_uuid][action_name]['end']

    keyframes = []

    frame = start_frame
    step = export_settings['gltf_frame_step']

    while frame <= end_frame:
        key = Keyframe(None, frame, channel)

        mat = get_cache_data(
            'matrix',
            obj_uuid,
            None,
            action_name,
            frame,
            step,
            slot_identifier,
            export_settings)

        trans, rot, sca = mat.decompose()
        key.value_total = {
            "location": trans,
            "rotation_quaternion": rot,
            "scale": sca,
        }[channel]

        keyframes.append(key)
        frame += step

    if len(keyframes) == 0:
        # For example, option CROP negative frames, but all are negatives
        return None

    if not export_settings['gltf_optimize_animation']:
        # For objects, if all values are the same, keeping only if changing values, or if user want to keep data
        if node_channel_is_animated is True:
            return keyframes  # Always keeping
        else:
            # baked object
            if export_settings['gltf_optimize_animation_keep_object'] is False:
                # Not keeping if not changing property
                cst = fcurve_is_constant(keyframes)
                return None if cst is True else keyframes
            else:
                # Keep data, as requested by user. We keep all samples, as user don't want to optimize
                return keyframes

    else:

        # For objects, if all values are the same, we keep only first and last
        cst = fcurve_is_constant(keyframes)
        if node_channel_is_animated is True:
            return [keyframes[0], keyframes[-1]] if cst is True and len(keyframes) >= 2 else keyframes
        else:
            # baked object
            # Not keeping if not changing property if user decided to not keep
            if export_settings['gltf_optimize_animation_keep_object'] is False:
                return None if cst is True else keyframes
            else:
                # Keep at least 2 keyframes if data are not changing
                return [keyframes[0], keyframes[-1]] if cst is True and len(keyframes) >= 2 else keyframes


def fcurve_is_constant(keyframes):
    return all([j < 0.0001 for j in np.ptp([[k.value[i] for i in range(len(keyframes[0].value))] for k in keyframes], axis=0)])
