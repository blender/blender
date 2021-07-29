from . lists.base_lists cimport DoubleList

ctypedef double (*InterpolationFunction)(Interpolation, double)

cdef class Interpolation:
    cdef bint clamped
    cdef double evaluate(self, double x)
    cdef double derivative(self, double x)
