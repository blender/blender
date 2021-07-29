import bpy
from bpy.props import *
from . c_utils import splinesFromEdges
from ... events import propertyChanged
from ... base_types import VectorizedNode
from ... data_structures import DoubleList

radiusTypeItems = [
    ("EDGE", "Radius per Edge", "", "NONE", 0),
    ("VERTEX", "Radius per Vertex", "", "NONE", 1)
]

class SplinesFromEdgesNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_SplinesFromEdgesNode"
    bl_label = "Splines from Edges"
    bl_width_default = 160

    errorMessage = StringProperty()

    radiusType = EnumProperty(name = "Radius Type", default = "EDGE",
        description = "Only important if there is a list of radii.",
        update = propertyChanged, items = radiusTypeItems)

    useRadiusList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newInput("Vector List", "Vertices", "vertices", dataIsModified = True)
        self.newInput("Edge Indices List", "Edge Indices", "edgeIndices")

        self.newVectorizedInput("Float", "useRadiusList",
            ("Radius", "radius", dict(value = 0.1, minValue = 0)),
            ("Radii", "radii"))

        self.newOutput("Spline List", "Splines", "splines")

    def draw(self, layout):
        layout.prop(self, "radiusType", text = "")

        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def execute(self, vertices, edgeIndices, radius):
        self.errorMessage = ""

        radiiAmount = len(edgeIndices) if self.radiusType == "EDGE" else len(vertices)

        try:
            radii = self.prepareRadiusList(radius, radiiAmount)
            splines = splinesFromEdges(vertices, edgeIndices, radii, self.radiusType)
        except Exception as e:
            splines = []
            self.errorMessage = str(e)

        return splines

    def prepareRadiusList(self, radii, edgeAmount):
        if not isinstance(radii, DoubleList):
            return DoubleList.fromValue(radii, edgeAmount)

        if len(radii) != edgeAmount:
            raise Exception("wrong radius amount")
        else:
            return radii
