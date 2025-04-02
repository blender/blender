# SPDX-FileCopyrightText: 2018-2023 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from ....io.com import gltf2_io
from ....io.exp.user_extensions import export_user_extensions
from ..cache import cached
from ..tree import VExportNode
from .anim_utils import merge_tracks_perform, bake_animation, bake_data_animation, add_slide_data, reset_bone_matrix, reset_sk_data
from .drivers import get_sk_drivers
from .sampled.sampling_cache import get_cache_data


class TracksData:
    data_type = "TRACK"

    def __init__(self):
        self.tracks = []

    def add(self, track_data):
        self.tracks.append(track_data)

    def extend(self, tracks_data):
        if tracks_data is None:
            return
        self.tracks.extend(tracks_data.tracks)

    def __len__(self):
        return len(self.tracks)

    def loop_on_type(self, t):
        for tr in [track for track in self.tracks if track.on_type == t]:
            yield tr

    def values(self):
        for tr in self.tracks:
            yield tr


class TrackData:
    def __init__(self, tracks, track_name, on_type):
        self.tracks = tracks
        self.name = track_name
        self.on_type = on_type


def gather_tracks_animations(export_settings):

    animations = []
    merged_tracks = {}

    vtree = export_settings['vtree']
    for obj_uuid in vtree.get_all_objects():

        # Do not manage not exported objects
        if vtree.nodes[obj_uuid].node is None:
            if export_settings['gltf_armature_object_remove'] is True:
                # Manage armature object, as this is the object that has the animation
                if not vtree.nodes[obj_uuid].blender_object:
                    continue
            else:
                continue

        if export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.COLLECTION:
            continue

        animations_, merged_tracks = gather_track_animations(obj_uuid, merged_tracks, len(animations), export_settings)
        animations += animations_

    if export_settings['gltf_export_anim_pointer'] is True:
        # Manage Material tracks (for KHR_animation_pointer)
        for mat in export_settings['KHR_animation_pointer']['materials'].keys():
            animations_, merged_tracks = gather_data_track_animations(
                'materials', mat, merged_tracks, len(animations), export_settings)
            animations += animations_

        # Manage Cameras tracks (for KHR_animation_pointer)
        for cam in export_settings['KHR_animation_pointer']['cameras'].keys():
            animations_, merged_tracks = gather_data_track_animations(
                'cameras', cam, merged_tracks, len(animations), export_settings)
            animations += animations_

        # Manage lights tracks (for KHR_animation_pointer)
        for light in export_settings['KHR_animation_pointer']['lights'].keys():
            animations_, merged_tracks = gather_data_track_animations(
                'lights', light, merged_tracks, len(animations), export_settings)
            animations += animations_

    new_animations = merge_tracks_perform(merged_tracks, animations, export_settings)

    return new_animations


def gather_track_animations(obj_uuid: int,
                            tracks: typing.Dict[str,
                                                typing.List[int]],
                            offset: int,
                            export_settings) -> typing.Tuple[typing.List[gltf2_io.Animation],
                                                             typing.Dict[str,
                                                                         typing.List[int]]]:

    animations = []

    # Bake situation does not export any extra animation channels, as we bake TRS + weights on Track or scene level, without direct
    # Access to fcurve and action data

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object

    # Before exporting, make sure the nla is not in edit mode
    if bpy.context.scene.is_nla_tweakmode is True and blender_object.animation_data:
        blender_object.animation_data.use_tweak_mode = False

    # Collect all tracks affecting this object.
    blender_tracks = __get_blender_tracks(obj_uuid, export_settings)

    # If no tracks, return
    # This will avoid to set / reset some data
    if len(blender_tracks) == 0:
        return animations, tracks

    # Keep current situation
    current_action = None
    current_action_slot = None
    current_sk_action = None
    current_sk_action_slot = None
    current_world_matrix = None
    current_use_nla = None
    current_use_nla_sk = None
    restore_track_mute = {}
    restore_track_mute["OBJECT"] = {}
    restore_track_mute["KEY"] = {}

    if blender_object.animation_data:
        current_action = blender_object.animation_data.action
        current_action_slot = blender_object.animation_data.action_slot
        current_use_nla = blender_object.animation_data.use_nla
        restore_tweak_mode = blender_object.animation_data.use_tweak_mode
    current_world_matrix = blender_object.matrix_world.copy()

    if blender_object.type == "MESH" \
            and blender_object.data is not None \
            and blender_object.data.shape_keys is not None \
            and blender_object.data.shape_keys.animation_data is not None:
        current_sk_action = blender_object.data.shape_keys.animation_data.action
        current_sk_action_slot = blender_object.data.shape_keys.animation_data.action_slot
        current_use_nla_sk = blender_object.data.shape_keys.animation_data.use_nla

    # Prepare export for obj
    solo_track = None
    if blender_object.animation_data:
        if blender_object.animation_data.action is not None:
            blender_object.animation_data.action_slot = None
        blender_object.animation_data.action = None
        blender_object.animation_data.use_nla = True
    # Remove any solo (starred) NLA track. Restored after export
        for track in blender_object.animation_data.nla_tracks:
            if track.is_solo:
                solo_track = track
                track.is_solo = False
                break

    solo_track_sk = None
    if blender_object.type == "MESH" \
            and blender_object.data is not None \
            and blender_object.data.shape_keys is not None \
            and blender_object.data.shape_keys.animation_data is not None:
        # Remove any solo (starred) NLA track. Restored after export
        for track in blender_object.data.shape_keys.animation_data.nla_tracks:
            if track.is_solo:
                solo_track_sk = track
                track.is_solo = False
                break

    # Mute all channels
    for track_group in blender_tracks.loop_on_type("OBJECT"):
        for track in track_group.tracks:
            restore_track_mute["OBJECT"][track.idx] = blender_object.animation_data.nla_tracks[track.idx].mute
            blender_object.animation_data.nla_tracks[track.idx].mute = True
    for track_group in blender_tracks.loop_on_type("KEY"):
        for track in track_group.tracks:
            restore_track_mute["KEY"][track.idx] = blender_object.data.shape_keys.animation_data.nla_tracks[track.idx].mute
            blender_object.data.shape_keys.animation_data.nla_tracks[track.idx].mute = True

    export_user_extensions('animation_track_switch_loop_hook', export_settings, blender_object, False)

    # Export

    # Export all collected tracks.
    for track_data in blender_tracks.values():
        prepare_tracks_range(obj_uuid, track_data, export_settings)

        if track_data.on_type == "OBJECT":
            # Enable tracks
            for track in track_data.tracks:
                export_user_extensions(
                    'pre_animation_track_switch_hook',
                    export_settings,
                    blender_object,
                    track,
                    track_data.name,
                    track_data.on_type)
                blender_object.animation_data.nla_tracks[track.idx].mute = False
                export_user_extensions(
                    'post_animation_track_switch_hook',
                    export_settings,
                    blender_object,
                    track,
                    track_data.name,
                    track_data.on_type)
        else:
            # Enable tracks
            for track in track_data.tracks:
                export_user_extensions(
                    'pre_animation_track_switch_hook',
                    export_settings,
                    blender_object,
                    track,
                    track_data.name,
                    track_data.on_type)
                blender_object.data.shape_keys.animation_data.nla_tracks[track.idx].mute = False
                export_user_extensions(
                    'post_animation_track_switch_hook',
                    export_settings,
                    blender_object,
                    track,
                    track_data.name,
                    track_data.on_type)

        reset_bone_matrix(blender_object, export_settings)
        if track_data.on_type == "KEY":
            reset_sk_data(blender_object, blender_tracks, export_settings)

        # Export animation
        animation = bake_animation(obj_uuid, track_data.name, export_settings, mode=track_data.on_type)
        get_cache_data.reset_cache()
        if animation is not None:
            animations.append(animation)

            # Store data for merging animation later
            # Do not take into account default NLA track names
            if export_settings['gltf_merge_animation'] == "NLA_TRACK":
                if not (track_data.name.startswith("NlaTrack") or track_data.name.startswith("[Action Stash]")):
                    if track_data.name not in tracks.keys():
                        tracks[track_data.name] = []
                    tracks[track_data.name].append(offset + len(animations) - 1)  # Store index of animation in animations
            elif export_settings['gltf_merge_animation'] == "ACTION":
                if blender_action.name not in tracks.keys():
                    tracks[blender_action.name] = []
                tracks[blender_action.name].append(offset + len(animations) - 1)  # Store index of animation in animations
            elif export_settings['gltf_merge_animation'] == "NONE":
                pass  # Nothing to store, we are not going to merge animations
            else:
                pass  # This should not happen (or the developer added a new option, and forget to take it into account here)

        # Restoring muting
        if track_data.on_type == "OBJECT":
            for track in track_data.tracks:
                blender_object.animation_data.nla_tracks[track.idx].mute = True
        else:
            for track in track_data.tracks:
                blender_object.data.shape_keys.animation_data.nla_tracks[track.idx].mute = True

    # Restoring
    if current_action is not None:
        blender_object.animation_data.action = current_action
        if current_action is not None:
            blender_object.animation_data.action_slot = current_action_slot
    if current_sk_action is not None:
        blender_object.data.shape_keys.animation_data.action = current_sk_action
        if current_sk_action is not None:
            blender_object.data.shape_keys.animation_data.action_slot = current_sk_action_slot
    if solo_track is not None:
        solo_track.is_solo = True
    if solo_track_sk is not None:
        solo_track_sk.is_solo = True
    if blender_object.animation_data:
        blender_object.animation_data.use_nla = current_use_nla
        blender_object.animation_data.use_tweak_mode = restore_tweak_mode
        for track_group in blender_tracks.loop_on_type("OBJECT"):
            for track in track_group.tracks:
                blender_object.animation_data.nla_tracks[track.idx].mute = restore_track_mute["OBJECT"][track.idx]
    if blender_object.type == "MESH" \
            and blender_object.data is not None \
            and blender_object.data.shape_keys is not None \
            and blender_object.data.shape_keys.animation_data is not None:
        blender_object.data.shape_keys.animation_data.use_nla = current_use_nla_sk
        for track_group in blender_tracks.loop_on_type("KEY"):
            for track in track_group.tracks:
                blender_object.data.shape_keys.animation_data.nla_tracks[track.idx].mute = restore_track_mute["KEY"][track.idx]

    blender_object.matrix_world = current_world_matrix

    export_user_extensions('animation_track_switch_loop_hook', export_settings, blender_object, True)

    return animations, tracks


@cached
def __get_blender_tracks(obj_uuid: str, export_settings):

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object
    export_user_extensions('pre_gather_tracks_hook', export_settings, blender_object)

    tracks_data = __get_nla_tracks_obj(obj_uuid, export_settings)
    tracks_data_sk = __get_nla_tracks_sk(obj_uuid, export_settings)

    tracks_data.extend(tracks_data_sk)

    export_user_extensions('gather_tracks_hook', export_settings, blender_object, tracks_data)

    return tracks_data


class NLATrack:
    def __init__(self, idx, frame_start, frame_end, default_solo, default_muted):
        self.idx = idx
        self.frame_start = frame_start
        self.frame_end = frame_end
        self.default_solo = default_solo
        self.default_muted = default_muted


def __get_nla_tracks_obj(obj_uuid: str, export_settings):

    obj = export_settings['vtree'].nodes[obj_uuid].blender_object

    if not obj.animation_data:
        return TracksData()
    if len(obj.animation_data.nla_tracks) == 0:
        return TracksData()

    current_exported_tracks = []

    tracks_data = TracksData()

    for idx_track, track in enumerate(obj.animation_data.nla_tracks):
        if len(track.strips) == 0:
            continue

        stored_track = NLATrack(
            idx_track,
            track.strips[0].frame_start,
            track.strips[-1].frame_end,
            track.is_solo,
            track.mute
        )

        # Keep tracks where some blending together
        if any([strip.blend_type != 'REPLACE' for strip in track.strips]):
            # There is some blending. Keeping with previous track
            pass
        else:
            # The previous one(s) can go to the list, if any (not for first track)
            if len(current_exported_tracks) != 0:

                # Store data
                track_data = TrackData(
                    current_exported_tracks,
                    obj.animation_data.nla_tracks[current_exported_tracks[0].idx].name,
                    "OBJECT"
                )
                current_exported_tracks = []

                tracks_data.add(track_data)

        # Start a new stack
        current_exported_tracks.append(stored_track)

    # End of loop. Keep the last one(s), if any
    if len(current_exported_tracks) != 0:

        # Store data for the last one
        track_data = TrackData(
            current_exported_tracks,
            obj.animation_data.nla_tracks[current_exported_tracks[0].idx].name,
            "OBJECT"
        )
        tracks_data.add(track_data)

    return tracks_data


def __get_nla_tracks_sk(obj_uuid: str, export_settings):

    obj = export_settings['vtree'].nodes[obj_uuid].blender_object

    if not obj.type == "MESH":
        return TracksData()
    if obj.data is None:
        return TracksData()
    if obj.data.shape_keys is None:
        return TracksData()
    if not obj.data.shape_keys.animation_data:
        return TracksData()
    if len(obj.data.shape_keys.animation_data.nla_tracks) == 0:
        return TracksData()

    exported_tracks = []

    current_exported_tracks = []

    tracks_data = TracksData()

    for idx_track, track in enumerate(obj.data.shape_keys.animation_data.nla_tracks):
        if len(track.strips) == 0:
            continue

        stored_track = NLATrack(
            idx_track,
            track.strips[0].frame_start,
            track.strips[-1].frame_end,
            track.is_solo,
            track.mute
        )

        # Keep tracks where some blending together
        if any([strip.blend_type != 'REPLACE' for strip in track.strips]):
            # There is some blending. Keeping with previous track
            pass
        else:
            # The previous one(s) can go to the list, if any (not for first track)
            if len(current_exported_tracks) != 0:
                exported_tracks.append(current_exported_tracks)
                current_exported_tracks = []

                # Store data
                track_data = TrackData(
                    current_exported_tracks,
                    obj.data.shape_keys.animation_data.nla_tracks[exported_tracks[-1][0].idx].name,
                    "KEY"
                )

                tracks_data.add(track_data)

        # Start a new stack
        current_exported_tracks.append(stored_track)

    # End of loop. Keep the last one(s)
    exported_tracks.append(current_exported_tracks)
    # Store data for the last one
    track_data = TrackData(
        current_exported_tracks,
        obj.data.shape_keys.animation_data.nla_tracks[exported_tracks[-1][0].idx].name,
        "KEY"
    )

    tracks_data.add(track_data)

    return tracks_data


def prepare_tracks_range(obj_uuid, track_data, export_settings, with_driver=True):

    tracks = track_data.tracks
    track_name = track_data.name

    track_slide = {}

    for idx, btrack in enumerate(tracks):
        frame_start = btrack.frame_start if idx == 0 else min(frame_start, btrack.frame_start)
        frame_end = btrack.frame_end if idx == 0 else max(frame_end, btrack.frame_end)

    # If some negative frame and crop -> set start at 0
    if frame_start < 0 and export_settings['gltf_negative_frames'] == "CROP":
        frame_start = 0

    if export_settings['gltf_frame_range'] is True:
        frame_start = max(bpy.context.scene.frame_start, frame_start)
        frame_end = min(bpy.context.scene.frame_end, frame_end)

    export_settings['ranges'][obj_uuid] = {}
    export_settings['ranges'][obj_uuid][track_name] = {}
    export_settings['ranges'][obj_uuid][track_name]['start'] = int(frame_start)
    export_settings['ranges'][obj_uuid][track_name]['end'] = int(frame_end)

    if export_settings['gltf_negative_frames'] == "SLIDE":
        if not (track_name.startswith("NlaTrack") or track_name.startswith("[Action Stash]")):
            if track_name not in track_slide.keys() or (
                    track_name in track_slide.keys() and frame_start < track_slide[track_name]):
                track_slide.update({track_name: frame_start})
        else:
            if frame_start < 0:
                add_slide_data(frame_start, obj_uuid, track_name, export_settings)

    if export_settings['gltf_anim_slide_to_zero'] is True and frame_start > 0:
        if not (track_name.startswith("NlaTrack") or track_name.startswith("[Action Stash]")):
            if track_name not in track_slide.keys() or (
                    track_name in track_slide.keys() and frame_start < track_slide[track_name]):
                track_slide.update({track_name: frame_start})
        else:
            add_slide_data(frame_start, obj_uuid, track_name, export_settings)

    # For drivers
    if with_driver is True:
        if export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.ARMATURE and export_settings['gltf_morph_anim'] is True:
            obj_drivers = get_sk_drivers(obj_uuid, export_settings)
            for obj_dr in obj_drivers:
                if obj_dr not in export_settings['ranges']:
                    export_settings['ranges'][obj_dr] = {}
                export_settings['ranges'][obj_dr][obj_uuid + "_" + track_name] = {}
                export_settings['ranges'][obj_dr][obj_uuid + "_" + track_name]['start'] = frame_start
                export_settings['ranges'][obj_dr][obj_uuid + "_" + track_name]['end'] = frame_end

    if (export_settings['gltf_negative_frames'] == "SLIDE"
            or export_settings['gltf_anim_slide_to_zero'] is True) \
            and len(track_slide) > 0:

        if track_name in track_slide.keys():
            if export_settings['gltf_negative_frames'] == "SLIDE" and track_slide[track_name] < 0:
                add_slide_data(track_slide[track_name], obj_uuid, track_name, export_settings)
            elif export_settings['gltf_anim_slide_to_zero'] is True:
                add_slide_data(track_slide[track_name], obj_uuid, track_name, export_settings)


def gather_data_track_animations(
        blender_type_data: str,
        blender_id: str,
        tracks: typing.Dict[str, typing.List[int]],
        offset: int,
        export_settings) -> typing.Tuple[typing.List[gltf2_io.Animation], typing.Dict[str, typing.List[int]]]:

    animations = []

    # Collect all tracks affecting this object.
    blender_tracks = __get_data_blender_tracks(blender_type_data, blender_id, export_settings)

    if blender_type_data == "materials":
        blender_data_object = [mat for mat in bpy.data.materials if id(mat) == blender_id][0]
    elif blender_type_data == "cameras":
        blender_data_object = [cam for cam in bpy.data.cameras if id(cam) == blender_id][0]
    elif blender_type_data == "lights":
        blender_data_object = [light for light in bpy.data.lights if id(light) == blender_id][0]
    else:
        pass  # Should not happen

    # Keep current situation
    current_action = None
    current_action_slot = None
    current_nodetree_action = None
    current_nodetree_action_slot = None
    current_use_nla = None
    current_use_nla_node_tree = None
    restore_track_mute = {}
    restore_track_mute["MATERIAL"] = {}
    restore_track_mute["NODETREE"] = {}
    restore_track_mute["LIGHT"] = {}
    restore_track_mute["CAMERA"] = {}

    if blender_data_object.animation_data:
        current_action = blender_data_object.animation_data.action
        current_action_slot = blender_data_object.animation_data.action_slot
        current_use_nla = blender_data_object.animation_data.use_nla
        restore_tweak_mode = blender_data_object.animation_data.use_tweak_mode

    if blender_type_data in ["materials", "lights"] \
            and blender_data_object.node_tree is not None \
            and blender_data_object.node_tree.animation_data is not None:
        current_nodetree_action = blender_data_object.node_tree.animation_data.action
        current_nodetree_action_slot = blender_data_object.node_tree.animation_data.action_slot
        current_use_nla_node_tree = blender_data_object.node_tree.animation_data.use_nla

    # Prepare export for obj
    solo_track = None
    if blender_data_object.animation_data:
        if blender_data_object.animation_data.action is not None:
            blender_data_object.animation_data.action_slot = None
        blender_data_object.animation_data.action = None
        blender_data_object.animation_data.use_nla = True
    # Remove any solo (starred) NLA track. Restored after export
        for track in blender_data_object.animation_data.nla_tracks:
            if track.is_solo:
                solo_track = track
                track.is_solo = False
                break

    solo_track_sk = None
    if blender_type_data == ["materials", "lights"] \
            and blender_data_object.node_tree is not None \
            and blender_data_object.node_tree.animation_data is not None:
        # Remove any solo (starred) NLA track. Restored after export
        for track in blender_data_object.node_tree.animation_data.nla_tracks:
            if track.is_solo:
                solo_track_sk = track
                track.is_solo = False
                break

    # Mute all channels
    if blender_type_data == "materials":
        for track_group in blender_tracks.loop_on_type("MATERIAL"):
            for track in track_group.tracks:
                restore_track_mute["MATERIAL"][track.idx] = blender_data_object.animation_data.nla_tracks[track.idx].mute
                blender_data_object.animation_data.nla_tracks[track.idx].mute = True
        for track_group in blender_tracks.loop_on_type("NODETREE"):
            for track in track_group.tracks:
                restore_track_mute["NODETREE"][track.idx] = blender_data_object.node_tree.animation_data.nla_tracks[track.idx].mute
                blender_data_object.node_tree.animation_data.nla_tracks[track.idx].mute = True
    elif blender_type_data == "cameras":
        for track_group in blender_tracks.loop_on_type("CAMERA"):
            for track in track_group.tracks:
                restore_track_mute["CAMERA"][track.idx] = blender_data_object.animation_data.nla_tracks[track.idx].mute
                blender_data_object.animation_data.nla_tracks[track.idx].mute = True
    elif blender_type_data == "lights":
        for track_group in blender_tracks.loop_on_type("LIGHT"):
            for track in track_group.tracks:
                restore_track_mute["LIGHT"][track.idx] = blender_data_object.animation_data.nla_tracks[track.idx].mute
                blender_data_object.animation_data.nla_tracks[track.idx].mute = True
        for track_group in blender_tracks.loop_on_type("NODETREE"):
            for track in track_group.tracks:
                restore_track_mute["NODETREE"][track.idx] = blender_data_object.node_tree.animation_data.nla_tracks[track.idx].mute
                blender_data_object.node_tree.animation_data.nla_tracks[track.idx].mute = True

    # Export

    # Export all collected tracks.
    for track_data in blender_tracks.values():
        prepare_tracks_range(blender_id, track_data, export_settings, with_driver=False)

        if track_data.on_type in ["MATERIAL", "CAMERA", "LIGHT"]:
            # Enable tracks
            for track in track_data.tracks:
                blender_data_object.animation_data.nla_tracks[track.idx].mute = False
        elif track_data.on_type == "NODETREE":
            # Enable tracks
            for track in track_data.tracks:
                blender_data_object.node_tree.animation_data.nla_tracks[track.idx].mute = False

        # Export animation
        animation = bake_data_animation(blender_type_data, blender_id, track_data.name, None, track_data.on_type, export_settings)
        get_cache_data.reset_cache()
        if animation is not None:
            animations.append(animation)

            # Store data for merging animation later
            # Do not take into account default NLA track names
            if not (track_data.name.startswith("NlaTrack") or track_data.name.startswith("[Action Stash]")):
                if track_data.name not in tracks.keys():
                    tracks[track_data.name] = []
                tracks[track_data.name].append(offset + len(animations) - 1)  # Store index of animation in animations

        # Restoring muting
        if track_data.on_type in ["MATERIAL", "CAMERA", "LIGHT"]:
            for track in track_data.tracks:
                blender_data_object.animation_data.nla_tracks[track.idx].mute = True
        elif track_data.on_type == "NODETREE":
            for track in track_data.tracks:
                blender_data_object.node_tree.animation_data.nla_tracks[track.idx].mute = True

    # Restoring
    if current_action is not None:
        blender_data_object.animation_data.action = current_action
        if current_action is not None:
            blender_data_object.animation_data.action_slot = current_action_slot
    if current_nodetree_action is not None:
        blender_data_object.node_tree.animation_data.action = current_nodetree_action
        if current_nodetree_action is not None:
            blender_data_object.node_tree.animation_data.action_slot = current_nodetree_action_slot
    if solo_track is not None:
        solo_track.is_solo = True
    if solo_track_sk is not None:
        solo_track_sk.is_solo = True
    if blender_data_object.animation_data:
        blender_data_object.animation_data.use_nla = current_use_nla
        blender_data_object.animation_data.use_tweak_mode = restore_tweak_mode
        if blender_type_data == "materials":
            for track_group in blender_tracks.loop_on_type("MATERIAL"):
                for track in track_group.tracks:
                    blender_data_object.animation_data.nla_tracks[track.idx].mute = restore_track_mute["MATERIAL"][track.idx]
        elif blender_type_data == "cameras":
            for track_group in blender_tracks.loop_on_type("CAMERA"):
                for track in track_group.tracks:
                    blender_data_object.animation_data.nla_tracks[track.idx].mute = restore_track_mute["CAMERA"][track.idx]
        elif blender_type_data == "lights":
            for track_group in blender_tracks.loop_on_type("LIGHT"):
                for track in track_group.tracks:
                    blender_data_object.animation_data.nla_tracks[track.idx].mute = restore_track_mute["LIGHT"][track.idx]
    if blender_type_data in ["materials", "lights"] \
            and blender_data_object.node_tree is not None \
            and blender_data_object.node_tree.animation_data is not None:
        blender_data_object.node_tree.animation_data.use_nla = current_use_nla_node_tree
        for track_group in blender_tracks.loop_on_type("NODETREE"):
            for track in track_group.tracks:
                blender_data_object.node_tree.animation_data.nla_tracks[track.idx].mute = restore_track_mute["NODETREE"][track.idx]

    return animations, tracks


def __get_data_blender_tracks(blender_type_data, blender_id, export_settings):
    tracks_data = __get_nla_tracks_material(blender_type_data, blender_id, export_settings)
    if blender_type_data in ["materials", "lights"]:
        tracks_data_tree = __get_nla_tracks_material_node_tree(
            blender_type_data, blender_id, export_settings)
    else:
        tracks_data_tree = TracksData()

    tracks_data.extend(tracks_data_tree)

    return tracks_data


def __get_nla_tracks_material(blender_type_data, blender_id, export_settings):
    if blender_type_data == "materials":
        blender_data_object = [mat for mat in bpy.data.materials if id(mat) == blender_id][0]
        on_type = "MATERIAL"
    elif blender_type_data == "cameras":
        blender_data_object = [cam for cam in bpy.data.cameras if id(cam) == blender_id][0]
        on_type = "CAMERA"
    elif blender_type_data == "lights":
        blender_data_object = [light for light in bpy.data.lights if id(light) == blender_id][0]
        on_type = "LIGHT"
    else:
        pass  # Should not happen

    if not blender_data_object.animation_data:
        return TracksData()
    if len(blender_data_object.animation_data.nla_tracks) == 0:
        return TracksData()

    exported_tracks = []

    current_exported_tracks = []

    tracks_data = TracksData()

    for idx_track, track in enumerate(blender_data_object.animation_data.nla_tracks):
        if len(track.strips) == 0:
            continue

        stored_track = NLATrack(
            idx_track,
            track.strips[0].frame_start,
            track.strips[-1].frame_end,
            track.is_solo,
            track.mute
        )

        # Keep tracks where some blending together
        if any([strip.blend_type != 'REPLACE' for strip in track.strips]):
            # There is some blending. Keeping with previous track
            pass
        else:
            # The previous one(s) can go to the list, if any (not for first track)
            if len(current_exported_tracks) != 0:
                exported_tracks.append(current_exported_tracks)
                current_exported_tracks = []

                # Store data
                track_data = TrackData(
                    current_exported_tracks,
                    blender_data_object.animation_data.nla_tracks[exported_tracks[-1][0].idx].name,
                    on_type
                )

                tracks_data.add(track_data)

        # Start a new stack
        current_exported_tracks.append(stored_track)

    # End of loop. Keep the last one(s)
    exported_tracks.append(current_exported_tracks)

    # Store data for the last one
    track_data = TrackData(
        current_exported_tracks,
        blender_data_object.animation_data.nla_tracks[exported_tracks[-1][0].idx].name,
        on_type
    )

    tracks_data.add(track_data)

    return tracks_data


def __get_nla_tracks_material_node_tree(blender_type_data, blender_id, export_settings):
    on_type = "NODETREE"
    if blender_type_data == "materials":
        blender_object_data = [mat for mat in bpy.data.materials if id(mat) == blender_id][0]
    elif blender_type_data == "lights":
        blender_object_data = [light for light in bpy.data.lights if id(light) == blender_id][0]

    if not blender_object_data.node_tree:
        return TracksData()
    if not blender_object_data.node_tree.animation_data:
        return TracksData()
    if len(blender_object_data.node_tree.animation_data.nla_tracks) == 0:
        return TracksData()

    exported_tracks = []

    current_exported_tracks = []

    tracks_data = TracksData()

    for idx_track, track in enumerate(blender_object_data.node_tree.animation_data.nla_tracks):
        if len(track.strips) == 0:
            continue

        stored_track = NLATrack(
            idx_track,
            track.strips[0].frame_start,
            track.strips[-1].frame_end,
            track.is_solo,
            track.mute
        )

        # Keep tracks where some blending together
        if any([strip.blend_type != 'REPLACE' for strip in track.strips]):
            # There is some blending. Keeping with previous track
            pass
        else:
            # The previous one(s) can go to the list, if any (not for first track)
            if len(current_exported_tracks) != 0:
                exported_tracks.append(current_exported_tracks)
                current_exported_tracks = []

                # Store data
                track_data = TrackData(
                    current_exported_tracks,
                    blender_object_data.node_tree.animation_data.nla_tracks[exported_tracks[-1][0].idx].name,
                    on_type
                )

                tracks_data.add(track_data)

        # Start a new stack
        current_exported_tracks.append(stored_track)

    # End of loop. Keep the last one(s)
    exported_tracks.append(current_exported_tracks)

    # Store data for the last one
    track_data = TrackData(
        current_exported_tracks,
        blender_object_data.node_tree.animation_data.nla_tracks[exported_tracks[-1][0].idx].name,
        on_type
    )

    tracks_data.add(track_data)

    return tracks_data
