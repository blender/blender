import bpy
from ... base_types import AnimationNode
from ... data_structures cimport BaseFalloff, DoubleList

class CustomFalloffNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CustomFalloffNode"
    bl_label = "Custom Falloff"

    def create(self):
        self.newInput("Float List", "Strengths", "strengths")
        self.newInput("Float", "Fallback", "fallback", hide = True).setRange(0, 1)
        self.newOutput("Falloff", "Falloff", "falloff")

    def execute(self, strength, fallback):
        return CustomFalloff(strength, fallback)


cdef class CustomFalloff(BaseFalloff):
    cdef DoubleList strengths
    cdef long length
    cdef double fallback

    def __cinit__(self, DoubleList strengths, double fallback):
        self.strengths = strengths
        self.length = strengths.length
        self.fallback = fallback
        self.clamped = False
        self.dataType = "All"

    cdef double evaluate(self, void* object, long index):
        if index < self.length:
            return self.strengths[index]
        return self.fallback
