import bpy
import random
from bpy.props import *
from ... base_types import VectorizedNode
from ... sockets.info import toBaseDataType
from ... tree_info import getNodeByIdentifier
from . subprogram_sockets import subprogramInterfaceChanged

class LoopGeneratorOutputNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_LoopGeneratorOutputNode"
    bl_label = "Loop Generator Output"
    dynamicLabelType = "ALWAYS"

    def settingChanged(self, context):
        self.refresh()
        subprogramInterfaceChanged()

    outputName = StringProperty(name = "Generator Name", update = settingChanged)
    loopInputIdentifier = StringProperty(update = settingChanged)
    sortIndex = IntProperty(default = 0)

    listDataType = StringProperty(default = "Vector List", update = settingChanged)
    useList = VectorizedNode.newVectorizeProperty()

    def setup(self):
        self.sortIndex = getRandomInt()
        self.outputName = self.listDataType

    def create(self):
        listDataType = self.listDataType
        baseDataType = toBaseDataType(listDataType)

        if self.listDataType == "Generic List":
            self.newInput("Generic", "Generic", "input")
        else:
            self.newVectorizedInput(baseDataType, "useList",
                (baseDataType, "input", dict(defaultDrawType = "TEXT_ONLY")),
                (listDataType, "input", dict(defaultDrawType = "TEXT_ONLY")))

        self.newInput("Boolean", "Condition", "condition", value = True, hide = True)

    def draw(self, layout):
        node = self.loopInputNode
        if node is not None:
            layout.label(node.subprogramName, icon = "GROUP_VERTEX")

    def drawAdvanced(self, layout):
        layout.prop(self, "outputName", text = "Name")
        self.invokeSelector(layout, "DATA_TYPE", "setListDataType",
            dataTypes = "LIST", text = "Change Type", icon = "TRIA_RIGHT")

        layout.label("No vectorization for generic type.", icon = "INFO")

    def drawLabel(self):
        return self.outputName

    def edit(self):
        network = self.network
        if network.type != "Invalid": return
        if network.loopInAmount != 1: return
        loopInput = network.getLoopInputNode()
        if self.loopInputIdentifier == loopInput.identifier: return
        self.loopInputIdentifier = loopInput.identifier

    def setListDataType(self, dataType):
        self.listDataType = dataType

    def delete(self):
        subprogramInterfaceChanged()

    def duplicate(self, source):
        self.sortIndex = getRandomInt()
        subprogramInterfaceChanged()

    @property
    def loopInputNode(self):
        try: return getNodeByIdentifier(self.loopInputIdentifier)
        except: return None

    @property
    def conditionSocket(self):
        return self.inputs[1]

    @property
    def dataInputSocket(self):
        return self.inputs[0]

    @property
    def generatorType(self):
        if self.listDataType == "Generic List":
            return "append"
        else:
            return "extend" if self.useList else "append"

def getRandomInt():
    random.seed()
    return int(random.random() * 10000) + 100
