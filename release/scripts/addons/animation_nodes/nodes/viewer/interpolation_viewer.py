import bpy
from bpy.props import *
from mathutils import Vector
from ... draw_handler import drawHandler
from ... tree_info import getNodesByType
from ... base_types import AnimationNode
from ... graphics.interpolation_preview import InterpolationPreview

interpolationByNode = {}

class InterpolationViewerNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_InterpolationViewerNode"
    bl_label = "Interpolation Viewer"
    bl_width_default = 160
    options = {"NO_TIMING"}

    resolution = IntProperty(name = "Resolution", min = 5, default = 100)

    def create(self):
        self.newInput("Interpolation", "Interpolation", "interpolation", defaultDrawType = "PROPERTY_ONLY")

    def drawAdvanced(self, layout):
        layout.prop(self, "resolution")

    def execute(self, interpolation):
        interpolationByNode[self.identifier] = interpolation

@drawHandler("SpaceNodeEditor", "WINDOW")
def drawInterpolationPreviews():
    nodes = getNodesByType("an_InterpolationViewerNode")
    nodesInCurrentTree = getattr(bpy.context.space_data.node_tree, "nodes", [])
    for node in nodes:
        if node.name in nodesInCurrentTree and not node.hide:
            drawNodePreview(node)

def drawNodePreview(node):
    interpolation = interpolationByNode.get(node.identifier, None)
    if interpolation is None: return

    region = bpy.context.region
    leftBottom = node.getRegionBottomLeft(region)
    rightBottom = node.getRegionBottomRight(region)
    width = rightBottom.x - leftBottom.x

    preview = InterpolationPreview(interpolation, leftBottom, width, node.resolution)
    preview.calculateBoundaries()
    preview.draw()
