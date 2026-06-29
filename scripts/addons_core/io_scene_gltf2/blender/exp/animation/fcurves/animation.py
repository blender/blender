# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from .....io.exp.user_extensions import export_user_extensions
from .channels import gather_animation_fcurves_channels, gather_animation_material_fcurves_channels, gather_animation_data_fcurves_channels


def gather_animation_fcurves(
        obj_uuid: str,
        blender_action: bpy.types.Action,
        slot_identifier: str,
        export_settings
):

    channels, to_be_sampled, extra_samplers = __gather_channels_fcurves(
        obj_uuid, blender_action, slot_identifier, export_settings)

    if not channels:
        return None, to_be_sampled, extra_samplers

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object
    export_user_extensions('animation_gather_fcurve', export_settings, blender_object, blender_action)

    return channels, to_be_sampled, extra_samplers


def __gather_channels_fcurves(
        obj_uuid: str,
        blender_action: bpy.types.Action,
        slot_identifier: str,
        export_settings):
    return gather_animation_fcurves_channels(obj_uuid, blender_action, slot_identifier, export_settings)


def gather_animation_material_fcurves(
        mat_uuid: str,
        blender_action: bpy.types.Action,
        slot_identifier: str,
        export_settings
):

    channels, to_be_sampled, extra_samplers = __gather_channels_material_fcurves(
        mat_uuid, blender_action, slot_identifier, export_settings)

    if not channels:
        return None

    # TODO : sampled what needed to be sampled

    blender_material = export_settings['material_identifiers'][mat_uuid]['blender']
    export_user_extensions('animation_gather_fcurve_material', export_settings, blender_material, blender_action)

    return channels


def __gather_channels_material_fcurves(
        mat_uuid: str,
        blender_action: bpy.types.Action,
        slot_identifier: str,
        export_settings):

    return gather_animation_material_fcurves_channels(mat_uuid, blender_action, slot_identifier, export_settings)


def gather_animation_data_fcurves(
        blender_main_type: str,
        blender_type_data: str,
        blender_id: str,
        blender_action: bpy.types.Action,
        slot_identifier: str,
        export_settings
):

    channels, to_be_sampled, extra_samplers = __gather_data_channels_fcurves(
        blender_main_type, blender_type_data, blender_id, blender_action, slot_identifier, export_settings)

    if not channels:
        return None

    # TODO : sampled what needed to be sampled

    return channels


def __gather_data_channels_fcurves(
    blender_main_type: str,
    blender_type_data: str,
    blender_id: str,
    blender_action: bpy.types.Action,
    slot_identifier: str,
    export_settings
):

    return gather_animation_data_fcurves_channels(
        blender_main_type, blender_type_data, blender_id, blender_action, slot_identifier, export_settings)
