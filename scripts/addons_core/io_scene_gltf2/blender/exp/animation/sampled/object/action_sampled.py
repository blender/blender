# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from ......io.com import gltf2_io
from ......io.exp.user_extensions import export_user_extensions
from .....com.extras import generate_extras
from ...fcurves.sampler import gather_animation_fcurves_sampler
from .channels import gather_object_sampled_channels


def gather_action_object_sampled(object_uuid: str,
                                 blender_action: typing.Optional[bpy.types.Action],
                                 slot_identifier: str,
                                 cache_key: str,
                                 export_settings):

    extra_samplers = []

    # If no animation in file, no need to bake
    if len(bpy.data.actions) == 0:
        return None, extra_samplers

    channels, extra_channels = __gather_channels(
        object_uuid, blender_action.name if blender_action else cache_key, slot_identifier if blender_action else None, export_settings)

    if export_settings['gltf_export_extra_animations']:
        for chan in [chan for chan in extra_channels.values() if len(chan['properties']) != 0]:
            for channel_group_name, channel_group in chan['properties'].items():

                # No glTF channel here, as we don't have any target
                # Trying to retrieve sampler directly
                sampler = gather_animation_fcurves_sampler(
                    object_uuid, tuple(channel_group), None, None, True, export_settings)
                if sampler is not None:
                    extra_samplers.append((channel_group_name, sampler, "OBJECT", None))

    if not channels:
        return None, extra_samplers

    blender_object = export_settings['vtree'].nodes[object_uuid].blender_object
    export_user_extensions(
        'animation_channels_object_sampled',
        export_settings,
        channels,
        blender_object,
        blender_action,
        slot_identifier,
        cache_key)

    return channels, extra_samplers


def __gather_channels(object_uuid: str, blender_action_name: str, slot_identifier: str,
                      export_settings) -> typing.List[gltf2_io.AnimationChannel]:
    return gather_object_sampled_channels(object_uuid, blender_action_name, slot_identifier, export_settings)
