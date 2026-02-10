# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "connect_sockets",
    "find_base_socket_type",
    "find_node_input",
)


def find_base_socket_type(socket):
    """
    Find the base class of the socket.

    Sockets can have a subtype such as NodeSocketFloatFactor,
    but only the base type is allowed, e.g. NodeSocketFloat

    :param socket: The socket to find the base type for.
    :type socket: :class:`bpy.types.NodeSocket`
    :return: The base socket type identifier.
    :rtype: str
    """
    if socket.type == 'CUSTOM':
        # Custom socket types are used directly
        return socket.bl_idname
    if socket.type == 'VALUE':
        return 'NodeSocketFloat'
    if socket.type == 'INT':
        return 'NodeSocketInt'
    if socket.type == 'BOOLEAN':
        return 'NodeSocketBool'
    if socket.type == 'VECTOR':
        return 'NodeSocketVector'
    if socket.type == 'ROTATION':
        return 'NodeSocketRotation'
    if socket.type == 'STRING':
        return 'NodeSocketString'
    if socket.type == 'RGBA':
        return 'NodeSocketColor'
    if socket.type == 'SHADER':
        return 'NodeSocketShader'
    if socket.type == 'OBJECT':
        return 'NodeSocketObject'
    if socket.type == 'IMAGE':
        return 'NodeSocketImage'
    if socket.type == 'GEOMETRY':
        return 'NodeSocketGeometry'
    if socket.type == 'COLLECTION':
        return 'NodeSocketCollection'
    if socket.type == 'TEXTURE':
        return 'NodeSocketTexture'
    if socket.type == 'MATERIAL':
        return 'NodeSocketMaterial'


def connect_sockets(input, output):
    """
    Connect sockets in a node tree.

    This is useful because the links created through the normal Python API are
    invalid when one of the sockets is a virtual socket (grayed out sockets in
    Group Input and Group Output nodes).

    It replaces node_tree.links.new(input, output)

    :param input: The input socket.
    :type input: :class:`bpy.types.NodeSocket`
    :param output: The output socket.
    :type output: :class:`bpy.types.NodeSocket`
    """
    import bpy

    # Swap sockets if they are not passed in the proper order
    if input.is_output and not output.is_output:
        input, output = output, input

    input_node = output.node
    output_node = input.node

    if input_node.id_data is not output_node.id_data:
        print("Sockets do not belong to the same node tree")
        return

    if type(input) == type(output) == bpy.types.NodeSocketVirtual:
        print("Cannot connect two virtual sockets together")
        return

    return input_node.id_data.links.new(input, output, handle_dynamic_sockets=True)


def find_node_input(node, name):
    """
    Find a node input socket by name.

    Note that names are not unique, returns the first match.

    :param node: The node to search.
    :type node: :class:`bpy.types.Node`
    :param name: The name of the input socket.
    :type name: str
    :return: The input socket or None if not found.
    :rtype: :class:`bpy.types.NodeSocket` | None
    """
    for input in node.inputs:
        if input.name == name:
            return input

    return None
