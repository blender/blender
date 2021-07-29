import bpy
from bpy.props import *
from mathutils import Matrix
from ... data_structures import Spline
from ... base_types import VectorizedNode

transformationTypeItems = [
    ("Matrix List", "Matrices", "", "NONE", 0),
    ("Vector List", "Vectors", "", "NONE", 1)
]

class ReplicateSplineNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ReplicateSplineNode"
    bl_label = "Replicate Spline"

    useSplineList = VectorizedNode.newVectorizeProperty()

    transformationType = EnumProperty(name = "Transformation Type", default = "Matrix List",
        items = transformationTypeItems, update = VectorizedNode.refresh)

    def create(self):
        self.newVectorizedInput("Spline", "useSplineList",
            ("Spline", "spline", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Splines", "splines"))

        self.newInput(self.transformationType, "Transformations", "transformations")

        self.newOutput("Spline List", "Splines", "outSplines")

    def draw(self, layout):
        layout.prop(self, "transformationType", text = "")

    def getExecutionFunctionName(self):
        if self.transformationType == "Matrix List":
            return "execute_MatrixList"
        elif self.transformationType == "Vector List":
            return "execute_VectorList"

    def execute_MatrixList(self, splines, matrices):
        if isinstance(splines, Spline):
            splines = [splines]

        outSplines = []
        for matrix in matrices:
            for spline in splines:
                newSpline = spline.copy()
                newSpline.transform(matrix)
                outSplines.append(newSpline)
        return outSplines

    def execute_VectorList(self, splines, vectors):
        if isinstance(splines, Spline):
            splines = [splines]

        outSplines = []
        for vector in vectors:
            for spline in splines:
                newSpline = spline.copy()
                newSpline.transform(Matrix.Translation(vector))
                outSplines.append(newSpline)
        return outSplines
