# SPDX-FileCopyrightText: 2018-2023 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from ......io.com import gltf2_io
from ......blender.com.gltf2_blender_conversion import get_gltf_interpolation
from .gltf2_blender_gather_data_channel_target import gather_data_sampled_channel_target
from .gltf2_blender_gather_data_sampler import gather_data_sampled_animation_sampler


def gather_data_sampled_channels(blender_type_data, blender_id, blender_action_name,
                                 additional_key, export_settings) -> typing.List[gltf2_io.AnimationChannel]:
    channels = []

    list_of_animated_data_channels = {}  # TODOPointer

    baseColorFactor_alpha_merged_already_done = False
    for path in export_settings['KHR_animation_pointer'][blender_type_data][blender_id]['paths'].keys():

        # Do not manage alpha, as it will be managaed by the baseColorFactor (merging Color and alpha)
        if export_settings['KHR_animation_pointer'][blender_type_data][blender_id]['paths'][path][
                'path'] == "/materials/XXX/pbrMetallicRoughness/baseColorFactor" and baseColorFactor_alpha_merged_already_done is True:
            continue

        channel = gather_sampled_data_channel(
            blender_type_data,
            blender_id,
            path,
            blender_action_name,
            path in list_of_animated_data_channels.keys(),
            list_of_animated_data_channels[path] if path in list_of_animated_data_channels.keys() else get_gltf_interpolation("LINEAR"),
            additional_key,
            export_settings)
        if channel is not None:
            channels.append(channel)

        if export_settings['KHR_animation_pointer'][blender_type_data][blender_id]['paths'][path]['path'] == "/materials/XXX/pbrMetallicRoughness/baseColorFactor":
            baseColorFactor_alpha_merged_already_done = True

    return channels


def gather_sampled_data_channel(
        blender_type_data: str,
        blender_id: str,
        channel: str,
        action_name: str,
        node_channel_is_animated: bool,
        node_channel_interpolation: str,
        additional_key: str,  # Used to differentiate between material / material node_tree
        export_settings
):

    __target = __gather_target(blender_type_data, blender_id, channel, additional_key, export_settings)
    if __target.path is not None:
        sampler = __gather_sampler(
            blender_type_data,
            blender_id,
            channel,
            action_name,
            node_channel_is_animated,
            node_channel_interpolation,
            additional_key,
            export_settings)

        if sampler is None:
            # After check, no need to animate this node for this channel
            return None

        animation_channel = gltf2_io.AnimationChannel(
            extensions=None,
            extras=None,
            sampler=sampler,
            target=__target
        )

        return animation_channel
    return None


def __gather_target(
    blender_type_data: str,
    blender_id: str,
    channel: str,
    additional_key: str,  # Used to differentiate between material / material node_tree
    export_settings
) -> gltf2_io.AnimationChannelTarget:

    return gather_data_sampled_channel_target(
        blender_type_data, blender_id, channel, additional_key, export_settings)


def __gather_sampler(
        blender_type_data,
        blender_id,
        channel,
        action_name,
        node_channel_is_animated,
        node_channel_interpolation,
        additional_key,
        export_settings):
    return gather_data_sampled_animation_sampler(
        blender_type_data,
        blender_id,
        channel,
        action_name,
        node_channel_is_animated,
        node_channel_interpolation,
        additional_key,
        export_settings
    )
