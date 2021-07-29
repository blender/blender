import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... events import executionCodeChanged

compareTypeItems = [
    ("ALL_TRUE", "All True", "Only true if all elements are true", "NONE", 0),
    ("ALL_FALSE", "All False", "Only true if all elements are false", "NONE", 1),
    ("NOT_ALL_TRUE", "Not All True", "Only true if at least one element is false", "NONE", 2),
    ("NOT_ALL_FALSE", "Not All False", "Only true if at least one element is true", "NONE", 3)]

compareLabels = {t[0] : t[1] for t in compareTypeItems}

class BooleanListLogicNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_BooleanListLogicNode"
    bl_label = "Boolean List Logic"
    dynamicLabelType = "HIDDEN_ONLY"

    compareType = EnumProperty(name = "Compare Type", default = "ALL_TRUE",
        items = compareTypeItems, update = executionCodeChanged)

    def create(self):
        self.newInput("Boolean List", "Boolean List", "inList")
        self.newOutput("Boolean", "Result", "result")

    def draw(self, layout):
        layout.prop(self, "compareType", text = "")

    def drawLabel(self):
        return compareLabels[self.compareType]

    def getExecutionCode(self):
        t = self.compareType

        yield "if len(inList) > 0:"
        if t == "ALL_TRUE":        yield "    result = inList.allTrue()"
        elif t == "ALL_FALSE":     yield "    result = inList.allFalse()"
        elif t == "NOT_ALL_TRUE":  yield "    result = not inList.allTrue()"
        elif t == "NOT_ALL_FALSE": yield "    result = not inList.allFalse()"
        yield "else:"
        if t == "ALL_TRUE":        yield "    result = True"
        elif t == "ALL_FALSE":     yield "    result = True"
        elif t == "NOT_ALL_TRUE":  yield "    result = False"
        elif t == "NOT_ALL_FALSE": yield "    result = False"
