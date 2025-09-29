# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0


import bpy
from ....io.com import gltf2_io
from ....io.exp.user_extensions import export_user_extensions
from ..cache import cached
from .texture_info import gather_texture_info
from .search_node_tree import \
    get_socket_from_gltf_material_node, \
    has_image_node_from_socket, \
    get_const_from_default_value_socket, \
    get_socket, \
    get_factor_from_socket, \
    gather_alpha_info, \
    gather_color_info


@cached
def gather_material_pbr_metallic_roughness(blender_material, orm_texture, export_settings):
    if not __filter_pbr_material(blender_material, export_settings):
        return None, {}, {'color': None, 'alpha': None, 'color_type': None, 'alpha_type': None, 'alpha_mode': "OPAQUE"}, {}

    uvmap_infos = {}
    udim_infos = {}

    base_color_texture, uvmap_info, udim_info_bc, _ = __gather_base_color_texture(blender_material, export_settings)
    uvmap_infos.update(uvmap_info)
    udim_infos.update(udim_info_bc)
    metallic_roughness_texture, uvmap_info, udim_info_mr, _ = __gather_metallic_roughness_texture(
        blender_material, orm_texture, export_settings)
    uvmap_infos.update(uvmap_info)
    udim_infos.update(udim_info_mr)

    base_color_factor, vc_info = __gather_base_color_factor(blender_material, export_settings)

    material = gltf2_io.MaterialPBRMetallicRoughness(
        base_color_factor=base_color_factor,
        base_color_texture=base_color_texture,
        extensions=__gather_extensions(blender_material, export_settings),
        extras=__gather_extras(blender_material, export_settings),
        metallic_factor=__gather_metallic_factor(blender_material, export_settings),
        metallic_roughness_texture=metallic_roughness_texture,
        roughness_factor=__gather_roughness_factor(blender_material, export_settings)
    )

    export_user_extensions(
        'gather_material_pbr_metallic_roughness_hook',
        export_settings,
        material,
        blender_material,
        orm_texture)

    return material, uvmap_infos, vc_info, udim_infos


def __filter_pbr_material(blender_material, export_settings):
    return True


def __gather_base_color_factor(blender_material, export_settings):

    rgb, alpha = None, None
    vc_info = {"color": None, "alpha": None, "color_type": None, "alpha_type": None, "alpha_mode": "OPAQUE"}

    path_alpha = None
    path = None
    alpha_socket = get_socket(blender_material.node_tree, "Alpha")
    if alpha_socket.socket is not None and isinstance(alpha_socket.socket, bpy.types.NodeSocket):
        alpha_info = gather_alpha_info(alpha_socket.to_node_nav())
        vc_info['alpha'] = alpha_info['alphaColorAttrib']
        vc_info['alpha_type'] = alpha_info['alphaColorAttribType']
        vc_info['alpha_mode'] = alpha_info['alphaMode']
        alpha = alpha_info['alphaFactor']
        path_alpha = alpha_info['alphaPath']

    base_color_socket = get_socket(blender_material.node_tree, "Base Color")
    if base_color_socket.socket is None:
        base_color_socket = get_socket(blender_material.node_tree, "BaseColor")
    if base_color_socket.socket is None:
        base_color_socket = get_socket_from_gltf_material_node(
            blender_material.node_tree, "BaseColorFactor")
    if base_color_socket.socket is not None and isinstance(base_color_socket.socket, bpy.types.NodeSocket):
        if export_settings['gltf_image_format'] != "NONE":
            rgb_vc_info = gather_color_info(base_color_socket.to_node_nav())
            vc_info['color'] = rgb_vc_info['colorAttrib']
            vc_info['color_type'] = rgb_vc_info['colorAttribType']
            rgb = rgb_vc_info['colorFactor']
            path = rgb_vc_info['colorPath']
        else:
            rgb, path = get_const_from_default_value_socket(base_color_socket, kind='RGB')

    # Storing path for KHR_animation_pointer
    if path is not None:
        path_ = {}
        path_['length'] = 3
        path_['path'] = "/materials/XXX/pbrMetallicRoughness/baseColorFactor"
        path_['additional_path'] = path_alpha
        export_settings['current_paths'][path] = path_

    # Storing path for KHR_animation_pointer
    if path_alpha is not None:
        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/pbrMetallicRoughness/baseColorFactor"
        path_['additional_path'] = path
        export_settings['current_paths'][path_alpha] = path_

    if rgb is None:
        rgb = [1.0, 1.0, 1.0]
    if alpha is None:
        alpha = 1.0

    # Need to clamp between 0.0 and 1.0: Blender color can be outside this range
    rgb = [max(min(c, 1.0), 0.0) for c in rgb]

    rgba = [*rgb, alpha]

    if rgba == [1, 1, 1, 1]:
        return None, vc_info
    return rgba, vc_info


def __gather_base_color_texture(blender_material, export_settings):
    base_color_socket = get_socket(blender_material.node_tree, "Base Color")
    if base_color_socket.socket is None:
        base_color_socket = get_socket(blender_material.node_tree, "BaseColor")
    if base_color_socket.socket is None:
        base_color_socket = get_socket_from_gltf_material_node(
            blender_material.node_tree, "BaseColor")

    alpha_socket = get_socket(blender_material.node_tree, "Alpha")

    # keep sockets that have some texture : color and/or alpha
    inputs = tuple(
        socket for socket in [base_color_socket, alpha_socket]
        if socket.socket is not None and has_image_node_from_socket(socket, export_settings)
    )
    if not inputs:
        return None, {}, {}, None

    tex, uvmap_info, udim_info, factor = gather_texture_info(inputs[0], inputs, export_settings)

    if len(export_settings['current_texture_transform']) != 0:
        for k in export_settings['current_texture_transform'].keys():
            path_ = {}
            path_['length'] = export_settings['current_texture_transform'][k]['length']
            path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                "YYY", "pbrMetallicRoughness/baseColorTexture/extensions")
            path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
            export_settings['current_paths'][k] = path_

    export_settings['current_texture_transform'] = {}

    return tex, {
        'baseColorTexture': uvmap_info}, {
        'baseColorTexture': udim_info} if len(
            udim_info.keys()) > 0 else {}, factor


def __gather_extensions(blender_material, export_settings):
    return None


def __gather_extras(blender_material, export_settings):
    return None


def __gather_metallic_factor(blender_material, export_settings):

    metallic_socket = get_socket(blender_material.node_tree, "Metallic")
    if metallic_socket.socket is None:
        metallic_socket = get_socket_from_gltf_material_node(
            blender_material.node_tree, "MetallicFactor")
    if metallic_socket.socket is not None and isinstance(metallic_socket.socket, bpy.types.NodeSocket):
        fac, path = get_factor_from_socket(metallic_socket, kind='VALUE')

        # Storing path for KHR_animation_pointer
        if path is not None:
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/pbrMetallicRoughness/metallicFactor"
            export_settings['current_paths'][path] = path_

        return fac if fac != 1 else None

    return None


def __gather_metallic_roughness_texture(blender_material, orm_texture, export_settings):
    metallic_socket = get_socket(blender_material.node_tree, "Metallic")
    roughness_socket = get_socket(blender_material.node_tree, "Roughness")

    hasMetal = metallic_socket.socket is not None and has_image_node_from_socket(metallic_socket, export_settings)
    hasRough = roughness_socket.socket is not None and has_image_node_from_socket(roughness_socket, export_settings)

    # Warning: for default socket, do not use NodeSocket object, because it will break cache
    # Using directlty the Blender socket object
    if not hasMetal and not hasRough:
        metallic_roughness = get_socket_from_gltf_material_node(
            blender_material.node_tree, "MetallicRoughness")
        if metallic_roughness.socket is None or not has_image_node_from_socket(metallic_roughness, export_settings):
            return None, {}, {}, None
        else:
            texture_input = (metallic_roughness, metallic_roughness)
    elif not hasMetal:
        texture_input = (roughness_socket,)
    elif not hasRough:
        texture_input = (metallic_socket,)
    else:
        texture_input = (metallic_socket, roughness_socket)

    tex, uvmap_info, udim_info, factor = gather_texture_info(

        texture_input[0],
        orm_texture or texture_input,
        export_settings,
    )

    return tex, {
        'metallicRoughnessTexture': uvmap_info}, {
        'metallicRoughnessTexture': udim_info} if len(
            udim_info.keys()) > 0 else {}, factor


def __gather_roughness_factor(blender_material, export_settings):

    roughness_socket = get_socket(blender_material.node_tree, "Roughness")
    if roughness_socket is None:
        roughness_socket = get_socket_from_gltf_material_node(
            blender_material.node_tree, "RoughnessFactor")
    if roughness_socket.socket is not None and isinstance(roughness_socket.socket, bpy.types.NodeSocket):
        fac, path = get_factor_from_socket(roughness_socket, kind='VALUE')

        # Storing path for KHR_animation_pointer
        if path is not None:
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/pbrMetallicRoughness/roughnessFactor"
            export_settings['current_paths'][path] = path_

        return fac if fac != 1 else None
    return None


def get_default_pbr_for_emissive_node():
    return gltf2_io.MaterialPBRMetallicRoughness(
        base_color_factor=[0.0, 0.0, 0.0, 1.0],
        base_color_texture=None,
        extensions=None,
        extras=None,
        metallic_factor=None,
        metallic_roughness_texture=None,
        roughness_factor=None
    )
