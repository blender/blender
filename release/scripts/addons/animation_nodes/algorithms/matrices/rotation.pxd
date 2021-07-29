from ... math cimport (
    Matrix4,
    Euler3
)

from ... data_structures cimport (
    Matrix4x4List,
    DoubleList,
    CDefaultList
)

ctypedef void (*RotateFunction)(Matrix4 *target, Matrix4 *m, Euler3 *v)

cpdef getRotatedMatrixList(Matrix4x4List matrices, str type,
                           CDefaultList rotations, DoubleList influences)

cdef RotateFunction getRotateFunction(str type) except *
