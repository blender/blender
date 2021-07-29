import bpy
from bpy.props import *
from collections import defaultdict
from ... sockets.info import toIdName
from ... base_types import AnimationNode

dataByIdentifier = defaultdict(None)

dataDirectionItems = {
    ("IMPORT", "Import", "Receive the data from somewhere else", "IMPORT", 0),
    ("EXPORT", "Export", "Another script can read the data from this node", "EXPORT", 1) }

class DataInterfaceNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_DataInterfaceNode"
    bl_label = "Data Interface"

    dataDirection = EnumProperty(name = "Data Direction", default = "IMPORT",
        items = dataDirectionItems, update = AnimationNode.refresh)

    def create(self):
        if self.dataDirection == "EXPORT":
            self.newInput("Generic", "Value", "value")
        if self.dataDirection == "IMPORT":
            self.newOutput("Generic", "Value", "value")

    def draw(self, layout):
        layout.prop(self, "dataDirection", text = "")

    def getExecutionCode(self):
        if self.dataDirection == "EXPORT":
            return "self.setValue(value)"
        if self.dataDirection == "IMPORT":
            return "value = self.getValue()"

    def setValue(self, value):
        dataByIdentifier[self.identifier] = value

    def getValue(self):
        return dataByIdentifier.get(self.identifier)

    @property
    def value(self):
        return self.getValue()

    @value.setter
    def value(self, value):
        self.setValue(value)
