# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from .....io.com.gltf2_io_extensions import Extension
from ...material import texture_info as gltf2_blender_gather_texture_info
from ..search_node_tree import \
    has_image_node_from_socket, \
    get_socket, \
    get_factor_from_socket


def export_transmission(blender_material, export_settings):
    has_transmission_texture = False

    transmission_extension = {}
    transmission_slots = ()

    transmission_socket = get_socket(blender_material.node_tree, 'Transmission Weight')

    if transmission_socket.socket is not None and isinstance(
            transmission_socket.socket,
            bpy.types.NodeSocket) and not transmission_socket.socket.is_linked:
        if transmission_socket.socket.default_value != 0.0:
            transmission_extension['transmissionFactor'] = transmission_socket.socket.default_value

        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/extensions/KHR_materials_transmission/transmissionFactor"
        export_settings['current_paths']["node_tree." + transmission_socket.socket.path_from_id() +
                                         ".default_value"] = path_

    elif has_image_node_from_socket(transmission_socket, export_settings):
        fac, path = get_factor_from_socket(transmission_socket, kind='VALUE')
        transmission_extension['transmissionFactor'] = fac if fac is not None else 1.0
        has_transmission_texture = True

        # Storing path for KHR_animation_pointer
        if path is not None:
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/extensions/KHR_materials_transmission/transmissionFactor"
            export_settings['current_paths'][path] = path_

    uvmap_info = {}
    udim_info = {}

    # Pack transmission channel (R).
    if has_transmission_texture:
        transmission_slots = (transmission_socket,)

    if len(transmission_slots) > 0:
        combined_texture, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_texture_info(
            transmission_socket,
            transmission_slots,
            export_settings,
        )
        if has_transmission_texture:
            transmission_extension['transmissionTexture'] = combined_texture

        if len(export_settings['current_texture_transform']) != 0:
            for k in export_settings['current_texture_transform'].keys():
                path_ = {}
                path_['length'] = export_settings['current_texture_transform'][k]['length']
                path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                    "YYY", "extensions/KHR_materials_transmission/transmissionTexture/extensions")
                path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
                export_settings['current_paths'][k] = path_

        export_settings['current_texture_transform'] = {}

    return Extension(
        'KHR_materials_transmission', transmission_extension, False), {
        'transmissionTexture': uvmap_info}, {
            'transmissionTexture': udim_info} if len(udim_info) > 0 else {}
