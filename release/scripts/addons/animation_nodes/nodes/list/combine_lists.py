import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... sockets.info import getListDataTypes, toBaseDataType, toListDataType

class CombineListsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CombineListsNode"
    bl_label = "Combine Lists"
    dynamicLabelType = "ALWAYS"
    onlySearchTags = True

    @classmethod
    def getSearchTags(cls):
        return [("Combine " + dataType, {"assignedType" : repr(toBaseDataType(dataType))})
                for dataType in getListDataTypes()]

    def assignedTypeChanged(self, context):
        self.recreateSockets()

    assignedType = StringProperty(update = assignedTypeChanged)

    def setup(self):
        self.assignedType = "Float"

    def draw(self, layout):
        row = layout.row(align = True)
        self.invokeFunction(row, "newInputSocket",
            text = "New Input",
            description = "Create a new input socket",
            icon = "PLUS")
        self.invokeFunction(row, "removeUnlinkedInputs",
            description = "Remove unlinked inputs",
            confirm = True,
            icon = "X")

    def drawAdvanced(self, layout):
        self.invokeSelector(layout, "DATA_TYPE", "assignListDataType",
            dataTypes = "LIST", text = "Change Type", icon = "TRIA_RIGHT")

    def drawLabel(self):
        return "Combine " + toListDataType(self.assignedType)

    def getInputSocketVariables(self):
        return {socket.identifier : "list_" + str(i) for i, socket in enumerate(self.inputs)}

    def getExecutionCode(self):
        listNames = ["list_" + str(i) for i, socket in enumerate(self.inputs[:-1])]
        joinListsCode = self.outputs[0].getJoinListsCode()
        yield "outList = " + joinListsCode.replace("value", ", ".join(listNames))

    def edit(self):
        emptySocket = self.inputs["..."]
        origin = emptySocket.directOrigin
        if origin is None: return
        socket = self.newInputSocket()
        socket.linkWith(origin)
        emptySocket.removeLinks()

    def assignListDataType(self, listDataType):
        self.assignedType = toBaseDataType(listDataType)

    def recreateSockets(self, inputAmount = 2):
        self.clearSockets()

        self.newInput("Node Control", "...")
        for _ in range(inputAmount):
            self.newInputSocket()
        self.newOutput(toListDataType(self.assignedType), "List", "outList")

    def newInputSocket(self):
        socket = self.newInput(toListDataType(self.assignedType), "List")
        socket.defaultDrawType = "PREFER_PROPERTY"
        socket.textProps.editable = True
        socket.dataIsModified = True
        socket.display.text = True
        socket.removeable = True
        socket.moveable = True
        socket.text = "List"
        socket.moveUp()

        if len(self.inputs) > 2:
            socket.copyDisplaySettingsFrom(self.inputs[0])

        return socket

    def removeUnlinkedInputs(self):
        for socket in self.inputs[:-1]:
            if not socket.is_linked:
                socket.remove()
