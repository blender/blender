import bpy
import string
from ... base_types import AnimationNode

class CharactersNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CharactersNode"
    bl_label = "Characters"

    def create(self):
        self.newOutput("Text", "Lower Case", "lower")
        self.newOutput("Text", "Upper Case", "upper")
        self.newOutput("Text", "Digits", "digits")
        self.newOutput("Text", "Special", "special")
        self.newOutput("Text", "Line Break", "lineBreak")
        self.newOutput("Text", "Tab", "tab")

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()

        if isLinked["lower"]:
            yield "lower = '{}'".format(string.ascii_lowercase)
        if isLinked["upper"]:
            yield "upper = '{}'".format(string.ascii_uppercase)
        if isLinked["digits"]:
            yield "digits = '{}'".format(string.digits)
        if isLinked["special"]:
            yield "special = '!$%&/\\()=?*+#\\'-_.:,;\"'"
        if isLinked["lineBreak"]:
            yield "lineBreak = '\\n'"
        if isLinked["tab"]:
            yield "tab = '\\t'"
