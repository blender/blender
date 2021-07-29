import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... data_structures cimport BaseFalloff
from ... algorithms.perlin_noise cimport perlinNoise1D

class WiggleFalloffNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_WiggleFalloffNode"
    bl_label = "Wiggle Falloff"

    def create(self):
        self.newInput("Integer", "Seed", "seed")
        self.newInput("Float", "Evolution", "evolution")
        self.newInput("Float", "Speed", "speed", value = 1, minValue = 0)
        self.newInput("Float", "Offset", "offset", value = 0.5)
        self.newInput("Float", "Amplitude", "amplitude", value = 0.5)
        self.newInput("Integer", "Octaves", "octaves", value = 2, minValue = 0)
        self.newInput("Float", "Persistance", "persistance", value = 0.3)

        self.newOutput("Falloff", "Falloff", "falloff")

    def execute(self, seed, evolution, speed, offset, amplitude, octaves, persistance):
        evolution *= max(speed, 0) / 10
        return WiggleFalloff(seed, evolution, offset, amplitude, octaves, persistance)

cdef class WiggleFalloff(BaseFalloff):
    cdef:
        double evolution
        double offset, amplitude
        double persistance
        int octaves

    def __cinit__(self, double seed, double evolution,
                        double offset, double amplitude,
                        int octaves, double persistance):
        self.evolution = seed * 3413123 + evolution
        self.amplitude = amplitude
        self.persistance = persistance
        self.offset = offset
        self.octaves = min(max(octaves, 0), 100)
        self.clamped = False
        self.dataType = "All"

    cdef double evaluate(self, void* object, long index):
        cdef double x = self.evolution + index * 134127
        cdef double noise = perlinNoise1D(x, self.persistance, self.octaves)
        return self.amplitude * noise + self.offset
