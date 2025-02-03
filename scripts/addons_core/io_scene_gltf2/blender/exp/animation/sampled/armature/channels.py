# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from ......io.com import gltf2_io
from ......io.exp.user_extensions import export_user_extensions
from .....com.conversion import get_gltf_interpolation
from .....com.conversion import get_target, get_channel_from_target
from ...fcurves.channels import get_channel_groups
from ...fcurves.channels import needs_baking
from ...drivers import get_sk_drivers
from ..object.channels import gather_sampled_object_channel
from ..shapekeys.channels import gather_sampled_sk_channel
from .channel_target import gather_armature_sampled_channel_target
from .sampler import gather_bone_sampled_animation_sampler


def gather_armature_sampled_channels(armature_uuid, blender_action_name, slot_identifier,
                                     export_settings) -> typing.List[gltf2_io.AnimationChannel]:
    channels = []
    extra_channels = {}

    # Then bake all bones
    bones_to_be_animated = []
    bones_uuid = export_settings["vtree"].get_all_bones(armature_uuid)
    bones_to_be_animated = [
        export_settings["vtree"].nodes[b].blender_bone.name for b in bones_uuid if export_settings["vtree"].nodes[b].leaf_reference is None]

    # List of really animated bones is needed for optimization decision
    list_of_animated_bone_channels = {}
    if slot_identifier is not None:
        if armature_uuid != blender_action_name and blender_action_name in bpy.data.actions:
            # Not bake situation
            channels_animated, to_be_sampled, extra_channels = get_channel_groups(
                armature_uuid, bpy.data.actions[blender_action_name],
                    bpy.data.actions[blender_action_name].slots[slot_identifier], export_settings)
            for chan in [chan for chan in channels_animated.values() if chan['bone'] is not None]:
                for prop in chan['properties'].keys():
                    list_of_animated_bone_channels[(chan['bone'], get_channel_from_target(get_target(prop)))] = get_gltf_interpolation(
                        chan['properties'][prop][0].keyframe_points[0].interpolation, export_settings)  # Could be exported without sampling : keep interpolation

            for _, _, chan_prop, chan_bone in [chan for chan in to_be_sampled if chan[1] == "BONE"]:
                list_of_animated_bone_channels[
                    (
                        chan_bone,
                        chan_prop,
                    )
                ] = get_gltf_interpolation(export_settings['gltf_sampling_interpolation_fallback'], export_settings)  # if forced to be sampled, keep the interpolation chosen by the user
    else:
        pass
        # There is no animated channels (because if it was, we would have a slot_identifier)
        # We are in a bake situation

    for bone in bones_to_be_animated:
        for p in ["location", "rotation_quaternion", "scale"]:
            channel = gather_sampled_bone_channel(
                armature_uuid,
                bone,
                p,
                blender_action_name,
                slot_identifier,
                (bone, p) in list_of_animated_bone_channels.keys(),
                list_of_animated_bone_channels[(bone, p)] if (bone, p) in list_of_animated_bone_channels.keys() else get_gltf_interpolation(export_settings['gltf_sampling_interpolation_fallback'], export_settings),
                export_settings)
            if channel is not None:
                channels.append(channel)

    bake_interpolation = get_gltf_interpolation(export_settings['gltf_sampling_interpolation_fallback'], export_settings)
    # Retrieve animation on armature object itself, if any
    if blender_action_name == armature_uuid or export_settings['gltf_animation_mode'] in ["SCENE", "NLA_TRACKS"]:
        # If armature is baked (no animation of armature), need to use all channels
        armature_channels = [
            ["location", bake_interpolation],
            ["rotation_quaternion", bake_interpolation],
            ["scale", bake_interpolation]
        ]
        animated_channels = []
    else:
        # The armature has some channel(s) animated, checking which one(s)
        armature_channels = __gather_armature_object_channel(
            armature_uuid, bpy.data.actions[blender_action_name], slot_identifier, export_settings)
        animated_channels = armature_channels

    for (p, i) in armature_channels:
        armature_channel = gather_sampled_object_channel(
            armature_uuid,
            p,
            blender_action_name,
            slot_identifier,
            p in [a[0] for a in animated_channels],
            [c[1] for c in animated_channels if c[0] == p][0] if p in [a[0] for a in animated_channels] else bake_interpolation,
            export_settings
        )

        if armature_channel is not None:
            channels.append(armature_channel)

    # Retrieve channels for drivers, if needed
    drivers_to_manage = get_sk_drivers(armature_uuid, export_settings)
    for obj_driver_uuid in drivers_to_manage:
        channel = gather_sampled_sk_channel(obj_driver_uuid, armature_uuid + "_" + blender_action_name, slot_identifier, export_settings)
        if channel is not None:
            channels.append(channel)

    return channels, extra_channels


def gather_sampled_bone_channel(
        armature_uuid: str,
        bone: str,
        channel: str,
        action_name: str,
        slot_identifier: str,
        node_channel_is_animated: bool,
        node_channel_interpolation: str,
        export_settings
):

    __target = __gather_target(armature_uuid, bone, channel, export_settings)
    if __target.path is not None:
        sampler = __gather_sampler(
            armature_uuid,
            bone,
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
                               export_settings['vtree'].nodes[armature_uuid].blender_object,
                               bone,
                               action_name,
                               node_channel_is_animated
                               )

        return animation_channel
    return None


def __gather_target(armature_uuid: str,
                    bone: str,
                    channel: str,
                    export_settings
                    ) -> gltf2_io.AnimationChannelTarget:

    return gather_armature_sampled_channel_target(
        armature_uuid, bone, channel, export_settings)


def __gather_sampler(
        armature_uuid,
        bone,
        channel,
        action_name,
        slot_identifier, #TODOSLOT
        node_channel_is_animated,
        node_channel_interpolation,
        export_settings):
    return gather_bone_sampled_animation_sampler(
        armature_uuid,
        bone,
        channel,
        action_name,
        slot_identifier, #TODOSLOT
        node_channel_is_animated,
        node_channel_interpolation,
        export_settings
    )


def __gather_armature_object_channel(obj_uuid: str, blender_action, slot_identifier, export_settings):
    channels = []

    channels_animated, to_be_sampled, extra_channels = get_channel_groups(obj_uuid, blender_action, blender_action.slots[slot_identifier], export_settings)
    # Remove all channel linked to bones, keep only directly object channels
    channels_animated = [c for c in channels_animated.values() if c['type'] == "OBJECT"]
    to_be_sampled = [c for c in to_be_sampled if c[1] == "OBJECT"]

    original_channels = []
    for c in channels_animated:
        original_channels.extend([(prop, c['properties'][prop][0].keyframe_points[0].interpolation)
                                 for prop in c['properties'].keys()])

    for c, inter in original_channels:
        channels.append(
            (
                {
                    "location": "location",
                    "rotation_quaternion": "rotation_quaternion",
                    "rotation_euler": "rotation_quaternion",
                    "scale": "scale",
                    "delta_location": "location",
                    "delta_scale": "scale",
                    "delta_rotation_euler": "rotation_quaternion",
                    "delta_rotation_quaternion": "rotation_quaternion"
                }.get(c),
                get_gltf_interpolation(inter, export_settings)
            )
        )

    for c in to_be_sampled:
        channels.append(
            (
                {
                    "location": "location",
                    "rotation_quaternion": "rotation_quaternion",
                    "rotation_euler": "rotation_quaternion",
                    "scale": "scale",
                    "delta_location": "location",
                    "delta_scale": "scale",
                    "delta_rotation_euler": "rotation_quaternion",
                    "delta_rotation_quaternion": "rotation_quaternion"
                }.get(c[2]),
                get_gltf_interpolation(export_settings['gltf_sampling_interpolation_fallback'], export_settings)  # Forced to be sampled, so use the interpolation chosen by the user
            )
        )

    return channels
