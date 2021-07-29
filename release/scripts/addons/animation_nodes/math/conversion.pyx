from mathutils import Vector, Matrix, Euler
from mathutils import Quaternion as PyQuaternion

# Vectors
##########################################################

cdef Vector3 toVector3(value) except *:
    cdef Vector3 v
    setVector3(&v, value)
    return v

cdef setVector3(Vector3* v, value):
    if len(value) != 3:
        raise TypeError("element is not a 3D vector")
    v.x = value[0]
    v.y = value[1]
    v.z = value[2]

cdef toPyVector3(Vector3* v):
    return Vector((v.x, v.y, v.z))


cdef Vector4 toVector4(value) except *:
    cdef Vector4 v
    setVector4(&v, value)
    return v

cdef setVector4(Vector4* v, value):
    if len(value) != 4:
        raise TypeError("element is not a 3D vector")
    v.x = value[0]
    v.y = value[1]
    v.z = value[2]
    v.w = value[3]

cdef toPyVector4(Vector4* v):
    return Vector((v.x, v.y, v.z, v.w))


# Matrices
##########################################################

cdef Matrix4 toMatrix4(value) except *:
    cdef Matrix4 m
    setMatrix4(&m, value)
    return m

cdef setMatrix4(Matrix4* m, value):
    if not (len(value.row) == len(value.col) == 4):
        raise TypeError("element is not a 4x4 matrix")

    row1 = value[0]
    row2 = value[1]
    row3 = value[2]
    row4 = value[3]
    m.a11, m.a12, m.a13, m.a14 = row1[0], row1[1], row1[2], row1[3]
    m.a21, m.a22, m.a23, m.a24 = row2[0], row2[1], row2[2], row2[3]
    m.a31, m.a32, m.a33, m.a34 = row3[0], row3[1], row3[2], row3[3]
    m.a41, m.a42, m.a43, m.a44 = row4[0], row4[1], row4[2], row4[3]


cdef toPyMatrix4(Matrix4* m):
    return Matrix(((m.a11, m.a12, m.a13, m.a14),
                   (m.a21, m.a22, m.a23, m.a24),
                   (m.a31, m.a32, m.a33, m.a34),
                   (m.a41, m.a42, m.a43, m.a44)))

cdef toPyMatrix3(Matrix3* m):
    return Matrix(((m.a11, m.a12, m.a13),
                   (m.a21, m.a22, m.a23),
                   (m.a31, m.a32, m.a33)))


# Eulers
##########################################################

cdef Euler3 toEuler3(value) except *:
    cdef Euler3 e
    setEuler3(&e, value)
    return e

cdef setEuler3(Euler3* e, value):
    if len(value) != 3:
        raise TypeError("value is no euler value")
    e.x = value[0]
    e.y = value[1]
    e.z = value[2]
    cdef str order
    if isinstance(value, Euler):
        order = value.order
        if   order == "XYZ": e.order = 0
        elif order == "XZY": e.order = 1
        elif order == "YXZ": e.order = 2
        elif order == "YZX": e.order = 3
        elif order == "ZXY": e.order = 4
        elif order == "ZYX": e.order = 5
    else:
        e.order = 0

cdef toPyEuler3(Euler3* e):
    if e.order == 0: return Euler((e.x, e.y, e.z), "XYZ")
    if e.order == 1: return Euler((e.x, e.y, e.z), "XZY")
    if e.order == 2: return Euler((e.x, e.y, e.z), "YXZ")
    if e.order == 3: return Euler((e.x, e.y, e.z), "YZX")
    if e.order == 4: return Euler((e.x, e.y, e.z), "ZXY")
    if e.order == 5: return Euler((e.x, e.y, e.z), "ZYX")


# Quaternions
##########################################################

cdef Quaternion toQuaternion(value) except *:
    cdef Quaternion q
    setQuaternion(&q, value)
    return q

cdef setQuaternion(Quaternion* q, value):
    q.w = value.w
    q.x = value.x
    q.y = value.y
    q.z = value.z

cdef toPyQuaternion(Quaternion* q):
    return PyQuaternion((q.w, q.x, q.y, q.z))
