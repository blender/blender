# SPDX-FileCopyrightText: 2018-2026 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ...io.com.gltf2_io import TextureInfo
from ...io.com.constants import GLTF_IRIDESCENCE_IOR
from .material_utils import scalar_factor_and_texture
from .texture import texture


def iridescence(
    mh,
    locs,
    iridescence_factor_socket,
    iridescence_ior_socket,
    iridescence_thickness_maximum_socket,
    iridescence_thickness_minimum_socket,
):

    if iridescence_factor_socket is None or iridescence_thickness_maximum_socket is None or iridescence_thickness_minimum_socket is None:
        return

    x, y = locs['iridescence']
    try:
        ext = mh.pymat.extensions['KHR_materials_iridescence']
        # Needed for KHR_animation_pointer
        mh.pymat.extensions['KHR_materials_iridescence']['blender_nodetree'] = mh.node_tree
        mh.pymat.extensions['KHR_materials_iridescence']['blender_mat'] = mh.mat  # Needed for KHR_animation_pointer
    except Exception:
        return

    iridescence_factor = ext.get('iridescenceFactor', 0.0)
    iridescence_thickness_maximum = ext.get('iridescenceThickness', 400.0)
    iridescence_thickness_minimum = ext.get('iridescenceThicknessMinimum', 100.0)
    iridescence_ior = ext.get('iridescenceIor', GLTF_IRIDESCENCE_IOR)

    tex_info = ext.get('iridescenceTexture')
    thickness_tex_info = ext.get('iridescenceThicknessTexture')

    if tex_info is not None:
        tex_info = TextureInfo.from_dict(tex_info)
    if thickness_tex_info is not None:
        thickness_tex_info = TextureInfo.from_dict(thickness_tex_info)

    if tex_info is None and thickness_tex_info is None:
        iridescence_factor_socket.default_value = iridescence_factor
        iridescence_thickness_maximum_socket.default_value = iridescence_thickness_maximum
        iridescence_thickness_minimum_socket.default_value = iridescence_thickness_minimum
        iridescence_ior_socket.default_value = iridescence_ior
        return

    if tex_info is not None:

        # We will need iridescence factor (Mix node) if animated by KHR_animation_pointer (and standard case if iridescenceFactor != 0)
        # Check if animated by KHR_animation_pointer
        force_iridescence_factor = False
        if mh.gltf.data.extensions_used is not None and "KHR_animation_pointer" in mh.gltf.data.extensions_used:
            if mh.pymat.extensions and "KHR_materials_iridescence" in mh.pymat.extensions and len(
                    mh.pymat.extensions["KHR_materials_iridescence"]["animations"]) > 0:
                for anim_idx in mh.pymat.extensions["KHR_materials_iridescence"]["animations"].keys():
                    for channel_idx in mh.pymat.extensions["KHR_materials_iridescence"]["animations"][anim_idx]:
                        channel = mh.gltf.data.animations[anim_idx].channels[channel_idx]
                        pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                                pointer_tab[3] == "extensions" and \
                                pointer_tab[4] == "KHR_materials_iridescence" and \
                                pointer_tab[5] == "iridescenceFactor":
                            force_iridescence_factor = True

        scalar_factor_and_texture(
            mh,
            location=locs['iridescence'],
            label='Iridescence',
            socket=iridescence_factor_socket,
            factor=iridescence_factor,
            tex_info=tex_info,
            channel=0,  # Red
            force_mix_node=force_iridescence_factor
        )

    if thickness_tex_info is not None:
        # We need to construct a node setup

        # Mix node
        mix_node = mh.node_tree.nodes.new('ShaderNodeMix')
        mix_node.label = 'Iridescence Thickness'
        mix_node.location = x - 180, y - 300
        mh.node_tree.links.new(iridescence_thickness_maximum_socket, mix_node.outputs[0])

        # Separate RGB node
        separate_node = mh.node_tree.nodes.new('ShaderNodeSeparateColor')
        separate_node.location = x - 180 * 2, y - 300
        mh.node_tree.links.new(separate_node.outputs[1], mix_node.inputs[0])  # Factor is in the green channel

        # Texture node
        texture(
            mh,
            tex_info=tex_info,
            label='IRIDESCENCE THICKNESS',
            location=(x - 180 * 3, y - 600),
            is_data=True,
            color_socket=separate_node.inputs[0]
        )

        # Value node (minimum thickness)
        value_node = mh.node_tree.nodes.new('ShaderNodeValue')
        value_node.label = 'Iridescence Thickness Minimum'
        value_node.location = x - 180 * 2, y - 500
        value_node.outputs[0].default_value = iridescence_thickness_minimum
        mh.node_tree.links.new(mix_node.inputs[2], value_node.outputs[0])
        mh.node_tree.links.new(iridescence_thickness_minimum_socket, value_node.outputs[0])

        # Value node (maximum thickness)
        value_node_max = mh.node_tree.nodes.new('ShaderNodeValue')
        value_node_max.label = 'Iridescence Thickness Maximum'
        value_node_max.location = x - 180 * 2, y - 600
        value_node_max.outputs[0].default_value = iridescence_thickness_maximum
        mh.node_tree.links.new(mix_node.inputs[3], value_node_max.outputs[0])
