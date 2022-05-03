# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator


def geometry_node_group_empty_new():
    group = bpy.data.node_groups.new("Geometry Nodes", 'GeometryNodeTree')
    group.inputs.new('NodeSocketGeometry', "Geometry")
    group.outputs.new('NodeSocketGeometry', "Geometry")
    input_node = group.nodes.new('NodeGroupInput')
    output_node = group.nodes.new('NodeGroupOutput')
    output_node.is_active_output = True

    input_node.select = False
    output_node.select = False

    input_node.location.x = -200 - input_node.width
    output_node.location.x = 200

    group.links.new(output_node.inputs[0], input_node.outputs[0])

    return group


def geometry_modifier_poll(context):
    ob = context.object

    # Test object support for geometry node modifier
    if not ob or ob.type not in {'MESH', 'POINTCLOUD', 'VOLUME', 'CURVE', 'FONT', 'CURVES'}:
        return False

    return True


class NewGeometryNodesModifier(Operator):
    """Create a new modifier with a new geometry node group"""

    bl_idname = "node.new_geometry_nodes_modifier"
    bl_label = "New Geometry Node Modifier"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return geometry_modifier_poll(context)

    def execute(self, context):
        modifier = context.object.modifiers.new("GeometryNodes", "NODES")

        if not modifier:
            return {'CANCELLED'}

        group = geometry_node_group_empty_new()
        modifier.node_group = group

        return {'FINISHED'}


class NewGeometryNodeTreeAssign(Operator):
    """Create a new geometry node group and assign it to the active modifier"""

    bl_idname = "node.new_geometry_node_group_assign"
    bl_label = "Assign New Geometry Node Group"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return geometry_modifier_poll(context)

    def execute(self, context):
        if context.area.type == 'PROPERTIES':
            modifier = context.modifier
        else:
            modifier = context.object.modifiers.active

        if not modifier:
            return {'CANCELLED'}

        group = geometry_node_group_empty_new()
        modifier.node_group = group

        return {'FINISHED'}


classes = (
    NewGeometryNodesModifier,
    NewGeometryNodeTreeAssign,
)
