from . vector cimport Vector3
from . matrix cimport Matrix4
from .. data_structures.lists.base_lists cimport Vector3DList, EulerList, Matrix4x4List

cpdef void transformVector3DList(Vector3DList vectors, matrix, bint ignoreTranslation = ?)
cpdef double distanceSumOfVector3DList(Vector3DList vectors)

cdef void mixVec3Arrays(Vector3* target, Vector3* a, Vector3* b, long arrayLength, float factor)
