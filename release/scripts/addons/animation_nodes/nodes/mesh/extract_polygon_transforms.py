import bpy
from bpy.props import *
from mathutils import Vector
from ... events import propertyChanged
from ... base_types import AnimationNode
from .. matrix.c_utils import getInvertedOrthogonalMatrices

from . c_utils import (
    matricesFromNormalizedAxisData,
    getPolygonEdgesByDirection,
    getPolygonEdgesByDistance,
    calculateEdgeCenters,
    calculateEdgeDirections
)

from ... data_structures.meshes.mesh_data import (
    calculatePolygonNormals,
    calculatePolygonCenters,
    calculatePolygonTangents,
    calculateCrossProducts
)

sourceTypeItems = [
    ("MESH", "Mesh", "", "NONE", 0),
    ("VERTICES_AND_POLYGONS", "Vertices and Polygons", "", "NONE", 1)
]

extractionTypeItems = [
    ("DEFAULT", "Default", "Use polygon center and first edge as basis.", "NONE", 0),
    ("EDGE", "Edge", "Use one of the edges as basis", "NONE", 1)
]

edgeSelectionTypeItems = [
    ("DIRECTION", "Direction", "", "NONE", 0),
    ("DISTANCE", "Distance", "", "NONE", 1)
]

directionTypeItems = [
    ("X", "X", "", "NONE", 0),
    ("Y", "Y", "", "NONE", 1),
    ("Z", "Z", "", "NONE", 2),
    ("CUSTOM", "Custom", "", "NONE", 3)
]

distanceTypeItems = [
    ("CLOSEST", "Closest", "", "NONE", 0),
    ("FURTHEST", "Furthest", "", "NONE", 1)
]

class ExtractPolygonTransformsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ExtractPolygonTransformsNode"
    bl_label = "Extract Polygon Transforms"
    errorHandlingType = "EXCEPTION"

    sourceType = EnumProperty(name = "Source Type", default = "MESH",
        update = AnimationNode.refresh, items = sourceTypeItems)

    extractionType = EnumProperty(name = "Extraction Type", default = "DEFAULT",
        update = AnimationNode.refresh, items = extractionTypeItems)

    edgeSelectionType = EnumProperty(name = "Edge Selection Type", default = "DIRECTION",
        update = AnimationNode.refresh, items = edgeSelectionTypeItems)

    directionType = EnumProperty(name = "Direction Type", default = "X",
        update = AnimationNode.refresh, items = directionTypeItems)

    negateDirection = BoolProperty(name = "Negate Vertex Direction", default = False)

    distanceType = EnumProperty(name = "Distance Type", default = "CLOSEST",
        update = AnimationNode.refresh, items = distanceTypeItems)

    polygonsAreFlat = BoolProperty(name = "Polygons are Flat", default = False,
        description = ("Performance can be improved when you are certain that all polygons are flat. "
                       "Enabling this can lead to artifacts when the polygons are not actually flat."),
        update = propertyChanged)

    def create(self):
        if self.sourceType == "MESH":
            self.newInput("Mesh", "Mesh", "mesh")
        elif self.sourceType == "VERTICES_AND_POLYGONS":
            self.newInput("Vector List", "Vertices", "vertices")
            self.newInput("Polygon Indices List", "Polygon Indices", "polygonIndices")

        if self.useDirectionInput:
            self.newInput("Vector", "Direction", "direction", value = (1, 0, 0))
        if self.useCenterInput:
            self.newInput("Vector", "Center", "center")

        self.newOutput("Matrix List", "Transforms", "transforms")
        self.newOutput("Matrix List", "Inverted Transforms", "invertedTransforms", hide = True)

    def draw(self, layout):
        col = layout.column()
        col.prop(self, "extractionType", text = "")
        if self.extractionType == "EDGE":
            subcol = col.column(align = True)
            subcol.prop(self, "edgeSelectionType", text = "")
            if self.edgeSelectionType == "DIRECTION":
                row = subcol.row(align = True)
                row.prop(self, "negateDirection", text = "", icon = "ZOOMOUT")
                row.prop(self, "directionType", text = "")
            elif self.edgeSelectionType == "DISTANCE":
                subcol.prop(self, "distanceType", text = "")

    def drawAdvanced(self, layout):
        layout.prop(self, "sourceType", text = "Source")
        layout.prop(self, "polygonsAreFlat")

    def getExecutionCode(self, required):
        if self.sourceType == "MESH":
            yield "vertices, polygonIndices = mesh.vertices, mesh.polygons"
        elif self.sourceType == "VERTICES_AND_POLYGONS":
            yield "self.validateMeshData(vertices, polygonIndices)"

        yield "extraArgs = {}"
        if self.useDirectionInput:
            yield "extraArgs['Direction'] = direction"
        if self.useCenterInput:
            yield "extraArgs['Center'] = center"

        yield "transforms = self.getTransforms(vertices, polygonIndices, extraArgs)"
        if "invertedTransforms" in required:
            yield "invertedTransforms = self.invertTransforms(transforms)"

    def validateMeshData(self, vertices, polygons):
        if len(polygons) > 0 and polygons.getMaxIndex() >= len(vertices):
            self.raiseErrorMessage("Invalid polygon indices")

    def getTransforms(self, vertices, polygons, extraArgs):
        if self.extractionType == "DEFAULT":
            origins, tangents = self.getTransforms_Default(vertices, polygons)
        elif self.extractionType == "EDGE":
            origins, tangents = self.getTransforms_Edge(vertices, polygons, extraArgs)

        if self.polygonsAreFlat:
            normals = calculatePolygonNormals(vertices, polygons)
            normals.normalize()
            tangents.normalize()
            bitangents = calculateCrossProducts(tangents, normals)
        else:
            normals = calculatePolygonNormals(vertices, polygons)
            tangents.normalize()
            bitangents = calculateCrossProducts(tangents, normals)
            bitangents.normalize()
            normals = calculateCrossProducts(tangents, bitangents)

        return matricesFromNormalizedAxisData(origins, tangents, bitangents, normals)

    def getTransforms_Default(self, vertices, polygons):
        origins = calculatePolygonCenters(vertices, polygons)
        tangents = calculatePolygonTangents(vertices, polygons)
        return origins, tangents

    def getTransforms_Edge(self, vertices, polygons, extraArgs):
        if self.edgeSelectionType == "DIRECTION":
            direction = self.getDirection(extraArgs)
            edges = getPolygonEdgesByDirection(vertices, polygons, direction, self.negateDirection)
        elif self.edgeSelectionType == "DISTANCE":
            center = extraArgs["Center"]
            edges = getPolygonEdgesByDistance(vertices, polygons, center, self.distanceType == "FURTHEST")

        origins = calculateEdgeCenters(vertices, edges)
        tangents = calculateEdgeDirections(vertices, edges)
        return origins, tangents

    def getDirection(self, extraArgs):
        t = self.directionType
        if t == "X":   dir = Vector((1, 0, 0))
        elif t == "Y": dir = Vector((0, 1, 0))
        elif t == "Z": dir = Vector((0, 0, 1))
        elif t == "CUSTOM": dir = extraArgs["Direction"]
        return dir

    def invertTransforms(self, transforms):
        return getInvertedOrthogonalMatrices(transforms)

    @property
    def useDirectionInput(self):
        return (self.extractionType == "EDGE" and
                self.edgeSelectionType == "DIRECTION" and
                self.directionType == "CUSTOM")

    @property
    def useCenterInput(self):
        return (self.extractionType == "EDGE" and
                self.edgeSelectionType == "DISTANCE")