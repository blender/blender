import bpy
from bpy.props import *
from ... utils.code import isCodeValid
from ... events import executionCodeChanged
from ... base_types import AnimationNode

class ObjectAttributeInputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ObjectAttributeInputNode"
    bl_label = "Object Attribute Input"
    bl_width_default = 160

    attribute = StringProperty(name = "Attribute", default = "",
        update = executionCodeChanged)

    errorMessage = StringProperty()

    def create(self):
        self.newInput("Object", "Object", "object", defaultDrawType = "PROPERTY_ONLY")
        self.newOutput("Generic", "Value", "value")

    def draw(self, layout):
        layout.prop(self, "attribute", text = "")
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def drawAdvanced(self, layout):
        self.invokeFunction(layout, "createAutoExecutionTrigger", text = "Create Execution Trigger")

    def getExecutionCode(self):
        code = self.evaluationExpression

        if not isCodeValid(code):
            self.errorMessage = "Invalid Syntax"
            yield "value = None"
            return
        else: self.errorMessage = ""

        yield "try:"
        yield "    self.errorMessage = ''"
        yield "    " + code
        yield "except:"
        yield "    if object: self.errorMessage = 'Attribute not found'"
        yield "    value = None"

    @property
    def evaluationExpression(self):
        if self.attribute.startswith("["): return "value = object" + self.attribute
        else: return "value = object." + self.attribute

    def createAutoExecutionTrigger(self):
        item = self.nodeTree.autoExecution.customTriggers.new("MONITOR_PROPERTY")
        item.idType = "OBJECT"
        item.dataPath = self.attribute
        item.idObjectName = self.inputs["Object"].objectName
