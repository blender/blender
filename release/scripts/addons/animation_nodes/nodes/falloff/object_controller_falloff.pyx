import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import VectorizedNode
from ... algorithms.rotations import eulerToDirection
from ... data_structures cimport BaseFalloff, DoubleList

from . mix_falloffs import MixFalloffs
from . invert_falloff import InvertFalloff
from . constant_falloff import ConstantFalloff
from . interpolate_falloff import InterpolateFalloff
from . directional_falloff import UniDirectionalFalloff
from . point_distance_falloff import PointDistanceFalloff

falloffTypeItems = [
    ("SPHERE", "Sphere", "", "NONE", 0),
    ("DIRECTIONAL", "Directional", "", "NONE", 1)
]

mixListTypeItems = [
    ("MAX", "Max", "", "NONE", 0),
    ("ADD", "Add", "", "NONE", 1)
]

axisDirectionItems = [(axis, axis, "") for axis in ("X", "Y", "Z", "-X", "-Y", "-Z")]

class ObjectControllerFalloffNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ObjectControllerFalloffNode"
    bl_label = "Object Controller Falloff"
    bl_width_default = 170

    falloffType = EnumProperty(name = "Falloff Type", items = falloffTypeItems,
        update = VectorizedNode.refresh)

    axisDirection = EnumProperty(name = "Axis Direction", default = "Z",
        items = axisDirectionItems, update = propertyChanged)

    mixListType = EnumProperty(name = "Mix List Type", default = "MAX",
        items = mixListTypeItems, update = propertyChanged)

    useObjectList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Object", "useObjectList",
            ("Object", "object", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Objects", "objects"))

        if self.falloffType == "SPHERE":
            self.newInput("Float", "Offset", "offset", value = 0)
            self.newInput("Float", "Falloff Width", "falloffWidth", value = 1.0)
        self.newInput("Interpolation", "Interpolation", "interpolation", defaultDrawType = "PROPERTY_ONLY")
        self.newInput("Boolean", "Invert", "invert", value = False)
        self.newOutput("Falloff", "Falloff", "falloff")

    def draw(self, layout):
        col = layout.column()
        col.prop(self, "falloffType", text = "")
        if self.falloffType == "DIRECTIONAL":
            col.row().prop(self, "axisDirection", expand = True)
        if self.useObjectList:
            col.prop(self, "mixListType", text = "")

    def getExecutionFunctionName(self):
        if self.useObjectList:
            if self.falloffType == "SPHERE":
                return "execute_Sphere_List"
            elif self.falloffType == "DIRECTIONAL":
                return "execute_Directional_List"
        else:
            if self.falloffType == "SPHERE":
                return "execute_Sphere"
            elif self.falloffType == "DIRECTIONAL":
                return "execute_Directional"

    def execute_Sphere_List(self, objects, offset, falloffWidth, interpolation, invert):
        falloffs = []
        for object in objects:
            if object is not None:
                falloffs.append(self.getSphereFalloff(object, offset, falloffWidth))
        if len(falloffs) == 0:
            return ConstantFalloff(0)
        else:
            return self.applyMixInterpolationAndInvert(falloffs, interpolation, invert)

    def execute_Sphere(self, object, offset, falloffWidth, interpolation, invert):
        falloff = self.getSphereFalloff(object, offset, falloffWidth)
        return self.applyInterpolationAndInvert(falloff, interpolation, invert)

    def getSphereFalloff(self, object, offset, falloffWidth):
        if object is None:
            return ConstantFalloff(0)

        matrix = object.matrix_world
        center = matrix.to_translation()
        size = abs(matrix.to_scale().x) + offset
        return PointDistanceFalloff(center, size-1, falloffWidth)

    def execute_Directional_List(self, objects, interpolation, invert):
        falloffs = []
        for object in objects:
            if object is not None:
                falloffs.append(self.getDirectionalFalloff(object))
        if len(falloffs) == 0:
            return ConstantFalloff(0)
        else:
            return self.applyMixInterpolationAndInvert(falloffs, interpolation, invert)

    def execute_Directional(self, object, interpolation, invert):
        falloff = self.getDirectionalFalloff(object)
        return self.applyInterpolationAndInvert(falloff, interpolation, invert)

    def getDirectionalFalloff(self, object):
        if object is None:
            return ConstantFalloff(0)

        matrix = object.matrix_world
        location = matrix.to_translation()
        size = max(matrix.to_scale().x, 0.0001)
        direction = eulerToDirection(matrix.to_euler(), self.axisDirection)

        return UniDirectionalFalloff(location, direction, size)

    def applyMixInterpolationAndInvert(self, falloffs, interpolation, invert):
        if self.mixListType == "MAX":
            falloff = MixFalloffs(falloffs, "MAX")
            return self.applyInterpolationAndInvert(falloff, interpolation, invert)
        elif self.mixListType == "ADD":
            falloffs = [InterpolateFalloff(falloff, interpolation) for falloff in falloffs]
            falloff = MixFalloffs(falloffs, "ADD")
            if invert: falloff = InvertFalloff(falloff)
            return falloff

    def applyInterpolationAndInvert(self, falloff, interpolation, invert):
        falloff = InterpolateFalloff(falloff, interpolation)
        if invert: falloff = InvertFalloff(falloff)
        return falloff
