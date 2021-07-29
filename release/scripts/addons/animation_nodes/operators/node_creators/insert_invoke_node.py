import bpy
from bpy.props import *
from . node_creator import NodeCreator
from ... tree_info import getNodeByIdentifier

class InsertSubprogramInvokeNode(bpy.types.Operator, NodeCreator):
    bl_idname = "an.insert_invoke_subprogram_node"
    bl_label = "Insert Invoke Node"

    subprogramIdentifier = StringProperty()

    def insert(self):
        invokeNode = self.newNode("an_InvokeSubprogramNode")
        invokeNode.subprogramIdentifier = self.subprogramIdentifier
