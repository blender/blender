# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from ......io.com import gltf2_io
from ......io.exp.gltf2_io_user_extensions import export_user_extensions
from .....com.gltf2_blender_extras import generate_extras
from ...fcurves.gltf2_blender_gather_fcurves_sampler import gather_animation_fcurves_sampler
from .gltf2_blender_gather_object_channels import gather_object_sampled_channels


def gather_action_object_sampled(object_uuid: str,
                                 blender_action: typing.Optional[bpy.types.Action],
                                 cache_key: str,
                                 export_settings):

    extra_samplers = []

    # If no animation in file, no need to bake
    if len(bpy.data.actions) == 0:
        return None, extra_samplers

    channels, extra_channels = __gather_channels(
        object_uuid, blender_action.name if blender_action else cache_key, export_settings)
    animation = gltf2_io.Animation(
        channels=channels,
        extensions=None,
        extras=__gather_extras(blender_action, export_settings),
        name=__gather_name(object_uuid, blender_action, cache_key, export_settings),
        samplers=[]
    )

    if export_settings['gltf_export_extra_animations']:
        for chan in [chan for chan in extra_channels.values() if len(chan['properties']) != 0]:
            for channel_group_name, channel_group in chan['properties'].items():

                # No glTF channel here, as we don't have any target
                # Trying to retrieve sampler directly
                sampler = gather_animation_fcurves_sampler(
                    object_uuid, tuple(channel_group), None, None, True, export_settings)
                if sampler is not None:
                    extra_samplers.append((channel_group_name, sampler, "OBJECT", None))

    if not animation.channels:
        return None, extra_samplers

    blender_object = export_settings['vtree'].nodes[object_uuid].blender_object
    export_user_extensions(
        'animation_action_object_sampled',
        export_settings,
        animation,
        blender_object,
        blender_action,
        cache_key)

    return animation, extra_samplers


def __gather_name(object_uuid: str, blender_action: typing.Optional[bpy.types.Action], cache_key: str, export_settings):
    if blender_action:
        return blender_action.name
    elif cache_key == object_uuid:
        return export_settings['vtree'].nodes[object_uuid].blender_object.name
    else:
        return cache_key


def __gather_channels(object_uuid: str, blender_action_name: str,
                      export_settings) -> typing.List[gltf2_io.AnimationChannel]:
    return gather_object_sampled_channels(object_uuid, blender_action_name, export_settings)


def __gather_extras(blender_action, export_settings):
    if export_settings['gltf_extras']:
        return generate_extras(blender_action) if blender_action else None
    return None
