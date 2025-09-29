# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from .....io.com.gltf2_io_extensions import Extension
from ...material import texture_info as gltf2_blender_gather_texture_info
from ..search_node_tree import \
    get_const_from_default_value_socket, \
    get_socket, \
    get_factor_from_socket, \
    get_const_from_socket, \
    NodeSocket, \
    get_socket_from_gltf_material_node


def export_emission_factor(blender_material, export_settings):
    emissive_socket = get_socket(blender_material.node_tree, "Emissive")
    if emissive_socket.socket is None:
        emissive_socket = get_socket_from_gltf_material_node(
            blender_material.node_tree, "EmissiveFactor")
    if emissive_socket is not None and isinstance(emissive_socket.socket, bpy.types.NodeSocket):
        if export_settings['gltf_image_format'] != "NONE":
            factor, path = get_factor_from_socket(emissive_socket, kind='RGB')
        else:
            factor, path = get_const_from_default_value_socket(emissive_socket, kind='RGB')

        if factor is None and emissive_socket.socket.is_linked:
            # In glTF, the default emissiveFactor is all zeros, so if an emission texture is connected,
            # we have to manually set it to all ones.
            factor = [1.0, 1.0, 1.0]

        if factor is None:
            factor = [0.0, 0.0, 0.0]

        # Handle Emission Strength
        strength_socket = None
        strength_path = None
        if emissive_socket.socket.node.type == 'EMISSION':
            strength_socket = emissive_socket.socket.node.inputs['Strength']
        elif 'Emission Strength' in emissive_socket.socket.node.inputs:
            strength_socket = emissive_socket.socket.node.inputs['Emission Strength']
        if strength_socket is not None and isinstance(strength_socket, bpy.types.NodeSocket):
            strength, strength_path = get_factor_from_socket(NodeSocket(
                strength_socket, emissive_socket.group_path), kind='VALUE')
        strength = (
            strength
            if strength_socket is not None
            else None
        )
        if strength is not None:
            factor = [f * strength for f in factor]

        # Clamp to range [0,1]
        # Official glTF clamp to range [0,1]
        # If we are outside, we need to use extension KHR_materials_emissive_strength

        if factor == [0, 0, 0]:
            factor = None

        # Storing path for KHR_animation_pointer
        if path is not None:
            path_ = {}
            path_['length'] = 3
            path_['path'] = "/materials/XXX/emissiveFactor"
            path_['strength_channel'] = strength_path
            export_settings['current_paths'][path] = path_

        # Storing path for KHR_animation_pointer, for emissiveStrength (if needed)
        if strength_path is not None:
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/extensions/KHR_materials_emissive_strength/emissiveStrength"
            path_['factor_channel'] = path
            export_settings['current_paths'][strength_path] = path_

        return factor

    return None


def export_emission_texture(blender_material, export_settings):
    emissive = get_socket(blender_material.node_tree, "Emissive")
    if emissive.socket is None:
        emissive = get_socket_from_gltf_material_node(
            blender_material.node_tree, "Emissive")
    emissive_texture, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_texture_info(
        emissive, (emissive,), export_settings)

    if len(export_settings['current_texture_transform']) != 0:
        for k in export_settings['current_texture_transform'].keys():
            path_ = {}
            path_['length'] = export_settings['current_texture_transform'][k]['length']
            path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                "YYY", "emissiveTexture/extensions")
            path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
            export_settings['current_paths'][k] = path_

    export_settings['current_texture_transform'] = {}

    return emissive_texture, {
        'emissiveTexture': uvmap_info}, {
        'emissiveTexture': udim_info} if len(
            udim_info.keys()) > 0 else {}


def export_emission_strength_extension(emissive_factor, export_settings):
    # Always export the extension if the emissive factor
    # If the emissive factor is animated, we need to export the extension, even if the initial value is < 1.0
    # We will check if the strength is animated and this extension is needed at end of the export
    emissive_strength_extension = {}
    if any([i > 1.0 for i in emissive_factor or []]):
        emissive_strength_extension['emissiveStrength'] = max(emissive_factor)

    return Extension('KHR_materials_emissive_strength', emissive_strength_extension, False)
