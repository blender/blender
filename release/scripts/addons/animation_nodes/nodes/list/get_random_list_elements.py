import bpy
import random
from bpy.props import *
from ... sockets.info import isList
from ... events import propertyChanged
from ... base_types import AnimationNode, ListTypeSelectorSocket

selectionTypeItems = [
    ("SINGLE", "Single", "Select only one random element from the list", "NONE", 0),
    ("MULTIPLE", "Multiple", "Select multiple random elements from the list", "NONE", 1)]

class GetRandomListElementsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_GetRandomListElementsNode"
    bl_label = "Get Random List Elements"

    assignedType = ListTypeSelectorSocket.newProperty(default = "Float List")

    selectionType = EnumProperty(name = "Select Type", default = "MULTIPLE",
        items = selectionTypeItems, update = AnimationNode.refresh)

    nodeSeed = IntProperty(update = propertyChanged)

    def setup(self):
        self.randomizeNodeSeed()

    def create(self):
        prop = ("assignedType", "LIST")
        self.newInput("Integer", "Seed", "seed")
        self.newInput(ListTypeSelectorSocket(
            "List", "inList", "LIST", prop, dataIsModified = True))

        if self.selectionType == "SINGLE":
            self.newOutput(ListTypeSelectorSocket("Element", "outElement", "BASE", prop))
        elif self.selectionType == "MULTIPLE":
            self.newInput("Integer", "Amount", "amount", value = 3, minValue = 0)
            self.newOutput(ListTypeSelectorSocket("List", "outList", "LIST", prop))

    def draw(self, layout):
        layout.prop(self, "selectionType", text = "")
        layout.prop(self, "nodeSeed", text = "Node Seed")

    def getExecutionCode(self, required):
        yield "_seed = self.nodeSeed * 154245 + seed * 13412"
        if self.selectionType == "SINGLE":
            yield "random.seed(_seed)"
            yield "if len(inList) == 0: outElement = self.outputs['Element'].getDefaultValue()"
            yield "else: outElement = random.choice(inList)"
        elif self.selectionType == "MULTIPLE":
            yield "_seed += amount * 45234"
            yield "_amount = min(max(amount, 0), len(inList))"
            yield "outList = AN.algorithms.lists.sample('%s', inList, _amount, _seed)" % self.assignedType

    def getUsedModules(self):
        return ["random"]

    def assignType(self, listDataType):
        if not isList(listDataType): return
        if listDataType == self.assignedType: return
        self.assignedType = listDataType
        self.refresh()

    def duplicate(self, sourceNode):
        self.randomizeNodeSeed()

    def randomizeNodeSeed(self):
        self.nodeSeed = int(random.random() * 100)
