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


def export_sheen(blender_material, export_settings):
    sheen_extension = {}

    sheenTint_socket = get_socket(blender_material.node_tree, "Sheen Tint")
    sheenRoughness_socket = get_socket(blender_material.node_tree, "Sheen Roughness")
    sheen_socket = get_socket(blender_material.node_tree, "Sheen Weight")

    if sheenTint_socket.socket is None or sheenRoughness_socket.socket is None or sheen_socket.socket is None:
        return None, {}, {}

    if sheen_socket.socket.is_linked is False and sheen_socket.socket.default_value == 0.0:
        return None, {}, {}

    uvmap_infos = {}
    udim_infos = {}

    # TODOExt : What to do if sheen_socket is linked? or is not between 0 and 1?

    sheenTint_non_linked = sheenTint_socket.socket is not None and isinstance(
        sheenTint_socket.socket, bpy.types.NodeSocket) and not sheenTint_socket.socket.is_linked
    sheenRoughness_non_linked = sheenRoughness_socket.socket is not None and isinstance(
        sheenRoughness_socket.socket, bpy.types.NodeSocket) and not sheenRoughness_socket.socket.is_linked

    if sheenTint_non_linked is True:
        color = sheenTint_socket.socket.default_value[:3]
        if color != (0.0, 0.0, 0.0):
            sheen_extension['sheenColorFactor'] = color

        # Storing path for KHR_animation_pointer
        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/extensions/KHR_materials_sheen/sheenColorFactor"
        export_settings['current_paths']["node_tree." + sheenTint_socket.socket.path_from_id() +
                                         ".default_value"] = path_

    else:
        # Factor
        fac, path = get_factor_from_socket(sheenTint_socket, kind='RGB')
        if fac is None:
            fac = [1.0, 1.0, 1.0]  # Default is 0.0/0.0/0.0, so we need to set it to 1 if no factor
        if fac is not None and fac != [0.0, 0.0, 0.0]:
            sheen_extension['sheenColorFactor'] = fac

        # Storing path for KHR_animation_pointer
        if path is not None:
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/extensions/KHR_materials_sheen/sheenColorFactor"
            export_settings['current_paths'][path] = path_

        # Texture
        if has_image_node_from_socket(sheenTint_socket, export_settings):
            original_sheenColor_texture, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_texture_info(
                sheenTint_socket, (sheenTint_socket,), export_settings, )
            sheen_extension['sheenColorTexture'] = original_sheenColor_texture
            uvmap_infos.update({'sheenColorTexture': uvmap_info})
            udim_infos.update({'sheenColorTexture': udim_info} if len(udim_info) > 0 else {})

            if len(export_settings['current_texture_transform']) != 0:
                for k in export_settings['current_texture_transform'].keys():
                    path_ = {}
                    path_['length'] = export_settings['current_texture_transform'][k]['length']
                    path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                        "YYY", "extensions/KHR_materials_sheen/sheenColorTexture/extensions")
                    path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
                    export_settings['current_paths'][k] = path_

            export_settings['current_texture_transform'] = {}

    if sheenRoughness_non_linked is True:
        fac = sheenRoughness_socket.socket.default_value
        if fac != 0.0:
            sheen_extension['sheenRoughnessFactor'] = fac

        # Storing path for KHR_animation_pointer
        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/extensions/KHR_materials_sheen/sheenRoughnessFactor"
        export_settings['current_paths']["node_tree." + sheenRoughness_socket.socket.path_from_id() +
                                         ".default_value"] = path_

    else:
        # Factor
        fac, path = get_factor_from_socket(sheenRoughness_socket, kind='VALUE')
        if fac is None:
            fac = 1.0  # Default is 0.0 so we need to set it to 1.0 if no factor
        if fac is not None and fac != 0.0:
            sheen_extension['sheenRoughnessFactor'] = fac

        # Storing path for KHR_animation_pointer
        if path is not None:
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/extensions/KHR_materials_sheen/sheenRoughnessFactor"
            export_settings['current_paths'][path] = path_

        # Texture
        if has_image_node_from_socket(sheenRoughness_socket, export_settings):
            original_sheenRoughness_texture, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_texture_info(
                sheenRoughness_socket, (sheenRoughness_socket,), export_settings, )
            sheen_extension['sheenRoughnessTexture'] = original_sheenRoughness_texture
            uvmap_infos.update({'sheenRoughnessTexture': uvmap_info})
            udim_infos.update({'sheenRoughnessTexture': udim_info} if len(udim_info) > 0 else {})

            if len(export_settings['current_texture_transform']) != 0:
                for k in export_settings['current_texture_transform'].keys():
                    path_ = {}
                    path_['length'] = export_settings['current_texture_transform'][k]['length']
                    path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                        "YYY", "extensions/KHR_materials_sheen/sheenRoughnessTexture/extensions")
                    path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
                    export_settings['current_paths'][k] = path_

            export_settings['current_texture_transform'] = {}

    return Extension('KHR_materials_sheen', sheen_extension, False), uvmap_infos, udim_infos
