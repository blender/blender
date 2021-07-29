import bpy
from bpy.props import *
from mathutils.bvhtree import BVHTree
from ... base_types import AnimationNode

sourceTypeItems = [
    ("MESH_DATA", "Mesh Data", "", "NONE", 0),
    ("BMESH", "BMesh", "", "NONE", 1),
    ("OBJECT", "Object", "", "NONE", 2) ]

class ConstructBVHTreeNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ConstructBVHTreeNode"
    bl_label = "Construct BVHTree"
    bl_width_default = 160

    sourceType = EnumProperty(name = "Source Type", default = "MESH_DATA",
        items = sourceTypeItems, update = AnimationNode.refresh)

    def create(self):
        if self.sourceType == "MESH_DATA":
            self.newInput("Vector List", "Vector List", "vectorList")
            self.newInput("Polygon Indices List", "Polygon Indices", "polygonsIndices")
        elif self.sourceType == "BMESH":
            self.newInput("BMesh", "BMesh", "bm")
        elif self.sourceType == "OBJECT":
            self.newInput("Object", "Object", "object", defaultDrawType = "PROPERTY_ONLY")

        self.newInput("Float", "Epsilon", "epsilon", hide = True, minValue = 0)
        self.newOutput("BVHTree", "BVHTree", "bvhTree")

    def draw(self, layout):
        layout.prop(self, "sourceType", text = "Source")

    def getExecutionFunctionName(self):
        if self.sourceType == "MESH_DATA":
            return "execute_MeshData"
        elif self.sourceType == "BMESH":
            return "execute_BMesh"
        elif self.sourceType == "OBJECT":
            return "execute_Object"

    def execute_MeshData(self, vectorList, polygonsIndices, epsilon):
        if len(polygonsIndices) == 0:
            return self.getFallbackBVHTree()

        if 0 <= polygonsIndices.getMinIndex() <= polygonsIndices.getMaxIndex() < len(vectorList):
            return BVHTree.FromPolygons(vectorList, polygonsIndices, epsilon = max(epsilon, 0))

    def execute_BMesh(self, bm, epsilon):
        return BVHTree.FromBMesh(bm, epsilon = max(epsilon, 0))

    def execute_Object(self, object, epsilon):
        if object is None:
            return self.getFallbackBVHTree()
        if object.type != "MESH":
            return self.getFallbackBVHTree()

        mesh = object.data
        vertices = mesh.an.getVertices()
        vertices.transform(object.matrix_world)
        polygons = mesh.an.getPolygonIndices()
        return self.execute_MeshData(vertices, polygons, epsilon)

    def getFallbackBVHTree(self):
        return self.outputs[0].getDefaultValue()
