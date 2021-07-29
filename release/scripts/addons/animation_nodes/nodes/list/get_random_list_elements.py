import bpy
import random
from bpy.props import *
from ... events import propertyChanged
from ... sockets.info import isList, toBaseDataType
from ... base_types import AnimationNode, AutoSelectListDataType

selectionTypeItems = [
    ("SINGLE", "Single", "Select only one random element from the list", "NONE", 0),
    ("MULTIPLE", "Multiple", "Select multiple random elements from the list", "NONE", 1)]

class GetRandomListElementsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_GetRandomListElementsNode"
    bl_label = "Get Random List Elements"

    assignedType = StringProperty(update = AnimationNode.refresh, default = "Float List")

    selectionType = EnumProperty(name = "Select Type", default = "MULTIPLE",
        items = selectionTypeItems, update = AnimationNode.refresh)

    nodeSeed = IntProperty(update = propertyChanged)

    def setup(self):
        self.randomizeNodeSeed()

    def create(self):
        listDataType = self.assignedType
        baseDataType = toBaseDataType(listDataType)

        self.newInput("Integer", "Seed", "seed")
        self.newInput(listDataType, "List", "inList", dataIsModified = True)
        if self.selectionType == "SINGLE":
            self.newOutput(baseDataType, "Element", "outElement")
        elif self.selectionType == "MULTIPLE":
            self.newInput("Integer", "Amount", "amount", value = 3, minValue = 0)
            self.newOutput(listDataType, "List", "outList")

        self.newSocketEffect(AutoSelectListDataType("assignedType", "LIST",
            [(self.inputs[1], "LIST"),
             (self.outputs[0], "BASE" if self.selectionType == "SINGLE" else "LIST")]
        ))

    def draw(self, layout):
        layout.prop(self, "selectionType", text = "")
        layout.prop(self, "nodeSeed", text = "Node Seed")

    def getExecutionCode(self):
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

    def duplicate(self, sourceNode):
        self.randomizeNodeSeed()

    def randomizeNodeSeed(self):
        self.nodeSeed = int(random.random() * 100)
