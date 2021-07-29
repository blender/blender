cdef class FalloffEvaluator:
    '''The evaluator of a falloff is only valid while the falloff exists'''
    cdef object pyEvaluator

    cdef double evaluate(self, void* value, long index)
