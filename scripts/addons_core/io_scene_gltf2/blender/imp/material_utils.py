# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ...io.com.gltf2_io import (
    TextureInfo,
    MaterialNormalTextureInfoClass,
    MaterialPBRMetallicRoughness,
)
from .texture import texture


class MaterialHelper:
    """Helper class. Stores material stuff to be passed around everywhere."""

    def __init__(self, gltf, pymat, mat, vertex_color):
        self.gltf = gltf
        self.pymat = pymat
        self.mat = mat
        self.node_tree = mat.node_tree
        self.nodes = mat.node_tree.nodes
        self.links = mat.node_tree.links
        self.vertex_color = vertex_color
        if pymat.pbr_metallic_roughness is None:
            pymat.pbr_metallic_roughness = \
                MaterialPBRMetallicRoughness.from_dict({})
            # We need to initialize the animations array, for KHR_animation_pointer
            pymat.pbr_metallic_roughness.animations = []
        self.settings_node = None

    def is_opaque(self):
        alpha_mode = self.pymat.alpha_mode
        return alpha_mode is None or alpha_mode == 'OPAQUE'

    def needs_emissive(self):
        return (
            self.pymat.emissive_texture is not None or
            (self.pymat.emissive_factor or [0, 0, 0]) != [0, 0, 0]
        )

    def get_ext(self, ext_name, default=None):
        if not self.pymat.extensions:
            return default
        return self.pymat.extensions.get(ext_name, default)


# Creates nodes for multiplying a texture channel and scalar factor.
# [Texture] => [Sep RGB] => [Mul Factor] => socket
def scalar_factor_and_texture(
    mh: MaterialHelper,
    location,
    label,
    socket,                 # socket to connect to
    factor,                 # scalar factor
    tex_info,               # texture
    channel,                # texture channel to use (0-4)
    force_mix_node=False,   # Needed for KHR_animation_pointer
):
    if isinstance(tex_info, dict):
        tex_info = TextureInfo.from_dict(tex_info)

    x, y = location

    if socket is None:
        return

    if tex_info is None:
        socket.default_value = factor
        return

    if factor != 1.0 or force_mix_node:
        node = mh.nodes.new('ShaderNodeMath')
        node.label = f'{label} Factor'
        node.location = x - 140, y
        node.operation = 'MULTIPLY'
        # Outputs
        mh.links.new(socket, node.outputs[0])
        # Inputs
        socket = node.inputs[0]
        node.inputs[1].default_value = factor

        x -= 200

    if channel != 4:
        # Separate RGB
        node = mh.nodes.new('ShaderNodeSeparateColor')
        node.location = x - 150, y - 75
        # Outputs
        mh.links.new(socket, node.outputs[channel])
        # Inputs
        socket = node.inputs[0]

        x -= 200

    texture(
        mh,
        tex_info=tex_info,
        label=label.upper(),
        location=(x, y),
        is_data=channel < 4,
        color_socket=socket if channel != 4 else None,
        alpha_socket=socket if channel == 4 else None,
    )


# Creates nodes for multiplying a texture color and color factor.
# [Texture] => [Mix Factor] => socket
def color_factor_and_texture(
    mh: MaterialHelper,
    location,
    label,
    socket,                # socket to connect to
    factor,                # color factor
    tex_info,              # texture
    force_mix_node=False,  # Needed for KHR_animation_pointer
):
    if isinstance(tex_info, dict):
        tex_info = TextureInfo.from_dict(tex_info)

    x, y = location

    if socket is None:
        return

    if tex_info is None:
        socket.default_value = [*factor, 1]
        return

    if factor != [1, 1, 1] or force_mix_node:
        node = mh.nodes.new('ShaderNodeMix')
        node.data_type = 'RGBA'
        node.label = f'{label} Factor'
        node.location = x - 140, y
        node.blend_type = 'MULTIPLY'
        # Outputs
        mh.links.new(socket, node.outputs[2])
        # Inputs
        node.inputs['Factor'].default_value = 1
        socket = node.inputs[6]
        node.inputs[7].default_value = [*factor, 1]

        x -= 200

    texture(
        mh,
        tex_info=tex_info,
        label=label.upper(),
        location=(x, y),
        is_data=False,
        color_socket=socket,
    )


# [Texture] => [Normal Map] => socket
def normal_map(
    mh: MaterialHelper,
    location,
    label,
    socket,
    tex_info,
):
    if isinstance(tex_info, dict):
        tex_info = MaterialNormalTextureInfoClass.from_dict(tex_info)

    if not tex_info:
        return

    x, y = location

    # Normal map
    node = mh.nodes.new('ShaderNodeNormalMap')
    node.location = x - 150, y - 40
    # Set UVMap
    uv_idx = tex_info.tex_coord or 0
    try:
        uv_idx = tex_info.extensions['KHR_texture_transform']['texCoord']
    except Exception:
        pass
    node.uv_map = 'UVMap' if uv_idx == 0 else 'UVMap.%03d' % uv_idx
    # Set strength
    scale = tex_info.scale
    scale = scale if scale is not None else 1
    node.inputs['Strength'].default_value = scale
    # Outputs
    mh.links.new(socket, node.outputs['Normal'])

    x -= 200

    texture(
        mh,
        tex_info=tex_info,
        label=label.upper(),
        location=(x, y),
        is_data=True,
        color_socket=node.inputs['Color'],
    )
