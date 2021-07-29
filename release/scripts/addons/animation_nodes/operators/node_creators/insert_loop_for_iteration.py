import bpy
from bpy.props import *
from mathutils import Vector
from ... sockets.info import isList
from . node_creator import NodeCreator
from ... tree_info import getNodeByIdentifier

class InsertLoopForIterator(bpy.types.Operator, NodeCreator):
    bl_idname = "an.insert_loop_for_iterator"
    bl_label = "Insert Loop for Iterator"

    nodeIdentifier = StringProperty()
    socketIndex = IntProperty()

    def insert(self):
        try:
            sourceNode = getNodeByIdentifier(self.nodeIdentifier)
            socket = sourceNode.outputs[self.socketIndex]
        except: return
        if not isList(socket.bl_idname): return

        loopInputNode = self.newNode("an_LoopInputNode")
        loopInputNode.newIterator(socket.dataType)
        invokeNode = self.newNode("an_InvokeSubprogramNode", move = False, mouseOffset = False)
        invokeNode.location = sourceNode.viewLocation + Vector((250, 0))

        invokeNode.subprogramIdentifier = loopInputNode.identifier
        self.updateSubprograms()

        socket.linkWith(invokeNode.inputs[0])
