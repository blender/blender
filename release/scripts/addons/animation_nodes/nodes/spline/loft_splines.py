import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode
from ... algorithms.mesh_generation.loft import LinearLoft, SmoothLoft
from ... data_structures import Vector3DList, EdgeIndicesList, PolygonIndicesList

interpolationTypeItems = [
    ("LINEAR", "Linear", "", "NONE", 0),
    ("SMOOTH", "Bezier", "", "NONE", 1)]

sampleDistributionTypeItems = [
    ("RESOLUTION", "Resolution", "", "NONE", 0),
    ("UNIFORM", "Uniform", "", "NONE", 1)]

class LoftSplinesNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_LoftSplinesNode"
    bl_label = "Loft Splines"
    bl_width_default = 160

    interpolationType = EnumProperty(name = "Interpolation Type", default = "LINEAR",
        items = interpolationTypeItems, update = AnimationNode.refresh)

    resolution = IntProperty(name = "Resolution", default = 5, min = 2,
        description = "Increase to have a more accurate uniform evaluation", update = propertyChanged)

    splineDistributionType = EnumProperty(name = "Spline Distribution", default = "RESOLUTION",
        items = sampleDistributionTypeItems, update = propertyChanged)

    surfaceDistributionType = EnumProperty(name = "Surface Distribution", default = "RESOLUTION",
        items = sampleDistributionTypeItems, update = propertyChanged)

    def create(self):
        self.newInput("Spline List", "Splines", "splines", defaultDrawType = "TEXT_ONLY")
        self.newInput("Integer", "Spline Samples", "splineSamples", value = 16, minValue = 2)
        if self.interpolationType == "LINEAR":
            self.newInput("Integer", "Subdivisions", "subdivisions", value = 0, minValue = 0)
        elif self.interpolationType == "SMOOTH":
            self.newInput("Integer", "Surface Samples", "surfaceSamples", value = 16, minValue = 2)
        self.newInput("Boolean", "Cyclic", "cyclic", value = False)
        if self.interpolationType == "SMOOTH":
            self.newInput("Float", "Smoothness", "smoothness", value = 1/3)
        self.newInput("Float", "Start", "start", hide = True, value = 0.0).setRange(0.0, 1.0)
        self.newInput("Float", "End", "end", hide = True, value = 1.0).setRange(0.0, 1.0)

        self.newOutput("Vector List", "Vertices", "vertices")
        self.newOutput("Edge Indices List", "Edge Indices", "edgeIndices")
        self.newOutput("Polygon Indices List", "Polygon Indices", "polygonIndices")

    def draw(self, layout):
        layout.prop(self, "interpolationType", text = "")

    def drawAdvanced(self, layout):
        col = layout.column()
        col.prop(self, "splineDistributionType")
        col.prop(self, "surfaceDistributionType")
        col.prop(self, "resolution")

    def getExecutionFunctionName(self):
        if self.interpolationType == "LINEAR":
            return "execute_Linear"
        elif self.interpolationType == "SMOOTH":
            return "execute_Smooth"

    def execute_Linear(self, splines, splineSamples, subdivisions, cyclic, start, end):
        loft = LinearLoft()
        loft.splines = splines
        loft.splineSamples = splineSamples
        loft.subdivisions = subdivisions
        loft.cyclic = cyclic
        loft.start = start
        loft.end = end
        loft.distributionType = self.splineDistributionType
        loft.uniformResolution = self.resolution
        return self.evaluateLoft(loft)

    def execute_Smooth(self, splines, splineSamples, surfaceSamples, cyclic, smoothness, start, end):
        loft = SmoothLoft()
        loft.splines = list(reversed(splines))
        loft.splineSamples = splineSamples
        loft.surfaceSamples = surfaceSamples
        loft.cyclic = cyclic
        loft.smoothness = smoothness
        loft.start = start
        loft.end = end
        loft.splineDistributionType = self.splineDistributionType
        loft.surfaceDistributionType = self.surfaceDistributionType
        loft.uniformResolution = self.resolution
        return self.evaluateLoft(loft)

    def evaluateLoft(self, loft):
        valid = loft.validate()

        vertices, edgeIndices, polygonIndices = None, None, None
        if valid:
            if self.outputs["Vertices"].isLinked: vertices = loft.calcVertices()
            if self.outputs["Edge Indices"].isLinked: edgeIndices = loft.calcEdgeIndices()
            if self.outputs["Polygon Indices"].isLinked: polygonIndices = loft.calcPolygonIndices()

        if vertices is None: vertices = Vector3DList()
        if edgeIndices is None: edgeIndices = EdgeIndicesList()
        if polygonIndices is None: polygonIndices = PolygonIndicesList()

        return vertices, edgeIndices, polygonIndices
