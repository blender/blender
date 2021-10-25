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

    def getExecutionCode(self, required):
        if "lower" in required:
            yield "lower = '{}'".format(string.ascii_lowercase)
        if "upper" in required:
            yield "upper = '{}'".format(string.ascii_uppercase)
        if "digits" in required:
            yield "digits = '{}'".format(string.digits)
        if "special" in required:
            yield "special = '!$%&/\\()=?*+#\\'-_.:,;\"'"
        if "lineBreak" in required:
            yield "lineBreak = '\\n'"
        if "tab" in required:
            yield "tab = '\\t'"
