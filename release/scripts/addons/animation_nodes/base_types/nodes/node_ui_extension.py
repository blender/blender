import bpy
from mathutils import Vector
from ... draw_handler import drawHandler
from ... graphics.text_box import TextBox
from ... utils.blender_ui import getDpiFactor, getNodeBottomCornerLocations
from ... graphics.interpolation_preview import InterpolationPreview

class NodeUIExtension:
    def draw(self, node, position, width):
        raise NotImplementedError()
        # returns the height

class InterpolationUIExtension(NodeUIExtension):
    def __init__(self, interpolation, resolution):
        self.interpolation = interpolation
        self.resolution = resolution

    def draw(self, node, position, width):
        preview = InterpolationPreview(self.interpolation, position, width, self.resolution)
        preview.calculateBoundaries()
        preview.draw()
        return preview.getHeight()

class TextUIExtension(NodeUIExtension):
    def __init__(self, text, fontSize = 12, maxRows = 150):
        self.text = text
        self.fontSize = fontSize
        self.maxRows = maxRows

    def draw(self, node, position, width):
        textBox = TextBox(self.text, position, width,
                          fontSize = self.fontSize / node.dimensions.x * width,
                          maxRows = self.maxRows)
        textBox.draw()
        return textBox.getHeight()

class ErrorUIExtension(NodeUIExtension):
    def __init__(self, text):
        self.text = text

    def draw(self, node, position, width):
        textBox = TextBox(self.text, position, width,
                          fontSize = 12 / node.dimensions.x * width)
        textBox.borderColor = (0.8, 0.2, 0.2, 1)
        textBox.borderThickness = 2
        textBox.draw()
        return textBox.getHeight()


# Invoking the draw code
###########################################

@drawHandler("SpaceNodeEditor", "WINDOW")
def drawNodeUIExtensions():
    tree = bpy.context.getActiveAnimationNodeTree()
    if tree is None: return

    region = bpy.context.region
    dpiFactor = getDpiFactor()

    for node in tree.nodes:
        if node.isAnimationNode:
            extensions = node.getAllUIExtensions()
            if len(extensions) > 0:
                position, width = getDrawPositionAndWidth(node, region, dpiFactor)
                drawExtensionsForNode(node, extensions, position, width)

def getDrawPositionAndWidth(node, region, dpiFactor):
    leftBottom, rightBottom = getNodeBottomCornerLocations(node, region, dpiFactor)
    width = rightBottom.x - leftBottom.x
    return leftBottom, width

def drawExtensionsForNode(node, extensions, position, width):
    for extension in extensions:
        height = extension.draw(node, position, width)
        position.y -= height
