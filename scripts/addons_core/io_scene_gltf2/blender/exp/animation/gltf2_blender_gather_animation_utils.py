# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from mathutils import Matrix
from ....blender.com.gltf2_blender_data_path import get_sk_exported
from ....io.com import gltf2_io
from ....io.exp.gltf2_io_user_extensions import export_user_extensions
from ..gltf2_blender_gather_tree import VExportNode
from .sampled.armature.armature_action_sampled import gather_action_armature_sampled
from .sampled.object.gltf2_blender_gather_object_action_sampled import gather_action_object_sampled
from .sampled.shapekeys.gltf2_blender_gather_sk_channels import gather_sampled_sk_channel
from .sampled.data.gltf2_blender_gather_data_channels import gather_data_sampled_channels
from .gltf2_blender_gather_drivers import get_sk_drivers


def link_samplers(animation: gltf2_io.Animation, export_settings):
    """
    Move animation samplers to their own list and store their indices at their previous locations.

    After gathering, samplers are stored in the channels properties of the animation and need to be moved
    to their own list while storing an index into this list at the position where they previously were.
    This behaviour is similar to that of the glTFExporter that traverses all nodes
    :param animation:
    :param export_settings:
    :return:
    """
    # TODO: move this to some util module and update gltf2 exporter also
    T = typing.TypeVar('T')

    def __append_unique_and_get_index(l: typing.List[T], item: T):
        if item in l:
            return l.index(item)
        else:
            index = len(l)
            l.append(item)
            return index

    for i, channel in enumerate(animation.channels):
        animation.channels[i].sampler = __append_unique_and_get_index(animation.samplers, channel.sampler)


def reset_bone_matrix(blender_object, export_settings) -> None:
    if export_settings['gltf_export_reset_pose_bones'] is False:
        return

    # Only for armatures
    if blender_object.type != "ARMATURE":
        return

    # Remove current action if any
    if blender_object.animation_data and blender_object.animation_data.action:
        blender_object.animation_data.action = None

    # Resetting bones TRS to avoid to keep not keyed value on a future action set
    for bone in blender_object.pose.bones:
        bone.matrix_basis = Matrix()


def reset_sk_data(blender_object, blender_actions, export_settings) -> None:
    # Using NLA for SK is not so common
    # Reset to 0.0 will happen here only if there are at least 2 tracks to export
    if export_settings['gltf_export_reset_sk_data'] is False:
        return

    if len([i for i in blender_actions if i[2] == "SHAPEKEY"]) <= 1:
        return

    if blender_object.type != "MESH":
        return

    # Reset
    for sk in get_sk_exported(blender_object.data.shape_keys.key_blocks):
        sk.value = 0.0


def add_slide_data(start_frame, uuid: int, key: str, export_settings, add_drivers=True):

    if uuid not in export_settings['slide'].keys():
        export_settings['slide'][uuid] = {}
    export_settings['slide'][uuid][key] = start_frame

    # Add slide info for driver sk too
    if add_drivers is True:
        obj_drivers = get_sk_drivers(uuid, export_settings)
        for obj_dr in obj_drivers:
            if obj_dr not in export_settings['slide'].keys():
                export_settings['slide'][obj_dr] = {}
            export_settings['slide'][obj_dr][uuid + "_" + key] = start_frame


def merge_tracks_perform(merged_tracks, animations, export_settings):
    to_delete_idx = []
    for merged_anim_track in merged_tracks.keys():
        if len(merged_tracks[merged_anim_track]) < 2:

            # There is only 1 animation in the track
            # If name of the track is not a default name, use this name for action
            if len(merged_tracks[merged_anim_track]) != 0:
                animations[merged_tracks[merged_anim_track][0]].name = merged_anim_track

            continue

        base_animation_idx = None
        offset_sampler = 0

        for idx, anim_idx in enumerate(merged_tracks[merged_anim_track]):
            if idx == 0:
                base_animation_idx = anim_idx
                animations[anim_idx].name = merged_anim_track
                already_animated = []
                for channel in animations[anim_idx].channels:
                    already_animated.append((channel.target.node, channel.target.path))
                continue

            to_delete_idx.append(anim_idx)

            # Merging extensions
            # Provide a hook to handle extension merging since there is no way to know author intent
            export_user_extensions(
                'merge_animation_extensions_hook',
                export_settings,
                animations[anim_idx],
                animations[base_animation_idx])

            # Merging extras
            # Warning, some values can be overwritten if present in multiple merged animations
            if animations[anim_idx].extras is not None:
                for k in animations[anim_idx].extras.keys():
                    if animations[base_animation_idx].extras is None:
                        animations[base_animation_idx].extras = {}
                    animations[base_animation_idx].extras[k] = animations[anim_idx].extras[k]

            offset_sampler = len(animations[base_animation_idx].samplers)
            for sampler in animations[anim_idx].samplers:
                animations[base_animation_idx].samplers.append(sampler)

            for channel in animations[anim_idx].channels:
                if (channel.target.node, channel.target.path) in already_animated:
                    export_settings['log'].warning(
                        "Some strips have same channel animation ({}), on node {} !".format(
                            channel.target.path, channel.target.node.name))
                    continue
                animations[base_animation_idx].channels.append(channel)
                animations[base_animation_idx].channels[-1].sampler = animations[base_animation_idx].channels[-1].sampler + offset_sampler
                already_animated.append((channel.target.node, channel.target.path))

    new_animations = []
    if len(to_delete_idx) != 0:
        for idx, animation in enumerate(animations):
            if idx in to_delete_idx:
                continue
            new_animations.append(animation)
    else:
        new_animations = animations

    # If some strips have same channel animations, we already ignored some.
    # But if the channels was exactly the same, we already pick index of sampler, and we have a mix of samplers, and index of samplers, in animation.samplers
    # So get back to list of objects only
    # This can lead to unused samplers... but keep them, as, anyway, data are not exported properly
    for anim in new_animations:
        new_samplers = []
        for s in anim.samplers:
            if type(s) == int:
                new_samplers.append(anim.samplers[s])
            else:
                new_samplers.append(s)
        anim.samplers = new_samplers

    return new_animations


def bake_animation(obj_uuid: str, animation_key: str, export_settings, mode=None):

    # Bake situation does not export any extra animation channels, as we bake TRS + weights on Track or scene level, without direct
    # Access to fcurve and action data

    # if there is no animation in file => no need to bake
    if len(bpy.data.actions) == 0:
        return None

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object

    # No TRS animation are found for this object.
    # But we may need to bake
    # (Only when force sampling is ON)
    # If force sampling is OFF, can lead to inconsistent export anyway
    if (export_settings['gltf_bake_animation'] is True
            or export_settings['gltf_animation_mode'] == "NLA_TRACKS") \
            and blender_object and blender_object.type != "ARMATURE" and export_settings['gltf_force_sampling'] is True:
        animation = None
        # We also have to check if this is a skinned mesh, because we don't have to force animation baking on this case
        # (skinned meshes TRS must be ignored, says glTF specification)
        if export_settings['vtree'].nodes[obj_uuid].skin is None:
            if mode is None or mode == "OBJECT":
                animation, _ = gather_action_object_sampled(obj_uuid, None, animation_key, export_settings)

        # Need to bake sk only if not linked to a driver sk by parent armature
        # In case of NLA track export, no baking of SK
        if export_settings['gltf_morph_anim'] \
                and blender_object \
                and blender_object.type == "MESH" \
                and blender_object.data is not None \
                and blender_object.data.shape_keys is not None:

            ignore_sk = False
            if export_settings['vtree'].nodes[obj_uuid].parent_uuid is not None \
                    and export_settings['vtree'].nodes[export_settings['vtree'].nodes[obj_uuid].parent_uuid].blender_type == VExportNode.ARMATURE:
                obj_drivers = get_sk_drivers(export_settings['vtree'].nodes[obj_uuid].parent_uuid, export_settings)
                if obj_uuid in obj_drivers:
                    ignore_sk = True

            if mode == "OBJECT":
                ignore_sk = True

            if ignore_sk is False:
                channel = gather_sampled_sk_channel(obj_uuid, animation_key, export_settings)
                if channel is not None:
                    if animation is None:
                        animation = gltf2_io.Animation(
                            channels=[channel],
                            extensions=None,  # as other animations
                            extras=None,  # Because there is no animation to get extras from
                            name=blender_object.name,  # Use object name as animation name
                            samplers=[]
                        )
                    else:
                        animation.channels.append(channel)

        if animation is not None and animation.channels:
            link_samplers(animation, export_settings)
            return animation

    elif (export_settings['gltf_bake_animation'] is True
            or export_settings['gltf_animation_mode'] == "NLA_TRACKS") \
            and blender_object \
            and blender_object.type == "ARMATURE" \
            and mode is None or mode == "OBJECT":
        # We need to bake all bones. Because some bone can have some constraints linking to
        # some other armature bones, for example

        animation, _ = gather_action_armature_sampled(obj_uuid, None, animation_key, export_settings)
        link_samplers(animation, export_settings)
        if animation is not None:
            return animation
    return None


def bake_data_animation(blender_type_data, blender_id, animation_key, on_type, export_settings):
    # if there is no animation in file => no need to bake
    if len(bpy.data.actions) == 0:
        return None

    total_channels = []
    animation = None

    if (export_settings['gltf_bake_animation'] is True
            or export_settings['gltf_animation_mode'] == "NLA_TRACKS"):

        if blender_type_data == "materials":
            blender_data_object = [i for i in bpy.data.materials if id(i) == blender_id][0]
        elif blender_type_data == "cameras":
            blender_data_object = [i for i in bpy.data.cameras if id(i) == blender_id][0]
        elif blender_type_data == "lights":
            blender_data_object = [i for i in bpy.data.lights if id(i) == blender_id][0]
        else:
            pass  # Should not happen

        # Export now KHR_animation_pointer for materials / light / camera
        for i in [a for a in export_settings['KHR_animation_pointer'][blender_type_data].keys() if a == blender_id]:
            if len(export_settings['KHR_animation_pointer'][blender_type_data][i]['paths']) == 0:
                continue

            channels = gather_data_sampled_channels(blender_type_data, i, animation_key, on_type, export_settings)
            if channels is not None:
                total_channels.extend(channels)

    if len(total_channels) > 0:
        animation = gltf2_io.Animation(
            channels=total_channels,
            extensions=None,  # as other animations
            extras=None,  # Because there is no animation to get extras from
            name=blender_data_object.name,  # Use object name as animation name
            samplers=[]
        )

    if animation is not None and animation.channels:
        link_samplers(animation, export_settings)
        return animation
