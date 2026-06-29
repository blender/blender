# SPDX-FileCopyrightText: 2018-2023 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ......io.com import gltf2_io
from ....cache import cached
from ...anim_extra_utils import gather_animated_node_for_data, gather_animated_blender_id
import typing


@cached
def gather_data_sampled_channel_target(
        blender_main_type: str,
        blender_type_data: str,
        blender_id,
        bone_name: typing.Optional[str],
        channel: str,
        additional_key: str,  # Used to differentiate between material / material node_tree
        export_settings
) -> gltf2_io.AnimationChannelTarget:

    animation_channel_target = gltf2_io.AnimationChannelTarget(
        extensions=__gather_extensions(blender_type_data, blender_id, channel, export_settings),
        extras=__gather_extras(blender_type_data, blender_id, channel, export_settings),
        node=__gather_node(blender_main_type, blender_type_data, blender_id, bone_name, export_settings),
        path=__gather_path(blender_main_type, blender_type_data, blender_id, bone_name, channel, export_settings)
    )

    return animation_channel_target


def __gather_extensions(blender_type_data, blender_id, channel, export_settings):
    return None


def __gather_extras(blender_type_data, blender_id, channel, export_settings):
    return None


def __gather_node(blender_main_type, blender_type_data, blender_id, bone_name, export_settings):
    return gather_animated_node_for_data(blender_main_type, blender_type_data, blender_id, bone_name, export_settings)


def __gather_path(blender_main_type, blender_type_data, blender_id, bone_name, channel, export_settings):
    used_blender_id = gather_animated_blender_id(
        blender_main_type,
        blender_type_data,
        blender_id,
        bone_name,
        export_settings)

    if blender_main_type == "extras" and \
            blender_type_data == "bones" and \
            export_settings['gltf_animation_mode'] in ["ACTIONS", "ACTIVE_ACTIONS"]:
        channel = channel.replace("pose.bones[\"" + bone_name + "\"]", "")

    return export_settings['KHR_animation_pointer'][blender_main_type][blender_type_data][used_blender_id]['paths'][channel]['path']
