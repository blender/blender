from ... math cimport (
    Matrix4,
    Vector3
)

from ... data_structures cimport (
    Matrix4x4List,
    DoubleList,
    CDefaultList
)

ctypedef void (*ScaleFunction)(Matrix4 *m, Vector3 *v)

cpdef scaleMatrixList(Matrix4x4List matrices, str type,
                      CDefaultList scales, DoubleList influences)

cdef ScaleFunction getScaleFunction(str type) except *
