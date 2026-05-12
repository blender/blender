# SPDX-FileCopyrightText: 2018-2026 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0


import bpy
from .....io.com.gltf2_io_extensions import Extension
from .....io.com.constants import GLTF_IRIDESCENCE_IOR
from ...material import texture_info as gltf2_blender_gather_texture_info
from ..search_node_tree import \
    has_image_node_from_socket, \
    get_socket_from_gltf_material_node, \
    detect_iridescence_thickness_texure, \
    get_socket, \
    get_factor_from_socket


def export_iridescence(bmat, export_settings):

    # There is no iridescence Factor (Intensity) in Blender
    # So we consider that we export the extension if the Iridescence thickness is not 0

    # Mapping:
    # Iridescence Thickness (Blender) -> Iridescence Thickness Maximum (glTF)
    # Iridesence Thickness Mininum (Blender, glTF Output Material Node) -> Iridescence Thickness Minimum (glTF)
    # Iridescence Thickness Texture Red (Blender) -> Iridescence Thickness Texture (glTF)
    # Tickness IOR (Blender) -> Iridescence IOR (glTF) . Warning, Blender default 1.333, glTF default 1.3
    # IridescenceFactor (Blender, glTF Output Material Node) -> Iridescence Factor (glTF)
    # IridescenceTexture Red (Blender, glTF Output Material Node texture on
    # IridescenceFactor) -> Iridescence Texture (glTF)

    iridescence_extension = {}
    uvmap_infos = {}
    udim_infos = {}

    # Cases where the extension is not exported:

    # No thickness socket found (no Principled Shader)
    iridescence_thickness_socket = get_socket(bmat.get_used_material().node_tree, "Thin Film Thickness")
    if iridescence_thickness_socket.socket is None:
        return None, {}, {}

    # factor (from glTF Output group node)
    iridescence_factor_socket = get_socket_from_gltf_material_node(
        bmat.get_used_material().node_tree, "Iridescence Factor")
    if iridescence_factor_socket.socket is None:
        return None, {}, {}

    # Thickness minimum (from glTF Output group node)
    iridescence_thickness_minimum_socket = get_socket_from_gltf_material_node(
        bmat.get_used_material().node_tree, "Iridescence Thickness Minimum")
    if iridescence_thickness_minimum_socket is None:
        return None, {}, {}

    # Thickness socket is not linked and default value is 0
    # But we have to export it anyway, because it can be animated and we need to export the path for KHR_animation_pointer
    # If not animated, it will be remove after export, because of the default value

    # Iridescence Factor socket is not linked and default value is 0
    # But we have to export it anyway, because it can be animated and we need to export the path for KHR_animation_pointer
    # If not animated, it will be remove after export, because of the default value

    # IOR
    iridescence_ior_socket = get_socket(bmat.get_used_material().node_tree, "Thin Film IOR")
    if abs(iridescence_ior_socket.socket.default_value - GLTF_IRIDESCENCE_IOR) > 0.0001:
        iridescence_extension['iridescenceIor'] = iridescence_ior_socket.socket.default_value

    # IOR storing for KHR_animation_pointer
    path_ = {}
    path_['length'] = 1
    path_['path'] = "/materials/XXX/extensions/KHR_materials_iridescence/iridescenceIor"
    export_settings['current_paths']["node_tree." +
                                     iridescence_ior_socket.socket.path_from_id() +
                                     ".default_value"] = path_

    # Iridescence Factor
    iridescence_factor, path = get_factor_from_socket(iridescence_factor_socket, kind='VALUE')
    if iridescence_factor != 0.0:
        iridescence_extension['iridescenceFactor'] = iridescence_factor

    # Iridescence Factor storing for KHR_animation_pointer
    path_ = {}
    path_['length'] = 1
    path_['path'] = "/materials/XXX/extensions/KHR_materials_iridescence/iridescenceFactor"
    export_settings['current_paths'][path] = path_

    # IridescenceTexture
    if has_image_node_from_socket(iridescence_factor_socket, export_settings) is True:

        texture_info, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_texture_info(
            iridescence_factor_socket,
            (iridescence_factor_socket,),
            export_settings
        )

        if texture_info is not None:
            iridescence_extension['iridescenceTexture'] = texture_info
            uvmap_infos.update({'iridescenceTexture': uvmap_info})
            udim_infos.update({'iridescenceTexture': udim_info} if len(udim_info) > 0 else {})

            if len(export_settings['current_texture_transform']) != 0:
                for k in export_settings['current_texture_transform'].keys():
                    path_ = {}
                    path_['length'] = export_settings['current_texture_transform'][k]['length']
                    path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                        "YYY", "extensions/KHR_materials_iridescence/iridescenceTexture/extensions")
                    path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
                    export_settings['current_paths'][k] = path_
            export_settings['current_texture_transform'] = {}

    # Iridescence Thickness Maximum
    thickness_non_linked = iridescence_thickness_socket.socket is not None and isinstance(
        iridescence_thickness_socket.socket, bpy.types.NodeSocket) and not iridescence_thickness_socket.socket.is_linked

    if thickness_non_linked is True:

        is_texture_iridescence = False

        if abs(iridescence_thickness_socket.socket.default_value - 400.0) > 0.0001:
            iridescence_extension['iridescenceThicknessMaximum'] = iridescence_thickness_socket.socket.default_value

        # Iridescence Thickness Maximum storing path for KHR_animation_pointer
        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/extensions/KHR_materials_iridescence/iridescenceThicknessMaximum"
        export_settings['current_paths']["node_tree." + iridescence_thickness_socket.socket.path_from_id() +
                                         ".default_value"] = path_

    else:
        is_texture_iridescence, iridescence_data = detect_iridescence_thickness_texure(
            iridescence_thickness_socket, iridescence_thickness_minimum_socket, export_settings)
        if is_texture_iridescence:
            # Texture found, so export from data retrieved
            if abs(iridescence_data['thickness_maximum'].node.outputs[0].default_value - 400.0) > 0.0001:
                iridescence_extension['iridescenceThicknessMaximum'] = iridescence_data['thickness_maximum'].node.outputs[0].default_value

            # Iridescence Thickness Maximum storing path for KHR_animation_pointer
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/extensions/KHR_materials_iridescence/iridescenceThicknessMaximum"
            export_settings['current_paths']["node_tree." +
                                             iridescence_data['thickness_maximum'].path_from_id() +
                                             ".default_value"] = path_

            # Thickness Minimum
            if abs(iridescence_data['thickness_minimum'].node.outputs[0].default_value - 100.0) > 0.0001:
                iridescence_extension['iridescenceThicknessMinimum'] = iridescence_data['thickness_minimum'].node.outputs[0].default_value

            # Iridescence Thickness Minimum storing path for KHR_animation_pointer
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/extensions/KHR_materials_iridescence/iridescenceThicknessMinimum"
            export_settings['current_paths']["node_tree." +
                                             iridescence_data['thickness_minimum'].path_from_id() +
                                             ".default_value"] = path_

            # Texture
            if iridescence_data['tex_socket'] is not None:
                texture_info, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_texture_info(
                    iridescence_data['tex_socket'],
                    (iridescence_data['tex_socket'],),
                    export_settings,
                )

                if texture_info is not None:
                    iridescence_extension['iridescenceThicknessTexture'] = texture_info
                    uvmap_infos.update({'iridescenceThicknessTexture': uvmap_info})
                    udim_infos.update({'iridescenceThicknessTexture': udim_info} if len(udim_info) > 0 else {})

                    if len(export_settings['current_texture_transform']) != 0:
                        for k in export_settings['current_texture_transform'].keys():
                            path_ = {}
                            path_['length'] = export_settings['current_texture_transform'][k]['length']
                            path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                                "YYY", "extensions/KHR_materials_iridescence/iridescenceThicknessTexture/extensions")
                            path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
                            export_settings['current_paths'][k] = path_
                    export_settings['current_texture_transform'] = {}

            # We have all data for returning the Extension
            return Extension('KHR_materials_iridescence', iridescence_extension, False), uvmap_infos, udim_infos

        else:
            pass  # No texture found

    fac, path = get_factor_from_socket(iridescence_thickness_socket, kind='VALUE')
    if fac != 400.0:
        iridescence_extension['iridescenceThicknessMaximum'] = fac

        # Iridescence Thickness Maximum storing path for KHR_animation_pointer
        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/extensions/KHR_materials_iridescence/iridescenceThicknessMaximum"
        export_settings['current_paths'][path] = path_

    # Iridescence Thickness Minimum
    if is_texture_iridescence is True:
        # Minimum is only used when there is a texture

        # No texture support here, only number
        fac, path = get_factor_from_socket(iridescence_thickness_minimum_socket, kind='VALUE')
        if fac != 100.0:
            iridescence_extension['iridescenceThicknessMinimum'] = fac

        # Iridescence Thickness Minimum storing path for KHR_animation_pointer
        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/extensions/KHR_materials_iridescence/iridescenceThicknessMinimum"
        export_settings['current_paths'][path] = path_

    return Extension('KHR_materials_iridescence', iridescence_extension, False), uvmap_infos, udim_infos
