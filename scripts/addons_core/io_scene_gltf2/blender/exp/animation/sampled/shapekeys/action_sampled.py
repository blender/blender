# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from ......io.exp.user_extensions import export_user_extensions
from ......io.com import gltf2_io
from .....com.extras import generate_extras
from .channels import gather_sk_sampled_channels


def gather_action_sk_sampled(object_uuid: str,
                             blender_action: typing.Optional[bpy.types.Action],
                             slot_identifier: str,
                             cache_key: str,
                             export_settings):

    # If no animation in file, no need to bake
    if len(bpy.data.actions) == 0:
        return None

    channels = __gather_channels(object_uuid, blender_action.name if blender_action else cache_key, slot_identifier if blender_action else None, export_settings)

    if not channels:
        return None

    blender_object = export_settings['vtree'].nodes[object_uuid].blender_object
    export_user_extensions(
        'animation_channels_sk_sampled',
        export_settings,
        channels,
        blender_object,
        blender_action,
        slot_identifier,
        cache_key)

    return channels


def __gather_channels(object_uuid: str, blender_action_name: str, slot_identifier: str,
                      export_settings) -> typing.List[gltf2_io.AnimationChannel]:
    return gather_sk_sampled_channels(object_uuid, blender_action_name, slot_identifier, export_settings)
