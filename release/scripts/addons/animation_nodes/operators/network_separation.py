import bpy
from . an_operator import AnimationNodeOperator

class SelectActiveNetwork(bpy.types.Operator, AnimationNodeOperator):
    bl_idname = "an.select_active_network"
    bl_label = "Select Active Network"
    bl_description = "Select all nodes that are connected to the active node"

    def execute(self, context):
        nodes = context.active_node.network.getNodes()
        for node in nodes:
            node.select = True
        return {"FINISHED"}

class ColorActiveNetwork(bpy.types.Operator, AnimationNodeOperator):
    bl_idname = "an.color_active_network"
    bl_label = "Color Active Network"
    bl_description = "Copy the color of the active node to all nodes in the same network"

    def execute(self, context):
        sourceNode = context.active_node
        nodes = sourceNode.network.getNodes()
        for node in nodes:
            node.use_custom_color = True
            node.color = sourceNode.color
        return {"FINISHED"}

class FrameActiveNetwork(bpy.types.Operator, AnimationNodeOperator):
    bl_idname = "an.frame_active_network"
    bl_label = "Frame Active Network"
    bl_description = "Make a new frame which contains the whole network"

    def execute(self, context):
        nodes = context.active_node.network.getNodes()
        nodeTree = nodes[0].getNodeTree()
        frameNode = nodeTree.nodes.new(type = "NodeFrame")
        for node in nodes:
            while node.parent is not None:
                node = node.parent
            node.parent = frameNode
        return {"FINISHED"}
