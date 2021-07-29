import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode

caseTypeItems = [("UPPER", "To Upper Case", ""),
                 ("LOWER", "To Lower Case", ""),
                 ("TITLE", "To Title Case", ""),
                 ("CAPITALIZE", "Capitalize Phrase", ""),
                 ("CAPWORDS", "Capitalize Words", "") ]

caseTypeCode = { item[0] : item[0].lower() for item in caseTypeItems }

class ChangeTextCaseNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ChangeTextCaseNode"
    bl_label = "Change Text Case"

    def caseTypeChanges(self, context):
        executionCodeChanged()

    caseType = EnumProperty(
        name = "Case Type", default = "CAPITALIZE",
        items = caseTypeItems, update = caseTypeChanges)

    def create(self):
        self.newInput("Text", "Text", "inText")
        self.newOutput("Text", "Text", "outText")

    def draw(self, layout):
        layout.prop(self, "caseType", text = "")

    def getExecutionCode(self):
        if self.caseType == "CAPWORDS":
            return "outText = string.capwords(inText)"
        return "outText = inText.{}()".format(caseTypeCode[self.caseType])

    def getUsedModules(self):
        return ["string"]
