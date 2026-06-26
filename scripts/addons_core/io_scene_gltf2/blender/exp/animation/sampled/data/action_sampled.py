# SPDX-FileCopyrightText: 2018-2026 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from ......io.exp.user_extensions import export_user_extensions
from .channels import gather_data_sampled_channels


def gather_action_mesh_sampled(obj_uuid: str,
                               blender_action,
                               slot_identifier: str,
                               cache_key: str,
                               export_settings):
    # Used for custom properties on mesh data

    # If no animation in file, no need to bake
    if len(bpy.data.actions) == 0:
        return None

    channels = __gather_channels('meshes', obj_uuid, blender_action.name if blender_action else cache_key,
                                 slot_identifier if blender_action else None, export_settings)

    if not channels:
        return None

    return channels


def gather_action_data_sampled(
        blender_main_type: str,
        blender_type_data: str,
        blender_id: str,
        blender_action,
        slot_identifier: str,
        cache_key: str,
        export_settings):
    """ For lights, cameras """

    # If no animation in file, no need to bake
    if len(bpy.data.actions) == 0:
        return None

    channels = gather_data_sampled_channels(
        blender_main_type,
        blender_type_data,
        blender_id,
        blender_action.name if blender_action else cache_key,
        slot_identifier if blender_action else None,
        None,
        export_settings)

    if not channels:
        return None

    export_user_extensions(
        'animation_channels_data_sampled',
        export_settings,
        channels,
        None,  # TODOEXTRAS This can be either for materials animation pointer or for extras
        blender_action,
        slot_identifier,
        cache_key)

    return channels


def gather_action_material_sampled(mat_uuid: str,
                                   blender_action,
                                   slot_identifier: str,
                                   cache_key: str,
                                   export_settings):

    # If no animation in file, no need to bake
    if len(bpy.data.actions) == 0:
        return None

    # Extras
    channels = __gather_channels('materials', mat_uuid, blender_action.name if blender_action else cache_key,
                                 slot_identifier if blender_action else None, export_settings)

    if not channels:
        channels = []

    # "classical" animation pointer (not extras)
    channels_classical = __gather_channels(None, mat_uuid, blender_action.name if blender_action else cache_key,
                                           slot_identifier if blender_action else None, export_settings)

    if channels_classical:
        channels.extend(channels_classical)

    if len(channels) == 0:
        return None

    blender_material = export_settings['material_identifiers'][mat_uuid]['blender']
    export_user_extensions(
        'animation_channels_material_sampled',
        export_settings,
        channels,
        blender_material,
        blender_action,
        slot_identifier,
        cache_key)

    return channels


def __gather_channels(data_type: str, uuid: str, blender_action_name: str, slot_identifier: str,
                      export_settings):

    # For meshes, this is only for custom properties
    if data_type == 'meshes':
        data_main_type = 'extras'
    elif data_type == 'materials':
        data_main_type = 'extras'  # TODOEXTRAS This can be either for materials animation pointer or for extras
        # Currently, material animation pointer is not supported (doubleSided ?)
    else:
        # This is for animation pointer
        data_main_type = None
        data_type = 'materials'

    return gather_data_sampled_channels(
        data_main_type,
        data_type,
        uuid,
        blender_action_name,
        slot_identifier,
        None,
        export_settings)
