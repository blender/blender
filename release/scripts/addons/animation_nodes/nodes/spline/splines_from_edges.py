import bpy
from bpy.props import *
from . c_utils import splinesFromEdges
from ... events import propertyChanged
from ... data_structures import DoubleList
from ... base_types import AnimationNode, VectorizedSocket

radiusTypeItems = [
    ("EDGE", "Radius per Edge", "", "NONE", 0),
    ("VERTEX", "Radius per Vertex", "", "NONE", 1)
]

class SplinesFromEdgesNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SplinesFromEdgesNode"
    bl_label = "Splines from Edges"
    bl_width_default = 160

    radiusType = EnumProperty(name = "Radius Type", default = "EDGE",
        description = "Only important if there is a list of radii.",
        update = propertyChanged, items = radiusTypeItems)

    useRadiusList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput("Vector List", "Vertices", "vertices", dataIsModified = True)
        self.newInput("Edge Indices List", "Edge Indices", "edgeIndices")

        self.newInput(VectorizedSocket("Float", "useRadiusList",
            ("Radius", "radius", dict(value = 0.1, minValue = 0)),
            ("Radii", "radii")))

        self.newOutput("Spline List", "Splines", "splines")

    def draw(self, layout):
        layout.prop(self, "radiusType", text = "")

    def execute(self, vertices, edgeIndices, radius):
        radiiAmount = len(edgeIndices) if self.radiusType == "EDGE" else len(vertices)

        radii = self.prepareRadiusList(radius, radiiAmount)
        splines = splinesFromEdges(vertices, edgeIndices, radii, self.radiusType)

        return splines

    def prepareRadiusList(self, radii, edgeAmount):
        if self.useRadiusList:
            if len(radii) == 0:
                return DoubleList.fromValue(0, edgeAmount)
            else:
                return radii.repeated(length = edgeAmount)
        else:
            return DoubleList.fromValue(radii, edgeAmount)
