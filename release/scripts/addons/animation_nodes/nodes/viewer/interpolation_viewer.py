import bpy
from bpy.props import *
from ... base_types import AnimationNode, InterpolationUIExtension

interpolationByNode = {}

class InterpolationViewerNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_InterpolationViewerNode"
    bl_label = "Interpolation Viewer"
    bl_width_default = 160

    resolution = IntProperty(name = "Resolution", min = 5, default = 100)

    def create(self):
        self.newInput("Interpolation", "Interpolation", "interpolation", defaultDrawType = "PROPERTY_ONLY")

    def drawAdvanced(self, layout):
        layout.prop(self, "resolution")

    def execute(self, interpolation):
        interpolationByNode[self.identifier] = interpolation

    def getUIExtensions(self):
        if self.hide:
            return []

        interpolation = interpolationByNode.get(self.identifier, None)
        if interpolation is None:
            return []

        return [InterpolationUIExtension(interpolation, self.resolution)]
