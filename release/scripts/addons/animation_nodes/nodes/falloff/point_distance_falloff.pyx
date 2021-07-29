import bpy
from ... data_structures cimport BaseFalloff
from ... base_types import AnimationNode
from ... math cimport Vector3, setVector3, distanceVec3

class PointDistanceFalloffNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_PointDistanceFalloffNode"
    bl_label = "Point Distance Falloff"

    def create(self):
        self.newInput("Vector", "Origin", "origin")
        self.newInput("Float", "Size", "size")
        self.newInput("Float", "Falloff Width", "falloffWidth", value = 4)
        self.newOutput("Falloff", "Falloff", "falloff")

    def execute(self, origin, size, falloffWidth):
        return PointDistanceFalloff(origin, size, falloffWidth)


cdef class PointDistanceFalloff(BaseFalloff):
    cdef:
        Vector3 origin
        double factor
        double minDistance, maxDistance

    def __cinit__(self, vector, double size, double falloffWidth):
        if falloffWidth < 0:
            size += falloffWidth
            falloffWidth = -falloffWidth
        self.minDistance = size
        self.maxDistance = size + falloffWidth

        if self.minDistance == self.maxDistance:
            self.minDistance -= 0.00001
        self.factor = 1 / (self.maxDistance - self.minDistance)
        setVector3(&self.origin, vector)

        self.dataType = "Location"
        self.clamped = True

    cdef double evaluate(self, void* value, long index):
        cdef double distance = distanceVec3(&self.origin, <Vector3*>value)
        if distance <= self.minDistance: return 1
        if distance <= self.maxDistance: return 1 - (distance - self.minDistance) * self.factor
        return 0
