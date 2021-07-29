from . sound cimport Sound
from .. lists.base_lists cimport FloatList

cdef class SpectrumSound(Sound):
    cdef:
        FloatList samples
        FloatList zeroList
        int startFrame
        int endFrame
        int samplesPerFrame

    cpdef FloatList evaluate(self, float frame)
    cpdef float evaluateFrequency(self, float frame, float frequency)

    cdef FloatList evaluateInt(self, int frame)
