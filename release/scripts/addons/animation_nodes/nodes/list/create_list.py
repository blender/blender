import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... utils.selection import getSortedSelectedObjectNames
from ... sockets.info import getListDataTypes, toBaseDataType, toListDataType

class CreateListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CreateListNode"
    bl_label = "Create List"
    dynamicLabelType = "ALWAYS"
    onlySearchTags = True

    @classmethod
    def getSearchTags(cls):
        return [("Create " + dataType, {"assignedType" : repr(toBaseDataType(dataType))})
                for dataType in getListDataTypes()]

    def assignedTypeChanged(self, context):
        self.recreateSockets()

    assignedType = StringProperty(update = assignedTypeChanged)

    def hideStatusChanged(self, context):
        for socket in self.inputs:
            socket.hide = self.hideInputs

    hideInputs = BoolProperty(name = "Hide Inputs", default = False, update = hideStatusChanged)

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

        self.drawTypeSpecifics(layout)

    def drawAdvanced(self, layout):
        self.invokeSelector(layout, "DATA_TYPE", "assignListDataType",
            dataTypes = "LIST", text = "Change Type", icon = "TRIA_RIGHT")

        layout.prop(self, "hideInputs")

        self.invokeFunction(layout, "removeUnlinkedInputs", "Remove Unlinked Inputs", icon = "X")

        self.drawAdvancedTypeSpecific(layout)

    def drawLabel(self):
        return "Create " + toListDataType(self.assignedType)

    def getInputSocketVariables(self):
        return {socket.identifier : "element_" + str(i) for i, socket in enumerate(self.inputs)}

    def getExecutionCode(self):
        variableNames = ["element_" + str(i) for i, socket in enumerate(self.inputs) if socket.dataType != "Node Control"]
        createPyListExpression = "[" + ", ".join(variableNames) + "]"
        createListExpression = self.outputs[0].getFromValuesCode().replace("value", createPyListExpression)
        return "outList = " + createListExpression

    def edit(self):
        self.updateOutputName()
        emptySocket = self.inputs["..."]
        origin = emptySocket.directOrigin
        if origin is None: return
        socket = self.newInputSocket()
        socket.linkWith(origin)
        emptySocket.removeLinks()

    def assignListDataType(self, listDataType):
        self.assignedType = toBaseDataType(listDataType)

    def assignBaseDataType(self, baseDataType, inputAmount = 2):
        self.assignedType = baseDataType
        self.recreateSockets(inputAmount)

    def recreateSockets(self, inputAmount = 2):
        self.clearSockets()

        self.newInput("Node Control", "...")
        for i in range(inputAmount):
            self.newInputSocket()
        self.newOutput(toListDataType(self.assignedType), "List", "outList")

    def newInputSocket(self):
        socket = self.newInput(self.assignedType, "Element")
        socket.dataIsModified = True
        socket.display.text = True
        socket.text = "Element"
        socket.removeable = True
        socket.moveable = True
        socket.defaultDrawType = "PREFER_PROPERTY"
        socket.moveUp()

        if len(self.inputs) > 2:
            socket.copyDisplaySettingsFrom(self.inputs[0])

        self.updateOutputName()
        return socket

    def updateOutputName(self):
        name = "List ({})".format(len(self.inputs) - 1)
        if len(self.outputs) > 0:
            self.outputs[0].name = name

    def removeUnlinkedInputs(self):
        for socket in self.inputs[:-1]:
            if not socket.is_linked:
                socket.remove()


    # type specific stuff
    #############################

    def drawTypeSpecifics(self, layout):
        if len(self.inputs) == 1:
            self.drawAdvancedTypeSpecific(layout)

    def drawAdvancedTypeSpecific(self, layout):
        if self.assignedType in ("Object", "Spline"):
            self.invokeFunction(layout, "createInputsForSelectedObjects", text = "From Selection", icon = "PLUS")
        if self.assignedType == "Object Group":
            self.invokeFunction(layout, "createInputsForSelectedObjectGroups", text = "From Selection", icon = "PLUS")

    def createInputsForSelectedObjects(self):
        names = getSortedSelectedObjectNames()
        for name in names:
            socket = self.newInputSocket()
            socket.objectName = name

    def createInputsForSelectedObjectGroups(self):
        groups = self.getGroupsOfObjects(bpy.context.selected_objects)
        for group in groups:
            socket = self.newInputSocket()
            socket.groupName = group.name

    def getGroupsOfObjects(self, objects):
        groups = set()
        for object in objects:
            groups.update(group for group in bpy.data.groups if object.name in group.objects)
        return list(groups)
