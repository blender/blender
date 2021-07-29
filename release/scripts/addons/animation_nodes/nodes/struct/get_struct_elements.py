import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode
from ... utils.layout import splitAlignment, writeText

class GetStructElementsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_GetStructElementsNode"
    bl_label = "Get Struct Elements"

    makeCopies = BoolProperty(name = "Make Copies", default = True,
        description = "Copy the data before outputting it",
        update = executionCodeChanged)

    errorMessage = StringProperty()

    def setup(self):
        self.newInput("Struct", "Struct", "struct")
        self.newOutput("Node Control", "New Output")

    def draw(self, layout):
        if self.errorMessage != "":
            writeText(layout, self.errorMessage, icon = "ERROR")

    def drawAdvanced(self, layout):
        layout.prop(self, "makeCopies")

    def drawControlSocket(self, layout, socket):
        left, right = splitAlignment(layout)
        left.label(socket.name)
        self.invokeSelector(right, "DATA_TYPE", "newOutputSocket",
            icon = "ZOOMIN", emboss = False)

    def edit(self):
        for target in self.outputs["New Output"].dataTargets:
            if target.dataType == "Node Control": continue
            socket = self.newOutputSocket(target.dataType, target.getDisplayedName())
            socket.linkWith(target)

    def newOutputSocket(self, dataType, name = None):
        if name is None: name = dataType
        socket = self.newOutput(dataType, name, "outputSocket")
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

    def getOutputSocketVariables(self):
        variables = {socket.identifier : "output_" + str(i) for i, socket in enumerate(self.outputs[:-1])}
        variables["New Output"] = "newOutput"
        return variables

    def getExecutionCode(self):
        yield "self.errorMessage = ''"
        for i, socket in enumerate(self.outputs[:-1]):
            name = "output_" + str(i)
            structAccess = "struct[({}, {})]".format(repr(socket.dataType), repr(socket.text))

            yield "try:"
            if socket.isCopyable() and self.makeCopies:
                yield "    {} = {}".format(name, socket.getCopyExpression().replace("value", structAccess))
            else:
                yield "    {} = {}".format(name, structAccess)

            yield "except:"
            yield "    socket = self.outputs[{}]".format(i)
            yield "    self.errorMessage = self.getErrorMessage(struct, socket)"
            if hasattr(socket, "getDefaultValueCode"):
                yield "    {} = {}".format(name, socket.getDefaultValueCode())
            else:
                yield "    {} = socket.getDefaultValue()".format(name)

    def getErrorMessage(self, struct, socket):
        possibleDataTypes = struct.findDataTypesWithName(socket.text)
        if len(possibleDataTypes) == 0:
            possibleNames = struct.findNamesWithDataType(socket.dataType)
            if len(possibleNames) == 0:
                return "Name {} does not exist".format(repr(socket.text))
            else:
                return "Name {} does not exist.\nOther names with type {}: {}".format(repr(socket.text), repr(socket.dataType), possibleNames)
        else:
            return "Name {} only exists with these data types: {}".format(repr(socket.text), possibleDataTypes)

    def socketChanged(self):
        executionCodeChanged()
