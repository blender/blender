# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

#
# Imports
#

import bpy
from io_scene_gltf2.blender.exp.cache import cached, cached_by_key
from ....blender.com.conversion import texture_transform_blender_to_gltf, inverted_trs_mapping_node
import typing


class Filter:
    """Base class for all node tree filter operations."""

    def __init__(self):
        pass

    def __call__(self, shader_node):
        return True

    def __name__(self):
        return self.__class__.__name__


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

    def __name__(self):
        return "FilterByName(" + self.name + ")"


class FilterByType(Filter):
    """Filter the material node tree by type."""

    def __init__(self, type):
        self.type = type
        super(FilterByType, self).__init__()

    def __call__(self, shader_node):
        return isinstance(shader_node, self.type)

    def __name__(self):
        return "FilterByType(" + self.type.__name__ + ")"


class NodeTreeSearchResult:
    def __init__(self,
                 shader_node: bpy.types.Node,
                 path: typing.List[bpy.types.NodeLink],
                 group_path: typing.List[bpy.types.Node]):
        self.shader_node = shader_node
        self.path = path
        self.group_path = group_path


class NodeTreeSearcher:
    """Helper for searching through node trees."""

    @classmethod
    def from_socket(cls, start_socket, filter, export_settings):

        if start_socket.socket is None:
            return []

        # Search if direct node of the socket matches the filter
        if filter(start_socket.socket.node):
            return [NodeTreeSearchResult(start_socket.socket.node, [], start_socket.group_path.copy())]

        return cls.__search_from_socket(
            start_socket.socket,
            start_socket.group_path.copy(),
            [],
            filter,
            export_settings)

    @classmethod
    @cached_by_key(key=lambda cls, start_socket, group_path, search_path, filter,
                   export_settings: (start_socket.as_pointer(), tuple(id(g) for g in group_path), filter.__name__))
    def __search_from_socket(cls, start_socket, group_path, search_path, filter, export_settings):
        def __get_socket_index(sockets, socket):
            for i, soc in enumerate(sockets):
                if soc == socket:
                    return i
            assert False

        results = []
        for link in start_socket.links:  # TODO perf ?
            # follow the link to a shader node
            linked_node = link.from_node

            if linked_node.type == "GROUP":
                group_output_node = [node for node in linked_node.node_tree.nodes if node.type == "GROUP_OUTPUT"][0]
                i = __get_socket_index(linked_node.outputs, link.from_socket)
                socket = group_output_node.inputs[i]
                new_group_path = group_path.copy()
                new_group_path.append(linked_node)
                linked_results = cls.__search_from_socket(
                    socket, new_group_path, search_path + [link], filter, export_settings)
                if linked_results:
                    search_path.append(link)
                    results += linked_results
                continue

            if linked_node.type == "GROUP_INPUT":
                i = __get_socket_index(linked_node.outputs, link.from_socket)
                socket = group_path[-1].inputs[i]
                linked_results = cls.__search_from_socket(
                    socket, group_path[:-1].copy(), search_path + [link], filter, export_settings)
                if linked_results:
                    search_path.append(link)
                    results += linked_results
                continue

            # check if the node matches the filter
            if filter(linked_node):
                results.append(NodeTreeSearchResult(linked_node, search_path + [link], group_path.copy()))
            # traverse into inputs of the node
            for input_socket in linked_node.inputs:
                linked_results = cls.__search_from_socket(
                    input_socket, group_path.copy(), search_path + [link], filter, export_settings)
                if linked_results:
                    search_path.append(link)
                    results += linked_results

        return results


@cached
def get_texture_node_from_socket(socket, export_settings):

    result = NodeTreeSearcher.from_socket(
        socket,
        FilterByType(bpy.types.ShaderNodeTexImage),
        export_settings)
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


class NodeNav:
    """Helper for navigating through node trees."""

    def __init__(self, node, in_socket=None, out_socket=None):
        self.node = node              # Current node
        self.out_socket = out_socket  # Socket through which we arrived at this node (when going backwards)
        self.in_socket = in_socket    # Socket through which we will leave this node (when going backwards)
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

    def select_output_socket(self, out_soc):
        """Selects an output socket.

        Most operations that operate on the output socket can be passed an out_soc
        parameter to select an output socket before running.
        """
        if out_soc is None:
            # Keep current selected output socket
            return
        elif isinstance(out_soc, bpy.types.NodeSocket):
            assert out_soc.node == self.node
            self.out_socket = out_soc
        elif isinstance(out_soc, int):
            self.out_socket = self.node.outputs[out_soc]
        else:
            assert isinstance(out_soc, str)
            # An identifier like "#A_Color" selects a socket by
            # identifier. This is useful for sockets that cannot be
            # selected because of non-unique names.
            if out_soc.startswith('#'):
                ident = out_soc.removeprefix('#')
                for socket in self.node.outputs:
                    if socket.identifier == ident:
                        self.out_socket = socket
                        return
            # Select by regular name
            self.out_socket = self.node.outputs[out_soc]

    def get_out_socket_index(self):
        assert self.out_socket
        for i, soc in enumerate(self.node.outputs):
            if soc == self.out_socket:
                return i
        assert False

    def get_in_socket_index(self):
        assert self.in_socket
        for i, soc in enumerate(self.node.inputs):
            if soc == self.in_socket:
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

    def descend_forward(self):
        if self.node and self.node.type == 'GROUP' and self.node.node_tree and self.in_socket:
            i = self.get_in_socket_index()
            self.stack.append((self.node, self.in_socket))
            self.node = next(node for node in self.node.node_tree.nodes if node.type == 'GROUP_INPUT')
            self.out_socket = self.node.outputs[i]
            self.in_socket = None

    def ascend(self):
        """Ascend from a group input node back to the group node."""
        if self.stack and self.node and self.node.type == 'GROUP_INPUT' and self.out_socket:
            i = self.get_out_socket_index()
            self.node, self.out_socket = self.stack.pop()
            self.in_socket = self.node.inputs[i]

    def ascend_forward(self):
        """Ascend from a group output node back to the group node."""
        if self.stack and self.node and self.node.type == 'GROUP_OUTPUT' and self.in_socket:
            i = self.get_in_socket_index()
            self.node, self.in_socket = self.stack.pop()
            self.out_socket = self.node.outputs[i]

    def move_forward(self, out_soc=None):
        """Move forwards through an output socket to the next node."""
        self.select_output_socket(out_soc)

        if not self.out_socket or not self.out_socket.is_linked:
            return

        # Warning, slow! socket.links is O(total number of links)!
        link = self.out_socket.links[0]

        self.node = link.to_node
        self.in_socket = link.to_socket
        self.out_socket = None
        self.moved = True

        # Continue moving
        if self.node.type == 'REROUTE':
            self.move_forward(0)
        elif self.node.type == 'GROUP':
            self.descend_forward()
            self.move_forward()
        elif self.node.type == 'GROUP_OUTPUT':
            self.ascend_forward()
            self.move_forward()

    def move_back(self, in_soc=None, no_moved_reinit=False):
        """Move backwards through an input socket to the next node."""
        if not no_moved_reinit:
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
            # Manage special case where we ascend from a group input node
            # But the node group socket is not linked
            # We need to avoid reinitializing moved to False in this case
            self.move_back(no_moved_reinit=True)

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
                    color = color[:3]  # drop unused alpha component (assumes shader tree)
                    return color, "node_tree." + nav.node.inputs['Color'].path_from_id() + ".default_value"
                # Ambient Occlusion node, linked, so check the next node
                elif nav.node.type == "AMBIENT_OCCLUSION" and nav.node.inputs['Color'].is_linked:
                    nav.move_back('Color')
                    continue
                elif nav.node.type == "GROUP":
                    # Special case: unlinked group input node
                    color = list(nav.in_socket.default_value)
                    color = color[:3]  # drop unused alpha component (assumes shader tree)
                    return color, "node_tree." + nav.in_socket.path_from_id() + ".default_value"
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
                    color = color[:3]  # drop unused alpha component (assumes shader tree)
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
                elif nav.node.type == "GROUP":
                    # Special case: unlinked group input node
                    return nav.in_socket.default_value, "node_tree." + nav.in_socket.path_from_id() + ".default_value"
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
        'alphaCutoffPath': None,
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
        info['alphaPath'] = alpha_path  # Maybe the alpha is animated, this will be managed later
        return info

    # Check for alpha clipping
    cutoff, cutoff_path = detect_alpha_clip(alpha_nav)
    if cutoff is not None:
        info['alphaMode'] = 'MASK'
        info['alphaCutoff'] = cutoff
        info['alphaCutoffPath'] = cutoff_path

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
            # In case alpha is animated, and started with zero, we need to overwrite it later...
        elif info['alphaFactor'] == 1.0:
            info['alphaMode'] = 'OPAQUE'
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
        return None, None

    # Detect [Math:Round]
    if nav.node.type == 'MATH' and nav.node.operation == 'ROUND':
        nav.select_input_socket(0)
        alpha_nav.assign(nav)
        return 0.5, None  # Round => can't be animated, so no path

    # Detect 1 - (X < cutoff)
    # (There is no >= node)
    if nav.node.type == 'MATH' and nav.node.operation == 'SUBTRACT':
        if nav.get_constant(0)[0] == 1.0:
            nav2 = nav.peek_back(1)
            if nav2.moved and nav2.node.type == 'MATH':
                in0, in_path0 = nav2.get_constant(0)
                in1, in_path1 = nav2.get_constant(1)
                # X < cutoff
                if nav2.node.operation == 'LESS_THAN' and in0 is None and in1 is not None:
                    nav2.select_input_socket(0)
                    alpha_nav.assign(nav2)
                    return in1, in_path1
                # cutoff > X
                elif nav2.node.operation == 'GREATER_THAN' and in0 is not None and in1 is None:
                    nav2.select_input_socket(1)
                    alpha_nav.assign(nav2)
                    return in0, in_path0

    # Detect (X > cutoff)
    # Wrong when X = cutoff, but backwards compatible with legacy
    # Alpha Clip setup
    if nav.node.type == 'MATH':
        in0, in_path0 = nav.get_constant(0)
        in1, in_path1 = nav.get_constant(1)
        if nav.node.operation == 'GREATER_THAN' and in1 is not None:
            nav.select_input_socket(0)
            alpha_nav.assign(nav)
            return in1, in_path1
        elif nav.node.operation == 'LESS_THAN' and in0 is not None:
            nav.select_input_socket(1)
            alpha_nav.assign(nav)
            return in0, in_path0

    return None, None


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


# Old, prefer NodeNav.get_factor in new code
def get_factor_from_socket(socket, kind):
    return socket.to_node_nav().get_factor()


# Old, prefer NodeNav.get_constant in new code
def get_const_from_socket(socket, kind):
    return socket.to_node_nav().get_constant()


def previous_socket(socket: NodeSocket):
    nav = socket.to_node_nav()
    nav.move_back()
    if nav.moved:
        return NodeSocket(nav.out_socket, [group for group, _ in nav.stack])
    return NodeSocket(None, None)


def previous_node(socket: NodeSocket):
    nav = socket.to_node_nav()
    nav.move_back()
    if nav.moved:
        return ShNode(nav.node, [group for group, _ in nav.stack])
    return ShNode(None, None)


def next_node(socket: NodeSocket):
    nav = socket.to_node_nav()
    nav.move_forward()
    if nav.moved:
        return ShNode(nav.node, [group for group, _ in nav.stack])
    return ShNode(None, None)


def next_socket(socket: NodeSocket):
    nav = socket.to_node_nav()
    nav.move_forward()
    if nav.moved:
        return NodeSocket(nav.in_socket, [group for group, _ in nav.stack])
    return NodeSocket(None, None)


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


def detect_iridescence_thickness_texure(socket, minimum_thickness_socket, export_settings):
    # Check that we have a specific tree branch with all data required for the iridescence thickness texture

    if socket.socket is None:
        return False, None

    # Check that the socket is linked to a Mix node
    if not socket.socket.is_linked:
        return False, None
    mix_node = previous_node(socket)
    if mix_node.node is None or mix_node.node.type != "MIX":
        return False, None
    if mix_node.node.data_type != "FLOAT":
        return False, None

    # Check that the mix node factor is linked to a separate RGB node, with R
    # linked to the iridescence thickness texture
    if not mix_node.node.inputs['Factor'].is_linked:
        return False, None
    separate_rgb_node = previous_node(NodeSocket(mix_node.node.inputs['Factor'], mix_node.group_path))
    if separate_rgb_node.node is None or separate_rgb_node.node.type != "SEPARATE_COLOR":
        return False, None
    if not separate_rgb_node.node.inputs[0].is_linked:
        return False, None
    iridescence_thickness_texture_node = previous_node(NodeSocket(
        separate_rgb_node.node.inputs[0], separate_rgb_node.group_path))
    if iridescence_thickness_texture_node.node is None or iridescence_thickness_texture_node.node.type != "TEX_IMAGE":
        return False, None

    # Check that the mix node A is linked to a value node, that is itself
    # linked (output) to the iridescence minimum thickness socket of the glTF
    # material output node
    if not mix_node.node.inputs['A'].is_linked:
        return False, None
    value_node = previous_node(NodeSocket(mix_node.node.inputs['A'], mix_node.group_path))
    if value_node.node is None or value_node.node.type != "VALUE":
        return False, None
    if not value_node.node.outputs[0].is_linked:
        return False, None
    if not len(value_node.node.outputs[0].links) == 2:
        return False, None
    if minimum_thickness_socket.socket is None:
        return False, None
    min_val_node = previous_node(minimum_thickness_socket)
    if min_val_node.node != value_node.node:
        return False, None

    # Check that the mix node B is linked to a value node
    if not mix_node.node.inputs['B'].is_linked:
        return False, None
    value_node_2 = previous_node(NodeSocket(mix_node.node.inputs['B'], mix_node.group_path))
    if value_node_2.node is None or value_node_2.node.type != "VALUE":
        return False, None

    return True, {
        'thickness_minimum': value_node.node.outputs[0],
        'thickness_maximum': value_node_2.node.outputs[0],
        'tex_socket': NodeSocket(separate_rgb_node.node.inputs[0], socket.group_path),
    }


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
    anisotropy_multiply_node = previous_node(anisotropy_socket)
    if anisotropy_multiply_node.node is None or anisotropy_multiply_node.node.type != "MATH":
        return False, None
    if anisotropy_multiply_node.node.operation != "MULTIPLY":
        return False, None
    # this multiply node should have the first input linked to separate XYZ, on Z
    if not anisotropy_multiply_node.node.inputs[0].is_linked:
        return False, None
    separate_xyz_node = previous_node(
        NodeSocket(
            anisotropy_multiply_node.node.inputs[0],
            anisotropy_multiply_node.group_path))
    if separate_xyz_node.node is None or separate_xyz_node.node.type != "SEPXYZ":
        return False, None
    separate_xyz_z_socket = previous_socket(
        NodeSocket(
            anisotropy_multiply_node.node.inputs[0],
            anisotropy_multiply_node.group_path))
    if separate_xyz_z_socket.socket is None or separate_xyz_z_socket.socket.identifier != "Z":
        return False, None
    # This separate XYZ node output should be linked to ArcTan2 node (X on inputs[1], Y on inputs[0])
    if not separate_xyz_node.node.outputs[0].is_linked:
        return False, None
    arctan2_node = next_node(NodeSocket(separate_xyz_node.node.outputs[0], separate_xyz_node.group_path))
    if arctan2_node.node.type != "MATH":
        return False, None
    if arctan2_node.node.operation != "ARCTAN2":
        return False, None
    arctan2_node_prev_y_socket = previous_socket(
        NodeSocket(
            arctan2_node.node.inputs[0],
            arctan2_node.group_path))
    if arctan2_node_prev_y_socket.socket is None or arctan2_node_prev_y_socket.socket.identifier != "Y":
        return False, None
    arctan2_node_prev_x_socket = previous_socket(
        NodeSocket(
            arctan2_node.node.inputs[1],
            arctan2_node.group_path))
    if arctan2_node_prev_x_socket.socket is None or arctan2_node_prev_x_socket.socket.identifier != "X":
        return False, None
    # This arctan2 node output should be linked to anisotropy rotation (Math add node)
    if not arctan2_node.node.outputs[0].is_linked:
        return False, None
    anisotropy_rotation_node = next_node(NodeSocket(arctan2_node.node.outputs[0], arctan2_node.group_path))
    if anisotropy_rotation_node.node.type != "MATH":
        return False, None
    if anisotropy_rotation_node.node.operation != "ADD":
        return False, None
    # This anisotropy rotation node should have the output linked to rotation conversion node
    if not anisotropy_rotation_node.node.outputs[0].is_linked:
        return False, None
    rotation_conversion_node = next_node(
        NodeSocket(
            anisotropy_rotation_node.node.outputs[0],
            anisotropy_rotation_node.group_path))
    if rotation_conversion_node.node.type != "MATH":
        return False, None
    if rotation_conversion_node.node.operation != "DIVIDE":
        return False, None
    # This rotation conversion node should have the second input value PI
    if abs(rotation_conversion_node.node.inputs[1].default_value - 6.283185) > 0.0001:
        return False, None
    # This rotation conversion node should have the output linked to anisotropy rotation socket of Principled BSDF
    if not rotation_conversion_node.node.outputs[0].is_linked:
        return False, None
    rotation_conversion_node_next_socket = next_socket(
        NodeSocket(
            rotation_conversion_node.node.outputs[0],
            rotation_conversion_node.group_path))

    if rotation_conversion_node_next_socket.socket is None or rotation_conversion_node_next_socket.socket.identifier != "Anisotropic Rotation":
        return False, None
    if rotation_conversion_node_next_socket.socket.node.type != "BSDF_PRINCIPLED":
        return False, None

    # Separate XYZ node should have the input linked to anisotropy multiply Add node (for normalization)
    if not separate_xyz_node.node.inputs[0].is_linked:
        return False, None
    anisotropy_multiply_add_node = previous_node(
        NodeSocket(
            separate_xyz_node.node.inputs[0],
            separate_xyz_node.group_path))
    if anisotropy_multiply_add_node.node.type != "VECT_MATH":
        return False, None
    if anisotropy_multiply_add_node.node.operation != "MULTIPLY_ADD":
        return False, None
    if list(anisotropy_multiply_add_node.node.inputs[1].default_value) != [2.0, 2.0, 1.0]:
        return False, None
    if list(anisotropy_multiply_add_node.node.inputs[2].default_value) != [-1.0, -1.0, 0.0]:
        return False, None
    if not anisotropy_multiply_add_node.node.inputs[0].is_linked:
        return False, None
    # This anisotropy multiply Add node should have the first input linked to a texture node
    anisotropy_texture_node = previous_node(
        NodeSocket(
            anisotropy_multiply_add_node.node.inputs[0],
            anisotropy_multiply_add_node.group_path))
    if anisotropy_texture_node.node.type != "TEX_IMAGE":
        return False, None

    tex_ok = has_image_node_from_socket(
        NodeSocket(
            anisotropy_multiply_add_node.node.inputs[0],
            anisotropy_multiply_add_node.group_path),
        export_settings)
    if tex_ok is False:
        return False, None

    strength, path_strength = get_const_from_socket(NodeSocket(
        anisotropy_multiply_node.node.inputs[1], anisotropy_multiply_node.group_path), 'VALUE')
    rotation, path_rotation = get_const_from_socket(NodeSocket(
        anisotropy_rotation_node.node.inputs[1], anisotropy_rotation_node.group_path), 'VALUE')

    return True, {
        'anisotropyStrength': (strength, path_strength),
        'anisotropyRotation': (rotation, path_rotation),
        'tangent': tangent_node.node.uv_map,
        'tex_socket': NodeSocket(anisotropy_multiply_add_node.node.inputs[0], anisotropy_multiply_add_node.group_path),
    }
