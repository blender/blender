# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from .....io.com.constants import BLENDER_COAT_ROUGHNESS
from .....io.com.gltf2_io_extensions import Extension
from ...material import texture_info as gltf2_blender_gather_texture_info

from ..search_node_tree import has_image_node_from_socket, get_socket, get_factor_from_socket


def export_clearcoat(blender_material, export_settings):
    has_clearcoat_texture = False
    has_clearcoat_roughness_texture = False

    clearcoat_extension = {}
    clearcoat_roughness_slots = ()

    clearcoat_socket = get_socket(blender_material.node_tree, 'Coat Weight')
    clearcoat_roughness_socket = get_socket(blender_material.node_tree, 'Coat Roughness')
    clearcoat_normal_socket = get_socket(blender_material.node_tree, 'Coat Normal')

    if clearcoat_socket.socket is not None and isinstance(
            clearcoat_socket.socket,
            bpy.types.NodeSocket) and not clearcoat_socket.socket.is_linked:
        if clearcoat_socket.socket.default_value != 0.0:
            clearcoat_extension['clearcoatFactor'] = clearcoat_socket.socket.default_value

        # Storing path for KHR_animation_pointer
        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/extensions/KHR_materials_clearcoat/clearcoatFactor"
        export_settings['current_paths']["node_tree." + clearcoat_socket.socket.path_from_id() +
                                         ".default_value"] = path_

    elif has_image_node_from_socket(clearcoat_socket, export_settings):
        fac, path = get_factor_from_socket(clearcoat_socket, kind='VALUE')
        # default value in glTF is 0.0, but if there is a texture without factor, use 1
        clearcoat_extension['clearcoatFactor'] = fac if fac is not None else 1.0
        has_clearcoat_texture = True

        # Storing path for KHR_animation_pointer
        if path is not None:
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/extensions/KHR_materials_clearcoat/clearcoatFactor"
            export_settings['current_paths'][path] = path_, {}

    if clearcoat_roughness_socket.socket is not None and isinstance(
            clearcoat_roughness_socket.socket,
            bpy.types.NodeSocket) and not clearcoat_roughness_socket.socket.is_linked:
        if abs(clearcoat_roughness_socket.socket.default_value -
               BLENDER_COAT_ROUGHNESS) > 1e-5 or (abs(clearcoat_roughness_socket.socket.default_value -
                                                      BLENDER_COAT_ROUGHNESS) > 1e-5 and 'clearcoatFactor' in clearcoat_extension):
            clearcoat_extension['clearcoatRoughnessFactor'] = clearcoat_roughness_socket.socket.default_value

        # Storing path for KHR_animation_pointer
        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/extensions/KHR_materials_clearcoat/clearcoatRoughnessFactor "
        export_settings['current_paths']["node_tree." +
                                         clearcoat_roughness_socket.socket.path_from_id() +
                                         ".default_value"] = path_

    elif has_image_node_from_socket(clearcoat_roughness_socket, export_settings):
        fac, path = get_factor_from_socket(clearcoat_roughness_socket, kind='VALUE')
        # default value in glTF is 0.0, but if there is a texture without factor, use 1
        clearcoat_extension['clearcoatRoughnessFactor'] = fac if fac is not None else 1.0
        has_clearcoat_roughness_texture = True

        # Storing path for KHR_animation_pointer
        if path is not None:
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/extensions/KHR_materials_clearcoat/clearcoatRoughnessFactor"
            export_settings['current_paths'][path] = path_

    # Pack clearcoat (R) and clearcoatRoughness (G) channels.
    if has_clearcoat_texture and has_clearcoat_roughness_texture:
        clearcoat_roughness_slots = (clearcoat_socket, clearcoat_roughness_socket,)
    elif has_clearcoat_texture:
        clearcoat_roughness_slots = (clearcoat_socket,)
    elif has_clearcoat_roughness_texture:
        clearcoat_roughness_slots = (clearcoat_roughness_socket,)

    uvmap_infos = {}
    udim_infos = {}

    if len(clearcoat_roughness_slots) > 0:
        if has_clearcoat_texture:
            clearcoat_texture, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_texture_info(
                clearcoat_socket,
                clearcoat_roughness_slots,
                export_settings,
            )
            clearcoat_extension['clearcoatTexture'] = clearcoat_texture
            uvmap_infos.update({'clearcoatTexture': uvmap_info})
            udim_infos.update({'clearcoatTexture': udim_info} if len(udim_info.keys()) > 0 else {})

        if len(export_settings['current_texture_transform']) != 0:
            for k in export_settings['current_texture_transform'].keys():
                path_ = {}
                path_['length'] = export_settings['current_texture_transform'][k]['length']
                path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                    "YYY", "extensions/KHR_materials_clearcoat/clearcoatTexture/extensions")
                path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
                export_settings['current_paths'][k] = path_

        export_settings['current_texture_transform'] = {}

        if has_clearcoat_roughness_texture:
            clearcoat_roughness_texture, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_texture_info(
                clearcoat_roughness_socket, clearcoat_roughness_slots, export_settings, )
            clearcoat_extension['clearcoatRoughnessTexture'] = clearcoat_roughness_texture
            uvmap_infos.update({'clearcoatRoughnessTexture': uvmap_info})
            udim_infos.update({'clearcoatRoughnessTexture': udim_info} if len(udim_info.keys()) > 0 else {})

        if len(export_settings['current_texture_transform']) != 0:
            for k in export_settings['current_texture_transform'].keys():
                path_ = {}
                path_['length'] = export_settings['current_texture_transform'][k]['length']
                path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                    "YYY", "extensions/KHR_materials_clearcoat/clearcoatRoughnessTexture/extensions")
                path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
                export_settings['current_paths'][k] = path_

        export_settings['current_texture_transform'] = {}

    if has_image_node_from_socket(clearcoat_normal_socket, export_settings):
        clearcoat_normal_texture, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_material_normal_texture_info_class(
            clearcoat_normal_socket, (clearcoat_normal_socket,), export_settings)
        clearcoat_extension['clearcoatNormalTexture'] = clearcoat_normal_texture
        uvmap_infos.update({'clearcoatNormalTexture': uvmap_info})
        udim_infos.update({'clearcoatNormalTexture': udim_info} if len(udim_info.keys()) > 0 else {})

        if len(export_settings['current_texture_transform']) != 0:
            for k in export_settings['current_texture_transform'].keys():
                path_ = {}
                path_['length'] = export_settings['current_texture_transform'][k]['length']
                path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                    "YYY", "extensions/KHR_materials_clearcoat/clearcoatRoughnessTexture/extensions")
                path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
                export_settings['current_paths'][k] = path_

        if len(export_settings['current_normal_scale']) != 0:
            for k in export_settings['current_normal_scale'].keys():
                path_ = {}
                path_['length'] = export_settings['current_normal_scale'][k]['length']
                path_['path'] = export_settings['current_normal_scale'][k]['path'].replace(
                    "YYY", "extensions/KHR_materials_clearcoat/clearcoatNormalTexture")
                export_settings['current_paths'][k] = path_

        export_settings['current_normal_scale'] = {}

    return Extension('KHR_materials_clearcoat', clearcoat_extension, False), uvmap_infos, udim_infos
