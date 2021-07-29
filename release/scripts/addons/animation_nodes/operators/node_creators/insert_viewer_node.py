import bpy
from bpy.props import *
from . node_creator import NodeCreator

class InsertViewerNode(bpy.types.Operator, NodeCreator):
    bl_idname = "an.insert_viewer_node"
    bl_label = "Insert Viewer Node"

    socketIndex = IntProperty(default = 0)

    @property
    def needsMenu(self):
        return len(list(self.iterPossibleSockets())) > 1

    def drawMenu(self, layout):
        layout.operator_context = "EXEC_DEFAULT"
        for socket in self.iterPossibleSockets():
            props = layout.operator(self.bl_idname, text = socket.getDisplayedName())
            props.socketIndex = socket.getIndex()

    def iterPossibleSockets(self):
        for socket in self.activeNode.outputs:
            if not socket.hide and len(socket.allowedInputTypes) > 0:
                yield socket

    def insert(self):
        activeNode = self.activeNode

        if self.usedMenu: socket = activeNode.outputs[self.socketIndex]
        else:
            try: socket = self.iterPossibleSockets().__next__()
            except: return

        dataType = socket.dataType
        if dataType == "Interpolation":
            viewerNode = self.newNode("an_InterpolationViewerNode")
        else:
            viewerNode = self.newNode("an_ViewerNode")
        socket.linkWith(viewerNode.inputs[0])

        self.setActiveNode(viewerNode)
