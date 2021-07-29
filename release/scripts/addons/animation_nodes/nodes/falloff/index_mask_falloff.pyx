import bpy
cimport cython
from bpy.props import *
from ... utils.limits cimport LONG_MIN, LONG_MAX
from ... data_structures cimport BaseFalloff
from ... algorithms.random cimport randomNumber_Positive
from ... base_types import AnimationNode

maskTypeItems = [
    ("EVERY_NTH", "Every Nth", "", "NONE", 0),
    ("RANDOM", "Random", "", "NONE", 1)
]

class IndexMaskFalloffNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_IndexMaskFalloffNode"
    bl_label = "Index Mask Falloff"
    bl_width_default = 150

    maskType = EnumProperty(name = "Mask Type",
        items = maskTypeItems, update = AnimationNode.refresh)

    def create(self):
        if self.maskType == "EVERY_NTH":
            self.newInput("Integer", "Step", "step", value = 2, minValue = 1)
            self.newInput("Integer", "Offset", "offset", value = 0)
        elif self.maskType == "RANDOM":
            self.newInput("Integer", "Seed", "seed")
            self.newInput("Float", "Probability", "probability", value = 0.5).setRange(0, 1)

        self.newInput("Float", "A", "valueA", value = 0).setRange(0, 1)
        self.newInput("Float", "B", "valueB", value = 1).setRange(0, 1)
        self.newOutput("Falloff", "Falloff", "falloff")

    def draw(self, layout):
        layout.prop(self, "maskType", text = "")

    def getExecutionFunctionName(self):
        if self.maskType == "EVERY_NTH":
            return "execute_EveryNth"
        elif self.maskType == "RANDOM":
            return "execute_Random"

    def execute_EveryNth(self, step, offset, valueA, valueB):
        return MaskEveryNthFalloff(step, offset, valueA, valueB)

    def execute_Random(self, seed, probability, valueA, valueB):
        return MaskRandomFalloff(seed, probability, valueA, valueB)

cdef class MaskEveryNthFalloff(BaseFalloff):
    cdef long step, offset
    cdef double valueA, valueB

    def __cinit__(self, step, offset, double valueA, double valueB):
        self.step = max(min(step, LONG_MAX), 1)
        self.offset = max(min(offset, LONG_MAX), LONG_MIN)
        self.valueA = valueA
        self.valueB = valueB
        self.dataType = "All"

    @cython.cdivision(True)
    cdef double evaluate(self, void* object, long index):
        if (index + self.offset) % self.step != 0:
            return self.valueA
        return self.valueB

cdef class MaskRandomFalloff(BaseFalloff):
    cdef long seed
    cdef double probability
    cdef double valueA, valueB

    def __cinit__(self, seed, double probability, double valueA, double valueB):
        self.seed = (seed * 7856353) % LONG_MAX
        self.probability = probability
        self.valueA = valueA
        self.valueB = valueB
        self.dataType = "All"

    @cython.cdivision(True)
    cdef double evaluate(self, void* object, long index):
        if randomNumber_Positive(index + self.seed) < self.probability:
            return self.valueA
        return self.valueB
