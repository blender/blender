from ... math cimport Vector3
from . base_spline cimport Spline
from .. lists.base_lists cimport FloatList, Vector3DList

cdef class PolySpline(Spline):
    cdef:
        public Vector3DList points
        public FloatList radii

    cpdef FloatList getUniformParameters(self, long amount)
    cdef inline int getSegmentAmount(self)
