# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from .....io.com.gltf2_io_extensions import Extension
from ...material.texture_info import gather_texture_info
from ..search_node_tree import \
    has_image_node_from_socket, \
    get_socket, \
    get_factor_from_socket


def export_specular(blender_material, export_settings):
    specular_extension = {}

    specular_socket = get_socket(blender_material.node_tree, 'Specular IOR Level')
    speculartint_socket = get_socket(blender_material.node_tree, 'Specular Tint')

    if specular_socket.socket is None or speculartint_socket.socket is None:
        return None, {}, {}

    uvmap_infos = {}
    udim_infos = {}

    specular_non_linked = specular_socket.socket is not None and isinstance(
        specular_socket.socket, bpy.types.NodeSocket) and not specular_socket.socket.is_linked
    specularcolor_non_linked = speculartint_socket.socket is not None and isinstance(
        speculartint_socket.socket, bpy.types.NodeSocket) and not speculartint_socket.socket.is_linked

    if specular_non_linked is True:
        fac = specular_socket.socket.default_value
        fac = fac * 2.0
        if fac < 1.0:
            specular_extension['specularFactor'] = fac
        elif fac > 1.0:
            # glTF specularFactor should be <= 1.0, so we will multiply ColorFactory
            # by specularFactor, and set SpecularFactor to 1.0 (default value)
            pass
        else:
            pass  # If fac == 1.0, no need to export specularFactor, the default value is 1.0

        # Storing path for KHR_animation_pointer
        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/extensions/KHR_materials_specular/specularFactor"
        export_settings['current_paths']["node_tree." +
                                         specular_socket.socket.path_from_id() + ".default_value"] = path_

    else:
        # Factor
        fac, path = get_factor_from_socket(specular_socket, kind='VALUE')
        if fac is not None and fac != 1.0:
            fac = fac * 2.0 if fac is not None else None
            if fac is not None and fac < 1.0:
                specular_extension['specularFactor'] = fac
            elif fac is not None and fac > 1.0:
                # glTF specularFactor should be <= 1.0, so we will multiply ColorFactory
                # by specularFactor, and set SpecularFactor to 1.0 (default value)
                pass

        if path is not None:
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/extensions/KHR_materials_specular/specularFactor"
            export_settings['current_paths'][path] = path_

        # Texture
        if has_image_node_from_socket(specular_socket, export_settings):
            specular_texture, uvmap_info, udim_info, _ = gather_texture_info(
                specular_socket,
                (specular_socket,),
                export_settings,
            )
            specular_extension['specularTexture'] = specular_texture
            uvmap_infos.update({'specularTexture': uvmap_info})
            udim_infos.update({'specularTexture': udim_info} if len(udim_info) > 0 else {})

            if len(export_settings['current_texture_transform']) != 0:
                for k in export_settings['current_texture_transform'].keys():
                    path_ = {}
                    path_['length'] = export_settings['current_texture_transform'][k]['length']
                    path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                        "YYY", "extensions/KHR_materials_specular/specularTexture/extensions")
                    path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
                    export_settings['current_paths'][k] = path_

            export_settings['current_texture_transform'] = {}

    if specularcolor_non_linked is True:
        color = speculartint_socket.socket.default_value[:3]
        if fac is not None and fac > 1.0:
            color = (color[0] * fac, color[1] * fac, color[2] * fac)
        if color != (1.0, 1.0, 1.0):
            specular_extension['specularColorFactor'] = color

         # Storing path for KHR_animation_pointer
        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/extensions/KHR_materials_specular/specularColorFactor"
        export_settings['current_paths']["node_tree." + speculartint_socket.socket.path_from_id() +
                                         ".default_value"] = path_
    else:
        # Factor
        fac_color, path = get_factor_from_socket(speculartint_socket, kind='RGB')
        if fac_color is not None and fac is not None and fac > 1.0:
            fac_color = (fac_color[0] * fac, fac_color[1] * fac, fac_color[2] * fac)
        elif fac_color is None and fac is not None and fac > 1.0:
            fac_color = (fac, fac, fac)
        if fac_color != (1.0, 1.0, 1.0):
            specular_extension['specularColorFactor'] = fac_color

        if path is not None:
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/extensions/KHR_materials_specular/specularColorFactor"
            export_settings['current_paths'][path] = path_

        # Texture
        if has_image_node_from_socket(speculartint_socket, export_settings):
            specularcolor_texture, uvmap_info, udim_info, _ = gather_texture_info(
                speculartint_socket,
                (speculartint_socket,),
                export_settings,
            )
            specular_extension['specularColorTexture'] = specularcolor_texture
            uvmap_infos.update({'specularColorTexture': uvmap_info})

    if len(export_settings['current_texture_transform']) != 0:
        for k in export_settings['current_texture_transform'].keys():
            path_ = {}
            path_['length'] = export_settings['current_texture_transform'][k]['length']
            path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                "YYY", "extensions/KHR_materials_specular/specularColorTexture/extensions")
            path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
            export_settings['current_paths'][k] = path_

    export_settings['current_texture_transform'] = {}

    return Extension('KHR_materials_specular', specular_extension, False), uvmap_infos, udim_infos
