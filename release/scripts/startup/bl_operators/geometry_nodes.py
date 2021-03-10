# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

import bpy


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

    # Test object support for geometry node modifier (No volume, curve, or hair object support yet)
    if not ob or ob.type not in {'MESH', 'POINTCLOUD'}:
        return False

    return True


class NewGeometryNodesModifier(bpy.types.Operator):
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

        return {'FINISHED'}


class NewGeometryNodeTreeAssign(bpy.types.Operator):
    """Create a new geometry node group and assign it to the active modifier"""

    bl_idname = "node.new_geometry_node_group_assign"
    bl_label = "Assign New Geometry Node Group"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return geometry_modifier_poll(context)

    def execute(self, context):
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
