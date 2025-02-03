# SPDX-FileCopyrightText: 2018-2023 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import typing
import math
import numpy as np
import bpy
from .....com.conversion import PBR_WATTS_TO_LUMENS
from ....cache import cached
from ...keyframes import Keyframe
from ..sampling_cache import get_cache_data


@cached
def gather_data_sampled_keyframes(
        blender_type_data: str,
        blender_id,
        channel,
        action_name,
        slot_identifier: str,
        node_channel_is_animated: bool,
        additional_key,  # Used to differentiate between material / material node_tree
        export_settings):

    start_frame = export_settings['ranges'][blender_id][action_name]['start']
    end_frame = export_settings['ranges'][blender_id][action_name]['end']

    keyframes = []

    frame = start_frame
    step = export_settings['gltf_frame_step']
    while frame <= end_frame:

        # Retrieve length of data to export
        if export_settings['KHR_animation_pointer'][blender_type_data][blender_id]['paths'][channel]['path'] != "/materials/XXX/pbrMetallicRoughness/baseColorFactor":
            length = export_settings['KHR_animation_pointer'][blender_type_data][blender_id]['paths'][channel]['length']
        else:
            length = 4

        key = Keyframe([None] * length, frame, 'value')

        value = get_cache_data(
            'value',
            blender_id,
            channel,
            action_name,
            frame,
            step,
            slot_identifier,
            export_settings
        )

        # Convert data if needed
        if blender_type_data == "materials":
            if "attenuationDistance" in export_settings['KHR_animation_pointer']['materials'][blender_id]['paths'][channel]['path']:
                value = 1.0 / value if value != 0.0 else 1e13

            if export_settings['KHR_animation_pointer']['materials'][blender_id]['paths'][channel]['path'] == "/materials/XXX/occlusionTexture/strength":
                if export_settings['KHR_animation_pointer']['materials'][blender_id]['paths'][channel]['reverse'] is True:
                    value = 1.0 - value

            if export_settings['KHR_animation_pointer']['materials'][blender_id]['paths'][channel]['path'] == "/materials/XXX/emissiveFactor":
                # We need to retrieve the strength of the emissive too
                strength = get_cache_data(
                    'value',
                    blender_id,
                    export_settings['KHR_animation_pointer']['materials'][blender_id]['paths'][channel]['strength_channel'],
                    action_name,
                    frame,
                    step,
                    slot_identifier,
                    export_settings
                )

                value = [f * strength for f in value]
                if any([i > 1.0 for i in value or []]):
                    # Clamp to range [0,1]
                    # Official glTF clamp to range [0,1]
                    # If we are outside, we need to use extension KHR_materials_emissive_strength
                    strength = max(value)
                    value = [f / strength for f in value]
                else:
                    pass  # Don't need to do anything, as we are in the range [0,1]

            if export_settings['KHR_animation_pointer']['materials'][blender_id]['paths'][channel][
                    'path'] == "/materials/XXX/extensions/KHR_materials_emissive_strength/emissiveStrength":

                if export_settings['KHR_animation_pointer']['materials'][blender_id]['paths'][channel]['factor_channel'] is not None:
                    factor = get_cache_data(
                        'value',
                        blender_id,
                        export_settings['KHR_animation_pointer']['materials'][blender_id]['paths'][channel]['factor_channel'],
                        action_name,
                        frame,
                        step,
                        slot_identifier,
                        export_settings
                    )

                    factor = [f * value for f in factor]
                    if any([i > 1.0 for i in factor or []]):
                        # Clamp to range [0,1]
                        # Official glTF clamp to range [0,1]
                        # If we are outside, we need to use extension KHR_materials_emissive_strength
                        value = max(factor)
                    else:
                        value = 1.0  # no need to have an emissiveStrength extension for this frame
                else:
                    # No factor exists, so set it as 1.0 / 1.0 / 1.0
                    # This is because the emission is linked to a texture, without a factor
                    # No need to change the value
                    factor = [1.0, 1.0, 1.0]

            # For specularFactor and specularColorFactor, we already multiplied it by 2.0, and clamp it to 1.0 (and adapt specularColor accordingly)
            # This is done in cache retrieval

        elif blender_type_data == "lights":
            if export_settings['KHR_animation_pointer']['lights'][blender_id]['paths'][channel]['path'] == "/extensions/KHR_lights_punctual/lights/XXX/intensity":
                # Lights need conversion in case quadratic_falloff_node is used, for intensity
                if 'quadratic_falloff_node' in channel:
                    value /= (math.pi * 4.0)

                if export_settings['gltf_lighting_mode'] == 'SPEC' \
                        and export_settings['KHR_animation_pointer']['lights'][blender_id]['paths'][channel]['lamp_type'] != "SUN":
                    value *= PBR_WATTS_TO_LUMENS

            if export_settings['KHR_animation_pointer']['lights'][blender_id]['paths'][channel]['path'] == "/extensions/KHR_lights_punctual/lights/XXX/spot.outerConeAngle":
                value *= 0.5

            # innerConeAngle is handled in cache retrieval, as it requires spot_size and spot_blend

        # Camera yvof is calculated in cache retrieval, as it requires sensor_fit, angle, aspect ratio

        key.value_total = value
        keyframes.append(key)
        frame += step

    if len(keyframes) == 0:
        # For example, option CROP negative frames, but all are negatives
        return None

    cst = fcurve_is_constant(keyframes)
    return None if cst is True else keyframes


def fcurve_is_constant(keyframes):
    if type(keyframes[0].value).__name__ == "float":
        return all([j < 0.0001 for j in np.ptp([[k.value] for k in keyframes], axis=0)])
    else:
        return all([j < 0.0001 for j in np.ptp([[k.value[i]
                   for i in range(len(keyframes[0].value))] for k in keyframes], axis=0)])
