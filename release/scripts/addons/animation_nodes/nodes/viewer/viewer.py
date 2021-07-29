import bpy
from bpy.props import *
from itertools import chain
from functools import lru_cache
from ... base_types import AnimationNode
from ... draw_handler import drawHandler
from ... graphics.text_box import TextBox

drawTextByIdentifier = {}

class ViewerNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ViewerNode"
    bl_label = "Viewer"
    bl_width_default = 190
    options = {"NO_TIMING"}

    maxRows = IntProperty(name = "Max Rows", default = 150, min = 0,
        description = "Max amount of lines visible in the floating text box.")
    fontSize = IntProperty(name = "Font Size", default = 12, min = 1, max = 1000)

    maxListStartElements = IntProperty(name = "Max List Start Elements", default = 15, min = 0)
    maxListEndElements = IntProperty(name = "Max List End Elements", default = 0, min = 0)

    outputConsole = BoolProperty(name = "Console Output", default = False,
        description = "Output data in the terminal/console. May slow down execution a lot.")

    outputTextBlock = BoolProperty(name = "Text Block Output", default = False,
        description = "Output data in a text block. May slow down execution a lot.")

    textBlockName = StringProperty(name = "Text Block Name")

    def create(self):
        self.newInput("Generic", "None", "data")

    def draw(self, layout):
        if self.outputTextBlock:
            row = layout.row(align = True)
            row.prop_search(self, "textBlockName",  bpy.data, "texts", text = "")
            if self.getTextBlock():
                self.invokeSelector(row, "AREA", "viewTextBlockInArea",
                    icon = "ZOOM_SELECTED")
            else:
                self.invokeFunction(row, "createNewTextBlock", icon = "ZOOMIN")

    def drawAdvanced(self, layout):
        col = layout.column(align = True)
        col.label("Display Settings:")
        col.prop(self, "fontSize")
        col.prop(self, "maxRows")

        col = layout.column(align = True)
        col.label("List Settings:")
        col.prop(self, "maxListStartElements", text = "Start")
        col.prop(self, "maxListEndElements", text = "End")

        col = layout.column(align = True)
        col.label("Output Settings:")
        col.prop(self, "outputConsole", "Console")
        col.prop(self, "outputTextBlock", "Text Block")

    def execute(self, data):
        if handleDataAsList(data):
            self.handleListData(data)
        else:
            self.handleNonListData(data)

    def handleNonListData(self, data):
        function = getHandleNonListFunction(type(data))
        nodeText, drawText, *args = function(data)
        self.setViewData(nodeText, drawText, *args)
        if self.outputTextBlock:
            self.setTextBlockData(nodeText + "\n" + drawText)

    def handleListData(self, data):
        length = len(data)
        startAmount = self.maxListStartElements
        endAmount = self.maxListEndElements
        toString, minWidth = getHandleListInfo(type(data))

        if length <= startAmount + endAmount:
            indexWidth = len(str(length - 1))
            text = "\n".join(self.iterListElements(data, 0, length, indexWidth, toString))
        else:
            if endAmount > 0: indexWidth = len(str(length - 1))
            else:             indexWidth = len(str(startAmount - 1))

            startElements = self.iterListElements(data, 0, startAmount, indexWidth, toString)
            endElements = self.iterListElements(data, length - endAmount, endAmount, indexWidth, toString)
            text = "\n".join(chain(startElements, ["..."], endElements))

        listInfo = "{} - Length: {}".format(type(data).__name__, length)
        self.setViewData(listInfo, text, minWidth)

        if self.outputTextBlock:
            indexWidth = len(str(length - 1))
            self.setTextBlockData("\n".join(self.iterListElements(data, 0, length, indexWidth, toString)))

    def iterListElements(self, data, start, length, indexWidth, toString):
        if length == 0:
            return
        for i, element in enumerate(data[start:start + length], start):
            yield "{}: {}".format(str(i).rjust(indexWidth), toString(element))

    def setViewData(self, nodeText, drawText, minWidth = None):
        self.inputs[0].name = nodeText
        drawTextByIdentifier[self.identifier] = drawText

        if minWidth is not None:
            self.width = max(self.width, minWidth)

        if self.outputConsole:
            print("Viewer: '{}'".format(self.name))
            print("  " + nodeText)
            print("\n".join("  " + line for line in drawText.splitlines()))

    def setTextBlockData(self, text):
        textBlock = self.getTextBlock()
        if textBlock is not None:
            textBlock.clear()
            textBlock.write(text)

    def delete(self):
        if self.identifier in drawTextByIdentifier:
            del drawTextByIdentifier[self.identifier]

    def viewTextBlockInArea(self, area):
        area.type = "TEXT_EDITOR"
        area.spaces.active.text = self.getTextBlock()

    def createNewTextBlock(self):
        textBlock = bpy.data.texts.new(name = "Viewer")
        self.textBlockName = textBlock.name

    def getTextBlock(self):
        return bpy.data.texts.get(self.textBlockName)


def handleDataAsList(data):
    if data is None:
        return False
    elif isinstance(data, str):
        return False
    elif hasattr(data, "__iter__") and hasattr(data, "__len__") and isSlicable(data):
        return True
    else:
        return False

def isSlicable(data):
    try:
        data[:0]
        return True
    except:
        return False


# Non List Helpers
########################################

@lru_cache(maxsize = 1024)
def getHandleNonListFunction(cls):
    from mathutils import Vector, Matrix, Euler, Quaternion
    if cls is str:
        return handleNonList_String
    elif cls is float:
        return handleNonList_Float
    elif cls is int:
        return handleNonList_Integer
    elif cls is Vector:
        return handleNonList_Vector
    elif cls is Euler:
        return handleNonList_Euler
    elif cls is Matrix:
        return handleNonList_Matrix
    else:
        return handleNonList_Generic

def handleNonList_Generic(data):
    text = str(data)
    if len(text) < 20 and "\n" not in text:
        return text, ""
    else:
        return "Type: " + type(data).__name__, str(data)

def handleNonList_String(text):
    if len(text) < 20 and "\n" not in text:
        return text, ""
    else:
        return "Text Length: " + str(len(text)), text

def handleNonList_Float(number):
    return str(round(number, 5)), ""

def handleNonList_Integer(number):
    return str(number), ""

def handleNonList_Vector(vector):
    return str(vector), "", 260

def handleNonList_Euler(euler):
    return str(euler), "", 340

def handleNonList_Matrix(matrix):
    return "Type: {}x{} Matrix".format(len(matrix.row), len(matrix.col)), str(matrix), 330


# List Helpers
##################################

@lru_cache(maxsize = 1024)
def getHandleListInfo(cls):
    from ... data_structures import (
        Vector3DList, Matrix4x4List, EulerList, QuaternionList, DoubleList
    )

    if cls is Vector3DList:
        return handleListElement_Vector, 260
    elif cls is Matrix4x4List:
        return handleListElement_Generic, 380
    elif cls is EulerList:
        return handleListElement_Euler, 320
    elif cls is QuaternionList:
        return handleListElement_Quaternion, 320
    elif cls is DoubleList:
        return handleListElement_Float, None
    else:
        return handleListElement_Generic, None

def handleListElement_Generic(data):
    return str(data)

def handleListElement_Vector(vector):
    return "V({:>7.3f}, {:>7.3f}, {:>7.3f})".format(*vector)

def handleListElement_Euler(euler):
    return "E({:>7.3f}, {:>7.3f}, {:>7.3f}, order = {})".format(*euler, euler.order)

def handleListElement_Quaternion(quaternion):
    return "Q({:>7.3f}, {:>7.3f}, {:>7.3f}, {:>7.3f})".format(*quaternion)

def handleListElement_Float(number):
    return "{:>10.5f}".format(number)


# Drawing
##################################

@drawHandler("SpaceNodeEditor", "WINDOW")
def drawTextBoxes():
    tree = bpy.context.getActiveAnimationNodeTree()
    if tree is None:
        return

    for node in tree.nodes:
        if node.bl_idname == "an_ViewerNode" and not node.hide:
            drawTextBoxForNode(node)

def drawTextBoxForNode(node):
    text = drawTextByIdentifier.get(node.identifier, "")
    if text == "":
        return

    region = bpy.context.region
    leftBottom = node.getRegionBottomLeft(region)
    rightBottom = node.getRegionBottomRight(region)
    width = rightBottom.x - leftBottom.x

    textBox = TextBox(text, leftBottom, width,
                      fontSize = node.fontSize / node.dimensions.x * width,
                      maxRows = node.maxRows)
    textBox.draw()
