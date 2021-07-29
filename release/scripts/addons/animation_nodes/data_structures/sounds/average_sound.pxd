from . sound cimport Sound
from .. lists.base_lists cimport FloatList

cdef class AverageSound(Sound):
    cdef:
        FloatList samples
        int startFrame
        int endFrame

    cpdef float evaluate(self, float frame)
    cdef float evaluateInt(self, int frame)
