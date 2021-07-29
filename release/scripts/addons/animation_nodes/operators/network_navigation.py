import bpy
from collections import defaultdict
from .. tree_info import getNetworkByIdentifier, getNodesByType

class MoveViewToSubprogram(bpy.types.Operator):
    bl_idname = "an.network_navigation"
    bl_label = "Move View to Subprogram"
    bl_description = ""

    @classmethod
    def poll(cls, context):
        activeNode = getattr(context, "active_node", None)
        if activeNode is None: return False
        if not activeNode.select: return False
        if context.area.type != "NODE_EDITOR": return False
        return activeNode.isAnimationNode

    def execute(self, context):
        activeNode = context.active_node
        activeNetwork = activeNode.network

        if activeNode.bl_idname == "an_InvokeSubprogramNode":
            subnetwork = getNetworkByIdentifier(activeNode.subprogramIdentifier)
            self.jumpToNetwork(subnetwork, activeNode = subnetwork.getOwnerNode())
        elif getattr(activeNode, "isSubprogramNode", False):
            invokers = activeNode.getInvokeNodes()
            if len(invokers) == 1:
                self.jumpToNode(invokers[0])
            elif len(invokers) > 1:
                invokersByTree = defaultdict(list)
                for invokerNode in invokers:
                    invokersByTree[invokerNode.nodeTree].append(invokerNode)

                invokersInActiveTree = invokersByTree[activeNode.nodeTree]
                if len(invokersInActiveTree) > 0:
                    self.jumpToNodes(invokersInActiveTree, activeNode = invokersInActiveTree[0])
                elif len(invokersByTree.keys()) == 2:
                    # 2 because the active tree is also in the dict
                    self.jumpToNodes(invokers)
                else:
                    self.report({"INFO"}, "Cannot decide which node to jump to (yet).")
        elif activeNetwork.isSubnetwork:
            self.jumpToNetwork(activeNetwork, activeNode = activeNetwork.getOwnerNode())
        else:
            self.jumpToNetwork(activeNetwork)

        return {"FINISHED"}

    def jumpToNetwork(self, network, activeNode = None):
        nodes = network.getNodes()
        self.jumpToNodes(nodes, activeNode)

    def jumpToNode(self, node):
        self.jumpToNodes([node], activeNode = node)

    def jumpToNodes(self, nodes, activeNode = None):
        if len(nodes) == 0: return
        activeTree = nodes[0].id_data
        bpy.context.space_data.node_tree = activeTree
        bpy.ops.node.select_all(action = "DESELECT")
        for node in nodes:
            node.select = True
        bpy.ops.node.view_selected()
        activeTree.nodes.active = activeNode
