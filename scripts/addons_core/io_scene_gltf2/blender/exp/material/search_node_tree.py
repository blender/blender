# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

#
# Imports
#

import bpy
from mathutils import Vector, Matrix
from io_scene_gltf2.blender.exp.cache import cached
from ...com.material_helpers import get_gltf_node_name, get_gltf_node_old_name, get_gltf_old_group_node_name
from ....blender.com.conversion import texture_transform_blender_to_gltf, inverted_trs_mapping_node
import typing


class Filter:
    """Base class for all node tree filter operations."""

    def __init__(self):
        pass

    def __call__(self, shader_node):
        return True


class FilterByName(Filter):
    """
    Filter the material node tree by name.

    example usage:
    find_from_socket(start_socket, ShaderNodeFilterByName("Normal"))
    """

    def __init__(self, name):
        self.name = name
        super(FilterByName, self).__init__()

    def __call__(self, shader_node):
        return shader_node.name == self.name


class FilterByType(Filter):
    """Filter the material node tree by type."""

    def __init__(self, type):
        self.type = type
        super(FilterByType, self).__init__()

    def __call__(self, shader_node):
        return isinstance(shader_node, self.type)


class NodeTreeSearchResult:
    def __init__(self,
                 shader_node: bpy.types.Node,
                 path: typing.List[bpy.types.NodeLink],
                 group_path: typing.List[bpy.types.Node]):
        self.shader_node = shader_node
        self.path = path
        self.group_path = group_path


# TODO: cache these searches
def from_socket(start_socket: NodeTreeSearchResult,
                shader_node_filter):
    """
    Find shader nodes where the filter expression is true.

    :param start_socket: the beginning of the traversal
    :param shader_node_filter: should be a function(x: shader_node) -> bool
    :return: a list of shader nodes for which filter is true
    """
    # hide implementation (especially the search path)
    def __search_from_socket(start_socket: bpy.types.NodeSocket,
                             shader_node_filter: typing.Union[Filter, typing.Callable],
                             search_path: typing.List[bpy.types.NodeLink],
                             group_path: typing.List[bpy.types.Node]) -> typing.List[NodeTreeSearchResult]:
        results = []
        for link in start_socket.links:
            # follow the link to a shader node
            linked_node = link.from_node

            if linked_node.type == "GROUP":
                group_output_node = [node for node in linked_node.node_tree.nodes if node.type == "GROUP_OUTPUT"][0]
                socket = [sock for sock in group_output_node.inputs if sock.name == link.from_socket.name][0]
                group_path.append(linked_node)
                linked_results = __search_from_socket(
                    socket, shader_node_filter, search_path + [link], group_path.copy())
                if linked_results:
                    # add the link to the current path
                    search_path.append(link)
                    results += linked_results
                continue

            if linked_node.type == "GROUP_INPUT":
                socket = [sock for sock in group_path[-1].inputs if sock.name == link.from_socket.name][0]
                linked_results = __search_from_socket(socket, shader_node_filter, search_path + [link], group_path[:-1].copy())
                if linked_results:
                    # add the link to the current path
                    search_path.append(link)
                    results += linked_results
                continue

            # check if the node matches the filter
            if shader_node_filter(linked_node):
                results.append(NodeTreeSearchResult(linked_node, search_path + [link], group_path.copy()))
            # traverse into inputs of the node
            for input_socket in linked_node.inputs:
                linked_results = __search_from_socket(
                    input_socket, shader_node_filter, search_path + [link], group_path.copy())
                if linked_results:
                    # add the link to the current path
                    search_path.append(link)
                    results += linked_results

        return results

    if start_socket.socket is None:
        return []

    # Search if direct node of the socket matches the filter
    if shader_node_filter(start_socket.socket.node):
        return [NodeTreeSearchResult(start_socket.socket.node, [], start_socket.group_path.copy())]

    return __search_from_socket(start_socket.socket, shader_node_filter, [], start_socket.group_path.copy())


@cached
def get_texture_node_from_socket(socket, export_settings):
    result = from_socket(
        socket,
        FilterByType(bpy.types.ShaderNodeTexImage))
    if not result:
        return None
    if result[0].shader_node.image is None:
        return None
    return result[0]


def has_image_node_from_socket(socket, export_settings):
    result = get_texture_node_from_socket(socket, export_settings)
    return result is not None

# return the default value of a socket, even if this socket is linked


def get_const_from_default_value_socket(socket, kind):
    if kind == 'RGB':
        if socket.socket.type != 'RGBA':
            return None
        return list(socket.socket.default_value)[:3], "node_tree." + socket.socket.path_from_id() + ".default_value"
    if kind == 'VALUE':
        if socket.socket.type != 'VALUE':
            return None
        return socket.socket.default_value, "node_tree." + socket.socket.path_from_id() + ".default_value"
    return None, None

# TODOSNode : @cached? If yes, need to use id of node tree, has this is probably not fully hashable
# For now, not caching it. If we encounter performance issue, we will see later


def get_material_nodes(node_tree: bpy.types.NodeTree, group_path, type):
    """
    For a given tree, recursively return all nodes including node groups.
    """

    nodes = []
    for node in [n for n in node_tree.nodes if isinstance(n, type) and not n.mute]:
        nodes.append((node, group_path.copy()))

    # Some weird node groups with missing datablock can have no node_tree, so checking n.node_tree (See #1797)
    for node in [n for n in node_tree.nodes if n.type == "GROUP" and n.node_tree is not None and not n.mute and n.node_tree.name !=
                 get_gltf_old_group_node_name()]:  # Do not enter the olf glTF node group
        new_group_path = group_path.copy()
        new_group_path.append(node)
        nodes.extend(get_material_nodes(node.node_tree, new_group_path, type))

    return nodes


def get_socket_from_gltf_material_node(blender_material_nodetree, name: str):
    """
    For a given material input name, retrieve the corresponding node tree socket in the special glTF node group.

    :param blender_material: a blender material for which to get the socket
    :param name: the name of the socket
    :return: a blender NodeSocket
    """
    gltf_node_group_names = [get_gltf_node_name().lower(), get_gltf_node_old_name().lower()]
    if blender_material_nodetree:
        nodes = get_material_nodes(blender_material_nodetree, [blender_material_nodetree], bpy.types.ShaderNodeGroup)
        # Some weird node groups with missing datablock can have no node_tree, so checking n.node_tree (See #1797)
        nodes = [n for n in nodes if n[0].node_tree is not None and any(
            [[n[0].node_tree.name.lower().startswith(g) for g in gltf_node_group_names]])]
        inputs = sum([[(input, node[1]) for input in node[0].inputs if input.name == name] for node in nodes], [])
        if inputs:
            return NodeSocket(inputs[0][0], inputs[0][1])

    return NodeSocket(None, None)


class NodeNav:
    """Helper for navigating through node trees."""

    def __init__(self, node, in_socket=None, out_socket=None):
        self.node = node              # Current node
        self.out_socket = out_socket  # Socket through which we arrived at this node
        self.in_socket = in_socket    # Socket through which we will leave this node
        self.stack = []      # Stack of (group node, socket) pairs descended through to get here
        self.moved = False   # Whether the last move_back call moved back or not

    def copy(self):
        new = NodeNav(self.node)
        new.assign(self)
        return new

    def assign(self, other):
        self.node = other.node
        self.in_socket = other.in_socket
        self.out_socket = other.out_socket
        self.stack = other.stack.copy()
        self.moved = other.moved

    def select_input_socket(self, in_soc):
        """Selects an input socket.

        Most operations that operate on the input socket can be passed an in_soc
        parameter to select an input socket before running.
        """
        if in_soc is None:
            # Keep current selected input socket
            return
        elif isinstance(in_soc, bpy.types.NodeSocket):
            assert in_soc.node == self.node
            self.in_socket = in_soc
        elif isinstance(in_soc, int):
            self.in_socket = self.node.inputs[in_soc]
        else:
            assert isinstance(in_soc, str)
            # An identifier like "#A_Color" selects a socket by
            # identifier. This is useful for sockets that cannot be
            # selected because of non-unique names.
            if in_soc.startswith('#'):
                ident = in_soc.removeprefix('#')
                for socket in self.node.inputs:
                    if socket.identifier == ident:
                        self.in_socket = socket
                        return
            # Select by regular name
            self.in_socket = self.node.inputs[in_soc]

    def get_out_socket_index(self):
        assert self.out_socket
        for i, soc in enumerate(self.node.outputs):
            if soc == self.out_socket:
                return i
        assert False

    def descend(self):
        """Descend into a group node."""
        if self.node and self.node.type == 'GROUP' and self.node.node_tree and self.out_socket:
            i = self.get_out_socket_index()
            self.stack.append((self.node, self.out_socket))
            self.node = next(node for node in self.node.node_tree.nodes if node.type == 'GROUP_OUTPUT')
            self.in_socket = self.node.inputs[i]
            self.out_socket = None

    def ascend(self):
        """Ascend from a group input node back to the group node."""
        if self.stack and self.node and self.node.type == 'GROUP_INPUT' and self.out_socket:
            i = self.get_out_socket_index()
            self.node, self.out_socket = self.stack.pop()
            self.in_socket = self.node.inputs[i]

    def move_back(self, in_soc=None):
        """Move backwards through an input socket to the next node."""
        self.moved = False

        self.select_input_socket(in_soc)

        if not self.in_socket or not self.in_socket.is_linked:
            return

        # Warning, slow! socket.links is O(total number of links)!
        link = self.in_socket.links[0]

        self.node = link.from_node
        self.out_socket = link.from_socket
        self.in_socket = None
        self.moved = True

        # Continue moving
        if self.node.type == 'REROUTE':
            self.move_back(0)
        elif self.node.type == 'GROUP':
            self.descend()
            self.move_back()
        elif self.node.type == 'GROUP_INPUT':
            self.ascend()
            self.move_back()

    def peek_back(self, in_soc=None):
        """Peeks backwards through an input socket without modifying self."""
        s = self.copy()
        s.select_input_socket(in_soc)
        s.move_back()
        return s

    def get_constant(self, in_soc=None):
        """Gets a constant from an input socket. Returns None if non-constant."""
        self.select_input_socket(in_soc)

        if not self.in_socket:
            return None, None

        # Get constant from unlinked socket's default value
        if not self.in_socket.is_linked:
            if self.in_socket.type == 'RGBA':
                color = list(self.in_socket.default_value)
                color = color[:3]  # drop unused alpha component (assumes shader tree)
                return color, "node_tree." + self.in_socket.path_from_id() + ".default_value"

            elif self.in_socket.type == 'SHADER':
                # Treat unlinked shader sockets as black
                return [0.0, 0.0, 0.0], None

            elif self.in_socket.type == 'VECTOR':
                return list(self.in_socket.default_value), None

            elif self.in_socket.type == 'VALUE':
                return self.in_socket.default_value, "node_tree." + self.in_socket.path_from_id() + ".default_value"

            else:
                return None, None

        # Check for a constant in the next node
        nav = self.peek_back()
        # Dev warning: because of loopbacks, this can be an infinite loop if not careful
        # Please check all branches of your if statements
        while True:
            if not nav.moved:
                break

            if self.in_socket.type == 'RGBA':

                # RGB node
                if nav.node.type == 'RGB':
                    color = list(nav.out_socket.default_value)
                    color = color[:3]  # drop unused alpha component (assumes shader tree)
                    return color, "node_tree." + nav.out_socket.path_from_id() + ".default_value"
                # Ambient Occlusion node, not linked
                elif nav.node.type == 'AMBIENT_OCCLUSION' and not nav.node.inputs['Color'].is_linked:
                    color = list(nav.node.inputs['Color'].default_value)
                    color = color[:3] # drop unused alpha component (assumes shader tree)
                    return color, "node_tree." + nav.node.inputs['Color'].path_from_id() + ".default_value"
                # Ambient Occlusion node, linked, so check the next node
                elif nav.node.type == "AMBIENT_OCCLUSION" and nav.node.inputs['Color'].is_linked:
                    nav.move_back('Color')
                    continue
                else:
                    break

            elif self.in_socket.type == 'SHADER':
                # Historicaly, we manage RGB node plugged into a shader socket (output node)
                if nav.node.type == 'RGB':
                    color = list(nav.out_socket.default_value)
                    color = color[:3]
                    return color, "node_tree." + nav.out_socket.path_from_id() + ".default_value"
                # Ambient Occlusion node, not linked
                elif nav.node.type == 'AMBIENT_OCCLUSION' and not nav.node.inputs['Color'].is_linked:
                    color = list(nav.node.inputs['Color'].default_value)
                    color = color[:3] # drop unused alpha component (assumes shader tree)
                    return color, "node_tree." + nav.node.inputs['Color'].path_from_id() + ".default_value"
                # Ambient Occlusion node, linked, so check the next node
                elif nav.node.type == "AMBIENT_OCCLUSION" and nav.node.inputs['Color'].is_linked:
                    nav.move_back('Color')
                    continue
                else:
                    break

            elif self.in_socket.type == 'VALUE':
                if nav.node.type == 'VALUE':
                    return nav.out_socket.default_value, "node_tree." + nav.out_socket.path_from_id() + ".default_value"
                else:
                    break
            else:
                break

        return None, None

    def get_factor(self, in_soc=None):
        """Gets a factor, eg. metallicFactor. Either a constant or constant multiplier."""
        self.select_input_socket(in_soc)

        if not self.in_socket:
            return None, None

        # Constant
        fac, path = self.get_constant()
        if fac is not None:
            return fac, path

        # Multiplied by constant
        nav = self.peek_back()
        if nav.moved:
            x1, x2 = None, None

            if self.in_socket.type == 'RGBA':
                is_mul = (
                    nav.node.type == 'MIX' and
                    nav.node.data_type == 'RGBA' and
                    nav.node.blend_type == 'MULTIPLY'
                )
                if is_mul:
                    # TODO: check factor is 1?
                    x1, path_1 = nav.get_constant('#A_Color')
                    x2, path_2 = nav.get_constant('#B_Color')

            elif self.in_socket.type == 'VALUE':
                if nav.node.type == 'MATH' and nav.node.operation == 'MULTIPLY':
                    x1, path_1 = nav.get_constant(0)
                    x2, path_2 = nav.get_constant(1)

            if x1 is not None and x2 is None:
                return x1, path_1
            if x2 is not None and x1 is None:
                return x2, path_2

        return None, None


# Gather information about factor and vertex color from the Color socket
# The general form for color is
#   color = factor * color attribute * texture
def gather_color_info(color_nav):
    info = {
        'colorFactor': None,
        'colorAttrib': None,
        'colorAttribType': None,
        'colorPath': None,
    }

    # Reads the factor and color attribute by checking for variations on
    # -> [Multiply by Factor] -> [Multiply by Color Attrib] ->

    for _ in range(2):  # Twice, to handle both factor and attrib
        # No factor found yet?
        if info['colorFactor'] is None:
            a, color_path = color_nav.get_constant()
            if a is not None:
                info['colorFactor'] = a[:3]
                info['colorPath'] = color_path
                break

            a, color_path = detect_multiply_by_constant(color_nav)
            if a is not None:
                info['colorFactor'] = a[:3]
                info['colorPath'] = color_path
                continue

        # No color attrib found yet?
        if info['colorAttrib'] is None:
            attr = get_color_attrib(color_nav)
            if attr is not None:
                info['colorAttrib'] = attr
                info['colorAttribType'] = 'active' if attr == "" else 'name'
                break

            attr = detect_multiply_by_color_attrib(color_nav)
            if attr is not None:
                info['colorAttrib'] = attr
                info['colorAttribType'] = 'active' if attr == "" else 'name'
                continue

        break

    return info


# Gather information about alpha from the Alpha socket. Alpha has the
# general form
#
#  alpha = alpha_clip(factor * color attribute * texture)
#
# Alpha mode is determined by the nodes too (previously it used the
# Eevee blend_method).
def gather_alpha_info(alpha_nav):
    info = {
        'alphaMode': None,
        'alphaCutoff': None,
        'alphaFactor': None,
        'alphaColorAttrib': None,
        'alphaColorAttribType': None,
        'alphaPath': None,
    }
    if not alpha_nav:
        return info

    # Opaque?
    c, alpha_path = alpha_nav.get_constant()
    if c == 1:
        info['alphaMode'] = 'OPAQUE'
        return info

    # Check for alpha clipping
    cutoff = detect_alpha_clip(alpha_nav)
    if cutoff is not None:
        info['alphaMode'] = 'MASK'
        info['alphaCutoff'] = cutoff

    # Reads the factor and color attribute by checking for variations on
    # -> [Multiply by Factor] -> [Multiply by Color Attrib Alpha] ->

    for _ in range(2):  # Twice, to handle both factor and attrib
        # No factor found yet?
        if info['alphaFactor'] is None:
            a, alpha_path = alpha_nav.get_constant()
            if a is not None:
                info['alphaFactor'] = a
                info['alphaPath'] = alpha_path
                break

            a, alpha_path = detect_multiply_by_constant(alpha_nav)
            if a is not None:
                info['alphaFactor'] = a
                info['alphaPath'] = alpha_path
                continue

        # No color attrib found yet?
        if info['alphaColorAttrib'] is None:
            attr = get_color_attrib(alpha_nav)
            if attr is not None:
                info['alphaColorAttrib'] = attr
                info['alphaColorAttribType'] = 'active' if attr == "" else 'name'
                break

            attr = detect_multiply_by_color_attrib(alpha_nav)
            if attr is not None:
                info['alphaColorAttrib'] = attr
                info['alphaColorAttribType'] = 'active' if attr == "" else 'name'
                continue

        break

    # Set alpha mode
    if info['alphaMode'] is None:
        # Is zero? Weird, but okay.
        if info['alphaFactor'] == 0:
            info['alphaMode'] = 'MASK'
            info['alphaCutoff'] = 0.5
        else:
            info['alphaMode'] = 'BLEND'

    return info


# Detects a node setup for doing alpha clip/mask, ie.
#
# alpha = alpha >= cutoff ? 1.0 : 0.0
#
# If detected, alpha_nav is advanced to point to the new Alpha socket
# and the alphaCutoff value is returned. Otherwise, returns None.
#
# Nodes will look like:
#  Alpha -> [Math:Round] -> Alpha Socket
#  Alpha -> [Math:X < cutoff] -> [Math:1 - X] -> Alpha Socket
#  Alpha -> [X > cutoff] -> Alpha Socket (Wrong, but backwards compatible with legacy)
def detect_alpha_clip(alpha_nav):
    nav = alpha_nav.peek_back()
    if not nav.moved:
        return None

    # Detect [Math:Round]
    if nav.node.type == 'MATH' and nav.node.operation == 'ROUND':
        nav.select_input_socket(0)
        alpha_nav.assign(nav)
        return 0.5

    # Detect 1 - (X < cutoff)
    # (There is no >= node)
    if nav.node.type == 'MATH' and nav.node.operation == 'SUBTRACT':
        if nav.get_constant(0)[0] == 1.0:
            nav2 = nav.peek_back(1)
            if nav2.moved and nav2.node.type == 'MATH':
                in0 = nav2.get_constant(0)[0]
                in1 = nav2.get_constant(1)[0]
                # X < cutoff
                if nav2.node.operation == 'LESS_THAN' and in0 is None and in1 is not None:
                    nav2.select_input_socket(0)
                    alpha_nav.assign(nav2)
                    return in1
                # cutoff > X
                elif nav2.node.operation == 'GREATER_THAN' and in0 is not None and in1 is None:
                    nav2.select_input_socket(1)
                    alpha_nav.assign(nav2)
                    return in0

    # Detect (X > cutoff)
    # Wrong when X = cutoff, but backwards compatible with legacy
    # Alpha Clip setup
    if nav.node.type == 'MATH':
        in0 = nav.get_constant(0)[0]
        in1 = nav.get_constant(1)[0]
        if nav.node.operation == 'GREATER_THAN' and in1 is not None:
            nav.select_input_socket(0)
            alpha_nav.assign(nav)
            return in1
        elif nav.node.operation == 'LESS_THAN' and in0 is not None:
            nav.select_input_socket(1)
            alpha_nav.assign(nav)
            return in0

    return None


# When nav connects to a multiply node (A*B), returns NodeNavs that
# point at both factors (A, B). Otherwise, returns None, None.
#
# Works for both colors and floats.
def get_multiply_factors(nav):
    prev = nav.peek_back()
    if prev.moved:
        if nav.in_socket.type == 'RGBA':
            is_mul = (
                prev.node.type == 'MIX' and
                prev.node.data_type == 'RGBA' and
                prev.node.blend_type == 'MULTIPLY' and
                prev.get_constant('Factor')[0] == 1
            )
            if is_mul:
                fac1 = prev
                fac1.select_input_socket('#A_Color')
                fac2 = prev.copy()
                fac2.select_input_socket('#B_Color')

                return fac1, fac2

        elif nav.in_socket.type == 'VALUE':
            if prev.node.type == 'MATH' and prev.node.operation == 'MULTIPLY':
                fac1 = prev
                fac1.select_input_socket(0)
                fac2 = prev.copy()
                fac2.select_input_socket(1)

                return fac1, fac2

    return None, None


# Detects if nav is multiplied by a constant. If detected, the constant
# is returned, and nav is advanced to the other factor in the
# multiplication. Otherwise, returns None.
def detect_multiply_by_constant(nav):
    fac1, fac2 = get_multiply_factors(nav)
    if fac1 is None:
        return None, None

    c, alpha_path = fac1.get_constant()
    if c is not None:
        nav.assign(fac2)
        return c, alpha_path

    # Try other order too
    c, alpha_path = fac2.get_constant()
    if c is not None:
        nav.assign(fac1)
        return c, alpha_path

    return None, None


# Detects if nav is multiplied by a color attribute. If detected, the
# color attribute name is returned, and nav is advanced to the other
# factor in the multiplication. Otherwise, returns None.
#
# Note: ignores whether the multiplication is by the attribute's RGB or
# Alpha.
def detect_multiply_by_color_attrib(nav):
    fac1, fac2 = get_multiply_factors(nav)
    if fac1 is None:
        return None

    attr = get_color_attrib(fac1)
    if attr is not None:
        nav.assign(fac2)
        return attr

    # Try other order too
    attr = get_color_attrib(fac2)
    if attr is not None:
        nav.assign(fac1)
        return attr

    return None


# Checks if nav connects to a Color Attribute node. An Attribute node is
# also accepted, but there's no check that the attribute is actually a
# *color* attribute in that case.
#
# Returns the name of the color attribute if detected. Note that a blank
# name, "", means to use the mesh's active render color attribute.
# Otherwise, returns None.
def get_color_attrib(nav):
    nav = nav.peek_back()
    if not nav.moved:
        return None

    if nav.node.type == 'VERTEX_COLOR':
        return nav.node.layer_name

    if nav.node.type == 'ATTRIBUTE':
        if nav.node.attribute_type == 'GEOMETRY':
            # Does NOT use color attribute when blank
            name = nav.node.attribute_name
            return name  # Fixed name or "" for active color

    return None


class NodeSocket:
    def __init__(self, socket, group_path):
        self.socket = socket
        self.group_path = group_path

    def to_node_nav(self):
        assert self.socket
        nav = NodeNav(
            self.socket.node,
            out_socket=self.socket if self.socket.is_output else None,
            in_socket=self.socket if not self.socket.is_output else None,
        )
        # No output socket information
        nav.stack = [(node, None) for node in self.group_path]
        return nav


class ShNode:
    def __init__(self, node, group_path):
        self.node = node
        self.group_path = group_path


def get_node_socket(blender_material_node_tree, type, name):
    """
    For a given material input name, retrieve the corresponding node tree socket for a given node type.

    :param blender_material: a blender material for which to get the socket
    :return: a blender NodeSocket for a given type
    """
    nodes = get_material_nodes(blender_material_node_tree, [blender_material_node_tree], type)
    # TODOSNode : Why checking outputs[0] ? What about alpha for texture node, that is outputs[1] ????
    nodes = [node for node in nodes if check_if_is_linked_to_active_output(node[0].outputs[0], node[1])]
    inputs = sum([[(input, node[1]) for input in node[0].inputs if input.name == name] for node in nodes], [])
    if inputs:
        return NodeSocket(inputs[0][0], inputs[0][1])
    return NodeSocket(None, None)


def get_socket(blender_material_nodetree, name: str, volume=False):
    """
    For a given material input name, retrieve the corresponding node tree socket.

    :param blender_material: a blender material for which to get the socket
    :param name: the name of the socket
    :return: a blender NodeSocket
    """
    if blender_material_nodetree:
        # i = [input for input in blender_material.node_tree.inputs]
        # o = [output for output in blender_material.node_tree.outputs]
        if name == "Emissive":
            # Check for a dedicated Emission node first, it must supersede the newer built-in one
            # because the newer one is always present in all Principled BSDF materials.
            emissive_socket = get_node_socket(blender_material_nodetree, bpy.types.ShaderNodeEmission, "Color")
            if emissive_socket.socket is not None:
                return emissive_socket
            # If a dedicated Emission node was not found, fall back to the Principled BSDF Emission Color socket.
            name = "Emission Color"
            type = bpy.types.ShaderNodeBsdfPrincipled
        elif name == "Background":
            type = bpy.types.ShaderNodeBackground
            name = "Color"
        else:
            if volume is False:
                type = bpy.types.ShaderNodeBsdfPrincipled
            else:
                type = bpy.types.ShaderNodeVolumeAbsorption

        return get_node_socket(blender_material_nodetree, type, name)

    return NodeSocket(None, None)


# Old, prefer NodeNav.get_factor in new code
def get_factor_from_socket(socket, kind):
    return socket.to_node_nav().get_factor()


# Old, prefer NodeNav.get_constant in new code
def get_const_from_socket(socket, kind):
    return socket.to_node_nav().get_constant()


def previous_socket(socket: NodeSocket):
    soc = socket.socket
    group_path = socket.group_path.copy()
    while True:
        if not soc.is_linked:
            return NodeSocket(None, None)

        from_socket = soc.links[0].from_socket

        # If we are entering a node group (from outputs)
        if from_socket.node.type == "GROUP":
            socket_name = from_socket.name
            sockets = [n for n in from_socket.node.node_tree.nodes if n.type == "GROUP_OUTPUT"][0].inputs
            socket = [s for s in sockets if s.name == socket_name][0]
            group_path.append(from_socket.node)
            soc = socket
            continue

        # If we are exiting a node group (from inputs)
        if from_socket.node.type == "GROUP_INPUT":
            socket_name = from_socket.name
            sockets = group_path[-1].inputs
            socket = [s for s in sockets if s.name == socket_name][0]
            group_path = group_path[:-1]
            soc = socket
            continue

        # Skip over reroute nodes
        if from_socket.node.type == 'REROUTE':
            soc = from_socket.node.inputs[0]
            continue

        return NodeSocket(from_socket, group_path)


def previous_node(socket: NodeSocket):
    prev_socket = previous_socket(socket)
    if prev_socket.socket is not None:
        return ShNode(prev_socket.socket.node, prev_socket.group_path)
    return ShNode(None, None)


def get_texture_transform_from_mapping_node(mapping_node, export_settings):
    if mapping_node.node.vector_type not in ["TEXTURE", "POINT", "VECTOR"]:
        export_settings['log'].warning(
            "Skipping exporting texture transform because it had type " +
            mapping_node.node.vector_type + "; recommend using POINT instead"
        )
        return None

    rotation_0, rotation_1 = mapping_node.node.inputs['Rotation'].default_value[0], mapping_node.node.inputs['Rotation'].default_value[1]
    if rotation_0 or rotation_1:
        # TODO: can we handle this?
        export_settings['log'].warning(
            "Skipping exporting texture transform because it had non-zero "
            "rotations in the X/Y direction; only a Z rotation can be exported!"
        )
        return None

    mapping_transform = {}
    if mapping_node.node.vector_type != "VECTOR":
        mapping_transform["offset"] = [
            mapping_node.node.inputs['Location'].default_value[0],
            mapping_node.node.inputs['Location'].default_value[1]]
    mapping_transform["rotation"] = mapping_node.node.inputs['Rotation'].default_value[2]
    mapping_transform["scale"] = [
        mapping_node.node.inputs['Scale'].default_value[0],
        mapping_node.node.inputs['Scale'].default_value[1]]

    if mapping_node.node.vector_type == "TEXTURE":
        mapping_transform = inverted_trs_mapping_node(mapping_transform)
        if mapping_transform is None:
            export_settings['log'].warning(
                "Skipping exporting texture transform with type TEXTURE because "
                "we couldn't convert it to TRS; recommend using POINT instead"
            )
            return None

    elif mapping_node.node.vector_type == "VECTOR":
        # Vectors don't get translated
        mapping_transform["offset"] = [0, 0]

    texture_transform = texture_transform_blender_to_gltf(mapping_transform)

    if all([component == 0 for component in texture_transform["offset"]]):
        del (texture_transform["offset"])
    if all([component == 1 for component in texture_transform["scale"]]):
        del (texture_transform["scale"])
    if texture_transform["rotation"] == 0:
        del (texture_transform["rotation"])

    # glTF Offset needs: offset, rotation, scale (note that Offset is not used for Vector mapping)
    # glTF Rotation needs: rotation
    # glTF Scale needs: scale

    if mapping_node.node.vector_type != "VECTOR":
        path_ = {}
        path_['length'] = 2
        path_['path'] = "/materials/XXX/YYY/KHR_texture_transform/offset"
        path_['vector_type'] = mapping_node.node.vector_type
        export_settings['current_texture_transform']["node_tree." +
                                                     mapping_node.node.inputs['Location'].path_from_id() + ".default_value"] = path_

    path_ = {}
    path_['length'] = 2
    path_['path'] = "/materials/XXX/YYY/KHR_texture_transform/scale"
    path_['vector_type'] = mapping_node.node.vector_type
    export_settings['current_texture_transform']["node_tree." +
                                                 mapping_node.node.inputs['Scale'].path_from_id() + ".default_value"] = path_

    path_ = {}
    path_['length'] = 1
    path_['path'] = "/materials/XXX/YYY/KHR_texture_transform/rotation"
    path_['vector_type'] = mapping_node.node.vector_type
    export_settings['current_texture_transform']["node_tree." +
                                                 mapping_node.node.inputs['Rotation'].path_from_id() + ".default_value[2]"] = path_

    return texture_transform


def check_if_is_linked_to_active_output(shader_socket, group_path):

    # Here, group_path must be copied, because if there are muliple links that enter/exit a group node
    # This will modify it, and we don't want to modify the original group_path (from the parameter) inside the loop
    for link in shader_socket.links:

        # If we are entering a node group
        if link.to_node.type == "GROUP":
            socket_name = link.to_socket.name
            sockets = [n for n in link.to_node.node_tree.nodes if n.type == "GROUP_INPUT"][0].outputs
            socket = [s for s in sockets if s.name == socket_name][0]
            new_group_path = group_path.copy()
            new_group_path.append(link.to_node)
            # TODOSNode : Why checking outputs[0] ? What about alpha for texture node, that is outputs[1] ????
            # recursive until find an output material node
            ret = check_if_is_linked_to_active_output(socket, new_group_path)
            if ret is True:
                return True
            continue

        # If we are exiting a node group
        if link.to_node.type == "GROUP_OUTPUT":
            socket_name = link.to_socket.name
            sockets = group_path[-1].outputs
            socket = [s for s in sockets if s.name == socket_name][0]
            new_group_path = group_path[:-1]
            # TODOSNode : Why checking outputs[0] ? What about alpha for texture node, that is outputs[1] ????
            # recursive until find an output material node
            ret = check_if_is_linked_to_active_output(socket, new_group_path)
            if ret is True:
                return True
            continue

        if isinstance(link.to_node, bpy.types.ShaderNodeOutputMaterial) and link.to_node.is_active_output is True:
            return True

        if len(link.to_node.outputs) > 0:  # ignore non active output, not having output sockets
            # TODOSNode : Why checking outputs[0] ? What about alpha for texture node, that is outputs[1] ????
            ret = check_if_is_linked_to_active_output(
                link.to_node.outputs[0],
                group_path)  # recursive until find an output material node
            if ret is True:
                return True

    return False

def get_attribute_name(socket, export_settings):
    node = previous_node(socket)
    if node.node is not None and node.node.type == "ATTRIBUTE" \
            and node.node.attribute_type == "GEOMETRY" \
            and node.node.attribute_name is not None \
            and node.node.attribute_name != "":
        return True, node.node.attribute_name, None
    elif node.node is not None and node.node.type == "ATTRIBUTE" \
            and node.node.attribute_type == "GEOMETRY" \
            and node.node.attribute_name == "":
        return True, None, True

    if node.node is not None and node.node.type == "VERTEX_COLOR" \
            and node.node.layer_name is not None \
            and node.node.layer_name != "":
        return True, node.node.layer_name, None
    elif node.node is not None and node.node.type == "VERTEX_COLOR" \
            and node.node.layer_name == "":
        return True, None, True

    return False, None, None


def detect_anisotropy_nodes(
        anisotropy_socket,
        anisotropy_rotation_socket,
        anisotropy_tangent_socket,
        export_settings):
    """
    Detects if the material uses anisotropy and returns the corresponding data.

    :param anisotropy_socket: the anisotropy socket
    :param anisotropy_rotation_socket: the anisotropy rotation socket
    :param anisotropy_tangent_socket: the anisotropy tangent socket
    :param export_settings: the export settings
    :return: a tuple (is_anisotropy, anisotropy_data)
    """

    if anisotropy_socket.socket is None:
        return False, None
    if anisotropy_rotation_socket.socket is None:
        return False, None
    if anisotropy_tangent_socket.socket is None:
        return False, None

    # Check that tangent is linked to a tangent node, with UVMap as input
    tangent_node = previous_node(anisotropy_tangent_socket)
    if tangent_node.node is None or tangent_node.node.type != "TANGENT":
        return False, None
    if tangent_node.node.direction_type != "UV_MAP":
        return False, None

    # Check that anisotropy is linked to a multiply node
    if not anisotropy_socket.socket.is_linked:
        return False, None
    if not anisotropy_rotation_socket.socket.is_linked:
        return False, None
    if not anisotropy_tangent_socket.socket.is_linked:
        return False, None
    anisotropy_multiply_node = anisotropy_socket.socket.links[0].from_node
    if anisotropy_multiply_node is None or anisotropy_multiply_node.type != "MATH":
        return False, None
    if anisotropy_multiply_node.operation != "MULTIPLY":
        return False, None
    # this multiply node should have the first input linked to separate XYZ, on Z
    if not anisotropy_multiply_node.inputs[0].is_linked:
        return False, None
    separate_xyz_node = anisotropy_multiply_node.inputs[0].links[0].from_node
    if separate_xyz_node is None or separate_xyz_node.type != "SEPXYZ":
        return False, None
    separate_xyz_z_socket = anisotropy_multiply_node.inputs[0].links[0].from_socket
    if separate_xyz_z_socket.name != "Z":
        return False, None
    # This separate XYZ node output should be linked to ArcTan2 node (X on inputs[1], Y on inputs[0])
    if not separate_xyz_node.outputs[0].is_linked:
        return False, None
    arctan2_node = separate_xyz_node.outputs[0].links[0].to_node
    if arctan2_node.type != "MATH":
        return False, None
    if arctan2_node.operation != "ARCTAN2":
        return False, None
    if arctan2_node.inputs[0].links[0].from_socket.name != "Y":
        return False, None
    if arctan2_node.inputs[1].links[0].from_socket.name != "X":
        return False, None
    # This arctan2 node output should be linked to anisotropy rotation (Math add node)
    if not arctan2_node.outputs[0].is_linked:
        return False, None
    anisotropy_rotation_node = arctan2_node.outputs[0].links[0].to_node
    if anisotropy_rotation_node.type != "MATH":
        return False, None
    if anisotropy_rotation_node.operation != "ADD":
        return False, None
    # This anisotropy rotation node should have the output linked to rotation conversion node
    if not anisotropy_rotation_node.outputs[0].is_linked:
        return False, None
    rotation_conversion_node = anisotropy_rotation_node.outputs[0].links[0].to_node
    if rotation_conversion_node.type != "MATH":
        return False, None
    if rotation_conversion_node.operation != "DIVIDE":
        return False, None
    # This rotation conversion node should have the second input value PI
    if abs(rotation_conversion_node.inputs[1].default_value - 6.283185) > 0.0001:
        return False, None
    # This rotation conversion node should have the output linked to anisotropy rotation socket of Principled BSDF
    if not rotation_conversion_node.outputs[0].is_linked:
        return False, None
    if rotation_conversion_node.outputs[0].links[0].to_socket.name != "Anisotropic Rotation":
        return False, None
    if rotation_conversion_node.outputs[0].links[0].to_node.type != "BSDF_PRINCIPLED":
        return False, None

    # Separate XYZ node should have the input linked to anisotropy multiply Add node (for normalization)
    if not separate_xyz_node.inputs[0].is_linked:
        return False, None
    anisotropy_multiply_add_node = separate_xyz_node.inputs[0].links[0].from_node
    if anisotropy_multiply_add_node.type != "VECT_MATH":
        return False, None
    if anisotropy_multiply_add_node.operation != "MULTIPLY_ADD":
        return False, None
    if list(anisotropy_multiply_add_node.inputs[1].default_value) != [2.0, 2.0, 1.0]:
        return False, None
    if list(anisotropy_multiply_add_node.inputs[2].default_value) != [-1.0, -1.0, 0.0]:
        return False, None
    if not anisotropy_multiply_add_node.inputs[0].is_linked:
        return False, None
    # This anisotropy multiply Add node should have the first input linked to a texture node
    anisotropy_texture_node = anisotropy_multiply_add_node.inputs[0].links[0].from_node
    if anisotropy_texture_node.type != "TEX_IMAGE":
        return False, None

    tex_ok = has_image_node_from_socket(
        NodeSocket(
            anisotropy_multiply_add_node.inputs[0],
            anisotropy_socket.group_path),
        export_settings)
    if tex_ok is False:
        return False, None

    strength, path_strength = get_const_from_socket(NodeSocket(
        anisotropy_multiply_node.inputs[1], anisotropy_socket.group_path), 'VALUE')
    rotation, path_rotation = get_const_from_socket(NodeSocket(
        anisotropy_rotation_node.inputs[1], anisotropy_socket.group_path), 'VALUE')

    return True, {
        'anisotropyStrength': (strength, path_strength),
        'anisotropyRotation': (rotation, path_rotation),
        'tangent': tangent_node.node.uv_map,
        'tex_socket': NodeSocket(anisotropy_multiply_add_node.inputs[0], anisotropy_socket.group_path),
    }
