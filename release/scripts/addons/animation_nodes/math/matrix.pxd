from . vector cimport Vector3, Vector4
from . euler cimport Euler3

cdef struct Matrix4:
    float a11, a12, a13, a14
    float a21, a22, a23, a24
    float a31, a32, a33, a34
    float a41, a42, a43, a44

cdef struct Matrix3:
    float a11, a12, a13
    float a21, a22, a23
    float a31, a32, a33

ctypedef fused Matrix3_or_Matrix4:
    Matrix3
    Matrix4

cdef void transformVec3AsPoint_InPlace(Vector3* vector, Matrix4* matrix)
cdef void transformVec3AsPoint(Vector3* target, Vector3* vector, Matrix4* matrix)

cdef void transformVec3AsDirection_InPlace(Vector3* v, Matrix3_or_Matrix4* m)
cdef void transformVec3AsDirection(Vector3* target, Vector3* v, Matrix3_or_Matrix4* m)

cdef void multMatrix4AndVec4(Vector4* target, Matrix4* m, Vector4* v)

cdef void setIdentityMatrix(Matrix3_or_Matrix4* m)
cdef void setTranslationMatrix(Matrix4* m, Vector3* v)
cdef void setRotationMatrix(Matrix3_or_Matrix4* m, Euler3* e)
cdef void setScaleMatrix(Matrix3_or_Matrix4* m, Vector3* s)
cdef void setMatrixTranslation(Matrix4* m, Vector3* v)

cdef void setTranslationScaleMatrix(Matrix4* m, Vector3* t, Vector3* s)
cdef void setRotationScaleMatrix(Matrix3_or_Matrix4* m, Euler3* e, Vector3* s)
cdef void setTranslationRotationScaleMatrix(Matrix4* m, Vector3* t, Euler3* e, Vector3* s)

cdef void setRotationXMatrix(Matrix3_or_Matrix4* m, float angle)
cdef void setRotationYMatrix(Matrix3_or_Matrix4* m, float angle)
cdef void setRotationZMatrix(Matrix3_or_Matrix4* m, float angle)

cdef void mult3xMatrix_Reversed(Matrix3_or_Matrix4* target,
            Matrix3_or_Matrix4* m1,
            Matrix3_or_Matrix4* m2,
            Matrix3_or_Matrix4* m3)

cdef void setComposedMatrix(Matrix4* m, Vector3* t, Euler3* e, Vector3* s)

cdef void convertMatrix3ToMatrix4(Matrix4* t, Matrix3* s)
cdef void convertMatrix4ToMatrix3(Matrix3* t, Matrix4* s)

cdef void multMatrix3(Matrix3_or_Matrix4* target, Matrix3_or_Matrix4* x, Matrix3_or_Matrix4* y)
cdef void multMatrix4(Matrix4* target, Matrix4* x, Matrix4* y)
cdef void multMatrix3Parts(Matrix4* target, Matrix4* x, Matrix4* y, bint keepFirst = ?)

cdef void normalizeMatrix_3x3_Part(Matrix3_or_Matrix4* t, Matrix3_or_Matrix4* m)

cdef void transposeMatrix_Inplace(Matrix3_or_Matrix4 *m)
