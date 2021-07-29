from ... data_structures cimport EulerList, Matrix4x4List, Vector3DList, CDefaultList
from ... math cimport (toVector3, toPyVector3, Matrix3, toEuler3,
                       transformVec3AsDirection_InPlace, setRotationMatrix,
                       setIdentityMatrix, toPyMatrix4, crossVec3, normalizeVec3,
                       normalizeVec3_InPlace)


axixNumbers = { "X" : 0,  "Y" : 1,  "Z" : 2,
               "-X" : 3, "-Y" : 4, "-Z" : 5}

cdef Vector3[6] axisVectors
axisVectors[0] = toVector3((1, 0, 0))
axisVectors[1] = toVector3((0, 1, 0))
axisVectors[2] = toVector3((0, 0, 1))
axisVectors[3] = toVector3((-1, 0, 0))
axisVectors[4] = toVector3((0, -1, 0))
axisVectors[5] = toVector3((0, 0, -1))

def eulerToDirection(rotation, str axis):
    cdef char _axis = axixNumbers[axis]
    cdef Euler3 _rotation = toEuler3(rotation)
    cdef Vector3 direction
    eulerToDirection_LowLevel(&direction, &_rotation, _axis)
    return toPyVector3(&direction)

cdef eulerToDirection_LowLevel(Vector3* target, Euler3* rotation, char axis):
    cdef Matrix3 matrix
    setRotationMatrix(&matrix, rotation)
    target[0] = axisVectors[axis]
    transformVec3AsDirection_InPlace(target, &matrix)

def directionToMatrix(direction, guide, trackAxis = "Z", guideAxis = "X"):
    cdef:
        Matrix4 result
        Vector3 _direction = toVector3(direction)
        Vector3 _guide = toVector3(guide)
        char _trackAxis = axixNumbers[trackAxis]
        char _guideAxis = axixNumbers[guideAxis]
    directionToMatrix_LowLevel(&result, &_direction, &_guide, _trackAxis, _guideAxis)
    return toPyMatrix4(&result)

cdef directionToMatrix_LowLevel(Matrix4* target,
                                Vector3* direction, Vector3* guide,
                                char trackAxis, char guideAxis):
    '''
    trackAxis in [0, 5] -> X, Y, Z, -X, -Y, -Z
    guideAxis in [0, 2] -> X, Y, Z
    '''
    if trackAxis % 3 == guideAxis or 0 == direction.x == direction.y == direction.z:
        setIdentityMatrix(target)
        return

    cdef Vector3 x, y, z
    cdef Vector3 _guide

    normalizeVec3(&z, direction)
    normalizeVec3(&_guide, guide)
    crossVec3(&y, &z, &_guide)

    cdef Vector3 tmp
    if 0 == y.x == y.y == y.z:
        crossVec3(&tmp, &z, axisVectors + guideAxis)
        if 0 == tmp.x == tmp.y == tmp.z:
            y = axisVectors[guideAxis]
        else:
            y = tmp

    crossVec3(&x, &y, &z)

    changeAxis(trackAxis, guideAxis, &x, &y, &z)

    normalizeVec3_InPlace(&x)
    normalizeVec3_InPlace(&y)
    # z is already normalized

    target.a11, target.a12, target.a13 = x.x, y.x, z.x
    target.a21, target.a22, target.a23 = x.y, y.y, z.y
    target.a31, target.a32, target.a33 = x.z, y.z, z.z

    target.a14 = target.a24 = target.a34 = 0
    target.a41 = target.a42 = target.a43 = 0
    target.a44 = 1

cdef Vector3 inv(Vector3* v):
    return {"x" : -v.x, "y" : -v.y, "z" : -v.z}

cdef void changeAxis(char track, char guide, Vector3* x, Vector3* y, Vector3* z):
    cdef char selector = track * 3 + guide
    if selector == 0: pass                                    # XX
    elif selector == 1: x[0], y[0], z[0] = z[0], x[0], y[0]   # XY
    elif selector == 2: x[0], y[0], z[0] = z[0], inv(y), x[0] # XZ

    elif selector == 3: x[0], y[0], z[0] = x[0], z[0], inv(y) # YX
    elif selector == 4: pass                                  # YY
    elif selector == 5: x[0], y[0], z[0] = y[0], z[0], x[0]   # YZ

    elif selector == 6: x[0], y[0], z[0] = x[0], y[0], z[0]   # ZX
    elif selector == 7: x[0], y[0], z[0] = inv(y), x[0], z[0] # ZY
    elif selector == 8: pass                                  # ZZ

    elif selector == 9: pass                                     # -XX
    elif selector == 10: x[0], y[0], z[0] = inv(z), x[0], inv(y) # -XY
    elif selector == 11: x[0], y[0], z[0] = inv(z), y[0], x[0]   # -XZ

    elif selector == 12: x[0], y[0], z[0] = x[0], inv(z), y[0]   # -YX
    elif selector == 13: pass                                    # -YY
    elif selector == 14: x[0], y[0], z[0] = inv(y), inv(z), x[0] # -YZ

    elif selector == 15: x[0], y[0], z[0] = x[0], inv(y), inv(z) # -ZX
    elif selector == 16: x[0], y[0], z[0] = y[0], x[0], inv(z)   # -ZY
    elif selector == 17: pass                                    # -ZZ


# List Operations
################################################

def eulersToDirections(EulerList rotations, str axis):
    cdef char _axis = axixNumbers[axis]
    cdef Vector3DList directions = Vector3DList(length = rotations.length)
    cdef long i
    for i in range(directions.length):
        eulerToDirection_LowLevel(directions.data + i, rotations.data + i, _axis)
    return directions

def directionsToMatrices(directions, guides, trackAxis = "Z", guideAxis = "X"):
    cdef:
        CDefaultList _directions = CDefaultList(Vector3DList, directions, (0, 0, 0))
        CDefaultList _guides = CDefaultList(Vector3DList, guides, (0, 0, 1))

        Matrix4x4List matrices = Matrix4x4List(length = CDefaultList.getMaxLength(_directions, _guides))
        char _trackAxis = axixNumbers[trackAxis]
        char _guideAxis = axixNumbers[guideAxis]
        long i

    for i in range(matrices.length):
        directionToMatrix_LowLevel(matrices.data + i,
                                   <Vector3*>_directions.get(i),
                                   <Vector3*>_guides.get(i),
                                   _trackAxis, _guideAxis)
    return matrices
