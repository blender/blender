from libc.math cimport atan2, sqrt, hypot
from . matrix cimport normalizeMatrix_3x3_Part
from . conversion cimport toPyEuler3

# Matrix to Euler
################################################################

def matrix4x4ListToEulerList(Matrix4x4List matrices, bint isNormalized = False):
    cdef EulerList rotations = EulerList(length = matrices.length)
    cdef long i
    if isNormalized:
        for i in range(rotations.length):
            normalizedMatrixToEuler(rotations.data + i, matrices.data + i)
    else:
        for i in range(rotations.length):
            matrixToEuler(rotations.data + i, matrices.data + i)
    return rotations

cdef matrixToEuler(Euler3* target, Matrix3_or_Matrix4* m):
    cdef Matrix3_or_Matrix4 _m
    normalizeMatrix_3x3_Part(&_m, m)
    normalizedMatrixToEuler(target, &_m)

cdef normalizedMatrixToEuler(Euler3* e, Matrix3_or_Matrix4* m):
    cdef Euler3 e1, e2
    normalizedMatrixToEuler2(&e1, &e2, m)

    cdef float abs1 = abs(e1.x) + abs(e1.y) + abs(e1.z)
    cdef float abs2 = abs(e2.x) + abs(e2.y) + abs(e2.z)

    if abs1 < abs2:
        e[0] = e1
    else:
        e[0] = e2

cdef normalizedMatrixToEuler2(Euler3* e1, Euler3* e2, Matrix3_or_Matrix4* m):
    '''Create 2 eulers and pick the better one later'''
    cdef float cy = hypot(m.a11, m.a21)
    e1.order = e2.order = 0
    if cy > 0.000001:
        e1.x = atan2(m.a32, m.a33)
        e1.y = atan2(-m.a31, cy)
        e1.z = atan2(m.a21, m.a11)

        e2.x = atan2(-m.a32, -m.a33)
        e2.y = atan2(-m.a31, -cy)
        e2.z = atan2(-m.a21, -m.a11)
    else:
        e1.x = e2.x = atan2(-m.a23, m.a22)
        e1.y = e2.y = atan2(-m.a31, cy)
        e1.z = e2.z = 0.0

# Matrix to Quaternion
################################################################

def matrix4x4ListToQuaternionList(Matrix4x4List matrices, bint isNormalized = False):
    cdef QuaternionList quaternions = QuaternionList(length = matrices.length)
    cdef long i
    if isNormalized:
        for i in range(quaternions.length):
            normalizedMatrixToQuaternion(quaternions.data + i, matrices.data + i)
    else:
        for i in range(quaternions.length):
            matrixToQuaternion(quaternions.data + i, matrices.data + i)
    return quaternions

cdef void matrixToQuaternion(Quaternion* target, Matrix3_or_Matrix4* m):
    cdef Matrix3_or_Matrix4 _m
    normalizeMatrix_3x3_Part(&_m, m)
    normalizedMatrixToQuaternion(target, &_m)

cdef void normalizedMatrixToQuaternion(Quaternion* q, Matrix3_or_Matrix4* m):
    cdef float trace, s

    trace = 0.25 * (1.0 + m.a11 + m.a22 + m.a33)
    if trace > 0.0001:
        s = sqrt(trace)
        q.w = s
        s = 1.0 / (4.0 * s)
        q.x = -s * (m.a23 - m.a32)
        q.y = -s * (m.a31 - m.a13)
        q.z = -s * (m.a12 - m.a21)
    elif m.a11 > m.a22 and m.a11 > m.a33:
        s = 2.0 * sqrt(1.0 + m.a11 - m.a22 - m.a33)
        q.x = 0.25 * s
        s = 1.0 / s
        q.w = -s * (m.a23 - m.a32)
        q.y = s * (m.a21 + m.a12)
        q.z = s * (m.a31 + m.a13)
    elif m.a22 > m.a33:
        s = 2.0 * sqrt(1.0 + m.a22 - m.a11 - m.a33)
        q.y = 0.25 * s
        s = 1.0 / s
        q.w = -s * (m.a31 - m.a13)
        q.x = s * (m.a21 + m.a12)
        q.z = s * (m.a32 + m.a23)
    else:
        s = 2.0 * sqrt(1.0 + m.a33 - m.a11 - m.a22)
        q.z = 0.25 * s
        s = 1.0 / s
        q.w = -s * (m.a12 - m.a21)
        q.x = s * (m.a31 + m.a13)
        q.y = s * (m.a32 + m.a23)
