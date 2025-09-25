# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from ...io.com import gltf2_io
from ...io.exp.user_extensions import export_user_extensions
from ...io.com.constants import TextureFilter, TextureWrap
from .cache import cached
from .material.search_node_tree import previous_node, previous_socket, get_const_from_socket, NodeSocket


@cached
def gather_sampler(blender_shader_node: bpy.types.Node, group_path_str, export_settings):
    # reconstruct group_path from group_path_str
    sep_item = "##~~gltf-sep~~##"
    sep_inside_item = "##~~gltf-inside-sep~~##"
    group_path = []
    tab = group_path_str.split(sep_item)
    if len(tab) > 0:
        group_path.append(bpy.data.materials[tab[0]])
    for idx, i in enumerate(tab[1:]):
        subtab = i.split(sep_inside_item)
        if idx == 0:
            group_path.append(bpy.data.materials[tab[0]].node_tree.nodes[subtab[1]])
        else:
            group_path.append(bpy.data.node_groups[subtab[0]].nodes[subtab[1]])

    wrap_s, wrap_t = __gather_wrap(blender_shader_node, group_path, export_settings)

    sampler = gltf2_io.Sampler(
        extensions=__gather_extensions(blender_shader_node, export_settings),
        extras=__gather_extras(blender_shader_node, export_settings),
        mag_filter=__gather_mag_filter(blender_shader_node, export_settings),
        min_filter=__gather_min_filter(blender_shader_node, export_settings),
        name=__gather_name(blender_shader_node, export_settings),
        wrap_s=wrap_s,
        wrap_t=wrap_t,
    )

    export_user_extensions('gather_sampler_hook', export_settings, sampler, blender_shader_node)

    if not sampler.extensions and not sampler.extras and not sampler.name:
        return __sampler_by_value(
            sampler.mag_filter,
            sampler.min_filter,
            sampler.wrap_s,
            sampler.wrap_t,
            export_settings,
        )

    return sampler


@cached
def __sampler_by_value(mag_filter, min_filter, wrap_s, wrap_t, export_settings):
    # @cached function to dedupe samplers with the same settings.
    return gltf2_io.Sampler(
        extensions=None,
        extras=None,
        mag_filter=mag_filter,
        min_filter=min_filter,
        name=None,
        wrap_s=wrap_s,
        wrap_t=wrap_t,
    )


def __gather_extensions(blender_shader_node, export_settings):
    return None


def __gather_extras(blender_shader_node, export_settings):
    return None


def __gather_mag_filter(blender_shader_node, export_settings):
    if blender_shader_node.interpolation == 'Closest':
        return TextureFilter.Nearest
    return TextureFilter.Linear


def __gather_min_filter(blender_shader_node, export_settings):
    if blender_shader_node.interpolation == 'Closest':
        return TextureFilter.NearestMipmapNearest
    return TextureFilter.LinearMipmapLinear


def __gather_name(blender_shader_node, export_settings):
    return None


def __gather_wrap(blender_shader_node, group_path, export_settings):
    # First gather from the Texture node
    if blender_shader_node.extension == 'EXTEND':
        wrap_s = TextureWrap.ClampToEdge
    elif blender_shader_node.extension == 'CLIP':
        # Not possible in glTF, but ClampToEdge is closest
        wrap_s = TextureWrap.ClampToEdge
    elif blender_shader_node.extension == 'MIRROR':
        wrap_s = TextureWrap.MirroredRepeat
    else:
        wrap_s = TextureWrap.Repeat
    wrap_t = wrap_s

    # Starting Blender 3.5, MIRROR is now an extension of image node
    # So this manual uv wrapping trick is no more useful for MIRROR x MIRROR
    # But still works for old files
    # Still needed for heterogen heterogeneous sampler, like MIRROR x REPEAT, for example
    # Take manual wrapping into account
    result = detect_manual_uv_wrapping(blender_shader_node, group_path)
    if result:
        if result['wrap_s'] is not None:
            wrap_s = result['wrap_s']
        if result['wrap_t'] is not None:
            wrap_t = result['wrap_t']

    # Omit if both are repeat
    if (wrap_s, wrap_t) == (TextureWrap.Repeat, TextureWrap.Repeat):
        wrap_s, wrap_t = None, None

    return wrap_s, wrap_t


def detect_manual_uv_wrapping(blender_shader_node, group_path):
    # Detects UV wrapping done using math nodes. This is for emulating wrap
    # modes Blender doesn't support. It looks like
    #
    #     next_socket => [Sep XYZ] => [Wrap S] => [Comb XYZ] => blender_shader_node
    #                              => [Wrap T] =>
    #
    # The [Wrap _] blocks are either math nodes (eg. PINGPONG for mirrored
    # repeat), or can be omitted.
    #
    # Returns None if not detected. Otherwise a dict containing the wrap
    # mode in each direction (or None), and next_socket.
    result = {}

    comb = previous_node(NodeSocket(blender_shader_node.inputs['Vector'], group_path))
    if comb.node is None or comb.node.type != 'COMBXYZ':
        return None

    for soc in ['X', 'Y']:
        node = previous_node(NodeSocket(comb.node.inputs[soc], comb.group_path))
        if node.node is None:
            return None

        if node.node.type == 'SEPXYZ':
            # Passed through without change
            wrap = None
            prev_socket = previous_socket(NodeSocket(comb.node.inputs[soc], comb.group_path))
        elif node.node.type == 'MATH':
            # Math node applies a manual wrap
            if (node.node.operation == 'PINGPONG' and get_const_from_socket(NodeSocket(
                    node.node.inputs[1], node.group_path), kind='VALUE')[0] == 1.0):  # scale = 1
                wrap = TextureWrap.MirroredRepeat
            elif (node.node.operation == 'WRAP' and
                    # min = 0
                    get_const_from_socket(NodeSocket(node.node.inputs[1], node.group_path), kind='VALUE')[0] == 0.0 and
                    get_const_from_socket(NodeSocket(node.node.inputs[2], node.group_path), kind='VALUE')[0] == 1.0):    # max = 1
                wrap = TextureWrap.Repeat
            else:
                return None

            prev_socket = previous_socket(NodeSocket(node.node.inputs[0], node.group_path))
        else:
            return None

        if prev_socket.socket is None:
            return None
        prev_node = prev_socket.socket.node
        if prev_node.type != 'SEPXYZ':
            return None
        # Make sure X goes to X, etc.
        if prev_socket.socket.name != soc:
            return None
        # Make sure both attach to the same SeparateXYZ node
        if soc == 'X':
            sep = prev_node
        else:
            if sep != prev_node:
                return None

        result['wrap_s' if soc == 'X' else 'wrap_t'] = wrap

    result['next_socket'] = NodeSocket(sep.inputs[0], prev_socket.group_path)
    return result
