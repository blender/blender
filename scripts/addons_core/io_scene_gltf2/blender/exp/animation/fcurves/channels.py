# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from .....io.exp.user_extensions import export_user_extensions
from .....io.com import gltf2_io
from ....exp.cache import cached
from ....com.data_path import get_target_object_path, get_target_property_name, get_rotation_modes, get_object_from_datapath, skip_sk, get_channelbag_for_slot
from ....com.conversion import get_target, get_channel_from_target
from .channel_target import gather_fcurve_channel_target, gather_fcurve_channel_target_extras, gather_fcurve_channel_target_data
from .sampler import gather_animation_fcurves_sampler


# Used only for extras on materials
@cached
def gather_animation_material_fcurves_channels(
        mat_uuid: str,
        blender_action: bpy.types.Action,
        slot_identifier: str,
        export_settings
) -> typing.List[gltf2_io.AnimationChannel]:

    channels_to_perform, to_be_sampled, additional_channels_to_perform, extras_channels_to_perform = get_channel_groups_material(
        mat_uuid, blender_action, blender_action.slots[slot_identifier], export_settings)

    custom_range = None
    if blender_action.use_frame_range:
        custom_range = (blender_action.frame_start, blender_action.frame_end)

    channels = []
    additional_samplers = []

    for chan in [chan for chan in channels_to_perform.values() if len(chan['properties']) != 0]:
        for channel_group in chan['properties'].values():
            channel = __gather_animation_fcurve_channel(
                chan['id_type'], chan['mat_uuid'], channel_group, None, custom_range, export_settings)
            if channel is not None:
                channels.append(channel)

    # Manage extras channels, only if user want to export custom properties as extra, and export animation pointer
    if export_settings['gltf_extras'] \
            and export_settings['gltf_export_anim_pointer']:
        for chan in [chan for chan in extras_channels_to_perform.values() if len(chan['properties']) != 0]:
            for custom_prop, channel_group in chan['properties'].items():
                channel = __gather_animation_fcurve_channel_extras(
                    chan['id_type'],
                    chan['mat_uuid'],
                    custom_prop,
                    channel_group,
                    None,  # No bone for material
                    custom_range,
                    export_settings)
                if channel is not None:
                    channels.append(channel)

    # TODO addition samplers for materials ?

    return channels, to_be_sampled, additional_samplers


@cached
def gather_animation_fcurves_channels(
        obj_uuid: int,
        blender_action: bpy.types.Action,
        slot_identifier: str,
        export_settings
):

    channels_to_perform, to_be_sampled, additional_channels_to_perform, extras_channels_to_perform = get_channel_groups(
        obj_uuid, blender_action, blender_action.slots[slot_identifier], export_settings)

    custom_range = None
    if blender_action.use_frame_range:
        custom_range = (blender_action.frame_start, blender_action.frame_end)

    channels = []
    additional_samplers = []

    for chan in [chan for chan in channels_to_perform.values() if len(chan['properties']) != 0]:
        for channel_group in chan['properties'].values():
            channel = __gather_animation_fcurve_channel(
                chan['id_type'], chan['obj_uuid'], channel_group, chan['bone'], custom_range, export_settings)
            if channel is not None:
                channels.append(channel)

    # Manage extras channels, only if user want to export custom properties as extra, and export animation pointer
    if export_settings['gltf_extras'] \
            and export_settings['gltf_export_anim_pointer']:
        for chan in [chan for chan in extras_channels_to_perform.values() if len(chan['properties']) != 0]:
            for custom_prop, channel_group in chan['properties'].items():
                channel = __gather_animation_fcurve_channel_extras(
                    chan['id_type'],
                    chan['obj_uuid'],
                    custom_prop,
                    channel_group,
                    chan['bone'],
                    custom_range,
                    export_settings)
                if channel is not None:
                    channels.append(channel)

    if export_settings['gltf_export_extra_animations']:
        for chan in [chan for chan in additional_channels_to_perform.values() if len(chan['properties']) != 0]:
            for channel_group_name, channel_group in chan['properties'].items():

                # No glTF channel here, as we don't have any target
                # Trying to retrieve sampler directly
                sampler = __gather_sampler(obj_uuid, tuple(channel_group), None, custom_range, True, export_settings)
                if sampler is not None:
                    additional_samplers.append((channel_group_name, sampler, "OBJECT", None))

    return channels, to_be_sampled, additional_samplers


def get_channel_groups_material(mat_uuid: str, blender_action: bpy.types.Action,
                                slot: bpy.types.ActionSlot, export_settings):

    targets = {}
    targets_additional = {}
    targets_extras = {}

    blender_material = export_settings['material_identifiers'][mat_uuid]['blender']

    to_be_sampled = []  # (mat_uuid , type , prop )

    channelbag = get_channelbag_for_slot(blender_action, slot)
    fcurves = channelbag.fcurves if channelbag else []
    for fcurve in fcurves:
        type_ = None
        # In some invalid files, channel hasn't any keyframes ... this channel need to be ignored
        if len(fcurve.keyframe_points) == 0:
            continue
        # Example of target property: ["my_custom_property"],
        # nodes["Principled BSDF"].inputs[0].default_value, use_backface_culling

        if fcurve.data_path.startswith("nodes["):
            type_ = "NODETREE"
        elif fcurve.data_path.startswith("["):
            type_ = "EXTRA"
        else:
            type_ = "MATERIAL"

        if type_ == "EXTRA":
            # No group by property, because we are going to export fcurve separately
            # We are going to evaluate fcurve, so no check if need to be sampled

            # We need here to split into 2 categories:
            # extras (custom properties), and additional
            # Extras will be managed only if user exports custom properties as extra, and
            # export animation pointer
            if export_settings['gltf_extras'] \
                    and export_settings['gltf_export_anim_pointer']:
                # Let's confirm that this is a custom property, and not a wrong path for example
                if fcurve.data_path.startswith('["') and fcurve.data_path.endswith('"]'):
                    test_custom_prop = fcurve.data_path[2:-2]
                    if blender_material.get(test_custom_prop) is not None:
                        # This is a custom property, we can export it as extra
                        target_data = targets_extras.get(blender_material, {})
                        target_data['type'] = type_
                        target_data['mat_uuid'] = mat_uuid
                        target_data['id_type'] = slot.target_id_type
                        target_properties = target_data.get('properties', {})
                        channels = target_properties.get(fcurve.data_path, [])
                        channels.append(fcurve)
                        target_properties[fcurve.data_path] = tuple(channels)
                        target_data['properties'] = target_properties
                        targets_extras[blender_material] = target_data
                        continue
        elif type_ == "MATERIAL":
            target_property = get_target_property_name(fcurve.data_path)
            if target_property is None:
                target_property = fcurve.data_path
            target_data = targets_additional.get(blender_material, {})
            target_data['type'] = type_
            target_data['mat_uuid'] = mat_uuid
            target_data['id_type'] = slot.target_id_type
            target_properties = target_data.get('properties', {})
            channels = target_properties.get(target_property, [])
            channels.append(fcurve)
            target_properties[target_property] = tuple(channels)
            target_data['properties'] = target_properties
            targets_additional[blender_material] = target_data
            continue

        elif type_ == "NODETREE":
            target_property = get_target_property_name(fcurve.data_path)
            if target_property is None:
                target_property = fcurve.data_path
            target_data = targets.get(blender_material, {})
            target_data['type'] = type_
            target_data['mat_uuid'] = mat_uuid
            target_data['id_type'] = slot.target_id_type
            target_properties = target_data.get('properties', {})
            channels = target_properties.get(target_property, [])
            channels.append(fcurve)
            target_properties[target_property] = channels
            target_data['properties'] = target_properties
            targets[blender_material] = target_data
            continue
        else:
            export_settings['log'].warning(
                "Invalid animation fcurve data path on action {}".format(
                    blender_action.name))
            continue

    for mat, target_data in targets.items():
        # Check if the property can be exported without sampling
        new_properties = {}
        for prop in target_data['properties'].keys():
            if needs_baking(
                    mat_uuid, target_data['properties'][prop], export_settings, check_armature=False) is True:
                to_be_sampled.append((mat_uuid, target_data['type'], get_channel_from_target(
                    get_target(prop)), None))  # No bone for material
            else:
                new_properties[prop] = target_data['properties'][prop]

        for prop in target_data['properties'].keys():
            target_data['properties'][prop] = tuple(target_data['properties'][prop])

    to_be_sampled = list(set(to_be_sampled))

    return targets, to_be_sampled, targets_additional, targets_extras


def get_channel_groups(obj_uuid: str, blender_action: bpy.types.Action,
                       slot: bpy.types.ActionSlot, export_settings, no_sample_option=False):
    # no_sample_option is used when we want to retrieve all SK channels, to be evaluate.
    targets = {}
    targets_additional = {}
    targets_extras = {}

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object

    # When multiple rotation mode detected, keep the currently used
    multiple_rotation_mode_detected = {}

    # When both normal and delta are used --> Set to to_be_sampled list
    to_be_sampled = []  # (object_uuid , type , prop, optional(bone.name) )

    channelbag = get_channelbag_for_slot(blender_action, slot)
    fcurves = channelbag.fcurves if channelbag else []
    for fcurve in fcurves:
        type_ = None
        # In some invalid files, channel hasn't any keyframes ... this channel need to be ignored
        if len(fcurve.keyframe_points) == 0:
            continue
        try:
            # example of target_property : location, rotation_quaternion, value
            target_property = get_target_property_name(fcurve.data_path)
        except Exception as _e:
            export_settings['log'].warning(
                "Invalid animation fcurve data path on action {}".format(
                    blender_action.name))
            continue
        object_path = get_target_object_path(fcurve.data_path)

        # find the object affected by this action
        # object_path : blank for blender_object itself, key_blocks["<name>"] for SK, pose.bones["<name>"] for bones
        if not object_path:
            if fcurve.data_path.startswith("["):
                target = blender_object
                type_ = "EXTRA"
            else:
                target = blender_object
                type_ = "OBJECT"
        else:
            try:
                target = get_object_from_datapath(blender_object, object_path)

                if blender_object.type == "ARMATURE" and fcurve.data_path.startswith("pose.bones["):
                    if target_property is not None:
                        if get_target(target_property) is not None:
                            type_ = "BONE"
                        else:
                            type_ = "EXTRA"
                    else:
                        type_ = "EXTRA"

                else:
                    type_ = "EXTRA"
                if blender_object.type == "MESH" and object_path.startswith("key_blocks"):
                    shape_key = blender_object.data.shape_keys.path_resolve(object_path)
                    if skip_sk(blender_object.data.shape_keys.key_blocks, shape_key):
                        continue
                    target = blender_object.data.shape_keys
                    type_ = "SK"
            except ValueError as _e:
                # if the object is a mesh and the action target path can not be resolved, we know that this is a morph
                # animation.
                if blender_object.type == "MESH":
                    try:
                        shape_key = blender_object.data.shape_keys.path_resolve(object_path)
                        if skip_sk(blender_object.data.shape_keys.key_blocks, shape_key):
                            continue
                        target = blender_object.data.shape_keys
                        type_ = "SK"
                    except Exception as _e:
                        # Something is wrong, for example a bone animation is linked to an object mesh...
                        export_settings['log'].warning(
                            "Invalid animation fcurve data path on action {}".format(
                                blender_action.name))
                        continue
                else:
                    export_settings['log'].warning("Animation target {} not found".format(object_path))
                    continue

        # Detect that object or bone are not multiple keyed for euler and quaternion
        # Keep only the current rotation mode used by object
        rotation, rotation_modes = get_rotation_modes(target_property)
        if rotation and target.rotation_mode not in rotation_modes:
            multiple_rotation_mode_detected[target] = True
            continue

        if type_ == "EXTRA":
            # No group by property, because we are going to export fcurve separately
            # We are going to evaluate fcurve, so no check if need to be sampled

            # We need here to split into 2 categories:
            # extras (custom properties), and additional
            # Extras will be managed only if user exports custom properties as extra, and
            # export animation pointer
            if export_settings['gltf_extras'] \
                    and export_settings['gltf_export_anim_pointer']:
                # Let's confirm that this is a custom property, and not a wrong path for example
                if fcurve.data_path.startswith('["') and fcurve.data_path.endswith('"]'):
                    test_custom_prop = fcurve.data_path[2:-2]
                    if export_settings['vtree'].nodes[obj_uuid].node is not None:
                        # Retrieve extras from right extra data ( node or mesh, ...)
                        if slot.target_id_type == 'MESH':
                            extras_target = export_settings['vtree'].nodes[obj_uuid].node.mesh.extras
                        elif slot.target_id_type == 'OBJECT':
                            extras_target = export_settings['vtree'].nodes[obj_uuid].node.extras
                        else:
                            extras_target = None  # Should not happen if all is implemeted # TODOEXTRAS ????

                        if extras_target is not None and extras_target.get(test_custom_prop) is not None:
                            # We manage only 1 item extras for now
                            if not isinstance(extras_target.get(test_custom_prop), (int, float, bool)):
                                continue

                            # This is a custom property, we can export it as extra
                            target_data = targets_extras.get(target, {})
                            target_data['type'] = type_
                            target_data['obj_uuid'] = obj_uuid
                            target_data['bone'] = target.name if type_ == "BONE" else None
                            target_data['id_type'] = slot.target_id_type
                            target_properties = target_data.get('properties', {})
                            channels = target_properties.get(fcurve.data_path, [])
                            channels.append(fcurve)
                            target_properties[fcurve.data_path] = tuple(channels)
                            target_data['properties'] = target_properties
                            targets_extras[target] = target_data
                            continue

                elif "pose.bones[" in object_path and "][" in fcurve.data_path and fcurve.data_path.endswith('"]'):
                    # This is a custom property on bone, we can export it as extra
                    tab = fcurve.data_path.split("][")
                    bone_name = tab[0][12:-1]
                    custom_prop = tab[1][1:-2]

                    armature_object = export_settings['vtree'].nodes[obj_uuid].blender_object
                    extra_target = armature_object.pose.bones[bone_name].get(custom_prop)

                    if extra_target:
                        # We manage only 1 item extras for now
                        if not isinstance(extra_target, (int, float, bool)):
                            continue
                        target_data = targets_extras.get(target, {})
                        target_data['type'] = type_
                        target_data['obj_uuid'] = obj_uuid
                        target_data['bone'] = bone_name
                        target_data['id_type'] = slot.target_id_type
                        target_properties = target_data.get('properties', {})
                        channels = target_properties.get(fcurve.data_path, [])
                        channels.append(fcurve)
                        target_properties[fcurve.data_path] = tuple(channels)
                        target_data['properties'] = target_properties
                        targets_extras[target] = target_data
                        continue
            else:

                if target_property is None:
                    target_property = fcurve.data_path
                if not target_property.startswith("pose.bones["):
                    target_property = fcurve.data_path
                target_data = targets_additional.get(target, {})
                target_data['type'] = type_
                target_data['bone'] = target.name if type_ == "BONE" else None
                target_data['obj_uuid'] = obj_uuid
                target_data['id_type'] = slot.target_id_type
                target_properties = target_data.get('properties', {})
                channels = target_properties.get(target_property, [])
                channels.append(fcurve)
                target_properties[target_property] = channels
                target_data['properties'] = target_properties
                targets_additional[target] = target_data
                continue

        # group channels by target object and affected property of the target
        target_data = targets.get(target, {})
        target_data['type'] = type_
        target_data['obj_uuid'] = obj_uuid
        target_data['bone'] = target.name if type_ == "BONE" else None
        target_data['id_type'] = slot.target_id_type

        target_properties = target_data.get('properties', {})
        channels = target_properties.get(target_property, [])
        channels.append(fcurve)
        target_properties[target_property] = channels
        target_data['properties'] = target_properties
        targets[target] = target_data

    for targ in multiple_rotation_mode_detected.keys():
        export_settings['log'].warning("Multiple rotation mode detected for {}".format(targ.name))

    # Now that all curves are extracted,
    #    - check that there is no normal + delta transforms
    #    - check that each group can be exported not sampled
    #    - be sure that shapekeys curves are correctly sorted

    for obj, target_data in targets.items():
        properties = target_data['properties'].keys()
        properties = [get_target(prop) for prop in properties]
        if len(properties) != len(set(properties)):
            new_properties = {}
            # There are some transformation + delta transformation
            # We can't use fcurve, so the property will be sampled
            for prop in target_data['properties'].keys():
                if len([get_target(p) for p in target_data['properties'] if get_target(p) == get_target(prop)]) > 1:
                    # normal + delta
                    to_be_sampled.append((obj_uuid, target_data['type'], get_channel_from_target(
                        get_target(prop)), None))  # None, because no delta exists on Bones
                else:
                    new_properties[prop] = target_data['properties'][prop]

            target_data['properties'] = new_properties

        # Check if the property can be exported without sampling
        new_properties = {}
        for prop in target_data['properties'].keys():
            if no_sample_option is False and needs_baking(
                    obj_uuid, target_data['properties'][prop], export_settings) is True:
                to_be_sampled.append((obj_uuid, target_data['type'], get_channel_from_target(
                    get_target(prop)), target_data['bone']))  # bone can be None if not a bone :)
            else:
                new_properties[prop] = target_data['properties'][prop]

        target_data['properties'] = new_properties

        # Make sure sort is correct for shapekeys
        if target_data['type'] == "SK":
            for prop in target_data['properties'].keys():
                target_data['properties'][prop] = tuple(
                    __get_channel_group_sorted(
                        target_data['properties'][prop],
                        export_settings['vtree'].nodes[obj_uuid].blender_object))
        else:
            for prop in target_data['properties'].keys():
                target_data['properties'][prop] = tuple(target_data['properties'][prop])

    to_be_sampled = list(set(to_be_sampled))

    return targets, to_be_sampled, targets_additional, targets_extras


def __get_channel_group_sorted(channels: typing.Tuple[bpy.types.FCurve], blender_object: bpy.types.Object):
    # if this is shapekey animation, we need to sort in same order than shapekeys
    # else, no need to sort
    if blender_object.type == "MESH":
        first_channel = channels[0]
        object_path = get_target_object_path(first_channel.data_path)
        if object_path:
            if not blender_object.data.shape_keys:
                # Something is wrong. Maybe the user assigned an armature action
                # to a mesh object. Returning without sorting
                return channels

            # This is shapekeys, we need to sort channels
            shapekeys_idx = {}
            cpt_sk = 0
            for sk in blender_object.data.shape_keys.key_blocks:
                if skip_sk(blender_object.data.shape_keys.key_blocks, sk):
                    continue
                shapekeys_idx[sk.name] = cpt_sk
                cpt_sk += 1

            # Note: channels will have some None items only for SK if some SK are not animated
            idx_channel_mapping = []
            all_sorted_channels = []
            for sk_c in channels:
                try:
                    sk_name = blender_object.data.shape_keys.path_resolve(get_target_object_path(sk_c.data_path)).name
                    idx_channel_mapping.append((shapekeys_idx[sk_name], sk_c))
                except Exception as _e:
                    # Something is wrong. For example, an armature action linked to a mesh object
                    continue

            existing_idx = dict(idx_channel_mapping)
            for i in range(0, cpt_sk):
                if i not in existing_idx.keys():
                    all_sorted_channels.append(None)
                else:
                    all_sorted_channels.append(existing_idx[i])

            if all([i is None for i in all_sorted_channels]):  # all channel in error, and some non keyed SK
                return channels             # This happen when an armature action is linked to a mesh object with non keyed SK

            return tuple(all_sorted_channels)

    # if not shapekeys, stay in same order, because order doesn't matter
    return channels


def __gather_animation_fcurve_channel_data(id_type: str,
                                           elem_uuid: str,
                                           channel_group: typing.Tuple[bpy.types.FCurve],
                                           bone: typing.Optional[str],
                                           custom_range: typing.Optional[set],
                                           export_settings
                                           ) -> typing.Union[gltf2_io.AnimationChannel, None]:

    sampler = __gather_sampler(id_type, elem_uuid, channel_group, bone, custom_range, True, export_settings)

    animation_channel = gltf2_io.AnimationChannel(
        extensions=None,
        extras=None,
        sampler=sampler,
        target=__gather_target_data(id_type, elem_uuid, bone, channel_group[0].data_path, export_settings)
    )

    return animation_channel


def __gather_animation_fcurve_channel_extras(id_type: str,
                                             elem_uuid: str,
                                             custom_property: str,
                                             channel_group: typing.Tuple[bpy.types.FCurve],
                                             bone: typing.Optional[str],
                                             custom_range: typing.Optional[set],
                                             export_settings
                                             ) -> typing.Union[gltf2_io.AnimationChannel, None]:

    sampler = __gather_sampler(id_type, elem_uuid, channel_group, bone, custom_range, True, export_settings)

    animation_channel = gltf2_io.AnimationChannel(
        extensions=None,
        extras=None,
        sampler=sampler,
        target=__gather_target_extras(id_type, elem_uuid, bone, custom_property, export_settings)
    )

    if id_type == "OBJECT":
        blender_object = export_settings['vtree'].nodes[elem_uuid].blender_object
        export_user_extensions(
            'animation_gather_fcurve_channel_extras',
            export_settings,
            blender_object,
            bone,
            channel_group)

    return animation_channel


def __gather_animation_fcurve_channel(id_type: str,
                                      obj_uuid: str,
                                      channel_group: typing.Tuple[bpy.types.FCurve],
                                      bone: typing.Optional[str],
                                      custom_range: typing.Optional[set],
                                      export_settings
                                      ) -> typing.Union[gltf2_io.AnimationChannel, None]:

    __target = __gather_target(id_type, obj_uuid, channel_group, bone, export_settings)
    if __target.path is not None:
        sampler = __gather_sampler(id_type, obj_uuid, channel_group, bone, custom_range, False, export_settings)

        if sampler is None:
            # After check, no need to animate this node for this channel
            return None

        animation_channel = gltf2_io.AnimationChannel(
            extensions=None,
            extras=None,
            sampler=sampler,
            target=__target
        )

        if id_type == "OBJECT":
            blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object
            export_user_extensions(
                'animation_gather_fcurve_channel',
                export_settings,
                blender_object,
                bone,
                channel_group)

        return animation_channel
    return None


def __gather_target_data(id_type,
                         elem_uuid,
                         bone,
                         prop,
                         export_settings
                         ) -> gltf2_io.AnimationChannelTarget:
    return gather_fcurve_channel_target_data(id_type, elem_uuid, bone, prop, export_settings)


def __gather_target_extras(id_type,
                           elem_uuid,
                           bone,
                           custom_property,
                           export_settings
                           ) -> gltf2_io.AnimationChannelTarget:
    return gather_fcurve_channel_target_extras(id_type, elem_uuid, bone, custom_property, export_settings)


def __gather_target(id_type: str,
                    elem_uuid: str,
                    channel_group: typing.Tuple[bpy.types.FCurve],
                    bone: typing.Optional[str],
                    export_settings
                    ) -> gltf2_io.AnimationChannelTarget:

    return gather_fcurve_channel_target(id_type, elem_uuid, channel_group, bone, export_settings)


def __gather_sampler(id_type: str,
                     elem_uuid: str,
                     channel_group: typing.Tuple[bpy.types.FCurve],
                     bone: typing.Optional[str],
                     custom_range: typing.Optional[set],
                     extra_mode: bool,
                     export_settings) -> gltf2_io.AnimationSampler:

    return gather_animation_fcurves_sampler(
        id_type,
        elem_uuid,
        channel_group,
        bone,
        custom_range,
        extra_mode,
        export_settings)


def needs_baking(obj_uuid: str,
                 channels: typing.Tuple[bpy.types.FCurve],
                 export_settings,
                 check_armature=True
                 ) -> bool:
    """
    Check if baking is needed.

    Some blender animations need to be baked as they can not directly be expressed in glTF.
    """
    def all_equal(lst):
        return lst[1:] == lst[:-1]

    # Note: channels has some None items only for SK if some SK are not animated
    # Sampling due to unsupported interpolation
    interpolation = [c for c in channels if c is not None][0].keyframe_points[0].interpolation
    if interpolation not in ["BEZIER", "LINEAR", "CONSTANT"]:
        export_settings['log'].warning(
            "Baking animation because of an unsupported interpolation method: {}".format(interpolation)
        )
        return True

    if any(any(k.interpolation != interpolation for k in c.keyframe_points) for c in channels if c is not None):
        # There are different interpolation methods in one action group
        export_settings['log'].warning(
            "Baking animation because there are keyframes with different "
            "interpolation methods in one channel"
        )
        return True

    if not all_equal([len(c.keyframe_points) for c in channels if c is not None]):
        export_settings['log'].warning(
            "Baking animation because the number of keyframes is not "
            "equal for all channel tracks"
        )
        return True

    if len([c for c in channels if c is not None][0].keyframe_points) <= 1:
        # we need to bake to 'STEP', as at least two keyframes are required to interpolate
        return True

    if not all_equal(list(zip([[k.co[0] for k in c.keyframe_points] for c in channels if c is not None]))):
        # The channels have differently located keyframes
        export_settings['log'].warning("Baking animation because of differently located keyframes in one channel")
        return True

    if check_armature:
        if export_settings['vtree'].nodes[obj_uuid].blender_object.type == "ARMATURE":
            animation_target = get_object_from_datapath(
                export_settings['vtree'].nodes[obj_uuid].blender_object, [
                    c for c in channels if c is not None][0].data_path)
            if isinstance(animation_target, bpy.types.PoseBone):
                if len(animation_target.constraints) != 0:
                    # Constraints such as IK act on the bone -> can not be represented in glTF atm
                    export_settings['log'].warning(
                        "Baking animation because of unsupported constraints acting on the bone")
                    return True

    return False


def gather_animation_data_fcurves_channels(
    blender_main_type, blender_type_data, blender_id, blender_action, slot_identifier, export_settings
):
    channels_to_perform, to_be_sampled, additional_channels_to_perform, extras_channels_to_perform = get_channel_groups_data(
        blender_main_type, blender_type_data, blender_id, blender_action, blender_action.slots[slot_identifier], export_settings)

    custom_range = None
    if blender_action.use_frame_range:
        custom_range = (blender_action.frame_start, blender_action.frame_end)

    channels = []
    additional_samplers = []

    for chan in [chan for chan in channels_to_perform.values() if len(chan['properties']) != 0]:
        for channel_group in chan['properties'].values():
            channel = __gather_animation_fcurve_channel_data(
                chan['id_type'], chan['data_uuid'], channel_group, None, custom_range, export_settings)
            if channel is not None:
                channels.append(channel)

    # Manage extras channels, only if user want to export custom properties as extra, and export animation pointer
    if export_settings['gltf_extras'] \
            and export_settings['gltf_export_anim_pointer']:
        for chan in [chan for chan in extras_channels_to_perform.values() if len(chan['properties']) != 0]:
            for custom_prop, channel_group in chan['properties'].items():
                channel = __gather_animation_fcurve_channel_extras(
                    chan['id_type'],
                    chan['data_uuid'],
                    custom_prop,
                    channel_group,
                    None,  # No bone for data
                    custom_range,
                    export_settings)
                if channel is not None:
                    channels.append(channel)

    # # TODO addition samplers for materials ?

    return channels, to_be_sampled, additional_samplers


def get_channel_groups_data(
        blender_main_type: str,
        blender_type_data: str,
        data_uuid: str,
        blender_action: bpy.types.Action,
        slot: bpy.types.ActionSlot,
        export_settings):

    targets = {}
    targets_additional = {}
    targets_extras = {}

    if blender_main_type is None:
        if blender_type_data == "lights":
            blender_element = [l for l in bpy.data.lights if id(l) == data_uuid][0]
        elif blender_type_data == "cameras":
            blender_element = [c for c in bpy.data.cameras if id(c) == data_uuid][0]
    else:
        if blender_type_data == "lights":
            blender_element = [l for l in bpy.data.lights if id(l) == data_uuid][0]
        elif blender_type_data == "cameras":
            blender_element = [c for c in bpy.data.cameras if id(c) == data_uuid][0]

    to_be_sampled = []  # (data_uuid , type , prop )

    channelbag = get_channelbag_for_slot(blender_action, slot)
    fcurves = channelbag.fcurves if channelbag else []
    for fcurve in fcurves:
        type_ = None
        # In some invalid files, channel hasn't any keyframes ... this channel need to be ignored
        if len(fcurve.keyframe_points) == 0:
            continue
        # Example of target property: ["my_custom_property"],
        # nodes["Principled BSDF"].inputs[0].default_value, use_backface_culling

        if fcurve.data_path.startswith("["):
            type_ = "EXTRA"
        else:
            type_ = "DATA"

        if type_ == "EXTRA":
            # No group by property, because we are going to export fcurve separately
            # We are going to evaluate fcurve, so no check if need to be sampled

            # We need here to split into 2 categories:
            # extras (custom properties), and additional
            # Extras will be managed only if user exports custom properties as extra, and
            # export animation pointer
            if export_settings['gltf_extras'] \
                    and export_settings['gltf_export_anim_pointer']:
                # Let's confirm that this is a custom property, and not a wrong path for example
                if fcurve.data_path.startswith('["') and fcurve.data_path.endswith('"]'):
                    test_custom_prop = fcurve.data_path[2:-2]
                    if blender_element.get(test_custom_prop) is not None:
                        # This is a custom property, we can export it as extra
                        target_data = targets_extras.get(blender_element, {})
                        target_data['type'] = type_
                        target_data['data_uuid'] = data_uuid
                        target_data['id_type'] = slot.target_id_type
                        target_properties = target_data.get('properties', {})
                        channels = target_properties.get(fcurve.data_path, [])
                        channels.append(fcurve)
                        target_properties[fcurve.data_path] = tuple(channels)
                        target_data['properties'] = target_properties
                        targets_extras[blender_element] = target_data
                        continue
        else:
            target_property = get_target_property_name(fcurve.data_path)
            if target_property is None:
                target_property = fcurve.data_path
            target_data = targets.get(blender_element, {})
            target_data['type'] = type_
            target_data['data_uuid'] = data_uuid
            target_data['id_type'] = slot.target_id_type
            target_properties = target_data.get('properties', {})
            channels = target_properties.get(target_property, [])
            channels.append(fcurve)
            target_properties[target_property] = channels
            target_data['properties'] = target_properties
            targets[blender_element] = target_data
            continue

    for elem, target_data in targets.items():
        # Check if the property can be exported without sampling
        new_properties = {}
        for prop in target_data['properties'].keys():
            if needs_baking(
                    data_uuid, target_data['properties'][prop], export_settings, check_armature=False) is True:
                to_be_sampled.append((data_uuid, target_data['type'], get_channel_from_target(
                    get_target(prop)), None))  # No bone for material
            else:
                new_properties[prop] = target_data['properties'][prop]

        for prop in target_data['properties'].keys():
            target_data['properties'][prop] = tuple(target_data['properties'][prop])

    to_be_sampled = list(set(to_be_sampled))

    return targets, to_be_sampled, targets_additional, targets_extras
