import bpy
from bpy.props import *
from .. tree_info import getNodeByIdentifier

class MoveViewToNode(bpy.types.Operator):
    bl_idname = "an.move_view_to_node"
    bl_label = "Move View to Node"
    bl_description = ""

    nodeIdentifier = StringProperty(default = "")
    treeName = StringProperty()
    nodeName = StringProperty()

    @classmethod
    def poll(cls, context):
        try: return context.space_data.node_tree.bl_idname == "an_AnimationNodeTree"
        except: return False

    def execute(self, context):
        searchNode = self.getNode()
        if searchNode is None: return {"CANCELLED"}

        tree = searchNode.id_data

        context.space_data.node_tree = tree
        for node in tree.nodes:
            node.select = False

        searchNode.select = True
        tree.nodes.active = searchNode

        bpy.ops.node.view_selected()
        return {"FINISHED"}

    def getNode(self):
        if self.nodeIdentifier != "":
            try: return getNodeByIdentifier(self.nodeIdentifier)
            except: return None
        else:
            try: return bpy.data.node_groups[self.treeName].nodes[self.nodeName]
            except: return None
