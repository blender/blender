# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from math import ceil
from ....io.com import gltf2_io
from ....io.exp.user_extensions import export_user_extensions
from ....blender.com.conversion import get_gltf_interpolation
from ...com.data_path import is_bone_anim_channel, get_channelbag_for_slot
from ...com.extras import generate_extras
from ..cache import cached
from ..tree import VExportNode
from .fcurves.animation import gather_animation_fcurves
from .sampled.armature.action_sampled import gather_action_armature_sampled
from .sampled.armature.channels import gather_sampled_bone_channel
from .sampled.object.action_sampled import gather_action_object_sampled
from .sampled.shapekeys.action_sampled import gather_action_sk_sampled
from .sampled.object.channels import gather_object_sampled_channels, gather_sampled_object_channel
from .sampled.shapekeys.channels import gather_sampled_sk_channel
from .drivers import get_sk_drivers, get_driver_on_shapekey
from .anim_utils import reset_bone_matrix, reset_sk_data, link_samplers, add_slide_data, merge_tracks_perform, bake_animation


class ActionsData:
    data_type = "ACTION"

    def __init__(self):
        self.actions = {}

    def add_action(self, action, force_new_action=False):
        if force_new_action:
            action.active = False # If we force a new action, it is not active (but in NLA or broadcasted)
            if id(action.action) not in self.actions.keys():
                self.actions[id(action.action)] = []
            self.actions[id(action.action)].append(action)
            return

        # Trying to add slot to existing action, if any (or create a new one)
        if id(action.action) not in self.actions.keys():
            self.actions[id(action.action)] = []
            self.actions[id(action.action)].append(action)
        else:
            active = self.get_active_action(action.action)
            idx = active if active is not None else -1

            # add to the active action, or the last action
            for slot in action.slots:
                self.actions[id(action.action)][idx].add_slot(slot.slot, slot.target_id_type, slot.track)

    def get(self):
        # sort animations alphabetically (case insensitive) so they have a defined order and match Blender's Action list
        values = list([i for action_data in self.actions.values() for i in action_data])
        values.sort(key=lambda a: a.action.name.lower())
        for action in values:
            action.sort()
        return values

    def exists(self, action, slot):
        if id(action) not in self.actions.keys():
            return False

        for action in self.actions[id(action)]:
            for slot_ in action.slots:
                if slot_.slot.handle == slot.handle:
                    return True

        return False

    def exists_action(self, action):
        return id(action) in self.actions.keys()

    def exists_action_slot_target(self, action, slot):
        if id(action) not in self.actions.keys():
            return False

        for action in self.actions[id(action)]:
            for slot_ in action.slots:
                if slot_.slot.handle == slot.handle:
                    continue
                if slot_.target_id_type == slot.target_id_type:
                    return True

        return False

    def get_active_action(self, action):
        if id(action) not in self.actions.keys():
            return None

        for idx, action in enumerate(self.actions[id(action)]):
            if action.active:
                return idx

        return None

    # Iterate over actions
    def values(self):
        # Create an iterator
        values = self.get()
        for action in values:
            yield action

    def __len__(self):
        return len(self.actions)


class ActionData:
    def __init__(self, action):
        self.action = action
        self.slots = []
        self.name = action.name
        self.active = True

    def add_slot(self, slot, target_id_type, track):
        # If slot already exists with None track (so active action/slot) => Replace it with the track (NLA)
        f = [s for s in self.slots if s.slot.handle == slot.handle and s.track is None]
        if len(f) > 0:
            self.slots.remove(f[0])
        new_slot = SlotData(slot, target_id_type, track)
        self.slots.append(new_slot)

    def sort(self):
        # Implement sorting, to be sure to get:
        # TRS first, and then SK
        sort_items = {'OBJECT': 1, 'KEY': 2}
        self.slots.sort(key=lambda x: sort_items.get(x.target_id_type))

    def has_slots(self):
        return len(self.slots) > 0

    def force_name(self, name):
        self.name = name


class SlotData:
    def __init__(self, slot, target_id_type, track):
        self.slot = slot
        self.target_id_type = target_id_type
        self.track = track


def gather_actions_animations(export_settings):

    prepare_actions_range(export_settings)

    animations = []
    merged_tracks = {}

    vtree = export_settings['vtree']
    for obj_uuid in vtree.get_all_objects():

        # Do not manage real collections (if case of full hierarchy export)
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

        if export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.COLLECTION:
            continue

        animations_, merged_tracks = gather_action_animations(obj_uuid, merged_tracks, len(animations), export_settings)
        animations += animations_

    if export_settings['gltf_animation_mode'] == "ACTIVE_ACTIONS":
        # Fake an animation with all animations of the scene
        merged_tracks = {}
        merged_tracks_name = 'Animation'
        if (len(export_settings['gltf_nla_strips_merged_animation_name']) > 0):
            merged_tracks_name = export_settings['gltf_nla_strips_merged_animation_name']
        merged_tracks[merged_tracks_name] = []
        for idx, animation in enumerate(animations):
            merged_tracks[merged_tracks_name].append(idx)

    if export_settings['gltf_merge_animation'] == "NONE":
        return animations

    new_animations = merge_tracks_perform(merged_tracks, animations, export_settings)

    return new_animations

# We need to align if step is not 1
# For example, cache will get frame 1/4/7/10 if step is 3, with an action starting at frame 1
# If all backing is enabled, and scene start at 0, we will get frame 0/3/6/9 => Cache will fail
# Set the reference frame from the first action retrieve, and align all actions to this frame


def _align_frame_start(reference_frame_start, frame, export_settings):

    if reference_frame_start is None:
        return frame

    if export_settings['gltf_frame_step'] == 1:
        return frame

    return reference_frame_start + export_settings['gltf_frame_step'] * ceil((frame - reference_frame_start) / export_settings['gltf_frame_step'])


def prepare_actions_range(export_settings):

    track_slide = {}

    start_frame_reference = None

    vtree = export_settings['vtree']
    for obj_uuid in vtree.get_all_objects():

        # Do not manage real collections (if case of full hierarchy export)
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

        for action_data in blender_actions.values():
            blender_action = action_data.action
            for slot in action_data.slots:
                type_ = slot.target_id_type
                track = slot.track

                # Frame range is set on action level, not on slot level
                # So, do not manage range data at slot level, but action level
                # (This is not the case for slide, that depends on track, so we have to get slot to get track)

                # What about frame_range bug for single keyframe animations ? 107030
                start_frame = int(blender_action.frame_range[0])
                end_frame = int(blender_action.frame_range[1])

                if end_frame - start_frame == 1:
                    # To workaround Blender bug 107030, check manually
                    try:  # Avoid crash in case of strange/buggy fcurves
                        chanelbag = get_channelbag_for_slot(blender_action, slot.slot)
                        fcurves = chanelbag.fcurves if chanelbag else []
                        start_frame = int(min([c.range()[0] for c in fcurves]))
                        end_frame = int(max([c.range()[1] for c in fcurves]))
                    except:
                        pass

                export_settings['ranges'][obj_uuid][blender_action.name] = {}

                # If some negative frame and crop -> set start at 0
                if start_frame < 0 and export_settings['gltf_negative_frames'] == "CROP":
                    start_frame = 0

                if export_settings['gltf_frame_range'] is True:
                    start_frame = max(bpy.context.scene.frame_start, start_frame)
                    end_frame = min(bpy.context.scene.frame_end, end_frame)

                export_settings['ranges'][obj_uuid][blender_action.name]['start'] = _align_frame_start(start_frame_reference, start_frame, export_settings)
                export_settings['ranges'][obj_uuid][blender_action.name]['end'] = end_frame

                if start_frame_reference is None:
                    start_frame_reference = start_frame

                    # Recheck all actions to align to this frame
                    for obj_uuid_tmp in export_settings['ranges'].keys():
                        for action_name_tmp in export_settings['ranges'][obj_uuid_tmp].keys():
                            export_settings['ranges'][obj_uuid_tmp][action_name_tmp]['start'] = _align_frame_start(start_frame_reference, export_settings['ranges'][obj_uuid_tmp][action_name_tmp]['start'], export_settings)

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

                if type_ == "KEY" and export_settings['gltf_bake_animation']:
                    export_settings['ranges'][obj_uuid][obj_uuid] = {}
                    export_settings['ranges'][obj_uuid][obj_uuid]['start'] = _align_frame_start(start_frame_reference, bpy.context.scene.frame_start, export_settings)
                    export_settings['ranges'][obj_uuid][obj_uuid]['end'] = bpy.context.scene.frame_end

                # For baking drivers
                if export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.ARMATURE and export_settings['gltf_morph_anim'] is True:
                    obj_drivers = get_sk_drivers(obj_uuid, export_settings)
                    for obj_dr in obj_drivers:
                        if obj_dr not in export_settings['ranges']:
                            export_settings['ranges'][obj_dr] = {}
                        export_settings['ranges'][obj_dr][obj_uuid + "_" + blender_action.name] = {}
                        export_settings['ranges'][obj_dr][obj_uuid + "_" + blender_action.name]['start'] = _align_frame_start(start_frame_reference, start_frame, export_settings)
                        export_settings['ranges'][obj_dr][obj_uuid + "_" + blender_action.name]['end'] = end_frame

        if len(blender_actions) == 0 and export_settings['gltf_bake_animation']:
            # No animation on this object
            # In case of baking animation, we will use scene frame range
            # Will be calculated later if max range. Can be set here if scene frame range
            export_settings['ranges'][obj_uuid][obj_uuid] = {}
            export_settings['ranges'][obj_uuid][obj_uuid]['start'] = _align_frame_start(start_frame_reference, bpy.context.scene.frame_start, export_settings)
            export_settings['ranges'][obj_uuid][obj_uuid]['end'] = bpy.context.scene.frame_end

            # For baking drivers
            if export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.ARMATURE and export_settings['gltf_morph_anim'] is True:
                obj_drivers = get_sk_drivers(obj_uuid, export_settings)
                for obj_dr in obj_drivers:
                    if obj_dr not in export_settings['ranges']:
                        export_settings['ranges'][obj_dr] = {}
                    export_settings['ranges'][obj_dr][obj_uuid + "_" + obj_uuid] = {}
                    export_settings['ranges'][obj_dr][obj_uuid + "_" +
                                                      obj_uuid]['start'] = _align_frame_start(start_frame_reference, bpy.context.scene.frame_start, export_settings)
                    export_settings['ranges'][obj_dr][obj_uuid + "_" + obj_uuid]['end'] = bpy.context.scene.frame_end

    if (export_settings['gltf_negative_frames'] == "SLIDE"
            or export_settings['gltf_anim_slide_to_zero'] is True) \
            and len(track_slide) > 0:
        # Need to store animation slides
        for obj_uuid in vtree.get_all_objects():

            # Do not manage real collections (if case of full hierarchy export)
            if vtree.nodes[obj_uuid].blender_type == VExportNode.COLLECTION:
                continue

            # Do not manage not exported objects
            if vtree.nodes[obj_uuid].node is None:
                if export_settings['gltf_armature_object_remove'] is True:
                    # Manage armature object, as this is the object that has the animation
                    if not vtree.nodes[obj_uuid].blender_object:
                        continue
                else:
                    continue

            blender_actions = __get_blender_actions(obj_uuid, export_settings)
            for action_data in blender_actions.values():
                blender_action = action_data.action
                for slot in action_data.slots:
                    track = slot.track
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
        # We can return early if no actions
        return animations, tracks

    # Keep current situation and prepare export
    current_action = None
    current_action_slot = None
    current_sk_action = None
    current_sk_action_slot = None
    current_world_matrix = None
    if blender_object and blender_object.animation_data and blender_object.animation_data.action and blender_object.animation_data.action_slot:
        # There is an active action. Storing it, to be able to restore after switching all actions during export
        current_action = blender_object.animation_data.action
        current_action_slot = blender_object.animation_data.action_slot
    elif len(blender_actions) != 0 and blender_object.animation_data is not None and blender_object.animation_data.action is None:
        # No current action set, storing world matrix of object
        current_world_matrix = blender_object.matrix_world.copy()
    elif len(blender_actions) != 0 and blender_object.animation_data is not None and blender_object.animation_data.action is not None and blender_object.animation_data.action_slot is None:
        # No current action set, storing world matrix of object
        current_world_matrix = blender_object.matrix_world.copy()

    if blender_object and blender_object.type == "MESH" \
            and blender_object.data is not None \
            and blender_object.data.shape_keys is not None \
            and blender_object.data.shape_keys.animation_data is not None \
            and blender_object.data.shape_keys.animation_data.action is not None \
            and blender_object.data.shape_keys.animation_data.action_slot is not None:
        current_sk_action = blender_object.data.shape_keys.animation_data.action
        current_sk_action_slot = blender_object.data.shape_keys.animation_data.action_slot

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

    # Disable all except armature in viewport, for performance
    if export_settings['gltf_optimize_disable_viewport'] \
            and export_settings['vtree'].nodes[obj_uuid].blender_object.type == "ARMATURE":

        # Before baking, disabling from viewport all meshes
        for obj in [n.blender_object for n in export_settings['vtree'].nodes.values() if n.blender_type in
                    [VExportNode.OBJECT, VExportNode.ARMATURE, VExportNode.COLLECTION]]:
            obj.hide_viewport = True
        export_settings['vtree'].nodes[obj_uuid].blender_object.hide_viewport = False

        # We need to create custom properties on armature to store shape keys drivers on disabled meshes
        # This way, we can evaluate drivers on shape keys, and bake them
        drivers = get_sk_drivers(obj_uuid, export_settings)
        if drivers:
            # So ... Let's create some custom properties and the armature
            # First, retrieve the armature object
            for mesh_uuid in drivers:
                _, channels = get_driver_on_shapekey(mesh_uuid, export_settings)
                blender_object["gltf_" + mesh_uuid] = [0.0] * len(channels)
                for idx, channel in enumerate(channels):
                    if channel is None:
                        continue
                    if blender_object.animation_data is None or blender_object.animation_data.drivers is None:
                        # There is no animation on the armature, so no need to create driver
                        # But, we need to copy the current value of the shape key to the custom property
                        blender_object["gltf_" + mesh_uuid][idx] = blender_object.data.shape_keys.key_blocks[channel.data_path.split('"')[
                            1]].value
                    else:
                        dr = blender_object.animation_data.drivers.from_existing(src_driver=channel)
                        dr.data_path = "[\"gltf_" + mesh_uuid + "\"]"
                        dr.array_index = idx

    export_user_extensions('animation_switch_loop_hook', export_settings, blender_object, False)

# Export

    # Export all collected actions.
    for action_data in blender_actions.values():
        all_channels = []
        for slot in action_data.slots:
            blender_action = action_data.action
            track_name = slot.track
            on_type = slot.target_id_type

            # Set action as active, to be able to bake if needed
            if on_type == "OBJECT":  # Not for shapekeys!
                if blender_object.animation_data.action is None \
                        or (blender_object.animation_data.action.name != blender_action.name) \
                        or (blender_object.animation_data.action_slot is None) \
                        or (blender_object.animation_data.action_slot.handle != slot.slot.handle):

                    if blender_object.animation_data.is_property_readonly('action'):
                        blender_object.animation_data.use_tweak_mode = False
                    try:
                        reset_bone_matrix(blender_object, export_settings)
                        export_user_extensions(
                            'pre_animation_switch_hook',
                            export_settings,
                            blender_object,
                            blender_action,
                            slot,
                            track_name,
                            on_type)
                        blender_object.animation_data.action = blender_action
                        blender_object.animation_data.action_slot = slot.slot
                        export_user_extensions(
                            'post_animation_switch_hook',
                            export_settings,
                            blender_object,
                            blender_action,
                            slot,
                            track_name,
                            on_type)
                    except:
                        error = "Action is readonly. Please check NLA editor"
                        export_settings['log'].warning(
                            "Animation '{}' could not be exported. Cause: {}".format(
                                blender_action.name, error))
                        continue
                else:
                    # No need to switch action, but we call the hook anyway, in case of user extension
                    export_user_extensions(
                        'pre_animation_switch_hook',
                        export_settings,
                        blender_object,
                        blender_action,
                        slot,
                        track_name,
                        on_type)
                    export_user_extensions(
                        'post_animation_switch_hook',
                        export_settings,
                        blender_object,
                        blender_action,
                        slot,
                        track_name,
                        on_type)

            if on_type == "KEY":
                if blender_object.data.shape_keys.animation_data.action is None \
                        or (blender_object.data.shape_keys.animation_data.action.name != blender_action.name) \
                        or (blender_object.data.shape_keys.animation_data.action_slot.handle != slot.slot.handle):
                    if blender_object.data.shape_keys.animation_data.is_property_readonly('action'):
                        blender_object.data.shape_keys.animation_data.use_tweak_mode = False
                    reset_sk_data(blender_object, blender_actions, export_settings)
                    export_user_extensions(
                        'pre_animation_switch_hook',
                        export_settings,
                        blender_object,
                        blender_action,
                        slot,
                        track_name,
                        on_type)
                    blender_object.data.shape_keys.animation_data.action = blender_action
                    blender_object.data.shape_keys.animation_data.action_slot = slot.slot
                    export_user_extensions(
                        'post_animation_switch_hook',
                        export_settings,
                        blender_object,
                        blender_action,
                        slot,
                        track_name,
                        on_type)
                else:
                    # No need to switch action, but we call the hook anyway, in case of user extension
                    export_user_extensions(
                        'pre_animation_switch_hook',
                        export_settings,
                        blender_object,
                        blender_action,
                        slot,
                        track_name,
                        on_type)
                    export_user_extensions(
                        'post_animation_switch_hook',
                        export_settings,
                        blender_object,
                        blender_action,
                        slot,
                        track_name,
                        on_type)

            if export_settings['gltf_force_sampling'] is True:
                if export_settings['vtree'].nodes[obj_uuid].blender_object.type == "ARMATURE":
                    channels, extra_samplers = gather_action_armature_sampled(
                        obj_uuid, blender_action, slot.slot.identifier, None, export_settings)
                    if channels:
                        all_channels.extend(channels)
                elif on_type == "OBJECT":
                    channels, extra_samplers = gather_action_object_sampled(
                        obj_uuid, blender_action, slot.slot.identifier, None, export_settings)
                    if channels:
                        all_channels.extend(channels)
                else:
                    channels = gather_action_sk_sampled(obj_uuid, blender_action, slot.slot.identifier, None, export_settings)
                    if channels:
                        all_channels.extend(channels)
            else:
                # Not sampled
                # This returns
                #  - animation on fcurves
                #  - fcurve that cannot be handled not sampled, to be sampled
                # to_be_sampled is : (object_uuid , type , prop, optional(bone.name) )
                channels, to_be_sampled, extra_samplers = gather_animation_fcurves(
                    obj_uuid, blender_action, slot.slot.identifier, export_settings)
                if channels:
                    all_channels.extend(channels)
                for (obj_uuid, type_, prop, bone) in to_be_sampled:
                    if type_ == "BONE":
                        channel = gather_sampled_bone_channel( #TODOSLOT
                            obj_uuid,
                            bone,
                            prop,
                            blender_action.name,
                            slot.slot.identifier,
                            True,
                            get_gltf_interpolation(export_settings['gltf_sampling_interpolation_fallback'], export_settings),
                            export_settings)
                    elif type_ == "OBJECT":
                        channel = gather_sampled_object_channel(
                            obj_uuid, prop, blender_action.name, slot.slot.identifier, True, get_gltf_interpolation(export_settings['gltf_sampling_interpolation_fallback'], export_settings), export_settings)
                    elif type_ == "SK":
                        channel = gather_sampled_sk_channel(obj_uuid, blender_action.name, slot.slot.identifier, export_settings)
                    elif type_ == "EXTRA":  # TODOSLOT slot-3
                        channel = None
                    else:
                        export_settings['log'].error("Type unknown. Should not happen")

                    if channel:
                        all_channels.append(channel)

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
                    all_channels)

            # If we are in a SK animation (without any TRS animation), and we need to bake
            if len([a for a in blender_actions.values() if len([s for s in a.slots if s.target_id_type == "OBJECT"]) != 0]) == 0 and slot.target_id_type == "KEY":
                if export_settings['gltf_bake_animation'] is True and export_settings['gltf_force_sampling'] is True:
                    # We also have to check if this is a skinned mesh, because we don't have to force animation baking on this case
                    # (skinned meshes TRS must be ignored, says glTF specification)
                    if export_settings['vtree'].nodes[obj_uuid].skin is None:
                        if obj_uuid not in export_settings['ranges'].keys():
                            export_settings['ranges'][obj_uuid] = {}
                        export_settings['ranges'][obj_uuid][obj_uuid] = export_settings['ranges'][obj_uuid][blender_action.name]
                        # No TRS animation, so no slot
                        channels, _ = gather_object_sampled_channels(obj_uuid, obj_uuid, None, export_settings)
                        if channels is not None:
                            all_channels.extend(channels)

            if len([a for a in blender_actions.values() if len([s for s in a.slots if s.target_id_type == "KEY"]) != 0]) == 0 \
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
                        channel = gather_sampled_sk_channel(obj_uuid, obj_uuid, None, export_settings)
                        if channel is not None:
                            all_channels.append(channel)

        # We went through all slots of the action, we can now create the animation
        if len(all_channels) != 0:
            animation = gltf2_io.Animation(
                channels=all_channels,
                name=blender_action.name,
                extras=__gather_extras(blender_action, export_settings),
                samplers=[],  # This will be generated later, in link_samplers
                extensions=None
            )

            # Hook for user extensions
            export_user_extensions(
                'animation_action_hook',
                export_settings,
                animation,
                blender_object,
                action_data)

            link_samplers(animation, export_settings)
            animations.append(animation)

            # Store data for merging animation later
            if export_settings['gltf_merge_animation'] == "NLA_TRACK":
                if track_name is not None:  # Do not take into account animation not in NLA
                    # Do not take into account default NLA track names
                    if not (track_name.startswith("NlaTrack") or track_name.startswith("[Action Stash]")):
                        if track_name not in tracks.keys():
                            tracks[track_name] = []
                        tracks[track_name].append(offset + len(animations) - 1)  # Store index of animation in animations
            elif export_settings['gltf_merge_animation'] == "ACTION":
                if action_data.name not in tracks.keys():
                    tracks[action_data.name] = []
                tracks[action_data.name].append(offset + len(animations) - 1)
            elif export_settings['gltf_merge_animation'] == "NONE":
                pass  # Nothing to store, we are not going to merge animations
            else:
                pass  # This should not happen (or the developer added a new option, and forget to take it into account here)


# Restoring current situation

    # Restore action status
    # TODO: do this in a finally
    if blender_object and blender_object.animation_data:
        if blender_object.animation_data.action is not None:
            if current_action is None:
                # remove last exported action
                reset_bone_matrix(blender_object, export_settings)
                if blender_object.animation_data.action is not None:
                    blender_object.animation_data.action_slot = None
                blender_object.animation_data.action = None
            elif blender_object.animation_data.action.name != current_action.name:  # TODO action name is not unique (library)
                # Restore action that was active at start of exporting
                reset_bone_matrix(blender_object, export_settings)
                blender_object.animation_data.action = current_action
                if current_action is not None:
                    blender_object.animation_data.action_slot = current_action_slot
            elif blender_object.animation_data.action_slot.handle != current_action_slot.handle:
                # Restore action that was active at start of exporting
                reset_bone_matrix(blender_object, export_settings)
                blender_object.animation_data.action = current_action
                if current_action is not None:
                    blender_object.animation_data.action_slot = current_action_slot
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
        if current_sk_action is not None:
            blender_object.data.shape_keys.animation_data.action_slot = current_sk_action_slot

    if blender_object and current_world_matrix is not None:
        blender_object.matrix_world = current_world_matrix

    if export_settings['gltf_optimize_disable_viewport'] \
            and export_settings['vtree'].nodes[obj_uuid].blender_object.type == "ARMATURE":
        # And now, restoring meshes in viewport
        for node, obj in [(n, n.blender_object) for n in export_settings['vtree'].nodes.values() if n.blender_type in
                          [VExportNode.OBJECT, VExportNode.ARMATURE, VExportNode.COLLECTION]]:
            obj.hide_viewport = node.default_hide_viewport
        export_settings['vtree'].nodes[obj_uuid].blender_object.hide_viewport = export_settings['vtree'].nodes[obj_uuid].default_hide_viewport
        # Let's remove the custom properties, and first, remove drivers
        drivers = get_sk_drivers(obj_uuid, export_settings)
        if drivers:
            for mesh_uuid in drivers:
                for armature_driver in blender_object.animation_data.drivers:
                    if "gltf_" + mesh_uuid in armature_driver.data_path:
                        blender_object.animation_data.drivers.remove(armature_driver)
                del blender_object["gltf_" + mesh_uuid]

    export_user_extensions('animation_switch_loop_hook', export_settings, blender_object, True)

    return animations, tracks


@cached
def __get_blender_actions(obj_uuid: str,
                          export_settings
                          ) -> ActionsData:

    actions = ActionsData()

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object

    export_user_extensions('pre_gather_actions_hook', export_settings, blender_object)

    if export_settings['gltf_animation_mode'] == "BROADCAST":
        return __get_blender_actions_broadcast(obj_uuid, export_settings)

    if blender_object and blender_object.animation_data is not None:
        # Collect active action.
        if blender_object.animation_data.action is not None and blender_object.animation_data.action_slot is not None:

            # Check the action is not in list of actions to ignore
            if hasattr(bpy.data.scenes[0], "gltf_action_filter") and id(blender_object.animation_data.action) in [
                    id(item.action) for item in bpy.data.scenes[0].gltf_action_filter if item.keep is False]:
                pass  # We ignore this action
            else:
                # Store Action info
                new_action = ActionData(blender_object.animation_data.action)
                new_action.add_slot(blender_object.animation_data.action_slot, blender_object.animation_data.action_slot.target_id_type, None)  # Active action => No track
                actions.add_action(new_action)

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

                    # Store Action info
                    new_action = ActionData(strip.action)
                    new_action.add_slot(strip.action_slot, strip.action_slot.target_id_type, track.name)
                    # If we already have a data for this action (so active or another NLA track on same target_id_type)
                    # We force creation of another animation
                    # Instead of adding a new slot to the existing action
                    if actions.exists_action_slot_target(strip.action, strip.action_slot):
                        new_action.force_name(strip.action.name + "-" + strip.action_slot.name_display)
                        actions.add_action(new_action, force_new_action=True)
                    else:
                        actions.add_action(new_action)

    # For caching, actions linked to SK must be after actions about TRS
    if export_settings['gltf_morph_anim'] and blender_object and blender_object.type == "MESH" \
            and blender_object.data is not None \
            and blender_object.data.shape_keys is not None \
            and blender_object.data.shape_keys.animation_data is not None:

        if blender_object.data.shape_keys.animation_data.action is not None and blender_object.data.shape_keys.animation_data.action_slot is not None:

            # Check the action is not in list of actions to ignore
            if hasattr(bpy.data.scenes[0], "gltf_action_filter") and id(blender_object.data.shape_keys.animation_data.action) in [
                    id(item.action) for item in bpy.data.scenes[0].gltf_action_filter if item.keep is False]:
                pass  # We ignore this action
            else:
                # Store Action info
                new_action = ActionData(blender_object.data.shape_keys.animation_data.action)
                new_action.add_slot(blender_object.data.shape_keys.animation_data.action_slot, blender_object.data.shape_keys.animation_data.action_slot.target_id_type, None)
                actions.add_action(new_action)

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

                    # Store Action info
                    new_action = ActionData(strip.action)
                    new_action.add_slot(strip.action_slot, strip.action_slot.target_id_type, track.name)
                    # If we already have a data for this action (so active or another NLA track on same target_id_type)
                    # We force creation of another animation
                    # Instead of adding a new slot to the existing action
                    if actions.exists_action_slot_target(strip.action, strip.action_slot):
                        new_action.force_name(strip.action.name + "-" + strip.action_slot.name_display)
                        actions.add_action(new_action, force_new_action=True)
                    else:
                        actions.add_action(new_action)

    # If there are only 1 armature, include all animations, even if not in NLA
    # But only if armature has already some animation_data
    # If not, we says that this armature is never animated, so don't add these additional actions
    if export_settings['gltf_export_anim_single_armature'] is True:
        if blender_object and blender_object.type == "ARMATURE" and blender_object.animation_data is not None:
            if len(export_settings['vtree'].get_all_node_of_type(VExportNode.ARMATURE)) == 1:
                # Keep all actions on objects (no Shapekey animation)
                for act in bpy.data.actions:
                    already_added_action = False

                    # For the assigned action, we already have the slot
                    if act == blender_object.animation_data.action:
                        continue

                    # If we already have this action, we skip it
                    # (We got it from NLA)
                    if actions.exists_action(act):
                        continue

                    for slot in [s for s in act.slots if s.target_id_type == "OBJECT"]:
                        # We need to check this is an armature action
                        # Checking that at least 1 bone is animated
                        if not __is_armature_slot(act, slot):
                            continue
                        # Check if this action is already taken into account
                        if actions.exists(act, slot):
                            continue

                        # Check the action is not in list of actions to ignore
                        if hasattr(bpy.data.scenes[0], "gltf_action_filter") and id(act) in [id(item.action)
                                                                                             for item in bpy.data.scenes[0].gltf_action_filter if item.keep is False]:
                            continue  # We ignore this action

                        if already_added_action is True:
                            # We already added an action for this object
                            # So forcing creation of another animation
                            # Instead of adding a new slot to the existing action
                            new_action = ActionData(act)
                            new_action.add_slot(slot, slot.target_id_type, None)
                            # Force new name to avoid merging later
                            new_action.force_name(act.name + "-" + slot.name_display)
                            actions.add_action(new_action, force_new_action=True)
                            continue

                        new_action = ActionData(act)
                        new_action.add_slot(slot, slot.target_id_type, None)
                        actions.add_action(new_action)
                        already_added_action = True

    export_user_extensions('gather_actions_hook', export_settings, blender_object, actions)

    # Duplicate actions/slot are already managed when inserting data
    # If an active action/slot is already in the list, this is because is active + in NLA
    # We keep only one of them (the NLA one)
    # sort animations alphabetically (case insensitive) so they have a defined order and match Blender's Action list
    # TODOSLOT slot-1-B hook
    # blender_actions.sort(key=lambda a: a.name.lower())

    return actions


def __is_armature_slot(blender_action, slot) -> bool:
    channelbag = get_channelbag_for_slot(blender_action, slot)
    if channelbag:
        for fcurve in channelbag.fcurves:
            if is_bone_anim_channel(fcurve.data_path):
                return True
    return False


def __gather_extras(blender_action, export_settings):
    if export_settings['gltf_extras']:
        return generate_extras(blender_action)
    return None


def __get_blender_actions_broadcast(obj_uuid, export_settings):

    blender_actions = ActionsData()

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object

    # Note : Like in FBX exporter:
    # - Object with animation data will get all actions
    # - Object without animation will not get any action

    # Collect all actions and corresponding slots
    for blender_action in bpy.data.actions:
        if hasattr(bpy.data.scenes[0], "gltf_action_filter") and id(blender_action) in [
                id(item.action) for item in bpy.data.scenes[0].gltf_action_filter if item.keep is False]:
            continue  # We ignore this action

        new_action = ActionData(blender_action)

        already_added_object_slot = False
        already_added_sk_slot = False
        for slot in blender_action.slots:

            if slot.target_id_type == "OBJECT":

                # Do not export actions on objects without animation data
                if blender_object.animation_data is None:
                    continue

                if blender_object and blender_object.type == "ARMATURE" and __is_armature_slot(blender_action, slot):

                    if already_added_object_slot is True:
                        # We already added an action for this object
                        # So forcing creation of another animation
                        # Instead of adding a new slot to the existing action
                        new_action_forced = ActionData(blender_action)
                        new_action_forced.add_slot(slot, slot.target_id_type, None)
                        # Force new name to avoid merging later
                        new_action_forced.force_name(blender_action.name + "-" + slot.name_display)
                        blender_actions.add_action(new_action_forced, force_new_action=True)
                        continue

                    new_action.add_slot(slot, slot.target_id_type, None)
                    blender_actions.add_action(new_action)
                    already_added_object_slot = True
                elif blender_object and blender_object.type == "MESH" and not __is_armature_slot(blender_action, slot):

                    if already_added_object_slot is True:
                        # We already added an action for this object
                        # So forcing creation of another animation
                        # Instead of adding a new slot to the existing action
                        new_action_forced = ActionData(blender_action)
                        new_action_forced.add_slot(slot, slot.target_id_type, None)
                        # Force new name to avoid merging later
                        new_action_forced.force_name(blender_action.name + "-" + slot.name_display)
                        blender_actions.add_action(new_action_forced, force_new_action=True)
                        continue

                    new_action.add_slot(slot, slot.target_id_type, None)
                    blender_actions.add_action(new_action)
                    already_added_object_slot = True

            elif slot.target_id_type == "KEY":
                if blender_object.type != "MESH" or blender_object.data is None or blender_object.data.shape_keys is None or blender_object.data.shape_keys.animation_data is None:
                    continue
                # Checking that the object has some SK and some animation on it
                if blender_object is None:
                    continue
                if blender_object.type != "MESH":
                    continue

                if already_added_sk_slot is True:
                    # We already added an action for this object
                    # So forcing creation of another animation
                    # Instead of adding a new slot to the existing action
                    new_action_forced = ActionData(blender_action)
                    new_action_forced.add_slot(slot, slot.target_id_type, None)
                    # Force new name to avoid merging later
                    new_action_forced.force_name(blender_action.name + "-" + slot.name_display)
                    blender_actions.add_action(new_action_forced, force_new_action=True)
                    continue

                new_action.add_slot(slot, slot.target_id_type, None)
                blender_actions.add_action(new_action)
                already_added_sk_slot = True

            else:
                pass  # TODOSLOT slot-3

        export_user_extensions('gather_actions_hook', export_settings, blender_object, blender_actions)

    return blender_actions
