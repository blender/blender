# SPDX-FileCopyrightText: 2012-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import bpy
from bpy.types import (
    FileHandler,
    Operator,
    PropertyGroup,
)
from bpy.props import (
    BoolProperty,
    CollectionProperty,
    EnumProperty,
    FloatVectorProperty,
    StringProperty,
    IntProperty,
)
from mathutils import (
    Vector,
)

from bpy.app.translations import (
    pgettext_tip as tip_,
    pgettext_data as data_,
    pgettext_rpt as rpt_,
    pgettext_n as n_,
)


math_nodes = {
    "ShaderNodeMath",
    "ShaderNodeVectorMath",
    "FunctionNodeIntegerMath",
    "FunctionNodeBooleanMath",
    "FunctionNodeBitMath",
}

switch_nodes = {
    "GeometryNodeMenuSwitch",
    "GeometryNodeIndexSwitch",
}


def cast_value(source, target):
    source_type = source.type
    target_type = target.type

    value = source.default_value

    def to_bool(value):
        return value > 0

    def single_value_to_color(value):
        return Vector((value, value, value, 1.0))

    def single_value_to_vector(value):
        return Vector([value,] * len(target.default_value))

    def color_to_float(color):
        return (0.2126 * color[0]) + (0.7152 * color[1]) + (0.0722 * color[2])

    def vector_to_float(vector):
        return sum(vector) / len(vector)

    func_map = {
        ('VALUE', 'INT'): int,
        ('VALUE', 'BOOLEAN'): to_bool,
        ('VALUE', 'RGBA'): single_value_to_color,
        ('VALUE', 'VECTOR'): single_value_to_vector,
        ('INT', 'VALUE'): float,
        ('INT', 'BOOLEAN'): to_bool,
        ('INT', 'RGBA'): single_value_to_color,
        ('INT', 'VECTOR'): single_value_to_vector,
        ('BOOLEAN', 'VALUE'): float,
        ('BOOLEAN', 'INT'): int,
        ('BOOLEAN', 'RGBA'): single_value_to_color,
        ('BOOLEAN', 'VECTOR'): single_value_to_vector,
        ('RGBA', 'VALUE'): color_to_float,
        ('RGBA', 'INT'): lambda color: int(color_to_float(color)),
        ('RGBA', 'BOOLEAN'): lambda color: to_bool(color_to_float(color)),
        ('RGBA', 'VECTOR'): lambda color: color[:len(target.default_value)],
        ('VECTOR', 'VALUE'): vector_to_float,
        ('VECTOR', 'INT'): lambda vector: int(vector_to_float(vector)),
        # Even negative vectors get implicitly converted to True, hence `to_bool` is not used.
        ('VECTOR', 'BOOLEAN'): lambda vector: bool(vector_to_float(vector)),
        ('VECTOR', 'RGBA'): lambda vector: list(vector).extend([0.0] * (len(target.default_value) - len(vector)))
    }

    if source_type == target_type:
        return value

    cast_func = func_map.get((source_type, target_type))
    if cast_func is not None:
        return cast_func(value)

    return None


class NodeSetting(PropertyGroup):
    __slots__ = ()

    value: StringProperty(
        name="Value",
        description="Python expression to be evaluated "
        "as the initial node setting",
        default="",
    )


class NodeOperator:
    settings: CollectionProperty(
        name="Settings",
        description="Settings to be applied on the newly created node",
        type=NodeSetting,
        options={'SKIP_SAVE'},
    )

    @classmethod
    def description(cls, _context, properties):
        from nodeitems_builtins import node_tree_group_type

        nodetype = properties["type"]
        if nodetype in node_tree_group_type.values():
            for setting in properties.settings:
                if setting.name == "node_tree":
                    node_group = eval(setting.value)
                    if node_group.description:
                        return node_group.description
        bl_rna = bpy.types.Node.bl_rna_get_subclass(nodetype)
        if bl_rna is not None:
            return tip_(bl_rna.description)
        else:
            return ""

    # Deselect all nodes in the tree.
    @staticmethod
    def deselect_nodes(context):
        space = context.space_data
        tree = space.edit_tree
        for n in tree.nodes:
            n.select = False

    def create_node(self, context, node_type):
        space = context.space_data
        tree = space.edit_tree

        try:
            node = tree.nodes.new(type=node_type)
        except RuntimeError as ex:
            self.report({'ERROR'}, str(ex))
            return None

        node.select = True
        tree.nodes.active = node
        node.location = space.cursor_location
        return node

    def apply_node_settings(self, node):
        for setting in self.settings:
            # XXX catch exceptions here?
            value = eval(setting.value)
            node_data = node
            node_attr_name = setting.name

            # Support path to nested data.
            if '.' in node_attr_name:
                node_data_path, node_attr_name = node_attr_name.rsplit(".", 1)
                node_data = node.path_resolve(node_data_path)

            try:
                setattr(node_data, node_attr_name, value)
            except AttributeError as ex:
                self.report(
                    {'ERROR_INVALID_INPUT'},
                    rpt_("Node has no attribute {:s}").format(setting.name))
                print(str(ex))
                # Continue despite invalid attribute.
        return node


# Base class for node "Add" operators.
class NodeAddOperator(NodeOperator):
    use_transform: BoolProperty(
        name="Use Transform",
        description="Start transform operator after inserting the node",
        default=False,
    )

    @staticmethod
    def store_mouse_cursor(context, event):
        space = context.space_data
        tree = space.edit_tree

        # Convert mouse position to the View2D for later node placement.
        if context.region.type == 'WINDOW':
            area = context.area
            horizontal_pad = int(area.width / 10)
            vertical_pad = int(area.height / 10)

            inspace_x = min(max(horizontal_pad, event.mouse_region_x), area.width - horizontal_pad)
            inspace_y = min(max(vertical_pad, event.mouse_region_y), area.height - vertical_pad)
            # Convert mouse position to the View2D for later node placement.
            space.cursor_location_from_region(inspace_x, inspace_y)
        else:
            space.cursor_location = tree.view_center

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # Needs active node editor and a tree to add nodes to.
        return (
            space and (space.type == 'NODE_EDITOR') and
            space.edit_tree and space.edit_tree.is_editable
        )

    # Default invoke stores the mouse position to place the node correctly
    # and optionally invokes the transform operator.
    def invoke(self, context, event):
        self.store_mouse_cursor(context, event)
        result = self.execute(context)

        if self.use_transform and ('FINISHED' in result):
            # Removes the node again if transform is canceled.
            bpy.ops.node.translate_attach_remove_on_cancel('INVOKE_DEFAULT')

        return result


class NodeSwapOperator(NodeOperator):
    properties_to_pass = (
        'color',
        'hide',
        'label',
        'mute',
        'parent',
        'show_options',
        'show_preview',
        'show_texture',
        'use_alpha',
        'use_clamp',
        'use_custom_color',
        "operation",
        "domain",
        "data_type",
    )

    @classmethod
    def poll(cls, context):
        if (context.area is None) or (context.area.type != "NODE_EDITOR"):
            return False

        if len(context.selected_nodes) <= 0:
            cls.poll_message_set("No nodes selected.")
            return False

        return True

    def transfer_node_properties(self, old_node, new_node):
        for attr in self.properties_to_pass:
            if (attr in self.settings):
                continue

            if hasattr(old_node, attr) and hasattr(new_node, attr):
                try:
                    setattr(new_node, attr, getattr(old_node, attr))
                except (TypeError, ValueError):
                    pass

    def transfer_input_values(self, old_node, new_node):
        if (old_node.bl_idname in math_nodes) and (new_node.bl_idname in math_nodes):
            for source_input, target_input in zip(old_node.inputs, new_node.inputs):

                new_value = cast_value(source=source_input, target=target_input)

                if new_value is not None:
                    target_input.default_value = new_value

        else:
            for input in old_node.inputs:
                try:
                    new_socket = new_node.inputs[input.name]
                    new_value = cast_value(source=input, target=new_socket)

                    settings_name = "inputs[\"{:s}\"].default_value".format(bpy.utils.escape_identifier(input.name))
                    already_defined = (settings_name in self.settings)

                    if (new_value is not None) and not already_defined:
                        new_socket.default_value = new_value

                except (AttributeError, KeyError, TypeError):
                    pass

    @staticmethod
    def transfer_links(tree, old_node, new_node, is_input):
        both_math_nodes = (old_node.bl_idname in math_nodes) and (new_node.bl_idname in math_nodes)

        if is_input:
            if both_math_nodes:
                for i, input in enumerate(old_node.inputs):
                    for link in input.links[:]:
                        try:
                            new_socket = new_node.inputs[i]

                            if new_socket.hide or not new_socket.enabled:
                                continue

                            tree.links.new(link.from_socket, new_socket)
                        except IndexError:
                            pass
            else:
                for input in old_node.inputs:
                    links = sorted(input.links, key=lambda link: link.multi_input_sort_id)

                    for link in links:
                        try:
                            new_socket = new_node.inputs[input.name]

                            if new_socket.hide or not new_socket.enabled:
                                continue

                            tree.links.new(link.from_socket, new_socket)
                        except KeyError:
                            pass

        else:
            if both_math_nodes:
                for i, output in enumerate(old_node.outputs):
                    for link in output.links[:]:
                        try:
                            new_socket = new_node.outputs[i]

                            if new_socket.hide or not new_socket.enabled:
                                continue

                            new_link = tree.links.new(new_socket, link.to_socket)
                        except IndexError:
                            pass

            else:
                for output in old_node.outputs:
                    for link in output.links[:]:
                        try:
                            new_socket = new_node.outputs[output.name]

                            if new_socket.hide or not new_socket.enabled:
                                continue

                            is_multi_input = link.to_socket.is_multi_input

                            new_link = tree.links.new(new_socket, link.to_socket)

                            if is_multi_input:
                                new_link.swap_multi_input_sort_id(link)

                        except KeyError:
                            pass

    @staticmethod
    def get_switch_items(node):
        switch_type = node.bl_idname

        if switch_type == "GeometryNodeMenuSwitch":
            return node.enum_definition.enum_items
        if switch_type == "GeometryNodeIndexSwitch":
            return node.index_switch_items
        return None

    def transfer_switch_data(self, old_node, new_node):
        old_switch_items = self.get_switch_items(old_node)
        new_switch_items = self.get_switch_items(new_node)

        new_switch_items.clear()

        if new_node.bl_idname == "GeometryNodeMenuSwitch":
            for i, old_item in enumerate(old_switch_items[:]):
                # Change the menu item names to numerical indices.
                # This makes it so that later functions that match by socket name work on the switches.
                if hasattr(old_item, "name"):
                    old_item.name = str(i)

                new_switch_items.new(str(i))

            if (old_switch_value := old_node.inputs[0].default_value) != '':
                new_node.inputs[0].default_value = str(old_switch_value)

        elif new_node.bl_idname == "GeometryNodeIndexSwitch":
            for i, old_item in enumerate(old_switch_items[:]):
                # Change the menu item names to numerical indices.
                # This makes it so that later functions that match by socket name work on the switches.
                if hasattr(old_item, "name"):
                    old_item.name = str(i)

                new_switch_items.new()

            if (old_switch_value := old_node.inputs[0].default_value) != '':
                new_node.inputs[0].default_value = int(old_switch_value)


# Simple basic operator for adding a node.
class NODE_OT_add_node(NodeAddOperator, Operator):
    """Add a node to the active tree"""
    bl_idname = "node.add_node"
    bl_label = "Add Node"
    bl_options = {'REGISTER', 'UNDO'}

    type: StringProperty(
        name="Node Type",
        description="Node type",
    )

    visible_output: StringProperty(
        name="Output Name",
        description="If provided, all outputs that are named differently will be hidden",
        options={'SKIP_SAVE'},
    )

    # Default execute simply adds a node.
    def execute(self, context):
        if self.properties.is_property_set("type"):
            self.deselect_nodes(context)
            if node := self.create_node(context, self.type):
                self.apply_node_settings(node)
                if self.visible_output:
                    for socket in node.outputs:
                        if socket.name != self.visible_output:
                            socket.hide = True
            return {'FINISHED'}
        else:
            return {'CANCELLED'}


class NODE_OT_swap_node(NodeSwapOperator, Operator):
    """Replace the selected nodes with the specified type"""
    bl_idname = "node.swap_node"
    bl_label = "Swap Node"
    bl_options = {"REGISTER", "UNDO"}

    type: StringProperty(
        name="Node Type",
        description="Node type",
    )

    visible_output: StringProperty(
        name="Output Name",
        description="If provided, all outputs that are named differently will be hidden",
        options={'SKIP_SAVE'},
    )

    @staticmethod
    def get_zone_pair(tree, node):
        # Get paired output node.
        if hasattr(node, "paired_output"):
            return node, node.paired_output

        # Get paired input node.
        for input_node in tree.nodes:
            if hasattr(input_node, "paired_output"):
                if input_node.paired_output == node:
                    return input_node, node

        return None

    def execute(self, context):
        tree = context.space_data.edit_tree
        nodes_to_delete = set()

        for old_node in context.selected_nodes[:]:
            if old_node in nodes_to_delete:
                continue

            if (old_node.bl_idname == self.type) and (not hasattr(old_node, "node_tree")):
                self.apply_node_settings(old_node)
                continue

            new_node = self.create_node(context, self.type)
            self.apply_node_settings(new_node)

            if self.visible_output:
                for socket in new_node.outputs:
                    if socket.name != self.visible_output:
                        socket.hide = True

            new_node.location_absolute = old_node.location_absolute
            new_node.select = True

            zone_pair = self.get_zone_pair(tree, old_node)

            if zone_pair is not None:
                input_node, output_node = zone_pair

                if input_node.select and output_node.select:
                    new_node.location_absolute = (input_node.location_absolute + output_node.location_absolute) / 2

                self.transfer_node_properties(old_node, new_node)
                self.transfer_input_values(input_node, new_node)

                self.transfer_links(tree, input_node, new_node, is_input=True)
                self.transfer_links(tree, output_node, new_node, is_input=False)

                for node in zone_pair:
                    nodes_to_delete.add(node)
            else:
                self.transfer_node_properties(old_node, new_node)

                if (old_node.bl_idname in switch_nodes) and (new_node.bl_idname in switch_nodes):
                    self.transfer_switch_data(old_node, new_node)

                self.transfer_input_values(old_node, new_node)

                self.transfer_links(tree, old_node, new_node, is_input=True)
                self.transfer_links(tree, old_node, new_node, is_input=False)

                nodes_to_delete.add(old_node)

        for node in nodes_to_delete:
            tree.nodes.remove(node)

        return {'FINISHED'}


class NODE_OT_add_empty_group(NodeAddOperator, bpy.types.Operator):
    bl_idname = "node.add_empty_group"
    bl_label = "Add Empty Group"
    bl_description = "Add a group node with an empty group"
    bl_options = {'REGISTER', 'UNDO'}

    # Override inherited method from NodeOperator.
    # Return None so that bl_description is used.
    @classmethod
    def description(cls, _context, properties):
        ...

    def execute(self, context):
        from nodeitems_builtins import node_tree_group_type
        tree = context.space_data.edit_tree
        group = self.create_empty_group(tree.bl_idname)
        self.deselect_nodes(context)
        node = self.create_node(context, node_tree_group_type[tree.bl_idname])
        self.apply_node_settings(node)
        node.node_tree = group
        return {"FINISHED"}

    @staticmethod
    def create_empty_group(idname):
        group = bpy.data.node_groups.new(name=data_("NodeGroup"), type=idname)
        input_node = group.nodes.new('NodeGroupInput')
        input_node.select = False
        input_node.location.x = -200 - input_node.width

        output_node = group.nodes.new('NodeGroupOutput')
        output_node.is_active_output = True
        output_node.select = False
        output_node.location.x = 200
        return group


class NODE_OT_swap_empty_group(NodeSwapOperator, bpy.types.Operator):
    bl_idname = "node.swap_empty_group"
    bl_label = "Swap Empty Group"
    bl_description = "Replace active node with an empty group"
    bl_options = {'REGISTER', 'UNDO'}

    # Override inherited method from NodeOperator.
    # Return None so that bl_description is used.
    @classmethod
    def description(cls, _context, properties):
        ...

    def execute(self, context):
        from nodeitems_builtins import node_tree_group_type
        tree = context.space_data.edit_tree
        group = self.create_empty_group(tree.bl_idname)

        bpy.ops.node.swap_node('INVOKE_DEFAULT', type=node_tree_group_type[tree.bl_idname])

        for node in context.selected_nodes:
            node.node_tree = group

        return {"FINISHED"}

    @staticmethod
    def create_empty_group(idname):
        group = bpy.data.node_groups.new(name="NodeGroup", type=idname)
        input_node = group.nodes.new('NodeGroupInput')
        input_node.select = False
        input_node.location.x = -200 - input_node.width

        output_node = group.nodes.new('NodeGroupOutput')
        output_node.is_active_output = True
        output_node.select = False
        output_node.location.x = 200
        return group


class ZoneOperator:
    offset: FloatVectorProperty(
        name="Offset",
        description="Offset of nodes from the cursor when added",
        size=2,
        default=(150, 0),
    )

    _zone_tooltips = {
        "GeometryNodeSimulationInput": (
            n_("Simulate the execution of nodes across a time span")
        ),
        "GeometryNodeRepeatInput": (
            n_("Execute nodes with a dynamic number of repetitions")
        ),
        "GeometryNodeForeachGeometryElementInput": (
            n_("Perform operations separately for each geometry element (e.g. vertices, edges, etc.)")
        ),
        "NodeClosureInput": (
            n_("Wrap nodes inside a closure that can be executed at a different part of the node-tree")
        ),
    }

    @classmethod
    def description(cls, _context, properties):
        input_node_type = getattr(properties, "input_node_type", None)

        # For Add Zone operators, use class variable instead of operator property.
        if input_node_type is None:
            input_node_type = cls.input_node_type

        return cls._zone_tooltips.get(input_node_type, None)


class NodeAddZoneOperator(ZoneOperator, NodeAddOperator):
    add_default_geometry_link = True

    def execute(self, context):
        space = context.space_data
        tree = space.edit_tree

        self.deselect_nodes(context)
        input_node = self.create_node(context, self.input_node_type)
        output_node = self.create_node(context, self.output_node_type)

        self.apply_node_settings(input_node)
        self.apply_node_settings(output_node)

        if input_node is None or output_node is None:
            return {'CANCELLED'}

        # Simulation input must be paired with the output.
        input_node.pair_with_output(output_node)

        input_node.location -= Vector(self.offset)
        output_node.location += Vector(self.offset)

        if tree.type == "GEOMETRY" and self.add_default_geometry_link:
            # Connect geometry sockets by default if available.
            # Get the sockets by their types, because the name is not guaranteed due to i18n.
            from_socket = next(s for s in input_node.outputs if s.type == 'GEOMETRY')
            to_socket = next(s for s in output_node.inputs if s.type == 'GEOMETRY')
            tree.links.new(to_socket, from_socket)

        return {'FINISHED'}


class NODE_OT_add_zone(NodeAddZoneOperator, Operator):
    bl_idname = "node.add_zone"
    bl_label = "Add Zone"
    bl_options = {'REGISTER', 'UNDO'}

    input_node_type: StringProperty(
        name="Input Node",
        description="Specifies the input node used by the created zone",
    )

    output_node_type: StringProperty(
        name="Output Node",
        description="Specifies the output node used by the created zone",
    )

    add_default_geometry_link: BoolProperty(
        name="Add Geometry Link",
        description="When enabled, create a link between geometry sockets in this zone",
        default=False,
    )


class NODE_OT_swap_zone(ZoneOperator, NodeSwapOperator, Operator):
    bl_idname = "node.swap_zone"
    bl_label = "Swap Zone"
    bl_options = {"REGISTER", "UNDO"}

    input_node_type: StringProperty(
        name="Input Node",
        description="Specifies the input node used by the created zone",
    )

    output_node_type: StringProperty(
        name="Output Node",
        description="Specifies the output node used by the created zone",
    )

    add_default_geometry_link: BoolProperty(
        name="Add Geometry Link",
        description="When enabled, create a link between geometry sockets in this zone",
        default=False,
    )

    @staticmethod
    def get_zone_pair(tree, node):
        # Get paired output node.
        if hasattr(node, "paired_output"):
            return node, node.paired_output

        # Get paired input node.
        for input_node in tree.nodes:
            if hasattr(input_node, "paired_output"):
                if input_node.paired_output == node:
                    return input_node, node

        return None

    def execute(self, context):
        tree = context.space_data.edit_tree
        nodes_to_delete = set()

        for old_node in context.selected_nodes[:]:
            if old_node in nodes_to_delete:
                continue

            zone_pair = self.get_zone_pair(tree, old_node)

            if (old_node.bl_idname in {self.input_node_type, self.output_node_type}):
                if zone_pair is not None:
                    old_input_node, old_output_node = zone_pair
                    self.apply_node_settings(old_input_node)
                    self.apply_node_settings(old_output_node)
                else:
                    self.apply_node_settings(old_node)

                continue

            input_node = self.create_node(context, self.input_node_type)
            output_node = self.create_node(context, self.output_node_type)

            self.apply_node_settings(input_node)
            self.apply_node_settings(output_node)

            if input_node is None or output_node is None:
                return {'CANCELLED'}

            # Simulation input must be paired with the output.
            input_node.pair_with_output(output_node)

            if zone_pair is not None:
                old_input_node, old_output_node = zone_pair

                input_node.location_absolute = old_input_node.location_absolute
                output_node.location_absolute = old_output_node.location_absolute

                self.transfer_node_properties(old_input_node, input_node)
                self.transfer_node_properties(old_output_node, output_node)

                self.transfer_input_values(old_input_node, input_node)
                self.transfer_input_values(old_output_node, output_node)

                self.transfer_links(tree, old_input_node, input_node, is_input=True)
                self.transfer_links(tree, old_input_node, input_node, is_input=False)

                self.transfer_links(tree, old_output_node, output_node, is_input=True)
                self.transfer_links(tree, old_output_node, output_node, is_input=False)

                for node in zone_pair:
                    nodes_to_delete.add(node)
            else:
                input_node.location_absolute = (old_node.location_absolute - Vector(self.offset))
                output_node.location_absolute = (old_node.location_absolute + Vector(self.offset))

                self.transfer_node_properties(old_node, input_node)
                self.transfer_node_properties(old_node, output_node)

                self.transfer_input_values(old_node, input_node)

                self.transfer_links(tree, old_node, input_node, is_input=True)
                self.transfer_links(tree, old_node, output_node, is_input=False)

                nodes_to_delete.add(old_node)

            if tree.type == "GEOMETRY" and self.add_default_geometry_link:
                # Connect geometry sockets by default if available.
                # Get the sockets by their types, because the name is not guaranteed due to i18n.
                from_socket = next(s for s in input_node.outputs if s.type == 'GEOMETRY')
                to_socket = next(s for s in output_node.inputs if s.type == 'GEOMETRY')

                if not (from_socket.is_linked or to_socket.is_linked):
                    tree.links.new(to_socket, from_socket)

        for node in nodes_to_delete:
            tree.nodes.remove(node)

        return {'FINISHED'}


class NODE_OT_add_simulation_zone(NodeAddZoneOperator, Operator):
    """Add simulation zone input and output nodes to the active tree"""
    bl_idname = "node.add_simulation_zone"
    bl_label = "Add Simulation Zone"
    bl_options = {'REGISTER', 'UNDO'}

    input_node_type = "GeometryNodeSimulationInput"
    output_node_type = "GeometryNodeSimulationOutput"


class NODE_OT_add_repeat_zone(NodeAddZoneOperator, Operator):
    """Add a repeat zone that allows executing nodes a dynamic number of times"""
    bl_idname = "node.add_repeat_zone"
    bl_label = "Add Repeat Zone"
    bl_options = {'REGISTER', 'UNDO'}

    input_node_type = "GeometryNodeRepeatInput"
    output_node_type = "GeometryNodeRepeatOutput"


class NODE_OT_add_foreach_geometry_element_zone(NodeAddZoneOperator, Operator):
    """Add a For Each Geometry Element zone that allows executing nodes e.g. for each vertex separately"""
    bl_idname = "node.add_foreach_geometry_element_zone"
    bl_label = "Add For Each Geometry Element Zone"
    bl_options = {'REGISTER', 'UNDO'}

    input_node_type = "GeometryNodeForeachGeometryElementInput"
    output_node_type = "GeometryNodeForeachGeometryElementOutput"
    add_default_geometry_link = False


class NODE_OT_add_closure_zone(NodeAddZoneOperator, Operator):
    """Add a Closure zone"""
    bl_idname = "node.add_closure_zone"
    bl_label = "Add Closure Zone"
    bl_options = {'REGISTER', 'UNDO'}

    input_node_type = "NodeClosureInput"
    output_node_type = "NodeClosureOutput"
    add_default_geometry_link = False


class NODE_OT_collapse_hide_unused_toggle(Operator):
    """Toggle collapsed nodes and hide unused sockets"""
    bl_idname = "node.collapse_hide_unused_toggle"
    bl_label = "Collapse and Hide Unused Sockets"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # Needs active node editor and a tree.
        return (space and (space.type == 'NODE_EDITOR') and
                (space.edit_tree and space.edit_tree.is_editable))

    def execute(self, context):
        space = context.space_data
        tree = space.edit_tree

        for node in tree.nodes:
            if node.select:
                hide = (not node.hide)

                node.hide = hide
                # NOTE: connected sockets are ignored internally.
                for socket in node.inputs:
                    socket.hide = hide
                for socket in node.outputs:
                    socket.hide = hide

        return {'FINISHED'}


class NODE_OT_tree_path_parent(Operator):
    """Go to parent node tree"""
    bl_idname = "node.tree_path_parent"
    bl_label = "Parent Node Tree"
    bl_options = {'REGISTER', 'UNDO'}

    parent_tree_index: IntProperty(
        name="Parent Index",
        description="Parent index in context path",
        default=0,
    )

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # Needs active node editor and a tree.
        return (space and (space.type == 'NODE_EDITOR') and len(space.path) > 1)

    def execute(self, context):
        space = context.space_data

        parent_number_to_pop = len(space.path) - 1 - self.parent_tree_index
        for _ in range(parent_number_to_pop):
            space.path.pop()

        return {'FINISHED'}


class NodeInterfaceOperator():
    @classmethod
    def poll(cls, context):
        space = context.space_data
        if not space or space.type != 'NODE_EDITOR' or not space.edit_tree:
            return False
        if space.edit_tree.is_embedded_data:
            return False
        return True


class NODE_OT_interface_item_new(NodeInterfaceOperator, Operator):
    """Add a new item to the interface"""
    bl_idname = "node.interface_item_new"
    bl_label = "New Item"
    bl_options = {'REGISTER', 'UNDO'}

    item_type: EnumProperty(
        name="Item Type",
        description="Type of the item to create",
        items=(
            ('INPUT', "Input", ""),
            ('OUTPUT', "Output", ""),
            ('PANEL', "Panel", ""),
        ),
        default='INPUT',
    )

    # Returns a valid socket type for the given tree or None.
    @staticmethod
    def find_valid_socket_type(tree):
        socket_type = 'NodeSocketFloat'
        # Socket type validation function is only available for custom
        # node trees. Assume that 'NodeSocketFloat' is valid for built-in node tree types.
        if not hasattr(tree, "valid_socket_type") or tree.valid_socket_type(socket_type):
            return socket_type
        # Custom nodes may not support float sockets, search all registered socket sub-classes.
        types_to_check = [bpy.types.NodeSocket]
        while types_to_check:
            t = types_to_check.pop()
            idname = getattr(t, "bl_idname", "")
            if tree.valid_socket_type(idname):
                return idname
            # Test all sub-classes
            types_to_check.extend(t.__subclasses__())

    def execute(self, context):
        snode = context.space_data
        tree = snode.edit_tree
        interface = tree.interface

        # Remember active item and position to determine target position.
        active_item = interface.active
        active_pos = active_item.position if active_item else -1

        if self.item_type == 'INPUT':
            item = interface.new_socket("Socket", socket_type=self.find_valid_socket_type(tree), in_out='INPUT')
        elif self.item_type == 'OUTPUT':
            item = interface.new_socket("Socket", socket_type=self.find_valid_socket_type(tree), in_out='OUTPUT')
        elif self.item_type == 'PANEL':
            item = interface.new_panel("Panel")
        else:
            return {'CANCELLED'}

        if active_item:
            # Insert into active panel if possible, otherwise insert after active item.
            if active_item.item_type == 'PANEL' and item.item_type != 'PANEL':
                interface.move_to_parent(item, active_item, len(active_item.interface_items))
            else:
                interface.move_to_parent(item, active_item.parent, active_pos + 1)
        interface.active = item

        return {'FINISHED'}


class NODE_OT_interface_item_new_panel_toggle(Operator):
    '''Add a checkbox to the currently selected panel'''
    bl_idname = "node.interface_item_new_panel_toggle"
    bl_label = "New Panel Toggle"
    bl_options = {'REGISTER', 'UNDO'}

    @staticmethod
    def get_panel_toggle(panel):
        if len(panel.interface_items) > 0:
            first_item = panel.interface_items[0]
            if type(first_item) is bpy.types.NodeTreeInterfaceSocketBool and first_item.is_panel_toggle:
                return first_item

        return None

    @classmethod
    def poll(cls, context):
        try:
            snode = context.space_data
            tree = snode.edit_tree
            interface = tree.interface

            active_item = interface.active

            if active_item.item_type != 'PANEL':
                cls.poll_message_set("Active item is not a panel")
                return False

            if cls.get_panel_toggle(active_item) is not None:
                cls.poll_message_set("Panel already has a toggle")
                return False

            return True
        except AttributeError:
            return False

    def execute(self, context):
        snode = context.space_data
        tree = snode.edit_tree

        interface = tree.interface
        active_panel = interface.active

        item = interface.new_socket(active_panel.name, socket_type='NodeSocketBool', in_out='INPUT')
        item.is_panel_toggle = True
        interface.move_to_parent(item, active_panel, 0)
        return {'FINISHED'}


class NODE_OT_interface_item_duplicate(NodeInterfaceOperator, Operator):
    """Add a copy of the active item to the interface"""
    bl_idname = "node.interface_item_duplicate"
    bl_label = "Duplicate Item"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False

        snode = context.space_data
        tree = snode.edit_tree
        interface = tree.interface
        return interface.active is not None

    def execute(self, context):
        snode = context.space_data
        tree = snode.edit_tree
        interface = tree.interface
        item = interface.active

        if item:
            item_copy = interface.copy(item)
            interface.active = item_copy

        return {'FINISHED'}


class NODE_OT_interface_item_remove(NodeInterfaceOperator, Operator):
    """Remove active item from the interface"""
    bl_idname = "node.interface_item_remove"
    bl_label = "Remove Item"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        snode = context.space_data
        tree = snode.edit_tree
        interface = tree.interface
        item = interface.active

        if item:
            if item.item_type == 'PANEL':
                children = item.interface_items
                if len(children) > 0:
                    first_child = children[0]
                    if isinstance(first_child, bpy.types.NodeTreeInterfaceSocket) and first_child.is_panel_toggle:
                        interface.remove(first_child)
            interface.remove(item)
            interface.active_index = min(interface.active_index, len(interface.items_tree) - 1)

            # If the active selection lands on internal toggle socket, move selection to parent instead.
            new_active = interface.active
            if isinstance(new_active, bpy.types.NodeTreeInterfaceSocket) and new_active.is_panel_toggle:
                interface.active_index = new_active.parent.index

        return {'FINISHED'}


class NODE_OT_interface_item_make_panel_toggle(NodeInterfaceOperator, Operator):
    """Make the active boolean socket a toggle for its parent panel"""
    bl_idname = "node.interface_item_make_panel_toggle"
    bl_label = "Make Panel Toggle"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False

        snode = context.space_data
        tree = snode.edit_tree
        interface = tree.interface
        active_item = interface.active
        if not active_item:
            return False

        if type(active_item) is not bpy.types.NodeTreeInterfaceSocketBool or active_item.in_out != 'INPUT':
            cls.poll_message_set("Only boolean input sockets are supported")
            return False

        parent_panel = active_item.parent
        if parent_panel.parent is None:
            cls.poll_message_set("Socket must be in a panel")
            return False
        if len(parent_panel.interface_items) > 0:
            first_item = parent_panel.interface_items[0]
            if first_item.is_panel_toggle:
                cls.poll_message_set("Panel already has a toggle")
                return False
        return True

    def execute(self, context):
        snode = context.space_data
        tree = snode.edit_tree
        interface = tree.interface
        active_item = interface.active

        parent_panel = active_item.parent
        if not parent_panel:
            return {'CANCELLED'}

        if type(active_item) is not bpy.types.NodeTreeInterfaceSocketBool:
            return {'CANCELLED'}

        active_item.is_panel_toggle = True
        # Use the same name as the panel in the UI for clarity.
        active_item.name = parent_panel.name

        # Move the socket to the first position.
        interface.move_to_parent(active_item, parent_panel, 0)
        # Make the panel active.
        interface.active = parent_panel

        return {'FINISHED'}


class NODE_OT_interface_item_unlink_panel_toggle(NodeInterfaceOperator, Operator):
    """Make the panel toggle a stand-alone socket"""
    bl_idname = "node.interface_item_unlink_panel_toggle"
    bl_label = "Unlink Panel Toggle"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False

        snode = context.space_data
        tree = snode.edit_tree
        interface = tree.interface
        active_item = interface.active
        if not active_item or active_item.item_type != 'PANEL':
            return False
        if len(active_item.interface_items) == 0:
            return False

        first_item = active_item.interface_items[0]
        return first_item.is_panel_toggle

    def execute(self, context):
        snode = context.space_data
        tree = snode.edit_tree
        interface = tree.interface
        active_item = interface.active

        if not active_item or active_item.item_type != 'PANEL':
            return {'CANCELLED'}

        if len(active_item.interface_items) == 0:
            return {'CANCELLED'}

        first_item = active_item.interface_items[0]
        if type(first_item) is not bpy.types.NodeTreeInterfaceSocketBool or not first_item.is_panel_toggle:
            return {'CANCELLED'}

        first_item.is_panel_toggle = False
        first_item.name = active_item.name

        # Make the socket active.
        interface.active = first_item

        return {'FINISHED'}


class NODE_OT_viewer_shortcut_set(Operator):
    """Create a viewer shortcut for the selected node by pressing ctrl+1,2,..9"""
    bl_idname = "node.viewer_shortcut_set"
    bl_label = "Fast Preview"
    bl_options = {'REGISTER', 'UNDO'}

    viewer_index: IntProperty(
        name="Viewer Index",
        description="Index corresponding to the shortcut, e.g. number key 1 corresponds to index 1 etc..")

    def get_connected_viewer(self, node):
        for out in node.outputs:
            for link in out.links:
                nv = link.to_node
                if nv.type == 'VIEWER':
                    return nv
        return None

    @classmethod
    def poll(cls, context):
        del cls
        space = context.space_data
        return (
            (space is not None) and
            space.type == 'NODE_EDITOR' and
            space.node_tree is not None and
            space.tree_type in {'CompositorNodeTree', 'GeometryNodeTree'}
        )

    def execute(self, context):
        selected_nodes = context.selected_nodes

        if len(selected_nodes) == 0:
            self.report({'ERROR'}, "Select a node to assign a shortcut")
            return {'CANCELLED'}

        fav_node = selected_nodes[0]

        # Only viewer nodes can be set to favorites. However, the user can
        # create a new favorite viewer by selecting any node and pressing Control+1.
        if fav_node.type == 'VIEWER':
            viewer_node = fav_node
        else:
            viewer_node = self.get_connected_viewer(fav_node)
            if not viewer_node:
                # Calling `link_viewer()` if a viewer node is connected
                # will connect the next available socket to the viewer node.
                # This behavior is not desired as we want to create a shortcut to the existing connected viewer node.
                # Therefore `link_viewer()` is called only when no viewer node is connected.
                bpy.ops.node.link_viewer()
                viewer_node = self.get_connected_viewer(fav_node)

        if not viewer_node:
            self.report(
                {'ERROR'},
                "Unable to set shortcut, selected node is not a viewer node or does not support viewing",
            )
            return {'CANCELLED'}

        with bpy.context.temp_override(node=viewer_node):
            bpy.ops.node.activate_viewer()

        viewer_node.ui_shortcut = self.viewer_index
        self.report({'INFO'}, rpt_("Assigned shortcut {:d} to {:s}").format(self.viewer_index, viewer_node.name))

        return {'FINISHED'}


class NODE_OT_viewer_shortcut_get(Operator):
    """Toggle a specific viewer node using 1,2,..,9 keys"""
    bl_idname = "node.viewer_shortcut_get"
    bl_label = "Fast Preview"
    bl_options = {'REGISTER', 'UNDO'}

    viewer_index: IntProperty(
        name="Viewer Index",
        description="Index corresponding to the shortcut, e.g. number key 1 corresponds to index 1 etc..")

    @classmethod
    def poll(cls, context):
        del cls
        space = context.space_data
        return (
            (space is not None) and
            space.type == 'NODE_EDITOR' and
            space.node_tree is not None and
            space.tree_type in {'CompositorNodeTree', 'GeometryNodeTree'}
        )

    def execute(self, context):
        nodes = context.space_data.edit_tree.nodes

        # Get viewer node with existing shortcut.
        viewer_node = None
        for n in nodes:
            if n.type == 'VIEWER' and n.ui_shortcut == self.viewer_index:
                viewer_node = n

        if not viewer_node:
            self.report({'INFO'}, rpt_("Shortcut {:d} is not assigned to a Viewer node yet").format(self.viewer_index))
            return {'CANCELLED'}

        with bpy.context.temp_override(node=viewer_node):
            bpy.ops.node.toggle_viewer()

        return {'FINISHED'}


class NODE_FH_image_node(FileHandler):
    bl_idname = "NODE_FH_image_node"
    bl_label = "Image node"
    bl_import_operator = "node.add_image"
    bl_file_extensions = ";".join((*bpy.path.extensions_image, *bpy.path.extensions_movie))

    @classmethod
    def poll_drop(cls, context):
        return (
            (context.area is not None) and
            (context.area.type == 'NODE_EDITOR') and
            (context.region is not None) and
            (context.region.type == 'WINDOW')
        )


classes = (
    NodeSetting,

    NODE_FH_image_node,

    NODE_OT_add_node,
    NODE_OT_swap_node,
    NODE_OT_add_empty_group,
    NODE_OT_swap_empty_group,
    NODE_OT_add_zone,
    NODE_OT_swap_zone,
    NODE_OT_add_simulation_zone,
    NODE_OT_add_repeat_zone,
    NODE_OT_add_foreach_geometry_element_zone,
    NODE_OT_add_closure_zone,
    NODE_OT_collapse_hide_unused_toggle,
    NODE_OT_interface_item_new,
    NODE_OT_interface_item_new_panel_toggle,
    NODE_OT_interface_item_duplicate,
    NODE_OT_interface_item_remove,
    NODE_OT_interface_item_make_panel_toggle,
    NODE_OT_interface_item_unlink_panel_toggle,
    NODE_OT_tree_path_parent,
    NODE_OT_viewer_shortcut_get,
    NODE_OT_viewer_shortcut_set,
)
