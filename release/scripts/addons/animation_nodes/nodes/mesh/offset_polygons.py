import bpy
from bpy.props import *
from mathutils import Matrix
from ... base_types import AnimationNode
from .. matrix.c_utils import getInvertedOrthogonalMatrices
from .. matrix.transformation_base_node import MatrixTransformationBase

from ... data_structures import (
    Mesh, EdgeIndicesList,
    VirtualVector3DList, VirtualMatrix4x4List
)

from . c_utils import (
    matricesFromNormalizedAxisData,
    transformPolygons, getIndividualPolygonsMesh
)

pivotSourceItems = [
    ("DEFAULT", "Default", "Use the center as pivot and guess some tangent and bitangent.", "NONE", 0),
    ("CUSTOM_POINTS", "Custom Points", "Provide custom pivots for every polygon. Tangent and bitangent are guessed.", "NONE", 1),
    ("CUSTOM_MATRICES", "Custom Matrices", "Provide a transformation matrix for each polygon that represents it. The rotation part of the matrices has to be orthogonal.", "NONE", 2)
]

class OffsetPolygonsNode(bpy.types.Node, AnimationNode, MatrixTransformationBase):
    bl_idname = "an_OffsetPolygonsNode"
    bl_label = "Offset Polygons"
    bl_width_default = 200
    errorHandlingType = "EXCEPTION"

    pivotSource = EnumProperty(name = "Pivot Source", default = "DEFAULT",
        description = "Determines the pivot and rotation axis for each polygon.",
        items = pivotSourceItems, update = AnimationNode.refresh)

    def create(self):
        self.newInput("Mesh", "Mesh", "inMesh")
        self.createMatrixTransformationInputs(useMatrixList = True)

        if self.pivotSource == "CUSTOM_POINTS":
            self.newInput("Vector List", "Pivots", "pivots")
        elif self.pivotSource == "CUSTOM_MATRICES":
            self.newInput("Matrix List", "Matrices", "matrices")

        self.newOutput("Mesh", "Mesh", "outMesh")

    def draw(self, layout):
        self.draw_MatrixTransformationProperties(layout)

    def drawAdvanced(self, layout):
        layout.prop(self, "pivotSource", text = "Local Pivots")

        if self.pivotSource == "CUSTOM_MATRICES" and "LOCAL_AXIS" not in self.rotationMode:
            layout.label("Try to change the rotation mode.", icon = "INFO")

        self.drawAdvanced_MatrixTransformationProperties(layout)

    def getExecutionFunctionName(self):
        if self.pivotSource == "DEFAULT":
            return "execute_Normal"
        elif self.pivotSource == "CUSTOM_POINTS":
            return "execute_CustomPivots"
        elif self.pivotSource == "CUSTOM_MATRICES":
            return "execute_Custom"

    def execute_Normal(self, mesh, *transformationArgs):
        newMesh = getIndividualPolygonsMesh(mesh)
        centers = newMesh.getPolygonCenters()
        normals, tangents, bitangents = newMesh.getPolygonOrientationMatrices(normalized = True)
        transforms = matricesFromNormalizedAxisData(centers, tangents, bitangents, normals)
        return self.transformMeshPolygons(newMesh, transforms, transformationArgs)

    def execute_CustomPivots(self, mesh, *args):
        *transformationArgs, pivots = args
        newMesh = getIndividualPolygonsMesh(mesh)

        virtualizedPivots = VirtualVector3DList.create(pivots, (0, 0, 0))
        pivots = virtualizedPivots.materialize(len(newMesh.polygons), canUseOriginal = True)

        normals, tangents, bitangents = newMesh.getPolygonOrientationMatrices(normalized = True)
        transforms = matricesFromNormalizedAxisData(pivots, tangents, bitangents, normals)
        return self.transformMeshPolygons(newMesh, transforms, transformationArgs)

    def execute_Custom(self, mesh, *args):
        *transformationArgs, transforms = args
        newMesh = getIndividualPolygonsMesh(mesh)

        virtualizedTransforms = VirtualMatrix4x4List.create(transforms, Matrix())
        transforms = virtualizedTransforms.materialize(len(newMesh.polygons), canUseOriginal = True)

        return self.transformMeshPolygons(newMesh, transforms, transformationArgs)

    def transformMeshPolygons(self, mesh, transforms, transformationArgs):
        invertedTransforms = getInvertedOrthogonalMatrices(transforms)

        transformPolygons(mesh.vertices, mesh.polygons, invertedTransforms)
        newTransforms = self.transformMatrices(transforms, transformationArgs)
        transformPolygons(mesh.vertices, mesh.polygons, newTransforms)
        mesh.verticesTransformed()

        return mesh

    def transformMatrices(self, matrices, args):
        name = self.getMatrixTransformationFunctionName(useMatrixList = True)
        return getattr(self, name)(matrices, *args)
