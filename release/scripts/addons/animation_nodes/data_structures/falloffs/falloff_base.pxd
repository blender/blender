from . evaluation cimport FalloffEvaluator

cdef class Falloff:
    cdef bint clamped
    cdef dict evaluators

    cpdef FalloffEvaluator getEvaluator(self, str sourceType, bint clamped = ?, bint onlyC = ?)

cdef class BaseFalloff(Falloff):
    cdef str dataType
    cdef double evaluate(self, void* object, long index)

cdef class CompoundFalloff(Falloff):
    cdef list getDependencies(self)
    cdef list getClampingRequirements(self)
    cdef double evaluate(self, double* dependencyResults)
