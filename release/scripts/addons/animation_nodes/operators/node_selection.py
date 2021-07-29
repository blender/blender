import bpy
from .. tree_info import getDirectlyLinkedSockets

class NodeOperator:
    @classmethod
    def poll(cls, context):
        tree = context.getActiveAnimationNodeTree()
        if tree is None: return False
        if tree.nodes.active is None: return False
        return True

class SelectDependentNodes(bpy.types.Operator, NodeOperator):
    bl_idname = "an.select_dependent_nodes"
    bl_label = "Select Dependent Nodes"

    def execute(self, context):
        for node in getNodesWhenFollowingLinks(context.active_node, followOutputs = True):
            node.select = True
        return {"FINISHED"}

class SelectDependenciesNodes(bpy.types.Operator, NodeOperator):
    bl_idname = "an.select_dependencies"
    bl_label = "Select Dependencies"

    def execute(self, context):
        for node in getNodesWhenFollowingLinks(context.active_node, followInputs = True):
            node.select = True
        return {"FINISHED"}

class SelectNetwork(bpy.types.Operator, NodeOperator):
    bl_idname = "an.select_network"
    bl_label = "Select Network"

    def execute(self, context):
        for node in context.active_node.network.getNodes():
            node.select = True
        return {"FINISHED"}

def getNodesWhenFollowingLinks(startNode, followInputs = False, followOutputs = False):
    nodes = set()
    nodesToCheck = {startNode}
    while nodesToCheck:
        node = nodesToCheck.pop()
        nodes.add(node)
        sockets = []
        if followInputs: sockets.extend(node.inputs)
        if followOutputs: sockets.extend(node.outputs)
        for socket in sockets:
            for linkedSocket in getDirectlyLinkedSockets(socket):
                node = linkedSocket.node
                if node not in nodes: nodesToCheck.add(node)
    nodes.remove(startNode)
    return list(nodes)
