import bpy
from bpy.props import *
from ... base_types import AnimationNode
from . constant_falloff import ConstantFalloff
from ... data_structures cimport CompoundFalloff, Falloff

mixTypeItems = [
    ("ADD", "Add", "", "NONE", 0),
    ("MULTIPLY", "Multiply", "", "NONE", 1),
    ("MAX", "Max", "", "NONE", 2),
    ("MIN", "Min", "", "NONE", 3)]

useFactorTypes = {"ADD"}

class MixFalloffsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_MixFalloffsNode"
    bl_label = "Mix Falloffs"

    mixType = EnumProperty(name = "Mix Type", items = mixTypeItems,
        default = "MAX", update = AnimationNode.refresh)

    mixFalloffList = BoolProperty(name = "Mix Falloff List", default = False,
        update = AnimationNode.refresh)

    def create(self):
        if self.mixFalloffList:
            self.newInput("Falloff List", "Falloffs", "falloffs")
        else:
            self.newInput("Falloff", "A", "a")
            self.newInput("Falloff", "B", "b")
        self.newOutput("Falloff", "Falloff", "falloff")

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "mixType", text = "")
        row.prop(self, "mixFalloffList", text = "", icon = "LINENUMBERS_ON")

    def getExecutionFunctionName(self):
        if self.mixFalloffList:
            return "execute_List"
        else:
            return "execute_Two"

    def execute_List(self, falloffs):
        return MixFalloffs(falloffs, self.mixType, default = 1)

    def execute_Two(self, a, b):
        return MixFalloffs([a, b], self.mixType, default = 1)


class MixFalloffs:
    def __new__(cls, list falloffs not None, str method not None, double default = 1):
        if len(falloffs) == 0:
            return ConstantFalloff(default)
        elif len(falloffs) == 1:
            return falloffs[0]
        elif len(falloffs) == 2:
            if method == "ADD": return AddTwoFalloffs(*falloffs)
            elif method == "MULTIPLY": return MultiplyTwoFalloffs(*falloffs)
            elif method == "MAX": return MaxTwoFalloffs(*falloffs)
            elif method == "MIN": return MinTwoFalloffs(*falloffs)
            raise Exception("invalid method")
        else:
            if method == "ADD": return AddFalloffs(falloffs)
            elif method == "MULTIPLY": return MultiplyFalloffs(falloffs)
            elif method == "MAX": return MaxFalloffs(falloffs)
            elif method == "MIN": return MinFalloffs(falloffs)
            raise Exception("invalid method")


cdef class MixTwoFalloffsBase(CompoundFalloff):
    cdef:
        Falloff a, b

    def __cinit__(self, Falloff a, Falloff b):
        self.a = a
        self.b = b

    cdef list getDependencies(self):
        return [self.a, self.b]

cdef class AddTwoFalloffs(MixTwoFalloffsBase):
    cdef double evaluate(self, double *dependencyResults):
        return dependencyResults[0] + dependencyResults[1]

cdef class MultiplyTwoFalloffs(MixTwoFalloffsBase):
    cdef double evaluate(self, double *dependencyResults):
        return dependencyResults[0] * dependencyResults[1]

cdef class MinTwoFalloffs(MixTwoFalloffsBase):
    cdef double evaluate(self, double *dependencyResults):
        return min(dependencyResults[0], dependencyResults[1])

cdef class MaxTwoFalloffs(MixTwoFalloffsBase):
    cdef double evaluate(self, double *dependencyResults):
        return max(dependencyResults[0], dependencyResults[1])


cdef class MixFalloffsBase(CompoundFalloff):
    cdef list falloffs
    cdef int amount

    def __init__(self, list falloffs not None):
        self.falloffs = falloffs
        self.amount = len(falloffs)
        if self.amount == 0:
            raise Exception("at least one falloff required")

    cdef list getDependencies(self):
        return self.falloffs

cdef class AddFalloffs(MixFalloffsBase):
    cdef double evaluate(self, double *dependencyResults):
        cdef int i
        cdef double sum = 0
        for i in range(self.amount):
            sum += dependencyResults[i]
        return sum

cdef class MultiplyFalloffs(MixFalloffsBase):
    cdef double evaluate(self, double *dependencyResults):
        cdef int i
        cdef double product = 1
        for i in range(self.amount):
            product *= dependencyResults[i]
        return product

cdef class MinFalloffs(MixFalloffsBase):
    cdef double evaluate(self, double *dependencyResults):
        cdef int i
        cdef double minValue = dependencyResults[0]
        for i in range(1, self.amount):
            if dependencyResults[i] < minValue:
                minValue = dependencyResults[i]
        return minValue

cdef class MaxFalloffs(MixFalloffsBase):
    cdef double evaluate(self, double *dependencyResults):
        cdef int i
        cdef double maxValue = dependencyResults[0]
        for i in range(1, self.amount):
            if dependencyResults[i] > maxValue:
                maxValue = dependencyResults[i]
        return maxValue
