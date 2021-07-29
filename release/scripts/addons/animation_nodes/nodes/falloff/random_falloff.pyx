import bpy
from ... data_structures cimport BaseFalloff
from ... base_types import AnimationNode
from ... algorithms.random cimport uniformRandomNumber

class RandomFalloffNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RandomFalloffNode"
    bl_label = "Random Falloff"

    def create(self):
        self.newInput("Integer", "Seed", "seed")
        self.newInput("Float", "Min", "minValue", value = 0).setRange(0, 1)
        self.newInput("Float", "Max", "maxValue", value = 1).setRange(0, 1)
        self.newOutput("Falloff", "Falloff", "falloff")

    def execute(self, seed, minValue, maxValue):
        return RandomFalloff(seed % 0x7fffffff, minValue, maxValue)


cdef class RandomFalloff(BaseFalloff):
    cdef:
        int seed
        double minValue, maxValue

    def __cinit__(self, int seed, double minValue, double maxValue):
        self.seed = (seed * 534523) % 0x7fffffff
        self.minValue = minValue
        self.maxValue = maxValue
        self.dataType = "All"
        self.clamped = 0 <= min(minValue, maxValue) <= max(minValue, maxValue) <= 1

    cdef double evaluate(self, void* object, long index):
        return uniformRandomNumber((self.seed + index) % 0x7fffffff, self.minValue, self.maxValue)
