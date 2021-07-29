import bpy
from ... events import executionCodeChanged
from ... utils.layout import splitAlignment
from ... base_types import AnimationNode

class SetStructElementsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SetStructElementsNode"
    bl_label = "Set Struct Elements"

    def setup(self):
        self.newInput("Struct", "Struct", "struct", dataIsModified = True)
        self.newInput("Node Control", "New Input")
        self.newOutput("Struct", "Struct", "struct")

    def drawControlSocket(self, layout, socket):
        left, right = splitAlignment(layout)
        left.label(socket.name)
        self.invokeSelector(right, "DATA_TYPE", "newInputSocket",
            icon = "ZOOMIN", emboss = False)

    def edit(self):
        newInputSocket = self.inputs["New Input"]
        dataOrigin = newInputSocket.dataOrigin
        directOrigin = newInputSocket.directOrigin

        if dataOrigin is None: return
        if dataOrigin.dataType == "Node Control": return
        socket = self.newInputSocket(dataOrigin.dataType, dataOrigin.getDisplayedName())
        socket.linkWith(directOrigin)

    def newInputSocket(self, dataType, name = None):
        if name is None: name = dataType
        socket = self.newInput(dataType, name, "inputSocket")
        socket.dataIsModified = True
        socket.text = name
        socket.moveable = True
        socket.removeable = True
        socket.display.text = True
        socket.textProps.editable = True
        socket.display.textInput = True
        socket.display.removeOperator = True
        socket.moveUp()
        return socket

    def getInputSocketVariables(self):
        variables = {socket.identifier : "input_" + str(i) for i, socket in enumerate(self.inputs[1:-1])}
        variables["struct"] = "struct"
        variables["New Input"] = "newInput"
        return variables

    def getExecutionCode(self):
        for i, socket in enumerate(self.inputs[1:-1]):
            yield "struct[({}, {})] = input_{}".format(repr(socket.dataType), repr(socket.text), i)

    def socketChanged(self):
        executionCodeChanged()
