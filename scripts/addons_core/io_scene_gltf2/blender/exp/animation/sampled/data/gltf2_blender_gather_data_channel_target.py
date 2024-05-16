# SPDX-FileCopyrightText: 2018-2023 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ......io.com import gltf2_io
from ....gltf2_blender_gather_cache import cached


@cached
def gather_data_sampled_channel_target(
        blender_type_data: str,
        blender_id,
        channel: str,
        additional_key: str,  # Used to differentiate between material / material node_tree
        export_settings
) -> gltf2_io.AnimationChannelTarget:

    animation_channel_target = gltf2_io.AnimationChannelTarget(
        extensions=__gather_extensions(blender_type_data, blender_id, channel, export_settings),
        extras=__gather_extras(blender_type_data, blender_id, channel, export_settings),
        node=__gather_node(blender_type_data, blender_id, export_settings),
        path=__gather_path(blender_type_data, blender_id, channel, export_settings)
    )

    return animation_channel_target


def __gather_extensions(blender_type_data, blender_id, channel, export_settings):
    return None


def __gather_extras(blender_type_data, blender_id, channel, export_settings):
    return None


def __gather_node(blender_type_data, blender_id, export_settings):
    if blender_type_data == "materials":
        return export_settings['KHR_animation_pointer']['materials'][blender_id]['glTF_material']
    elif blender_type_data == "lights":
        return export_settings['KHR_animation_pointer']['lights'][blender_id]['glTF_light']
    elif blender_type_data == "cameras":
        return export_settings['KHR_animation_pointer']['cameras'][blender_id]['glTF_camera']
    else:
        pass  # This should never happen


def __gather_path(blender_type_data, blender_id, channel, export_settings):
    return export_settings['KHR_animation_pointer'][blender_type_data][blender_id]['paths'][channel]['path']
