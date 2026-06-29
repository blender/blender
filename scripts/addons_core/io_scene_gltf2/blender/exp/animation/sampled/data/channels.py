# SPDX-FileCopyrightText: 2018-2023 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import typing
from copy import deepcopy
from ......io.com import gltf2_io
from ......blender.com.conversion import get_gltf_interpolation
from .channel_target import gather_data_sampled_channel_target
from .sampler import gather_data_sampled_animation_sampler
from ...anim_extra_utils import gather_animated_blender_id


def gather_data_sampled_channels(blender_main_type, blender_type_data, blender_id, blender_action_name, slot_identifier,
                                 additional_key, export_settings) -> typing.List[gltf2_io.AnimationChannel]:
    channels = []

    list_of_animated_data_channels = {}  # TODOPointer

    baseColorFactor_alpha_merged_already_done = False

    used_blender_id = gather_animated_blender_id(
        blender_main_type,
        blender_type_data,
        blender_id,
        None,
        export_settings)

    # Extras (for ACTIONS or ACTIVE_ACTIONS, NLA Tracks, as for scene, slot_identifier is None)
    # For Scene, data will be managed by "classical" loop (including extras)
    if slot_identifier is not None and not slot_identifier.startswith("NT") and blender_main_type == 'extras' \
            and used_blender_id in export_settings['KHR_animation_pointer'][blender_main_type][blender_type_data]:
        for path in export_settings['KHR_animation_pointer'][
                blender_main_type][blender_type_data][used_blender_id]['paths'].keys():

            if not path.startswith("[\""):
                continue

            channel = gather_sampled_data_channel(
                blender_main_type,
                blender_type_data,
                blender_id,
                None,
                path,
                blender_action_name,
                slot_identifier,
                path in list_of_animated_data_channels.keys(),
                list_of_animated_data_channels[path] if path in list_of_animated_data_channels.keys() else get_gltf_interpolation(
                    export_settings['gltf_sampling_interpolation_fallback'],
                    export_settings),
                additional_key,
                export_settings)
            if channel is not None:
                channels.append(channel)

    # "classical" animation pointer (not extras)
    if blender_main_type != "extras" or (
        slot_identifier is None and export_settings['gltf_animation_mode'] in [
            "SCENE",
            "NLA_TRACKS"]):
        if (slot_identifier is not None and blender_main_type is None) or slot_identifier is None:
            for path in export_settings['KHR_animation_pointer'][blender_main_type][blender_type_data][used_blender_id]['paths'].keys(
            ):

                if slot_identifier is not None and slot_identifier.startswith("NT") and path.startswith("[\""):
                    continue

                # Do not manage alpha, as it will be managaed by the baseColorFactor (merging Color and alpha)
                if export_settings['KHR_animation_pointer'][blender_main_type][blender_type_data][used_blender_id]['paths'][path][
                        'path'] == "/materials/XXX/pbrMetallicRoughness/baseColorFactor" and baseColorFactor_alpha_merged_already_done is True:
                    continue

                if slot_identifier is not None and not slot_identifier.startswith(
                        "NT") and path.startswith("node_tree."):
                    continue

                if blender_main_type == "extras" and blender_type_data == "bones" and export_settings[
                        'gltf_animation_mode'] == "NLA_TRACKS":
                    continue

                channel = gather_sampled_data_channel(
                    blender_main_type,
                    blender_type_data,
                    blender_id,
                    None,
                    path,
                    blender_action_name,
                    slot_identifier,
                    path in list_of_animated_data_channels.keys(),
                    list_of_animated_data_channels[path] if path in list_of_animated_data_channels.keys() else get_gltf_interpolation(
                        export_settings['gltf_sampling_interpolation_fallback'],
                        export_settings),
                    additional_key,
                    export_settings)
                if channel is not None:
                    channels.append(channel)

                    # Manage multiple Texture Transform for the same path
                    for additional_path in export_settings['KHR_animation_pointer'][blender_main_type][blender_type_data][blender_id]['paths'][path].get('additional', [
                    ]):

                        new_target = gltf2_io.AnimationChannelTarget(
                            extensions=channel.target.extensions,
                            extras=channel.target.extras,
                            node=channel.target.node,
                            path=additional_path
                        )

                        new_sampler = gltf2_io.AnimationSampler(
                            extensions=None,
                            extras=None,
                            input=deepcopy(channel.sampler.input),
                            interpolation=channel.sampler.interpolation,
                            output=deepcopy(channel.sampler.output)
                        )

                        new_channel = gltf2_io.AnimationChannel(
                            extensions=None,
                            extras=None,
                            sampler=new_sampler,
                            target=new_target
                        )

                        channels.append(new_channel)

                if export_settings['KHR_animation_pointer'][blender_main_type][blender_type_data][used_blender_id][
                        'paths'][path]['path'] == "/materials/XXX/pbrMetallicRoughness/baseColorFactor":
                    baseColorFactor_alpha_merged_already_done = True

    return channels


def gather_sampled_data_channel(
        blender_main_type,
        blender_type_data: str,
        blender_id: str,
        bone_name: typing.Optional[str],
        channel: str,
        action_name: str,
        slot_identifier: str,
        node_channel_is_animated: bool,
        node_channel_interpolation: str,
        additional_key: str,  # Used to differentiate between material / material node_tree
        export_settings
):

    __target = __gather_target(
        blender_main_type,
        blender_type_data,
        blender_id,
        bone_name,
        channel,
        additional_key,
        export_settings)
    if __target.path is not None:
        sampler, alpha_cst = __gather_sampler(
            blender_main_type,
            blender_type_data,
            blender_id,
            bone_name,
            channel,
            action_name,
            slot_identifier,
            node_channel_is_animated,
            node_channel_interpolation,
            additional_key,
            export_settings)

        if sampler is None:
            # After check, no need to animate this node for this channel
            return None

        # Add temporatory data for alpha, in target object
        __target.tmp_alpha_cst = alpha_cst

        animation_channel = gltf2_io.AnimationChannel(
            extensions=None,
            extras=None,
            sampler=sampler,
            target=__target
        )

        return animation_channel
    return None


def __gather_target(
    blender_main_type,
    blender_type_data: str,
    blender_id: str,
    bone_name: typing.Optional[str],
    channel: str,
    additional_key: str,  # Used to differentiate between material / material node_tree
    export_settings
) -> gltf2_io.AnimationChannelTarget:

    return gather_data_sampled_channel_target(
        blender_main_type, blender_type_data, blender_id, bone_name, channel, additional_key, export_settings)


def __gather_sampler(
        blender_main_type,
        blender_type_data,
        blender_id,
        bone_name,
        channel,
        action_name,
        slot_identifier,
        node_channel_is_animated,
        node_channel_interpolation,
        additional_key,
        export_settings):
    return gather_data_sampled_animation_sampler(
        blender_main_type,
        blender_type_data,
        blender_id,
        bone_name,
        channel,
        action_name,
        slot_identifier,
        node_channel_is_animated,
        node_channel_interpolation,
        additional_key,
        export_settings
    )
