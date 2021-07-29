from ... math cimport (
    setRotationMatrix,
    multMatrix3Parts,
    multMatrix4,
    toPyEuler3
)

cpdef getRotatedMatrixList(Matrix4x4List matrices, str type,
                           CDefaultList rotations, DoubleList influences):
    if len(matrices) != len(influences):
        raise Exception("amount of matrices and influences has to be equal")

    cdef RotateFunction rotate = getRotateFunction(type)
    cdef Euler3 *fullRotation
    cdef Euler3 influencedRotation
    cdef double influence
    cdef Py_ssize_t i
    cdef Matrix4x4List result = Matrix4x4List(length = len(matrices))

    for i in range(len(matrices)):
        influence = influences.data[i]
        fullRotation = <Euler3*>rotations.get(i)
        influencedRotation.x = fullRotation.x * influence
        influencedRotation.y = fullRotation.y * influence
        influencedRotation.z = fullRotation.z * influence
        influencedRotation.order = fullRotation.order
        rotate(result.data + i, matrices.data + i, &influencedRotation)
    return result

cdef RotateFunction getRotateFunction(str type) except *:
    if type == "LOCAL_AXIS__LOCAL_PIVOT":
        return rotate_LocalAxis_LocalPivot
    elif type == "GLOBAL_AXIS__LOCAL_PIVOT":
        return rotate_GlobalAxis_LocalPivot
    elif type == "GLOBAL_AXIS__GLOBAL_PIVOT":
        return rotate_GlobalAxis_GlobalPivot
    else:
        raise Exception("invalid type")

cdef void rotate_LocalAxis_LocalPivot(Matrix4 *target, Matrix4 *source, Euler3 *rotation):
    cdef Matrix4 rotationMatrix
    setRotationMatrix(&rotationMatrix, rotation)
    multMatrix3Parts(target, source, &rotationMatrix, keepFirst = True)

cdef void rotate_GlobalAxis_LocalPivot(Matrix4 *target, Matrix4 *source, Euler3 *rotation):
    cdef Matrix4 rotationMatrix
    setRotationMatrix(&rotationMatrix, rotation)
    multMatrix3Parts(target, &rotationMatrix, source, keepFirst = False)

cdef void rotate_GlobalAxis_GlobalPivot(Matrix4 *target, Matrix4 *source, Euler3 *rotation):
    cdef Matrix4 rotationMatrix
    setRotationMatrix(&rotationMatrix, rotation)
    multMatrix4(target, &rotationMatrix, source)
