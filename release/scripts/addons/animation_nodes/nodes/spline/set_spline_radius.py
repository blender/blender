import bpy
from ... data_structures import FloatList
from ... base_types import AnimationNode, VectorizedSocket

class SetSplineRadiusNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SetSplineRadiusNode"
    bl_label = "Set Spline Radius"

    useSplineList = VectorizedSocket.newProperty()
    useRadiusList = VectorizedSocket.newProperty()

    def create(self):
        socket = self.newInput(VectorizedSocket("Spline", "useSplineList",
            ("Spline", "spline"), ("Splines", "splines")))
        socket.defaultDrawType = "PROPERTY_ONLY"
        socket.dataIsModified = True

        self.newInput(VectorizedSocket("Float", "useRadiusList",
            ("Radius", "radius", dict(value = 0.1, minValue = 0)),
            ("Radii", "radii")))

        self.newOutput(VectorizedSocket("Spline", "useSplineList",
            ("Spline", "spline"),
            ("Splines", "splines")))

    def getExecutionFunctionName(self):
        if self.useSplineList:
            if self.useRadiusList:
                return "execute_MultipleSplines_MultipleRadii"
            else:
                return "execute_MultipleSplines_SingleRadius"
        else:
            if self.useRadiusList:
                return "execute_SingleSpline_MultipleRadii"
            else:
                return "execute_SingleSpline_SingleRadius"

    def execute_MultipleSplines_MultipleRadii(self, splines, radii):
        for spline in splines:
            self.execute_SingleSpline_MultipleRadii(spline, radii)
        return splines

    def execute_MultipleSplines_SingleRadius(self, splines, radius):
        for spline in splines:
            self.execute_SingleSpline_SingleRadius(spline, radius)
        return splines

    def execute_SingleSpline_SingleRadius(self, spline, radius):
        spline.radii = FloatList.fromValue(radius, len(spline.points))
        return spline

    def execute_SingleSpline_MultipleRadii(self, spline, radii):
        spline.radii = self.prepareRadiusList(radii, len(spline.points))
        return spline

    def prepareRadiusList(self, radii, length):
        if len(radii) == length:
            return FloatList.fromValues(radii)
        elif len(radii) < length:
            if len(radii) == 0:
                return FloatList.fromValue(0, length = length)
            else:
                return FloatList.fromValues(radii).repeated(length = length)
        else:
            return FloatList.fromValues(radii[:length])
