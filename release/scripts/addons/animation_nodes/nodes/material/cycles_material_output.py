import bpy
from bpy.props import *
from ... base_types import AnimationNode

allowedSocketTypes = {
    "NodeSocketVector" : "an_VectorSocket",
    "NodeSocketColor" : "an_ColorSocket",
    "NodeSocketFloatFactor" : "an_FloatSocket",
    "NodeSocketFloat" : "an_FloatSocket" }


class CyclesMaterialOutputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CyclesMaterialOutputNode"
    bl_label = "Cycles Material Output"
    bl_width_default = 165

    def getPossibleSocketItems(self, context):
        sockets = self.getPossibleSockets()
        items = []
        for socket in sockets:
            if socket.bl_idname in allowedSocketTypes.keys():
                items.append((socket.identifier, socket.identifier, ""))
        return items

    def getPossibleSockets(self):
        node = self.getSelectedNode()
        identifiers = []
        if node is not None:
            for socket in node.inputs:
                if socket.bl_idname in allowedSocketTypes.keys():
                    identifiers.append(socket)
        return identifiers

    materialName = StringProperty(update = AnimationNode.refresh)
    nodeName = StringProperty(update = AnimationNode.refresh)
    socketIdentifier = EnumProperty(name = "Socket", items = getPossibleSocketItems,
        update = AnimationNode.refresh)

    def create(self):
        socket = self.getSelectedSocket()
        if socket is not None:
            data = socket.default_value
            self.newInput(allowedSocketTypes[socket.bl_idname], "Data", "data")
            self.inputs["Data"].setProperty(data)

    def draw(self, layout):
        layout.prop_search(self, "materialName", bpy.data, "materials", text = "", icon = "MATERIAL_DATA")
        material = bpy.data.materials.get(self.materialName)
        if material is None: return

        nodeTree = material.node_tree
        if nodeTree is None: return

        layout.prop_search(self, "nodeName", nodeTree, "nodes", text = "", icon = "NODE")
        node = material.node_tree.nodes.get(self.nodeName)
        if node is None: return

        if self.hasPossibleInputs(node):
            layout.prop(self, "socketIdentifier", text = "")
        else:
            layout.label("No Animatable Inputs", icon = "INFO")

    def hasPossibleInputs(self, node):
        keys = allowedSocketTypes.keys()
        for socket in node.inputs:
            if socket.bl_idname in keys:
                return True
        return False

    def getExecutionCode(self):
        inputSocket = self.inputs.get("Data")
        if inputSocket is None: return

        yield "socket = self.getSelectedSocket()"
        yield "if socket is not None:"

        if inputSocket.dataType in ("Float", "Vector"):
            yield "    if socket.default_value != data:"
        elif inputSocket.dataType == "Color":
            yield "    if tuple(socket.default_value) != tuple(data):"

        yield "        socket.default_value = data"

    def getBakeCode(self):
        yield "socket = self.getSelectedSocket()"
        yield "if socket is not None:"
        yield "    socket.keyframe_insert('default_value')"

    def edit(self):
        inputSocket = self.inputs.get("Data")
        if inputSocket is None: return

        if inputSocket.isLinked:
            originIdName = inputSocket.dataOrigin.bl_idname
            possibleIdentifiers = self.getInputIdentifiersFromSocketType(originIdName)
            if inputSocket.bl_idname != originIdName and len(possibleIdentifiers) > 0:
                self.socketIdentifier = possibleIdentifiers[0]

    def getInputIdentifiersFromSocketType(self, searchType):
        identifiers = []
        sockets = self.getPossibleSockets()
        for socket in sockets:
            if allowedSocketTypes[socket.bl_idname] == searchType:
                identifiers.append(socket.identifier)
        return identifiers

    def getSelectedNode(self):
        material = bpy.data.materials.get(self.materialName)
        if material is None: return None

        nodeTree = material.node_tree
        if nodeTree is None: return

        node = nodeTree.nodes.get(self.nodeName)
        return node

    def getSelectedSocket(self):
        node = self.getSelectedNode()
        if node is not None:
            socket = self.getInputSocketWithIdentifier(node, self.socketIdentifier)
            return socket
        return None

    def getInputSocketWithIdentifier(self, node, identifier):
        for socket in node.inputs:
            if socket.identifier == identifier: return socket
        return None
