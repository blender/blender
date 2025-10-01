# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from . import texture_info as gltf2_blender_gather_texture_info
from .search_node_tree import \
    get_socket, \
    NodeSocket, \
    previous_socket, \
    previous_node, \
    get_factor_from_socket, \
    gather_alpha_info,  \
    gather_color_info


def detect_shadeless_material(blender_material_node_tree, export_settings):
    """Detect if this material is "shadeless" ie. should be exported
    with KHR_materials_unlit. Returns None if not. Otherwise, returns
    a dict with info from parsing the node tree.
    """

    # Old Background node detection (unlikely to happen)
    bg_socket = get_socket(blender_material_node_tree, "Background")
    if bg_socket.socket is not None:
        return {'rgb_socket': bg_socket}

    # Look for
    # * any color socket, connected to...
    # * optionally, the lightpath trick, connected to...
    # * optionally, a mix-with-transparent (for alpha), connected to...
    # * the output node

    info = {}

    # TODOSNode this can be a function call
    for node in blender_material_node_tree.nodes:
        if node.type == 'OUTPUT_MATERIAL' and node.is_active_output:
            socket = node.inputs[0]
            break
    else:
        return None

    socket = NodeSocket(socket, [blender_material_node_tree])

    # Be careful not to misidentify a lightpath trick as mix-alpha.
    result = __detect_lightpath_trick(socket)
    if result is not None:
        socket = result['next_socket']
    else:
        result = __detect_mix_alpha(socket)
        if result is not None:
            socket = result['next_socket']
            info['alpha_socket'] = result['alpha_socket']

        result = __detect_lightpath_trick(socket)
        if result is not None:
            socket = result['next_socket']

    # Check if a color socket, or connected to a color socket
    if socket.socket.type != 'RGBA':
        from_socket = previous_socket(socket)
        if from_socket.socket is None:
            return None
        if from_socket.socket.type != 'RGBA':
            return None

    info['rgb_socket'] = socket
    return info


def __detect_mix_alpha(socket):
    # Detects this (used for an alpha hookup)
    #
    #                  [   Mix   ]
    #  alpha_socket => [Factor   ] => socket
    # [Transparent] => [Shader   ]
    #   next_socket => [Shader   ]
    #
    # Returns None if not detected. Otherwise, a dict containing alpha_socket
    # and next_socket.
    prev = previous_node(socket)
    if prev.node is None or prev.node.type != 'MIX_SHADER':
        return None
    in1 = previous_node(NodeSocket(prev.node.inputs[1], prev.group_path))
    if in1.node is None or in1.node.type != 'BSDF_TRANSPARENT':
        return None
    return {
        'alpha_socket': NodeSocket(prev.node.inputs[0], prev.group_path),
        'next_socket': NodeSocket(prev.node.inputs[2], prev.group_path),
    }


def __detect_lightpath_trick(socket):
    # Detects this (used to prevent casting light on other objects) See ex.
    # https://blender.stackexchange.com/a/21535/88681
    #
    #                 [   Lightpath  ]    [    Mix    ]
    #                 [ Is Camera Ray] => [Factor     ] => socket
    #                     (don't care) => [Shader     ]
    #      next_socket => [ Emission ] => [Shader     ]
    #
    # The Emission node can be omitted.
    # Returns None if not detected. Otherwise, a dict containing
    # next_socket.
    prev = previous_node(socket)
    if prev.node is None or prev.node.type != 'MIX_SHADER':
        return None
    in0 = previous_socket(NodeSocket(prev.node.inputs[0], prev.group_path))
    if in0.socket is None or in0.socket.node.type != 'LIGHT_PATH':
        return None
    if in0.socket.name != 'Is Camera Ray':
        return None
    next_socket = NodeSocket(prev.node.inputs[2], prev.group_path)

    # Detect emission
    prev = previous_node(next_socket)
    if prev.node is not None and prev.node.type == 'EMISSION':
        next_socket = NodeSocket(prev.node.inputs[0], prev.group_path)

    return {'next_socket': next_socket}


def gather_base_color_factor(info, export_settings):
    rgb, alpha = None, None
    path, path_alpha = None, None
    vc_info = {"color": None, "alpha": None, "color_type": None, "alpha_type": None, "alpha_mode": "OPAQUE"}


    if 'rgb_socket' in info:
        rgb_vc_info = gather_color_info(info['rgb_socket'].to_node_nav())
        vc_info['color'] = rgb_vc_info['colorAttrib']
        vc_info['color_type'] = rgb_vc_info['colorAttribType']
        rgb = rgb_vc_info['colorFactor']
        path = rgb_vc_info['colorPath']
    if 'alpha_socket' in info:
        alpha_info = gather_alpha_info(info['alpha_socket'].to_node_nav())
        alpha = alpha_info['alphaFactor']
        path_alpha = alpha_info['alphaPath']
        vc_info['alpha'] = alpha_info['alphaColorAttrib']
        vc_info['alpha_type'] = alpha_info['alphaColorAttribType']
        vc_info['alpha_mode'] = alpha_info['alphaMode']

    # Storing path for KHR_animation_pointer
    if path is not None:
        path_ = {}
        path_['length'] = 3
        path_['path'] = "/materials/XXX/pbrMetallicRoughness/baseColorFactor"
        path_['additional_path'] = path_alpha
        export_settings['current_paths'][path] = path_

    if rgb is None:
        rgb = [1.0, 1.0, 1.0]
    if alpha is None:
        alpha = 1.0

    rgba = [*rgb, alpha]
    if rgba == [1, 1, 1, 1]:
        return None, vc_info
    return rgba, vc_info


def gather_base_color_texture(info, export_settings):
    sockets = (info.get('rgb_socket', NodeSocket(None, None)), info.get('alpha_socket', NodeSocket(None, None)))
    sockets = tuple(s for s in sockets if s.socket is not None)
    if sockets:
        # NOTE: separate RGB and Alpha textures will not get combined
        # because gather_image determines how to pack images based on the
        # names of sockets, and the names are hard-coded to a Principled
        # style graph.
        unlit_texture, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_texture_info(
            sockets[0],
            sockets,
            export_settings,
        )

        if len(export_settings['current_texture_transform']) != 0:
            for k in export_settings['current_texture_transform'].keys():
                path_ = {}
                path_['length'] = export_settings['current_texture_transform'][k]['length']
                path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                    "YYY", "pbrMetallicRoughness/baseColorTexture/extensions")
                path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
                export_settings['current_paths'][k] = path_

        export_settings['current_texture_transform'] = {}

        return unlit_texture, {
            'baseColorTexture': uvmap_info}, {
            'baseColorTexture': udim_info} if len(
            udim_info.keys()) > 0 else {}
    return None, {}, {}
