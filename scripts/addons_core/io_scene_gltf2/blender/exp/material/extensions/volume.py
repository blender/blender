# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from .....io.com.gltf2_io_extensions import Extension
from ...material import texture_info as gltf2_blender_gather_texture_info
from ..search_node_tree import \
    has_image_node_from_socket, \
    get_const_from_default_value_socket, \
    get_socket_from_gltf_material_node, \
    get_socket, \
    get_factor_from_socket


def export_volume(blender_material, export_settings):
    # Implementation based on https://github.com/KhronosGroup/glTF-Blender-IO/issues/1454#issuecomment-928319444

    # If no transmission --> No volume
    # But we need to keep it, in case it is animated

    volume_extension = {}
    has_thickness_texture = False
    thickness_slots = ()
    uvmap_info = {}

    thickness_socket = get_socket_from_gltf_material_node(
        blender_material.node_tree, 'Thickness')
    if thickness_socket.socket is None:
        # If no thickness (here because there is no glTF Material Output node), no volume extension export
        return None, {}, {}

    density_socket = get_socket(blender_material.node_tree, 'Density', volume=True)
    attenuation_color_socket = get_socket(blender_material.node_tree, 'Color', volume=True)
    # Even if density or attenuation are not set, we export volume extension

    if attenuation_color_socket.socket is not None and isinstance(
            attenuation_color_socket.socket, bpy.types.NodeSocket):
        rgb, path = get_const_from_default_value_socket(attenuation_color_socket, kind='RGB')
        volume_extension['attenuationColor'] = rgb

        # Storing path for KHR_animation_pointer
        if path is not None:
            path_ = {}
            path_['length'] = 3
            path_['path'] = "/materials/XXX/extensions/KHR_materials_volume/attenuationColor"
            export_settings['current_paths'][path] = path_

    if density_socket.socket is not None and isinstance(density_socket.socket, bpy.types.NodeSocket):
        density, path = get_const_from_default_value_socket(density_socket, kind='VALUE')
        volume_extension['attenuationDistance'] = 1.0 / \
            density if density != 0 else None  # infinity (Using None as glTF default)

        # Storing path for KHR_animation_pointer
        if path is not None:
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/extensions/KHR_materials_volume/attenuationDistance"
            export_settings['current_paths'][path] = path_

    if isinstance(thickness_socket.socket, bpy.types.NodeSocket) and not thickness_socket.socket.is_linked:
        val = thickness_socket.socket.default_value
        if val == 0.0:
            # If no thickness, no volume extension export
            return None, {}, {}
        volume_extension['thicknessFactor'] = val

        # Storing path for KHR_animation_pointer
        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/extensions/KHR_materials_volume/thicknessFactor"
        export_settings['current_paths']["node_tree." + thickness_socket.socket.path_from_id() +
                                         ".default_value"] = path_

    elif has_image_node_from_socket(thickness_socket, export_settings):
        fac, path = get_factor_from_socket(thickness_socket, kind='VALUE')
        # default value in glTF is 0.0, but if there is a texture without factor, use 1
        volume_extension['thicknessFactor'] = fac if fac is not None else 1.0

        # Storing path for KHR_animation_pointer
        if path is not None:
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/extensions/KHR_materials_volume/thicknessFactor"
            export_settings['current_paths'][path] = path_

        has_thickness_texture = True

       # Pack thickness channel (G).
    if has_thickness_texture:
        thickness_slots = (thickness_socket,)

    udim_info = {}
    if len(thickness_slots) > 0:
        combined_texture, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_texture_info(
            thickness_socket,
            thickness_slots,
            export_settings,
        )
        if has_thickness_texture:
            volume_extension['thicknessTexture'] = combined_texture

        if len(export_settings['current_texture_transform']) != 0:
            for k in export_settings['current_texture_transform'].keys():
                path_ = {}
                path_['length'] = export_settings['current_texture_transform'][k]['length']
                path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                    "YYY", "extensions/KHR_materials_volume/thicknessTexture/extensions")
                path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
                export_settings['current_paths'][k] = path_

        export_settings['current_texture_transform'] = {}

    return Extension(
        'KHR_materials_volume', volume_extension, False), {
        'thicknessTexture': uvmap_info}, {
            'thicknessTexture': udim_info} if len(
                udim_info.keys()) > 0 else {}
