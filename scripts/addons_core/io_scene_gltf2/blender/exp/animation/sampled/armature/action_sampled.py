# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from ......io.exp.user_extensions import export_user_extensions
from ......io.com import gltf2_io
from .....com.extras import generate_extras
from ...fcurves.sampler import gather_animation_fcurves_sampler
from .channels import gather_armature_sampled_channels


def gather_action_armature_sampled(armature_uuid: str,
                                   blender_action: typing.Optional[bpy.types.Action],
                                   slot_identifier: str,
                                   cache_key: str,
                                   export_settings):

    blender_object = export_settings['vtree'].nodes[armature_uuid].blender_object

    try:
        channels, extra_channels = __gather_channels(
            armature_uuid, blender_action.name if blender_action else cache_key, slot_identifier if blender_action else None, export_settings)
    except RuntimeError as error:
        export_settings['log'].warning("Animation channels on action '{}' could not be exported. Cause: {}".format(name, error))
        return None

    export_user_extensions('pre_gather_animation_hook', export_settings, channels, blender_action, slot_identifier, blender_object)

    extra_samplers = []
    if export_settings['gltf_export_extra_animations']:
        for chan in [chan for chan in extra_channels.values() if len(chan['properties']) != 0]:
            for channel_group_name, channel_group in chan['properties'].items():

                # No glTF channel here, as we don't have any target
                # Trying to retrieve sampler directly
                sampler = gather_animation_fcurves_sampler(
                    armature_uuid, tuple(channel_group), None, None, True, export_settings)
                if sampler is not None:
                    extra_samplers.append((channel_group_name, sampler))

    if not channels:
        return None, extra_samplers

    # To allow reuse of samplers in one animation : This will be done later, when we know all channels are here

    export_user_extensions('animation_channels_armature_sampled', export_settings,
                           channels, blender_object, blender_action, slot_identifier, cache_key)

    return channels, extra_samplers


def __gather_channels(armature_uuid, blender_action_name, slot_identifier, export_settings) -> typing.List[gltf2_io.AnimationChannel]:
    return gather_armature_sampled_channels(armature_uuid, blender_action_name, slot_identifier, export_settings)
