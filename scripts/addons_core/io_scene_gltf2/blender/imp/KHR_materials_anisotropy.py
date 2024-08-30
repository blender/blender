# SPDX-FileCopyrightText: 2018-2023 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ...io.com.gltf2_io import TextureInfo
from .texture import texture
from ..com.conversion import get_anisotropy_rotation_gltf_to_blender
from math import pi
from mathutils import Vector


def anisotropy(
        mh,
        location,
        anisotropy_socket,
        anisotropy_rotation_socket,
        anisotropy_tangent_socket
):

    if anisotropy_socket is None or anisotropy_rotation_socket is None or anisotropy_tangent_socket is None:
        return

    x, y = location
    try:
        ext = mh.pymat.extensions['KHR_materials_anisotropy']
        # Needed for KHR_animation_pointer
        mh.pymat.extensions['KHR_materials_anisotropy']['blender_nodetree'] = mh.node_tree
        mh.pymat.extensions['KHR_materials_anisotropy']['blender_mat'] = mh.mat  # Needed for KHR_animation_pointer
    except Exception:
        return

    anisotropy_strength = ext.get('anisotropyStrength', 0)
    anisotropy_rotation = ext.get('anisotropyRotation', 0)
    tex_info = ext.get('anisotropyTexture')
    if tex_info is not None:
        tex_info = TextureInfo.from_dict(tex_info)

    # We are going to use UVMap of Normal map if it exists, as input for the anisotropy tangent

    if tex_info is None:
        anisotropy_socket.default_value = anisotropy_strength
        anisotropy_rotation_socket.default_value = get_anisotropy_rotation_gltf_to_blender(anisotropy_rotation)
        return

    # Tangent node
    node = mh.node_tree.nodes.new('ShaderNodeTangent')
    node.direction_type = "UV_MAP"
    node.location = x - 180, y - 200
    uv_idx = tex_info.tex_coord or 0

    # Get the UVMap of the normal map if available (if not, keeping the first UVMap available, uv_idx = 0)
    tex_info_normal = mh.pymat.normal_texture
    if tex_info_normal is not None:
        try:
            uv_idx = tex_info.extensions['KHR_texture_transform']['texCoord']
        except Exception:
            pass

    node.uv_map = 'UVMap' if uv_idx == 0 else 'UVMap.%03d' % uv_idx
    mh.node_tree.links.new(anisotropy_tangent_socket, node.outputs['Tangent'])

    # Multiply node
    multiply_node = mh.node_tree.nodes.new('ShaderNodeMath')
    multiply_node.label = 'Anisotropy strength'
    multiply_node.operation = 'MULTIPLY'
    multiply_node.location = x - 180, y + 200
    mh.node_tree.links.new(anisotropy_socket, multiply_node.outputs[0])
    multiply_node.inputs[1].default_value = anisotropy_strength

    # Divide node
    divide_node = mh.node_tree.nodes.new('ShaderNodeMath')
    divide_node.label = 'Rotation conversion'
    divide_node.operation = 'DIVIDE'
    divide_node.location = x - 180, y
    mh.node_tree.links.new(anisotropy_rotation_socket, divide_node.outputs[0])
    divide_node.inputs[1].default_value = 2 * pi

    # Rotation node
    rotation_node = mh.node_tree.nodes.new('ShaderNodeMath')
    rotation_node.label = 'Anisotropy rotation'
    rotation_node.operation = 'ADD'
    rotation_node.location = x - 180 * 2, y
    mh.node_tree.links.new(divide_node.inputs[0], rotation_node.outputs[0])
    rotation_node.inputs[1].default_value = anisotropy_rotation

    # ArcTan node
    arctan_node = mh.node_tree.nodes.new('ShaderNodeMath')
    arctan_node.label = 'ArcTan2'
    arctan_node.operation = 'ARCTAN2'
    arctan_node.location = x - 180 * 3, y
    mh.node_tree.links.new(rotation_node.inputs[0], arctan_node.outputs[0])

    # Separate XYZ
    sep_node = mh.node_tree.nodes.new('ShaderNodeSeparateXYZ')
    sep_node.location = x - 180 * 4, y
    mh.node_tree.links.new(arctan_node.inputs[0], sep_node.outputs[1])
    mh.node_tree.links.new(arctan_node.inputs[1], sep_node.outputs[0])
    mh.node_tree.links.new(multiply_node.inputs[0], sep_node.outputs[2])

    # Multiply add node
    multiply_add_node = mh.node_tree.nodes.new('ShaderNodeVectorMath')
    multiply_add_node.location = x - 180 * 5, y
    multiply_add_node.operation = 'MULTIPLY_ADD'
    multiply_add_node.inputs[1].default_value = Vector((2, 2, 1))
    multiply_add_node.inputs[2].default_value = Vector((-1, -1, 0))
    mh.node_tree.links.new(sep_node.inputs[0], multiply_add_node.outputs[0])

    # Texture
    texture(
        mh,
        tex_info=tex_info,
        label='ANISOTROPY',
        location=(x - 180 * 6, y),
        is_data=True,
        color_socket=multiply_add_node.inputs[0]
    )
