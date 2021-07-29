import bpy
from bpy.props import *
from . node_creator import NodeCreator
from ... sockets.info import isList, toBaseDataType

class InsertDataCreationNode(bpy.types.Operator, NodeCreator):
    bl_idname = "an.insert_data_creation_node"
    bl_label = "Insert Data Creation Node"

    socketIndex = IntProperty(default = 0)

    @property
    def needsMenu(self):
        return len(list(self.iterPossibleSockets())) > 1

    def drawMenu(self, layout):
        layout.operator_context = "EXEC_DEFAULT"
        for socket in self.iterPossibleSockets():
            if len(socket.allowedInputTypes) > 0:
                props = layout.operator(self.bl_idname, text = socket.getDisplayedName())
                props.socketIndex = socket.getIndex()

    def iterPossibleSockets(self):
        for socket in self.activeNode.inputs:
            if not socket.hide and len(socket.allowedInputTypes) > 0:
                yield socket

    def insert(self):
        activeNode = self.activeNode
        if self.usedMenu: socket = activeNode.inputs[self.socketIndex]
        else:
            try: socket = self.iterPossibleSockets().__next__()
            except: return

        if socket.dataType in dataCreationNodes:
            originNode = self.newNode(dataCreationNodes[socket.dataType])
        elif isList(socket.bl_idname):
            originNode = self.newNode("an_CreateListNode")
            originNode.assignedType = toBaseDataType(socket.bl_idname)
        else:
            originNode = self.newNode("an_DataInputNode")
            originNode.assignedType = socket.dataType
            originNode.inputs[0].setProperty(socket.getProperty())

        socket.linkWith(originNode.outputs[0])
        self.setActiveNode(originNode)

dataCreationNodes = {
    "Interpolation" : "an_ConstructInterpolationNode",
    "Particle System" : "an_ParticleSystemsFromObjectNode",
    "BVHTree" : "an_ConstructBVHTreeNode",
    "KDTree" : "an_ConstructKDTreeNode",
    "Edge Indices" : "an_CreateEdgeIndicesNode",
    "Polygon Indices" : "an_CreatePolygonIndicesNode",
    "Vector" : "an_CombineVectorNode",
    "Euler" : "an_CombineEulerNode",
    "Struct" : "an_SetStructElementsNode",
    "Quaternion" : "an_CombineQuaternionNode",
    "Mesh Data" : "an_CombineMeshDataNode",
    "Matrix" : "an_ComposeMatrixNode"
}
