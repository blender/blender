# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from ......io.exp.gltf2_io_user_extensions import export_user_extensions
from ......io.com import gltf2_io
from .....com.gltf2_blender_extras import generate_extras
from ...fcurves.gltf2_blender_gather_fcurves_sampler import gather_animation_fcurves_sampler
from .armature_channels import gather_armature_sampled_channels


def gather_action_armature_sampled(armature_uuid: str,
                                   blender_action: typing.Optional[bpy.types.Action],
                                   cache_key: str,
                                   export_settings):

    blender_object = export_settings['vtree'].nodes[armature_uuid].blender_object

    name = __gather_name(blender_action, armature_uuid, cache_key, export_settings)

    try:
        channels, extra_channels = __gather_channels(
            armature_uuid, blender_action.name if blender_action else cache_key, export_settings)
        animation = gltf2_io.Animation(
            channels=channels,
            extensions=None,
            extras=__gather_extras(blender_action, export_settings),
            name=name,
            samplers=[]  # We need to gather the samplers after gathering all channels --> populate this list in __link_samplers
        )
    except RuntimeError as error:
        export_settings['log'].warning("Animation '{}' could not be exported. Cause: {}".format(name, error))
        return None

    export_user_extensions('pre_gather_animation_hook', export_settings, animation, blender_action, blender_object)

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

    if not animation.channels:
        return None, extra_samplers

    # To allow reuse of samplers in one animation : This will be done later, when we know all channels are here

    export_user_extensions(
        'gather_animation_hook',
        export_settings,
        animation,
        blender_action,
        blender_object)  # For compatibility for older version
    export_user_extensions('animation_action_armature_sampled', export_settings,
                           animation, blender_object, blender_action, cache_key)

    return animation, extra_samplers


def __gather_name(blender_action: bpy.types.Action,
                  armature_uuid: str,
                  cache_key: str,
                  export_settings
                  ) -> str:
    if blender_action:
        return blender_action.name
    elif armature_uuid == cache_key:
        return export_settings['vtree'].nodes[armature_uuid].blender_object.name
    else:
        return cache_key


def __gather_channels(armature_uuid, blender_action_name, export_settings) -> typing.List[gltf2_io.AnimationChannel]:
    return gather_armature_sampled_channels(armature_uuid, blender_action_name, export_settings)


def __gather_extras(blender_action, export_settings):
    if export_settings['gltf_extras']:
        return generate_extras(blender_action) if blender_action else None
    return None
