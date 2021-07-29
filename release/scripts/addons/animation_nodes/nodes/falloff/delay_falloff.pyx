import bpy
from ... base_types import AnimationNode
from ... data_structures cimport BaseFalloff, DoubleList
from . interpolate_falloff import InterpolateFalloff

class DelayFalloffNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_DelayFalloffNode"
    bl_label = "Delay Falloff"

    def create(self):
        self.newInput("Float", "Time", "time")
        self.newInput("Float", "Delay", "delay", value = 5)
        self.newInput("Float", "Duration", "duration", value = 20)
        self.newInput("Interpolation", "Interpolation", "interpolation", defaultDrawType = "PROPERTY_ONLY")
        self.newInput("Float List", "Offsets", "offsets")
        self.newOutput("Falloff", "Falloff", "falloff")

    def execute(self, frame, delay, duration, interpolation, offsets):
        falloff = DelayFalloff(frame, delay, duration, offsets)
        return InterpolateFalloff(falloff, interpolation)

cdef class DelayFalloff(BaseFalloff):
    cdef double frame
    cdef double delay
    cdef double duration
    cdef DoubleList offsets

    def __cinit__(self, double frame, double delay, double duration, DoubleList offsets = None):
        self.frame = frame
        self.delay = delay
        self.duration = duration
        self.offsets = DoubleList() if offsets is None else offsets
        self.clamped = True
        self.dataType = "All"

    cdef double evaluate(self, void* object, long index):
        cdef double offset
        if index >= self.offsets.length: offset = index
        else: offset = self.offsets.data[index]

        cdef double localFrame = self.frame - offset * self.delay
        if localFrame <= 0: return 0
        if localFrame <= self.duration: return localFrame / self.duration
        return 1
