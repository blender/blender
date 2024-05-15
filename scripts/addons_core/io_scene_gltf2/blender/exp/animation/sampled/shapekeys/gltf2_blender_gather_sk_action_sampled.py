# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from ......io.exp.gltf2_io_user_extensions import export_user_extensions
from ......io.com import gltf2_io
from .....com.gltf2_blender_extras import generate_extras
from .gltf2_blender_gather_sk_channels import gather_sk_sampled_channels


def gather_action_sk_sampled(object_uuid: str,
                             blender_action: typing.Optional[bpy.types.Action],
                             cache_key: str,
                             export_settings):

    # If no animation in file, no need to bake
    if len(bpy.data.actions) == 0:
        return None

    animation = gltf2_io.Animation(
        channels=__gather_channels(object_uuid, blender_action.name if blender_action else cache_key, export_settings),
        extensions=None,
        extras=__gather_extras(blender_action, export_settings),
        name=__gather_name(object_uuid, blender_action, cache_key, export_settings),
        samplers=[]
    )

    if not animation.channels:
        return None

    blender_object = export_settings['vtree'].nodes[object_uuid].blender_object
    export_user_extensions(
        'animation_action_sk_sampled',
        export_settings,
        animation,
        blender_object,
        blender_action,
        cache_key)

    return animation


def __gather_name(object_uuid: str, blender_action: typing.Optional[bpy.types.Action], cache_key: str, export_settings):
    if blender_action:
        return blender_action.name
    elif object_uuid == cache_key:
        return export_settings['vtree'].nodes[object_uuid].blender_object.name
    else:
        return cache_key


def __gather_channels(object_uuid: str, blender_action_name: str,
                      export_settings) -> typing.List[gltf2_io.AnimationChannel]:
    return gather_sk_sampled_channels(object_uuid, blender_action_name, export_settings)


def __gather_extras(blender_action, export_settings):
    if export_settings['gltf_extras']:
        return generate_extras(blender_action) if blender_action else None
    return None
