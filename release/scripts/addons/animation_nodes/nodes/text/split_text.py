import bpy, re
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode

splitTypeItems = [
    ("CHARACTERS", "Characters", "", "NONE", 0),
    ("WORDS", "Words", "", "NONE", 1),
    ("LINES", "Lines", "", "NONE", 2),
    ("REGULAR_EXPRESSION", "Regular Expression", "", "NONE", 3),
    ("N_CHARACTERS", "N Characters", "", "NONE", 4),
    ("SEPARATOR", "Separator", "", "NONE", 5) ]

class SplitTextNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SplitTextNode"
    bl_label = "Split Text"
    bl_width_default = 190

    splitType = EnumProperty(
        name = "Split Type", default = "LINES",
        items = splitTypeItems, update = AnimationNode.refresh)

    keepDelimiters = BoolProperty(default = False, update = propertyChanged)

    errorMessage = StringProperty()

    def create(self):
        self.newInput("Text", "Text", "text")
        if self.splitType in ("REGULAR_EXPRESSION", "SEPARATOR"):
            self.newInput("Text", "Split By", "splitBy")
        if self.splitType == "N_CHARACTERS":
            self.newInput("Integer", "N", "n", value = 5, minValue = 1)

        self.newOutput("Text List", "Text List", "textList")
        self.newOutput("Integer", "Length", "length")

    def draw(self, layout):
        layout.prop(self, "splitType", text = "")
        if self.splitType == "REGULAR_EXPRESSION":
            layout.prop(self, "keepDelimiters", text = "Keep Delimiters")
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def getExecutionCode(self):
        yield "self.errorMessage = ''"
        if self.splitType == "CHARACTERS":
            yield "textList = list(text)"
        elif self.splitType == "WORDS":
            yield "textList = text.split()"
        elif self.splitType == "LINES":
            yield "textList = text.splitlines()"
        elif self.splitType == "REGULAR_EXPRESSION":
            yield "textList = self.splitWithRegularExpression(text, splitBy)"
        elif self.splitType == "N_CHARACTERS":
            yield "textList = self.splitEveryNCharacters(text, n)"
        elif self.splitType == "SEPARATOR":
            yield "textList = [text] if splitBy == '' else text.split(splitBy)"
        yield "length = len(textList)"

    def splitWithRegularExpression(self, text, splitBy):
        if splitBy == "": return [text]
        else:
            try:
                if self.keepDelimiters: return re.split("("+splitBy+")", text)
                else: return re.split(splitBy, text)
            except:
                self.errorMessage = "Invalid Regular Expression"
                return [text]

    def splitEveryNCharacters(self, text, n):
        if n <= 0: return []

        textList = list(map(''.join, zip(*([iter(text)] * n))))
        missingChars = len(text) % n
        if missingChars > 0:
            textList.append(text[-missingChars:])
        return textList
