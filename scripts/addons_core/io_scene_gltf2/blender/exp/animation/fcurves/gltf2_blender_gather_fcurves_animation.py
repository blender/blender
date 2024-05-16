# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from .....io.com import gltf2_io
from .....io.exp.gltf2_io_user_extensions import export_user_extensions
from ....com.gltf2_blender_extras import generate_extras
from .gltf2_blender_gather_fcurves_channels import gather_animation_fcurves_channels


def gather_animation_fcurves(
        obj_uuid: str,
        blender_action: bpy.types.Action,
        export_settings
):

    name = __gather_name(blender_action, export_settings)

    channels, to_be_sampled, extra_samplers = __gather_channels_fcurves(obj_uuid, blender_action, export_settings)

    animation = gltf2_io.Animation(
        channels=channels,
        extensions=None,
        extras=__gather_extras(blender_action, export_settings),
        name=name,
        samplers=[]
    )

    if not animation.channels:
        return None, to_be_sampled, extra_samplers

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object
    export_user_extensions('animation_gather_fcurve', export_settings, blender_object, blender_action)

    return animation, to_be_sampled, extra_samplers


def __gather_name(blender_action: bpy.types.Action,
                  export_settings
                  ) -> str:
    return blender_action.name


def __gather_channels_fcurves(
        obj_uuid: str,
        blender_action: bpy.types.Action,
        export_settings):
    return gather_animation_fcurves_channels(obj_uuid, blender_action, export_settings)


def __gather_extras(blender_action, export_settings):
    if export_settings['gltf_extras']:
        return generate_extras(blender_action)
    return None
