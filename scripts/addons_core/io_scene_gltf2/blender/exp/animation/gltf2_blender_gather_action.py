# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from ....io.com import gltf2_io
from ....io.exp.gltf2_io_user_extensions import export_user_extensions
from ....blender.com.gltf2_blender_conversion import get_gltf_interpolation
from ...com.gltf2_blender_data_path import is_bone_anim_channel
from ...com.gltf2_blender_extras import generate_extras
from ..gltf2_blender_gather_cache import cached
from ..gltf2_blender_gather_tree import VExportNode
from .fcurves.gltf2_blender_gather_fcurves_animation import gather_animation_fcurves
from .sampled.armature.armature_action_sampled import gather_action_armature_sampled
from .sampled.armature.armature_channels import gather_sampled_bone_channel
from .sampled.object.gltf2_blender_gather_object_action_sampled import gather_action_object_sampled
from .sampled.shapekeys.gltf2_blender_gather_sk_action_sampled import gather_action_sk_sampled
from .sampled.object.gltf2_blender_gather_object_channels import gather_object_sampled_channels, gather_sampled_object_channel
from .sampled.shapekeys.gltf2_blender_gather_sk_channels import gather_sampled_sk_channel
from .gltf2_blender_gather_drivers import get_sk_drivers
from .gltf2_blender_gather_animation_utils import reset_bone_matrix, reset_sk_data, link_samplers, add_slide_data, merge_tracks_perform, bake_animation


def gather_actions_animations(export_settings):

    prepare_actions_range(export_settings)

    animations = []
    merged_tracks = {}

    vtree = export_settings['vtree']
    for obj_uuid in vtree.get_all_objects():

        # Do not manage not exported objects
        if vtree.nodes[obj_uuid].node is None:
            if export_settings["gltf_armature_object_remove"] is True:
                # Manage armature object, as this is the object that has the animation
                if not vtree.nodes[obj_uuid].blender_object:
                    continue
            else:
                continue

        if export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.COLLECTION:
            continue

        animations_, merged_tracks = gather_action_animations(obj_uuid, merged_tracks, len(animations), export_settings)
        animations += animations_

    if export_settings['gltf_animation_mode'] == "ACTIVE_ACTIONS":
        # Fake an animation with all animations of the scene
        merged_tracks = {}
        merged_tracks_name = 'Animation'
        if(len(export_settings['gltf_nla_strips_merged_animation_name']) > 0):
            merged_tracks_name = export_settings['gltf_nla_strips_merged_animation_name']
        merged_tracks[merged_tracks_name] = []
        for idx, animation in enumerate(animations):
            merged_tracks[merged_tracks_name].append(idx)

    new_animations = merge_tracks_perform(merged_tracks, animations, export_settings)

    return new_animations


def prepare_actions_range(export_settings):

    track_slide = {}

    vtree = export_settings['vtree']
    for obj_uuid in vtree.get_all_objects():

        if vtree.nodes[obj_uuid].blender_type == VExportNode.COLLECTION:
            continue

        # Do not manage not exported objects
        if vtree.nodes[obj_uuid].node is None:
            if export_settings["gltf_armature_object_remove"] is True:
                # Manage armature object, as this is the object that has the animation
                if not vtree.nodes[obj_uuid].blender_object:
                    continue
            else:
                continue

        if obj_uuid not in export_settings['ranges']:
            export_settings['ranges'][obj_uuid] = {}

        blender_actions = __get_blender_actions(obj_uuid, export_settings)
        for blender_action, track, type_ in blender_actions:

            # What about frame_range bug for single keyframe animations ? 107030
            start_frame = int(blender_action.frame_range[0])
            end_frame = int(blender_action.frame_range[1])

            if end_frame - start_frame == 1:
                # To workaround Blender bug 107030, check manually
                try:  # Avoid crash in case of strange/buggy fcurves
                    start_frame = int(min([c.range()[0] for c in blender_action.fcurves]))
                    end_frame = int(max([c.range()[1] for c in blender_action.fcurves]))
                except:
                    pass

            export_settings['ranges'][obj_uuid][blender_action.name] = {}

            # If some negative frame and crop -> set start at 0
            if start_frame < 0 and export_settings['gltf_negative_frames'] == "CROP":
                start_frame = 0

            if export_settings['gltf_frame_range'] is True:
                start_frame = max(bpy.context.scene.frame_start, start_frame)
                end_frame = min(bpy.context.scene.frame_end, end_frame)

            export_settings['ranges'][obj_uuid][blender_action.name]['start'] = start_frame
            export_settings['ranges'][obj_uuid][blender_action.name]['end'] = end_frame

            if export_settings['gltf_negative_frames'] == "SLIDE":
                if track is not None:
                    if not (track.startswith("NlaTrack") or track.startswith("[Action Stash]")):
                        if track not in track_slide.keys() or (
                                track in track_slide.keys() and start_frame < track_slide[track]):
                            track_slide.update({track: start_frame})
                    else:
                        if start_frame < 0:
                            add_slide_data(start_frame, obj_uuid, blender_action.name, export_settings)
                else:
                    if export_settings['gltf_animation_mode'] == "ACTIVE_ACTIONS":
                        if None not in track_slide.keys() or (
                                None in track_slide.keys() and start_frame < track_slide[None]):
                            track_slide.update({None: start_frame})
                    else:
                        if start_frame < 0:
                            add_slide_data(start_frame, obj_uuid, blender_action.name, export_settings)

            if export_settings['gltf_anim_slide_to_zero'] is True and start_frame > 0:
                if track is not None:
                    if not (track.startswith("NlaTrack") or track.startswith("[Action Stash]")):
                        if track not in track_slide.keys() or (
                                track in track_slide.keys() and start_frame < track_slide[track]):
                            track_slide.update({track: start_frame})
                    else:
                        add_slide_data(start_frame, obj_uuid, blender_action.name, export_settings)
                else:
                    if export_settings['gltf_animation_mode'] == "ACTIVE_ACTIONS":
                        if None not in track_slide.keys() or (
                                None in track_slide.keys() and start_frame < track_slide[None]):
                            track_slide.update({None: start_frame})
                    else:
                        add_slide_data(start_frame, obj_uuid, blender_action.name, export_settings)

            if type_ == "SHAPEKEY" and export_settings['gltf_bake_animation']:
                export_settings['ranges'][obj_uuid][obj_uuid] = {}
                export_settings['ranges'][obj_uuid][obj_uuid]['start'] = bpy.context.scene.frame_start
                export_settings['ranges'][obj_uuid][obj_uuid]['end'] = bpy.context.scene.frame_end

            # For baking drivers
            if export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.ARMATURE and export_settings['gltf_morph_anim'] is True:
                obj_drivers = get_sk_drivers(obj_uuid, export_settings)
                for obj_dr in obj_drivers:
                    if obj_dr not in export_settings['ranges']:
                        export_settings['ranges'][obj_dr] = {}
                    export_settings['ranges'][obj_dr][obj_uuid + "_" + blender_action.name] = {}
                    export_settings['ranges'][obj_dr][obj_uuid + "_" + blender_action.name]['start'] = start_frame
                    export_settings['ranges'][obj_dr][obj_uuid + "_" + blender_action.name]['end'] = end_frame

        if len(blender_actions) == 0 and export_settings['gltf_bake_animation']:
            # No animation on this object
            # In case of baking animation, we will use scene frame range
            # Will be calculated later if max range. Can be set here if scene frame range
            export_settings['ranges'][obj_uuid][obj_uuid] = {}
            export_settings['ranges'][obj_uuid][obj_uuid]['start'] = bpy.context.scene.frame_start
            export_settings['ranges'][obj_uuid][obj_uuid]['end'] = bpy.context.scene.frame_end

            # For baking drivers
            if export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.ARMATURE and export_settings['gltf_morph_anim'] is True:
                obj_drivers = get_sk_drivers(obj_uuid, export_settings)
                for obj_dr in obj_drivers:
                    if obj_dr not in export_settings['ranges']:
                        export_settings['ranges'][obj_dr] = {}
                    export_settings['ranges'][obj_dr][obj_uuid + "_" + obj_uuid] = {}
                    export_settings['ranges'][obj_dr][obj_uuid + "_" +
                                                      obj_uuid]['start'] = bpy.context.scene.frame_start
                    export_settings['ranges'][obj_dr][obj_uuid + "_" + obj_uuid]['end'] = bpy.context.scene.frame_end

    if (export_settings['gltf_negative_frames'] == "SLIDE"
            or export_settings['gltf_anim_slide_to_zero'] is True) \
            and len(track_slide) > 0:
        # Need to store animation slides
        for obj_uuid in vtree.get_all_objects():

            # Do not manage not exported objects
            if vtree.nodes[obj_uuid].node is None:
                if export_settings['gltf_armature_object_remove'] is True:
                    # Manage armature object, as this is the object that has the animation
                    if not vtree.nodes[obj_uuid].blender_object:
                        continue
                else:
                    continue

            blender_actions = __get_blender_actions(obj_uuid, export_settings)
            for blender_action, track, type_ in blender_actions:
                if track in track_slide.keys():
                    if export_settings['gltf_negative_frames'] == "SLIDE" and track_slide[track] < 0:
                        add_slide_data(track_slide[track], obj_uuid, blender_action.name, export_settings)
                    elif export_settings['gltf_anim_slide_to_zero'] is True:
                        add_slide_data(track_slide[track], obj_uuid, blender_action.name, export_settings)


def gather_action_animations(obj_uuid: int,
                             tracks: typing.Dict[str,
                                                 typing.List[int]],
                             offset: int,
                             export_settings) -> typing.Tuple[typing.List[gltf2_io.Animation],
                                                              typing.Dict[str,
                                                                          typing.List[int]]]:
    """
    Gather all animations which contribute to the objects property, and corresponding track names

    :param blender_object: The blender object which is animated
    :param export_settings:
    :return: A list of glTF2 animations and tracks
    """
    animations = []

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object

    # Collect all 'actions' affecting this object. There is a direct mapping between blender actions and glTF animations
    blender_actions = __get_blender_actions(obj_uuid, export_settings)

    # When object is not animated at all (no SK)
    # We can create an animation for this object
    if len(blender_actions) == 0:
        animation = bake_animation(obj_uuid, obj_uuid, export_settings)
        if animation is not None:
            animations.append(animation)


# Keep current situation and prepare export
    current_action = None
    current_sk_action = None
    current_world_matrix = None
    if blender_object and blender_object.animation_data and blender_object.animation_data.action:
        # There is an active action. Storing it, to be able to restore after switching all actions during export
        current_action = blender_object.animation_data.action
    elif len(blender_actions) != 0 and blender_object.animation_data is not None and blender_object.animation_data.action is None:
        # No current action set, storing world matrix of object
        current_world_matrix = blender_object.matrix_world.copy()

    if blender_object and blender_object.type == "MESH" \
            and blender_object.data is not None \
            and blender_object.data.shape_keys is not None \
            and blender_object.data.shape_keys.animation_data is not None \
            and blender_object.data.shape_keys.animation_data.action is not None:
        current_sk_action = blender_object.data.shape_keys.animation_data.action

    # Remove any solo (starred) NLA track. Restored after export
    solo_track = None
    if blender_object and blender_object.animation_data:
        for track in blender_object.animation_data.nla_tracks:
            if track.is_solo:
                solo_track = track
                track.is_solo = False
                break

    # Remove any tweak mode. Restore after export
    if blender_object and blender_object.animation_data:
        restore_tweak_mode = blender_object.animation_data.use_tweak_mode

    # Remove use of NLA. Restore after export
    if blender_object and blender_object.animation_data:
        current_use_nla = blender_object.animation_data.use_nla
        blender_object.animation_data.use_nla = False

    # Try to disable all except armature in viewport, for performance
    if export_settings['gltf_optimize_armature_disable_viewport'] \
            and export_settings['vtree'].nodes[obj_uuid].blender_object.type == "ARMATURE":

        # If the skinned mesh has driver(s), we can't disable it to bake armature.
        need_to_enable_again = False
        sk_drivers = get_sk_drivers(obj_uuid, export_settings)
        if len(sk_drivers) == 0:
            need_to_enable_again = True
            # Before baking, disabling from viewport all meshes
            for obj in [n.blender_object for n in export_settings['vtree'].nodes.values() if n.blender_type in
                        [VExportNode.OBJECT, VExportNode.ARMATURE, VExportNode.COLLECTION]]:
                obj.hide_viewport = True
            export_settings['vtree'].nodes[obj_uuid].blender_object.hide_viewport = False
        else:
            export_settings['log'].warning("Can't disable viewport because of drivers")
            # We changed the option here, so we don't need to re-check it later, during
            export_settings['gltf_optimize_armature_disable_viewport'] = False

    export_user_extensions('animation_switch_loop_hook', export_settings, blender_object, False)

# Export

    # Export all collected actions.
    for blender_action, track_name, on_type in blender_actions:

        # Set action as active, to be able to bake if needed
        if on_type == "OBJECT":  # Not for shapekeys!
            if blender_object.animation_data.action is None \
                    or (blender_object.animation_data.action.name != blender_action.name):
                if blender_object.animation_data.is_property_readonly('action'):
                    blender_object.animation_data.use_tweak_mode = False
                try:
                    reset_bone_matrix(blender_object, export_settings)
                    export_user_extensions(
                        'pre_animation_switch_hook',
                        export_settings,
                        blender_object,
                        blender_action,
                        track_name,
                        on_type)
                    blender_object.animation_data.action = blender_action
                    export_user_extensions(
                        'post_animation_switch_hook',
                        export_settings,
                        blender_object,
                        blender_action,
                        track_name,
                        on_type)
                except:
                    error = "Action is readonly. Please check NLA editor"
                    export_settings['log'].warning(
                        "Animation '{}' could not be exported. Cause: {}".format(
                            blender_action.name, error))
                    continue

        if on_type == "SHAPEKEY":
            if blender_object.data.shape_keys.animation_data.action is None \
                    or (blender_object.data.shape_keys.animation_data.action.name != blender_action.name):
                if blender_object.data.shape_keys.animation_data.is_property_readonly('action'):
                    blender_object.data.shape_keys.animation_data.use_tweak_mode = False
                reset_sk_data(blender_object, blender_actions, export_settings)
                export_user_extensions(
                    'pre_animation_switch_hook',
                    export_settings,
                    blender_object,
                    blender_action,
                    track_name,
                    on_type)
                blender_object.data.shape_keys.animation_data.action = blender_action
                export_user_extensions(
                    'post_animation_switch_hook',
                    export_settings,
                    blender_object,
                    blender_action,
                    track_name,
                    on_type)

        if export_settings['gltf_force_sampling'] is True:
            if export_settings['vtree'].nodes[obj_uuid].blender_object.type == "ARMATURE":
                animation, extra_samplers = gather_action_armature_sampled(
                    obj_uuid, blender_action, None, export_settings)
            elif on_type == "OBJECT":
                animation, extra_samplers = gather_action_object_sampled(
                    obj_uuid, blender_action, None, export_settings)
            else:
                animation = gather_action_sk_sampled(obj_uuid, blender_action, None, export_settings)
        else:
            # Not sampled
            # This returns
            #  - animation on fcurves
            #  - fcurve that cannot be handled not sampled, to be sampled
            # to_be_sampled is : (object_uuid , type , prop, optional(bone.name) )
            animation, to_be_sampled, extra_samplers = gather_animation_fcurves(
                obj_uuid, blender_action, export_settings)
            for (obj_uuid, type_, prop, bone) in to_be_sampled:
                if type_ == "BONE":
                    channel = gather_sampled_bone_channel(
                        obj_uuid,
                        bone,
                        prop,
                        blender_action.name,
                        True,
                        get_gltf_interpolation("LINEAR"),
                        export_settings)
                elif type_ == "OBJECT":
                    channel = gather_sampled_object_channel(
                        obj_uuid, prop, blender_action.name, True, get_gltf_interpolation("LINEAR"), export_settings)
                elif type_ == "SK":
                    channel = gather_sampled_sk_channel(obj_uuid, blender_action.name, export_settings)
                elif type_ == "EXTRA":
                    channel = None
                else:
                    export_settings['log'].error("Type unknown. Should not happen")

                if animation is None and channel is not None:
                    # If all channels need to be sampled, no animation was created
                    # Need to create animation, and add channel
                    animation = gltf2_io.Animation(
                        channels=[channel],
                        extensions=None,
                        extras=__gather_extras(blender_action, export_settings),
                        name=blender_action.name,
                        samplers=[]
                    )
                else:
                    if channel is not None:
                        animation.channels.append(channel)

        # Add extra samplers
        # Because this is not core glTF specification, you can add extra samplers using hook
        if export_settings['gltf_export_extra_animations'] and len(extra_samplers) != 0:
            export_user_extensions(
                'extra_animation_manage',
                export_settings,
                extra_samplers,
                obj_uuid,
                blender_object,
                blender_action,
                animation)

        # If we are in a SK animation, and we need to bake (if there also in TRS anim)
        if len([a for a in blender_actions if a[2] == "OBJECT"]) == 0 and on_type == "SHAPEKEY":
            if export_settings['gltf_bake_animation'] is True and export_settings['gltf_force_sampling'] is True:
                # We also have to check if this is a skinned mesh, because we don't have to force animation baking on this case
                # (skinned meshes TRS must be ignored, says glTF specification)
                if export_settings['vtree'].nodes[obj_uuid].skin is None:
                    if obj_uuid not in export_settings['ranges'].keys():
                        export_settings['ranges'][obj_uuid] = {}
                    export_settings['ranges'][obj_uuid][obj_uuid] = export_settings['ranges'][obj_uuid][blender_action.name]
                    channels, _ = gather_object_sampled_channels(obj_uuid, obj_uuid, export_settings)
                    if channels is not None:
                        if animation is None:
                            animation = gltf2_io.Animation(
                                channels=channels,
                                extensions=None,  # as other animations
                                extras=None,  # Because there is no animation to get extras from
                                name=blender_object.name,  # Use object name as animation name
                                samplers=[]
                            )
                        else:
                            animation.channels.extend(channels)

        if len([a for a in blender_actions if a[2] == "SHAPEKEY"]) == 0 \
                and export_settings['gltf_morph_anim'] \
                and blender_object.type == "MESH" \
                and blender_object.data is not None \
            and blender_object.data.shape_keys is not None:
            if export_settings['gltf_bake_animation'] is True and export_settings['gltf_force_sampling'] is True:
                # We need to check that this mesh is not driven by armature parent
                # In that case, no need to bake, because animation is already baked by driven sk armature
                ignore_sk = False
                if export_settings['vtree'].nodes[obj_uuid].parent_uuid is not None \
                        and export_settings['vtree'].nodes[export_settings['vtree'].nodes[obj_uuid].parent_uuid].blender_type == VExportNode.ARMATURE:
                    obj_drivers = get_sk_drivers(export_settings['vtree'].nodes[obj_uuid].parent_uuid, export_settings)
                    if obj_uuid in obj_drivers:
                        ignore_sk = True

                if ignore_sk is False:
                    if obj_uuid not in export_settings['ranges'].keys():
                        export_settings['ranges'][obj_uuid] = {}
                    export_settings['ranges'][obj_uuid][obj_uuid] = export_settings['ranges'][obj_uuid][blender_action.name]
                    channel = gather_sampled_sk_channel(obj_uuid, obj_uuid, export_settings)
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

        if animation is not None:
            link_samplers(animation, export_settings)
            animations.append(animation)

            # Store data for merging animation later
            if track_name is not None:  # Do not take into account animation not in NLA
                # Do not take into account default NLA track names
                if not (track_name.startswith("NlaTrack") or track_name.startswith("[Action Stash]")):
                    if track_name not in tracks.keys():
                        tracks[track_name] = []
                    tracks[track_name].append(offset + len(animations) - 1)  # Store index of animation in animations


# Restoring current situation

    # Restore action status
    # TODO: do this in a finally
    if blender_object and blender_object.animation_data:
        if blender_object.animation_data.action is not None:
            if current_action is None:
                # remove last exported action
                reset_bone_matrix(blender_object, export_settings)
                blender_object.animation_data.action = None
            elif blender_object.animation_data.action.name != current_action.name:
                # Restore action that was active at start of exporting
                reset_bone_matrix(blender_object, export_settings)
                blender_object.animation_data.action = current_action
        if solo_track is not None:
            solo_track.is_solo = True
        blender_object.animation_data.use_tweak_mode = restore_tweak_mode
        blender_object.animation_data.use_nla = current_use_nla

    if blender_object and blender_object.type == "MESH" \
            and blender_object.data is not None \
            and blender_object.data.shape_keys is not None \
            and blender_object.data.shape_keys.animation_data is not None:
        reset_sk_data(blender_object, blender_actions, export_settings)
        blender_object.data.shape_keys.animation_data.action = current_sk_action

    if blender_object and current_world_matrix is not None:
        blender_object.matrix_world = current_world_matrix

    if export_settings['gltf_optimize_armature_disable_viewport'] \
            and export_settings['vtree'].nodes[obj_uuid].blender_object.type == "ARMATURE":
        if need_to_enable_again is True:
            # And now, restoring meshes in viewport
            for node, obj in [(n, n.blender_object) for n in export_settings['vtree'].nodes.values()
                              if n.blender_type in [VExportNode.OBJECT, VExportNode.ARMATURE, VExportNode.COLLECTION]]:
                obj.hide_viewport = node.default_hide_viewport
            export_settings['vtree'].nodes[obj_uuid].blender_object.hide_viewport = export_settings['vtree'].nodes[obj_uuid].default_hide_viewport

    export_user_extensions('animation_switch_loop_hook', export_settings, blender_object, True)

    return animations, tracks


@cached
def __get_blender_actions(obj_uuid: str,
                          export_settings
                          ) -> typing.List[typing.Tuple[bpy.types.Action, str, str]]:
    blender_actions = []
    blender_tracks = {}
    action_on_type = {}

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object

    export_user_extensions('pre_gather_actions_hook', export_settings, blender_object)

    if export_settings['gltf_animation_mode'] == "BROADCAST":
        return __get_blender_actions_broadcast(obj_uuid, export_settings)

    if blender_object and blender_object.animation_data is not None:
        # Collect active action.
        if blender_object.animation_data.action is not None:

            # Check the action is not in list of actions to ignore
            if hasattr(bpy.data.scenes[0], "gltf_action_filter") and id(blender_object.animation_data.action) in [
                    id(item.action) for item in bpy.data.scenes[0].gltf_action_filter if item.keep is False]:
                pass  # We ignore this action
            else:
                blender_actions.append(blender_object.animation_data.action)
                blender_tracks[blender_object.animation_data.action.name] = None
                action_on_type[blender_object.animation_data.action.name] = "OBJECT"

        # Collect associated strips from NLA tracks.
        if export_settings['gltf_animation_mode'] == "ACTIONS":
            for track in blender_object.animation_data.nla_tracks:
                # Multi-strip tracks do not export correctly yet (they need to be baked),
                # so skip them for now and only write single-strip tracks.
                non_muted_strips = [strip for strip in track.strips if strip.action is not None and strip.mute is False]
                if track.strips is None or len(non_muted_strips) > 1:
                    # Warning if multiple strips are found, then ignore this track
                    # Ignore without warning if no strip
                    export_settings['log'].warning(
                        "NLA track '{}' has {} strips, but only single-strip tracks are supported in 'actions' mode.".format(
                            track.name, len(
                                track.strips)), popup=True)
                    continue
                for strip in non_muted_strips:

                    # Check the action is not in list of actions to ignore
                    if hasattr(bpy.data.scenes[0], "gltf_action_filter") and id(strip.action) in [
                            id(item.action) for item in bpy.data.scenes[0].gltf_action_filter if item.keep is False]:
                        continue  # We ignore this action

                    blender_actions.append(strip.action)
                    # Always set after possible active action -> None will be overwrite
                    blender_tracks[strip.action.name] = track.name
                    action_on_type[strip.action.name] = "OBJECT"

    # For caching, actions linked to SK must be after actions about TRS
    if export_settings['gltf_morph_anim'] and blender_object and blender_object.type == "MESH" \
            and blender_object.data is not None \
            and blender_object.data.shape_keys is not None \
            and blender_object.data.shape_keys.animation_data is not None:

        if blender_object.data.shape_keys.animation_data.action is not None:

            # Check the action is not in list of actions to ignore
            if hasattr(bpy.data.scenes[0], "gltf_action_filter") and id(blender_object.data.shape_keys.animation_data.action) in [
                    id(item.action) for item in bpy.data.scenes[0].gltf_action_filter if item.keep is False]:
                pass  # We ignore this action
            else:
                blender_actions.append(blender_object.data.shape_keys.animation_data.action)
                blender_tracks[blender_object.data.shape_keys.animation_data.action.name] = None
                action_on_type[blender_object.data.shape_keys.animation_data.action.name] = "SHAPEKEY"

        if export_settings['gltf_animation_mode'] == "ACTIONS":
            for track in blender_object.data.shape_keys.animation_data.nla_tracks:
                # Multi-strip tracks do not export correctly yet (they need to be baked),
                # so skip them for now and only write single-strip tracks.
                non_muted_strips = [strip for strip in track.strips if strip.action is not None and strip.mute is False]
                if track.strips is None or len(non_muted_strips) != 1:
                    continue
                for strip in non_muted_strips:
                    # Check the action is not in list of actions to ignore
                    if hasattr(bpy.data.scenes[0], "gltf_action_filter") and id(strip.action) in [
                            id(item.action) for item in bpy.data.scenes[0].gltf_action_filter if item.keep is False]:
                        continue  # We ignore this action

                    blender_actions.append(strip.action)
                    # Always set after possible active action -> None will be overwrite
                    blender_tracks[strip.action.name] = track.name
                    action_on_type[strip.action.name] = "SHAPEKEY"

    # If there are only 1 armature, include all animations, even if not in NLA
    # But only if armature has already some animation_data
    # If not, we says that this armature is never animated, so don't add these additional actions
    if export_settings['gltf_export_anim_single_armature'] is True:
        if blender_object and blender_object.type == "ARMATURE" and blender_object.animation_data is not None:
            if len(export_settings['vtree'].get_all_node_of_type(VExportNode.ARMATURE)) == 1:
                # Keep all actions on objects (no Shapekey animation)
                for act in [a for a in bpy.data.actions if a.id_root == "OBJECT"]:
                    # We need to check this is an armature action
                    # Checking that at least 1 bone is animated
                    if not __is_armature_action(act):
                        continue
                    # Check if this action is already taken into account
                    if act.name in blender_tracks.keys():
                        continue

                    # Check the action is not in list of actions to ignore
                    if hasattr(bpy.data.scenes[0], "gltf_action_filter") and id(act) in [id(item.action)
                                                                                         for item in bpy.data.scenes[0].gltf_action_filter if item.keep is False]:
                        continue  # We ignore this action

                    blender_actions.append(act)
                    blender_tracks[act.name] = None
                    action_on_type[act.name] = "OBJECT"

    # Use a class to get parameters, to be able to modify them
    class GatherActionHookParameters:
        def __init__(self, blender_actions, blender_tracks, action_on_type):
            self.blender_actions = blender_actions
            self.blender_tracks = blender_tracks
            self.action_on_type = action_on_type

    gatheractionhookparams = GatherActionHookParameters(blender_actions, blender_tracks, action_on_type)

    export_user_extensions('gather_actions_hook', export_settings, blender_object, gatheractionhookparams)

    # Get params back from hooks
    blender_actions = gatheractionhookparams.blender_actions
    blender_tracks = gatheractionhookparams.blender_tracks
    action_on_type = gatheractionhookparams.action_on_type

    # Remove duplicate actions.
    blender_actions = list(set(blender_actions))
    # sort animations alphabetically (case insensitive) so they have a defined order and match Blender's Action list
    blender_actions.sort(key=lambda a: a.name.lower())

    return [(blender_action, blender_tracks[blender_action.name], action_on_type[blender_action.name])
            for blender_action in blender_actions]


def __is_armature_action(blender_action) -> bool:
    for fcurve in blender_action.fcurves:
        if is_bone_anim_channel(fcurve.data_path):
            return True
    return False


def __gather_extras(blender_action, export_settings):
    if export_settings['gltf_extras']:
        return generate_extras(blender_action)
    return None


def __get_blender_actions_broadcast(obj_uuid, export_settings):
    blender_actions = []
    blender_tracks = {}
    action_on_type = {}

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object

    # Note : Like in FBX exporter:
    # - Object with animation data will get all actions
    # - Object without animation will not get any action

    # Collect all actions
    for blender_action in bpy.data.actions:
        if hasattr(bpy.data.scenes[0], "gltf_action_filter") and id(blender_action) in [
                id(item.action) for item in bpy.data.scenes[0].gltf_action_filter if item.keep is False]:
            continue  # We ignore this action

        # Keep all actions on objects (no Shapekey animation, No armature animation (on bones))
        if blender_action.id_root == "OBJECT":  # TRS and Bone animations
            if blender_object.animation_data is None:
                continue
            if blender_object and blender_object.type == "ARMATURE" and __is_armature_action(blender_action):
                blender_actions.append(blender_action)
                blender_tracks[blender_action.name] = None
                action_on_type[blender_action.name] = "OBJECT"
            elif blender_object.type == "MESH":
                if not __is_armature_action(blender_action):
                    blender_actions.append(blender_action)
                    blender_tracks[blender_action.name] = None
                    action_on_type[blender_action.name] = "OBJECT"
        elif blender_action.id_root == "KEY":
            if blender_object.type != "MESH" or blender_object.data is None or blender_object.data.shape_keys is None or blender_object.data.shape_keys.animation_data is None:
                continue
            # Checking that the object has some SK and some animation on it
            if blender_object is None:
                continue
            if blender_object.type != "MESH":
                continue
            if blender_object.data is None or blender_object.data.shape_keys is None:
                continue
            blender_actions.append(blender_action)
            blender_tracks[blender_action.name] = None
            action_on_type[blender_action.name] = "SHAPEKEY"

    # Use a class to get parameters, to be able to modify them

    class GatherActionHookParameters:
        def __init__(self, blender_actions, blender_tracks, action_on_type):
            self.blender_actions = blender_actions
            self.blender_tracks = blender_tracks
            self.action_on_type = action_on_type

    gatheractionhookparams = GatherActionHookParameters(blender_actions, blender_tracks, action_on_type)

    export_user_extensions('gather_actions_hook', export_settings, blender_object, gatheractionhookparams)

    # Get params back from hooks
    blender_actions = gatheractionhookparams.blender_actions
    blender_tracks = gatheractionhookparams.blender_tracks
    action_on_type = gatheractionhookparams.action_on_type

    # Remove duplicate actions.
    blender_actions = list(set(blender_actions))
    # sort animations alphabetically (case insensitive) so they have a defined order and match Blender's Action list
    blender_actions.sort(key=lambda a: a.name.lower())

    return [(blender_action, blender_tracks[blender_action.name], action_on_type[blender_action.name])
            for blender_action in blender_actions]
