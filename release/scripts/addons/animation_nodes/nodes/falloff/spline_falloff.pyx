import bpy
from bpy.props import *
from ... base_types import VectorizedNode
from ... events import propertyChanged
from ... math cimport Vector3, distanceVec3
from ... data_structures cimport Spline, PolySpline, BaseFalloff

from . mix_falloffs import MixFalloffs
from . constant_falloff import ConstantFalloff
from . interpolate_falloff import InterpolateFalloff

mixListTypeItems = [
    ("MAX", "Max", "", "NONE", 0),
    ("ADD", "Add", "", "NONE", 1)
]

class SplineFalloffNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_SplineFalloffNode"
    bl_label = "Spline Falloff"
    bl_width_default = 160

    resolution = IntProperty(name = "Resolution", default = 5, min = 2,
        description = "Poly spline segments per bezier spline segments")

    mixListType = EnumProperty(name = "Mix List Type", default = "MAX",
        items = mixListTypeItems, update = propertyChanged)

    useSplineList = VectorizedNode.newVectorizeProperty()

    def create(self):
        socketProps = dict(defaultDrawType = "PROPERTY_ONLY", dataIsModified = True)
        self.newVectorizedInput("Spline", "useSplineList",
            ("Spline", "spline", socketProps),
            ("Splines", "splines", socketProps))
        self.newInput("Float", "Distance", "distance", value = 0)
        self.newInput("Float", "Width", "width", value = 1, minValue = 0)
        self.newInput("Interpolation", "Interpolation", "interpolation",
            defaultDrawType = "PROPERTY_ONLY")

        self.newOutput("Falloff", "Falloff", "falloff")

    def draw(self, layout):
        if self.useSplineList:
            layout.prop(self, "mixListType", text = "")

    def drawAdvanced(self, layout):
        layout.prop(self, "resolution")

    def getExecutionFunctionName(self):
        if self.useSplineList:
            return "execute_List"
        else:
            return "execute_Single"

    def execute_Single(self, spline, distance, width, interpolation):
        falloff = self.falloffFromSpline(spline, distance, width)
        return InterpolateFalloff(falloff, interpolation)

    def execute_List(self, splines, distance, width, interpolation):
        falloffs = []
        for spline in splines:
            if spline.isEvaluable():
                falloffs.append(self.falloffFromSpline(spline, distance, width))

        if self.mixListType == "ADD":
            interpolatedFalloffs = [InterpolateFalloff(f, interpolation) for f in falloffs]
            return MixFalloffs(interpolatedFalloffs, "ADD", default = 0)
        elif self.mixListType == "MAX":
            falloff = MixFalloffs(falloffs, "MAX", default = 0)
            return InterpolateFalloff(falloff, interpolation)

    def falloffFromSpline(self, spline, distance, width):
        if not spline.isEvaluable():
            return ConstantFalloff(0)

        if spline.type == "POLY":
            falloffSpline = spline
        else:
            falloffSpline = PolySpline(spline.getSamples(self.resolution * (len(spline.points) - 1)))
            falloffSpline.cyclic = spline.cyclic

        return SplineFalloff(falloffSpline, distance, width)


cdef class SplineFalloff(BaseFalloff):
    cdef Spline spline
    cdef float distance, width

    def __cinit__(self, Spline spline, float distance, float width):
        self.spline = spline
        self.distance = distance
        self.width = max(width, 0.000001)
        self.clamped = False
        self.dataType = "Location"

    cdef double evaluate(self, void *point, long index):
        cdef Vector3 closestPoint
        cdef float parameter = self.spline.project_LowLevel(<Vector3*>point)
        self.spline.evaluate_LowLevel(parameter, &closestPoint)
        cdef float distance = distanceVec3(<Vector3*>point, &closestPoint)

        if distance < self.distance:
            return 1.0
        elif distance > self.distance + self.width:
            return 0.0
        else:
            return 1.0 - (distance - self.distance) / self.width
