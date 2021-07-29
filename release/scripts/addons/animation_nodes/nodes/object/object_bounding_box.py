import bpy
from bpy.props import *
from mathutils import Vector
from ... events import propertyChanged
from ... base_types import AnimationNode
from ... data_structures import Vector3DList, EdgeIndicesList, PolygonIndicesList

class ObjectBoundingBoxNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ObjectBoundingBoxNode"
    bl_label = "Object Bounding Box"

    useWorldSpace = BoolProperty(name = "Use World Space", default = True, update = propertyChanged)

    def create(self):
        self.newInput("Object", "Object", "object", defaultDrawType = "PROPERTY_ONLY")
        self.newOutput("Vector List", "Vertices", "vertices")
        self.newOutput("Edge Indices List", "Edges", "edges")
        self.newOutput("Polygon Indices List", "Polygons", "polygons")
        self.newOutput("Vector", "Center", "center")

    def drawAdvanced(self, layout):
        layout.prop(self, "useWorldSpace")

    def execute(self, object):
        if object is None:
            return Vector3DList(), EdgeIndicesList(), PolygonIndicesList(), Vector((0, 0, 0))

        vertices = Vector3DList.fromValues(object.bound_box)
        if self.useWorldSpace:
            vertices.transform(object.matrix_world)

        center = (vertices[0] + vertices[6]) / 2
        return vertices, edges.copy(), polygons.copy(), center

edges = EdgeIndicesList.fromValues(
    [(0, 1), (1, 2), (2, 3), (0, 3), (4, 5), (5, 6),
     (6, 7), (4, 7), (0, 4), (1, 5), (2, 6), (3, 7)]
)

polygons = PolygonIndicesList.fromValues(
    [(0, 1, 2, 3), (7, 6, 5, 4), (0, 3, 7, 4),
     (4, 5, 1, 0), (5, 6, 2, 1), (6, 7, 3, 2)]
)
