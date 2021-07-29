from libc.math cimport sin, cos, sqrt

cdef void transformVec3AsPoint_InPlace(Vector3* v, Matrix4* m):
    cdef float newX, newY, newZ
    newX = v.x * m.a11 + v.y * m.a12 + v.z * m.a13 + m.a14
    newY = v.x * m.a21 + v.y * m.a22 + v.z * m.a23 + m.a24
    newZ = v.x * m.a31 + v.y * m.a32 + v.z * m.a33 + m.a34
    v.x, v.y, v.z = newX, newY, newZ

cdef void transformVec3AsPoint(Vector3* target, Vector3* v, Matrix4* m):
    target.x = v.x * m.a11 + v.y * m.a12 + v.z * m.a13 + m.a14
    target.y = v.x * m.a21 + v.y * m.a22 + v.z * m.a23 + m.a24
    target.z = v.x * m.a31 + v.y * m.a32 + v.z * m.a33 + m.a34

cdef void transformVec3AsDirection_InPlace(Vector3* v, Matrix3_or_Matrix4* m):
    cdef float newX, newY, newZ
    newX = v.x * m.a11 + v.y * m.a12 + v.z * m.a13
    newY = v.x * m.a21 + v.y * m.a22 + v.z * m.a23
    newZ = v.x * m.a31 + v.y * m.a32 + v.z * m.a33
    v.x, v.y, v.z = newX, newY, newZ

cdef void transformVec3AsDirection(Vector3* target, Vector3* v, Matrix3_or_Matrix4* m):
    target.x = v.x * m.a11 + v.y * m.a12 + v.z * m.a13
    target.y = v.x * m.a21 + v.y * m.a22 + v.z * m.a23
    target.z = v.x * m.a31 + v.y * m.a32 + v.z * m.a33

cdef void multMatrix4AndVec4(Vector4* target, Matrix4* m, Vector4* v):
    target.x = v.x * m.a11 + v.y * m.a12 + v.z * m.a13 + v.w * m.a14
    target.y = v.x * m.a21 + v.y * m.a22 + v.z * m.a23 + v.w * m.a24
    target.z = v.x * m.a31 + v.y * m.a32 + v.z * m.a33 + v.w * m.a34
    target.w = v.x * m.a41 + v.y * m.a42 + v.z * m.a43 + v.w * m.a44

cdef void setIdentityMatrix(Matrix3_or_Matrix4* m):
    m.a12 = m.a13 = 0
    m.a21 = m.a23 = 0
    m.a31 = m.a32 = 0
    m.a11 = m.a22 = m.a33 = 1
    if Matrix3_or_Matrix4 is Matrix4:
        m.a14 = m.a24 = m.a34 = 0
        m.a41 = m.a42 = m.a43 = 0
        m.a44 = 1

cdef void setTranslationMatrix(Matrix4* m, Vector3* v):
    m.a14, m.a24, m.a34 = v.x, v.y, v.z
    m.a11 = m.a22 = m.a33 = m.a44 = 1
    m.a12 = m.a13 = 0
    m.a21 = m.a23 = 0
    m.a31 = m.a32 = 0
    m.a41 = m.a42 = m.a43 = 0

cdef void setMatrixTranslation(Matrix4* m, Vector3* v):
    m.a14, m.a24, m.a34 = v.x, v.y, v.z

cdef void setTranslationScaleMatrix(Matrix4* m, Vector3* t, Vector3* s):
    m.a11, m.a22, m.a33 = s.x, s.y, s.z
    m.a14, m.a24, m.a34 = t.x, t.y, t.z
    m.a44 = 1
    m.a12 = m.a13 = 0
    m.a21 = m.a23 = 0
    m.a31 = m.a32 = 0
    m.a41 = m.a42 = m.a43 = 0

cdef void setRotationMatrix(Matrix3_or_Matrix4* m, Euler3* e):
    if e.order == 0:
        setRotationXYZMatrix(m, e)
        return

    cdef Matrix3 xMat, yMat, zMat, rotation
    setRotationXMatrix(&xMat, e.x)
    setRotationYMatrix(&yMat, e.y)
    setRotationZMatrix(&zMat, e.z)

    if Matrix3_or_Matrix4 is Matrix3:
        joinRotationMatricesInOrder(e.order, &xMat, &yMat, &zMat, m)
    else:
        joinRotationMatricesInOrder(e.order, &xMat, &yMat, &zMat, &rotation)
        convertMatrix3ToMatrix4(m, &rotation)

cdef void setRotationScaleMatrix(Matrix3_or_Matrix4* m, Euler3* e, Vector3* s):
    cdef Matrix3 rotation, scale, rotationScale
    setScaleMatrix(&scale, s)
    setRotationMatrix(&rotation, e)

    if Matrix3_or_Matrix4 is Matrix3:
        multMatrix3(m, &rotation, &scale)
    else:
        multMatrix3(&rotationScale, &rotation, &scale)
        convertMatrix3ToMatrix4(m, &rotationScale)

cdef void setTranslationRotationScaleMatrix(Matrix4* m, Vector3* t, Euler3* e, Vector3* s):
    setRotationScaleMatrix(m, e, s)
    m.a14, m.a24, m.a34 = t.x, t.y, t.z

cdef void setScaleMatrix(Matrix3_or_Matrix4* m, Vector3* s):
    m.a11, m.a22, m.a33 = s.x, s.y, s.z
    m.a12 = m.a13 = 0
    m.a21 = m.a23 = 0
    m.a31 = m.a32 = 0
    if Matrix3_or_Matrix4 is Matrix4:
        m.a14 = m.a24 = m.a34 = 0
        m.a41 = m.a42 = m.a43 = 0
        m.a44 = 1

cdef void convertMatrix3ToMatrix4(Matrix4* t, Matrix3* s):
    t.a11, t.a12, t.a13, t.a14 = s.a11, s.a12, s.a13, 0
    t.a21, t.a22, t.a23, t.a24 = s.a21, s.a22, s.a23, 0
    t.a31, t.a32, t.a33, t.a34 = s.a31, s.a32, s.a33, 0
    t.a41, t.a42, t.a43, t.a44 = 0, 0, 0, 1

cdef void convertMatrix4ToMatrix3(Matrix3* t, Matrix4* s):
    t.a11, t.a12, t.a13 = s.a11, s.a12, s.a13
    t.a21, t.a22, t.a23 = s.a21, s.a22, s.a23
    t.a31, t.a32, t.a33 = s.a31, s.a32, s.a33

cdef void joinRotationMatricesInOrder(char order, Matrix3* x, Matrix3* y, Matrix3* z, Matrix3* target):
    if order == 0:   mult3xMatrix_Reversed(target, x, y, z)
    elif order == 1: mult3xMatrix_Reversed(target, x, z, y)
    elif order == 2: mult3xMatrix_Reversed(target, y, x, z)
    elif order == 3: mult3xMatrix_Reversed(target, y, z, x)
    elif order == 4: mult3xMatrix_Reversed(target, z, x, y)
    elif order == 5: mult3xMatrix_Reversed(target, z, y, x)

cdef void setRotationXMatrix(Matrix3_or_Matrix4* m, float angle):
    cdef float sinValue = sin(angle)
    cdef float cosValue = cos(angle)
    m.a11 = 1
    m.a12 = m.a13 = m.a21 = m.a31 = 0
    m.a22 = m.a33 = cosValue
    m.a23 = -sinValue
    m.a32 = sinValue
    if Matrix3_or_Matrix4 is Matrix4:
        m.a14 = m.a24 = m.a34 = 0
        m.a41 = m.a42 = m.a43 = 0
        m.a44 = 1

cdef void setRotationYMatrix(Matrix3_or_Matrix4* m, float angle):
    cdef float sinValue = sin(angle)
    cdef float cosValue = cos(angle)
    m.a22 = 1
    m.a12 = m.a21 = m.a23 = m.a32 = 0
    m.a11 = m.a33 = cosValue
    m.a13 = sinValue
    m.a31 = -sinValue
    if Matrix3_or_Matrix4 is Matrix4:
        m.a14 = m.a24 = m.a34 = 0
        m.a41 = m.a42 = m.a43 = 0
        m.a44 = 1

cdef void setRotationZMatrix(Matrix3_or_Matrix4* m, float angle):
    cdef float sinValue = sin(angle)
    cdef float cosValue = cos(angle)
    m.a33 = 1
    m.a13 = m.a23 = m.a31 = m.a32 = 0
    m.a11 = m.a22 = cosValue
    m.a12 = -sinValue
    m.a21 = sinValue
    if Matrix3_or_Matrix4 is Matrix4:
        m.a14 = m.a24 = m.a34 = 0
        m.a41 = m.a42 = m.a43 = 0
        m.a44 = 1

cdef void setRotationXYZMatrix(Matrix3_or_Matrix4 *m, Euler3 *rotation):
    cdef float sx, sy, sz
    cdef float cx, cy, cz
    cdef float cc, cs, sc, ss

    sx, sy, sz = sin(rotation.x), sin(rotation.y), sin(rotation.z)
    cx, cy, cz = cos(rotation.x), cos(rotation.y), cos(rotation.z)

    cc = cx * cz
    cs = cx * sz
    sc = sx * cz
    ss = sx * sz

    m.a11 = cy * cz
    m.a12 = sy * sc - cs
    m.a13 = sy * cc + ss
    m.a21 = cy * sz
    m.a22 = sy * ss + cc
    m.a23 = sy * cs - sc
    m.a31 = -sy
    m.a32 = cy * sx
    m.a33 = cy * cx

    if Matrix3_or_Matrix4 is Matrix4:
        m.a14 = m.a24 = m.a34 = 0
        m.a41 = m.a42 = m.a43 = 0
        m.a44 = 1

cdef void setComposedMatrix(Matrix4* m, Vector3* t, Euler3* e, Vector3* s):
    setRotationScaleMatrix(m, e, s)
    m.a14, m.a24, m.a34 = t.x, t.y, t.z


cdef void multMatrix4(Matrix4* target, Matrix4* x, Matrix4* y):
    target.a11 = x.a11 * y.a11  +  x.a12 * y.a21  +  x.a13 * y.a31  +  x.a14 * y.a41
    target.a12 = x.a11 * y.a12  +  x.a12 * y.a22  +  x.a13 * y.a32  +  x.a14 * y.a42
    target.a13 = x.a11 * y.a13  +  x.a12 * y.a23  +  x.a13 * y.a33  +  x.a14 * y.a43
    target.a14 = x.a11 * y.a14  +  x.a12 * y.a24  +  x.a13 * y.a34  +  x.a14 * y.a44

    target.a21 = x.a21 * y.a11  +  x.a22 * y.a21  +  x.a23 * y.a31  +  x.a24 * y.a41
    target.a22 = x.a21 * y.a12  +  x.a22 * y.a22  +  x.a23 * y.a32  +  x.a24 * y.a42
    target.a23 = x.a21 * y.a13  +  x.a22 * y.a23  +  x.a23 * y.a33  +  x.a24 * y.a43
    target.a24 = x.a21 * y.a14  +  x.a22 * y.a24  +  x.a23 * y.a34  +  x.a24 * y.a44

    target.a31 = x.a31 * y.a11  +  x.a32 * y.a21  +  x.a33 * y.a31  +  x.a34 * y.a41
    target.a32 = x.a31 * y.a12  +  x.a32 * y.a22  +  x.a33 * y.a32  +  x.a34 * y.a42
    target.a33 = x.a31 * y.a13  +  x.a32 * y.a23  +  x.a33 * y.a33  +  x.a34 * y.a43
    target.a34 = x.a31 * y.a14  +  x.a32 * y.a24  +  x.a33 * y.a34  +  x.a34 * y.a44

    target.a41 = x.a41 * y.a11  +  x.a42 * y.a21  +  x.a43 * y.a31  +  x.a44 * y.a41
    target.a42 = x.a41 * y.a12  +  x.a42 * y.a22  +  x.a43 * y.a32  +  x.a44 * y.a42
    target.a43 = x.a41 * y.a13  +  x.a42 * y.a23  +  x.a43 * y.a33  +  x.a44 * y.a43
    target.a44 = x.a41 * y.a14  +  x.a42 * y.a24  +  x.a43 * y.a34  +  x.a44 * y.a44

cdef void multMatrix3(Matrix3_or_Matrix4* target, Matrix3_or_Matrix4* x, Matrix3_or_Matrix4* y):
    target.a11 = x.a11 * y.a11  +  x.a12 * y.a21  +  x.a13 * y.a31
    target.a12 = x.a11 * y.a12  +  x.a12 * y.a22  +  x.a13 * y.a32
    target.a13 = x.a11 * y.a13  +  x.a12 * y.a23  +  x.a13 * y.a33

    target.a21 = x.a21 * y.a11  +  x.a22 * y.a21  +  x.a23 * y.a31
    target.a22 = x.a21 * y.a12  +  x.a22 * y.a22  +  x.a23 * y.a32
    target.a23 = x.a21 * y.a13  +  x.a22 * y.a23  +  x.a23 * y.a33

    target.a31 = x.a31 * y.a11  +  x.a32 * y.a21  +  x.a33 * y.a31
    target.a32 = x.a31 * y.a12  +  x.a32 * y.a22  +  x.a33 * y.a32
    target.a33 = x.a31 * y.a13  +  x.a32 * y.a23  +  x.a33 * y.a33

cdef void multMatrix3Parts(Matrix4* target, Matrix4* x, Matrix4* y, bint keepFirst = True):
    multMatrix3(target, x, y)
    cdef Matrix4* k = x if keepFirst else y
    target.a14, target.a24, target.a34 = k.a14, k.a24, k.a34
    target.a41, target.a42, target.a43 = k.a41, k.a42, k.a43
    target.a44 = k.a44

cdef void mult3xMatrix_Reversed(Matrix3_or_Matrix4* target,
            Matrix3_or_Matrix4* m1,
            Matrix3_or_Matrix4* m2,
            Matrix3_or_Matrix4* m3):
    cdef Matrix3_or_Matrix4 tmp
    if Matrix3_or_Matrix4 is Matrix3:
        multMatrix3(&tmp, m3, m2)
        multMatrix3(target, &tmp, m1)
    if Matrix3_or_Matrix4 is Matrix4:
        multMatrix4(&tmp, m3, m2)
        multMatrix4(target, &tmp, m1)

cdef void normalizeMatrix_3x3_Part(Matrix3_or_Matrix4* t, Matrix3_or_Matrix4* m):
    cdef float len1, len2, len3
    len1 = sqrt(m.a11 * m.a11 + m.a21 * m.a21 + m.a31 * m.a31)
    len2 = sqrt(m.a12 * m.a12 + m.a22 * m.a22 + m.a32 * m.a32)
    len3 = sqrt(m.a13 * m.a13 + m.a23 * m.a23 + m.a33 * m.a33)

    if len1 > 0: t.a11, t.a21, t.a31 = m.a11 / len1, m.a21 / len1, m.a31 / len1
    else: t.a11 = t.a21 = t.a31 = 0

    if len2 > 0: t.a12, t.a22, t.a32 = m.a12 / len2, m.a22 / len2, m.a32 / len2
    else: t.a12 = t.a22 = t.a32 = 0

    if len3 > 0: t.a13, t.a23, t.a33 = m.a13 / len3, m.a23 / len3, m.a33 / len3
    else: t.a13 = t.a23 = t.a33 = 0

    if Matrix3_or_Matrix4 is Matrix4:
        t.a14, t.a24, t.a34 = m.a14, m.a24, m.a34
        t.a41, t.a42, t.a43, t.a44 = m.a41, m.a42, m.a43, m.a44

cdef void transposeMatrix_Inplace(Matrix3_or_Matrix4 *m):
    m.a12, m.a21 = m.a21, m.a12
    m.a13, m.a31 = m.a31, m.a13
    m.a23, m.a32 = m.a32, m.a23

    if Matrix3_or_Matrix4 is Matrix4:
        m.a14, m.a41 = m.a41, m.a14
        m.a24, m.a42 = m.a42, m.a24
        m.a34, m.a43 = m.a43, m.a34
