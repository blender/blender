# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from ......io.com import gltf2_io
from ......io.exp.user_extensions import export_user_extensions
from ......blender.com.conversion import get_gltf_interpolation, get_target, get_channel_from_target
from ....cache import cached
from ...fcurves.channels import get_channel_groups
from .sampler import gather_object_sampled_animation_sampler
from .channel_target import gather_object_sampled_channel_target


def gather_object_sampled_channels(object_uuid: str, blender_action_name: str, slot_identifier: str,
                                   export_settings) -> typing.List[gltf2_io.AnimationChannel]:
    channels = []
    extra_channels = {}

    # Bake situation does not export any extra animation channels, as we bake TRS + weights on Track or scene level, without direct
    # Access to fcurve and action data

    list_of_animated_channels = {}
    if slot_identifier is not None:
        if object_uuid != blender_action_name and blender_action_name in bpy.data.actions:
            # Not bake situation
            channels_animated, to_be_sampled, extra_channels = get_channel_groups(
                object_uuid, bpy.data.actions[blender_action_name], bpy.data.actions[blender_action_name].slots[slot_identifier], export_settings)
            for chan in [chan for chan in channels_animated.values() if chan['bone'] is None]:
                for prop in chan['properties'].keys():
                    list_of_animated_channels[get_channel_from_target(get_target(prop))] = get_gltf_interpolation(
                        chan['properties'][prop][0].keyframe_points[0].interpolation, export_settings)  # Could be exported without sampling : keep interpolation

            for _, _, chan_prop, _ in [chan for chan in to_be_sampled if chan[1] == "OBJECT"]:
                list_of_animated_channels[chan_prop] = get_gltf_interpolation(
                    export_settings['gltf_sampling_interpolation_fallback'], export_settings)  # if forced to be sampled, keep the interpolation chosen by the user
    else:
        pass
        # There is no animated channels (because if it was, we would have a slot_identifier)
        # We are in a bake situation

    for p in ["location", "rotation_quaternion", "scale"]:
        channel = gather_sampled_object_channel(
            object_uuid,
            p,
            blender_action_name,
            slot_identifier, #TODOSLOT
            p in list_of_animated_channels.keys(),
            list_of_animated_channels[p] if p in list_of_animated_channels.keys() else get_gltf_interpolation(export_settings['gltf_sampling_interpolation_fallback'], export_settings),
            export_settings
        )
        if channel is not None:
            channels.append(channel)

    blender_object = export_settings['vtree'].nodes[object_uuid].blender_object
    export_user_extensions('animation_gather_object_channel', export_settings, blender_object, blender_action_name)

    return channels if len(channels) > 0 else None, extra_channels


@cached
def gather_sampled_object_channel(
        obj_uuid: str,
        channel: str,
        action_name: str,
        slot_identifier: str,
        node_channel_is_animated: bool,
        node_channel_interpolation: str,
        export_settings
):

    __target = __gather_target(obj_uuid, channel, export_settings)
    if __target.path is not None:
        sampler = __gather_sampler(
            obj_uuid,
            channel,
            action_name,
            slot_identifier,
            node_channel_is_animated,
            node_channel_interpolation,
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

        export_user_extensions('gather_animation_channel_hook',
                               export_settings,
                               animation_channel,
                               channel,
                               export_settings['vtree'].nodes[obj_uuid].blender_object,
                               None,  # No bone
                               action_name,
                               node_channel_is_animated
                               )

        return animation_channel
    return None


def __gather_target(
        obj_uuid: str,
        channel: str,
        export_settings
):

    return gather_object_sampled_channel_target(
        obj_uuid, channel, export_settings)


def __gather_sampler(
        obj_uuid: str,
        channel: str,
        action_name: str,
        slot_identifier: int,
        node_channel_is_animated: bool,
        node_channel_interpolation: str,
        export_settings):

    return gather_object_sampled_animation_sampler(
        obj_uuid,
        channel,
        action_name,
        slot_identifier,
        node_channel_is_animated,
        node_channel_interpolation,
        export_settings
    )
