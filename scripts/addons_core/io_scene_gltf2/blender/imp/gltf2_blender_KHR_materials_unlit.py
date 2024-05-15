# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from .gltf2_blender_pbrMetallicRoughness import base_color


def unlit(mh):
    """Creates node tree for unlit materials."""
    # Emission node for the base color
    emission_node = mh.nodes.new('ShaderNodeEmission')
    emission_node.location = 10, 126

    # Create a "Lightpath trick": makes Emission visible only to
    # camera rays, so it won't "glow" in Cycles.
    #
    # [Is Camera Ray] => [Mix] =>
    #   [Transparent] => [   ]
    #      [Emission] => [   ]
    lightpath_node = mh.nodes.new('ShaderNodeLightPath')
    transparent_node = mh.nodes.new('ShaderNodeBsdfTransparent')
    mix_node = mh.nodes.new('ShaderNodeMixShader')
    lightpath_node.location = 10, 600
    transparent_node.location = 10, 240
    mix_node.location = 260, 320
    mh.links.new(mix_node.inputs['Fac'], lightpath_node.outputs['Is Camera Ray'])
    mh.links.new(mix_node.inputs[1], transparent_node.outputs[0])
    mh.links.new(mix_node.inputs[2], emission_node.outputs[0])

    # Material output
    alpha_socket = None
    out_node = mh.nodes.new('ShaderNodeOutputMaterial')
    if mh.is_opaque():
        out_node.location = 490, 290
        mh.links.new(out_node.inputs[0], mix_node.outputs[0])
    else:
        # Create a "Mix with Transparent" setup so there's a
        # place to put Alpha.
        #
        #         Alpha => [Mix] => [Output]
        # [Transparent] => [   ]
        #         Color => [   ]
        mix2_node = mh.nodes.new('ShaderNodeMixShader')
        alpha_socket = mix2_node.inputs['Fac']
        mix2_node.location = 490, -50
        out_node.location = 700, -70
        mh.links.new(mix2_node.inputs[1], transparent_node.outputs[0])
        mh.links.new(mix2_node.inputs[2], mix_node.outputs[0])
        mh.links.new(out_node.inputs[0], mix2_node.outputs[0])

    base_color(
        mh,
        location=(-200, 380),
        color_socket=emission_node.inputs['Color'],
        alpha_socket=alpha_socket,
    )
