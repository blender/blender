from ... data_structures cimport (Vector3DList, EulerList, Matrix4x4List,
                                  CDefaultList, FloatList, DoubleList, Falloff,
                                  FalloffEvaluator)

from ... math cimport (Vector3, Euler3, Matrix4, toMatrix4,
                       multMatrix4, toPyMatrix4,
                       setTranslationRotationScaleMatrix,
                       setRotationXMatrix, setRotationYMatrix, setRotationZMatrix,
                       setRotationMatrix, setTranslationMatrix, setIdentityMatrix,
                       transposeMatrix_Inplace)
from ... math import matrix4x4ListToEulerList

from libc.math cimport sqrt


# Compose/Create Matrix
########################################

def composeMatrices(translations, rotations, scales):
    cdef:
        CDefaultList _translations = CDefaultList(Vector3DList, translations, (0, 0, 0))
        CDefaultList _rotations = CDefaultList(EulerList, rotations, (0, 0, 0))
        CDefaultList _scales = CDefaultList(Vector3DList, scales, (1, 1, 1))

        long length = CDefaultList.getMaxLength(_translations, _rotations, _scales)
        Matrix4x4List matrices = Matrix4x4List(length = length)
        long i

    for i in range(matrices.length):
        setTranslationRotationScaleMatrix(matrices.data + i,
            <Vector3*>_translations.get(i),
            <Euler3*>_rotations.get(i),
            <Vector3*>_scales.get(i))

    return matrices

def createAxisRotations(DoubleList angles, str axis):
    cdef Matrix4x4List matrices = Matrix4x4List(length = len(angles))
    cdef int i
    if axis == "X":
        for i in range(len(matrices)):
            setRotationXMatrix(matrices.data + i, angles.data[i])
    elif axis == "Y":
        for i in range(len(matrices)):
            setRotationYMatrix(matrices.data + i, angles.data[i])
    elif axis == "Z":
        for i in range(len(matrices)):
            setRotationZMatrix(matrices.data + i, angles.data[i])
    return matrices

def createRotationsFromEulers(EulerList rotations):
    cdef Matrix4x4List matrices = Matrix4x4List(length = len(rotations))
    cdef int i
    for i in range(len(matrices)):
        setRotationMatrix(matrices.data + i, rotations.data + i)
    return matrices

def createTranslationMatrices(Vector3DList vectors):
    cdef Matrix4x4List matrices = Matrix4x4List(length = vectors.length)
    cdef Py_ssize_t i
    for i in range(vectors.length):
        setTranslationMatrix(matrices.data + i, vectors.data + i)
    return matrices


# Extract Matrix Information
################################################

def extractMatrixTranslations(Matrix4x4List matrices):
    cdef Vector3DList translations = Vector3DList(length = len(matrices))
    cdef Matrix4 *_matrices = matrices.data
    cdef Vector3 *_translations = translations.data
    cdef Py_ssize_t i

    for i in range(len(translations)):
        _translations[i].x = _matrices[i].a14
        _translations[i].y = _matrices[i].a24
        _translations[i].z = _matrices[i].a34

    return translations

def extractMatrixRotations(Matrix4x4List matrices):
    return matrix4x4ListToEulerList(matrices)

def extractMatrixScales(Matrix4x4List matrices):
    cdef Vector3DList scales = Vector3DList(length = len(matrices))
    cdef Py_ssize_t i

    for i in range(len(scales)):
        scaleFromMatrix(scales.data + i, matrices.data + i)

    return scales

cdef void scaleFromMatrix(Vector3 *scale, Matrix4 *matrix):
    scale.x = sqrt(matrix.a11 * matrix.a11 + matrix.a21 * matrix.a21 + matrix.a31 * matrix.a31)
    scale.y = sqrt(matrix.a12 * matrix.a12 + matrix.a22 * matrix.a22 + matrix.a32 * matrix.a32)
    scale.z = sqrt(matrix.a13 * matrix.a13 + matrix.a23 * matrix.a23 + matrix.a33 * matrix.a33)


# Replicate Matrix
###############################################

def replicateMatrixAtMatrices(matrix, Matrix4x4List transformations):
    cdef Matrix4 _matrix = toMatrix4(matrix)
    cdef Matrix4x4List result = Matrix4x4List(length = len(transformations))
    cdef Py_ssize_t i
    for i in range(len(result)):
        multMatrix4(result.data + i, transformations.data + i, &_matrix)
    return result

def replicateMatrixAtVectors(matrix, Vector3DList translations):
    cdef Matrix4 _matrix = toMatrix4(matrix)
    cdef Matrix4x4List result = Matrix4x4List(length = len(translations))
    cdef Py_ssize_t i
    for i in range(len(result)):
        result.data[i] = _matrix
        result.data[i].a14 += translations.data[i].x
        result.data[i].a24 += translations.data[i].y
        result.data[i].a34 += translations.data[i].z
    return result

def replicateMatricesAtMatrices(Matrix4x4List matrices, Matrix4x4List transformations):
    cdef Matrix4x4List result = Matrix4x4List(length = len(matrices) * len(transformations))
    cdef Py_ssize_t i, j
    for i in range(len(transformations)):
        for j in range(matrices.length):
            multMatrix4(result.data + i * matrices.length + j,
                        transformations.data + i,
                        matrices.data + j)
    return result

def replicateMatricesAtVectors(Matrix4x4List matrices, Vector3DList translations):
    cdef Matrix4x4List result = Matrix4x4List(length = len(matrices) * len(translations))
    cdef Py_ssize_t i, j, index
    for i in range(len(translations)):
        for j in range(matrices.length):
            index = i * matrices.length + j
            result.data[index] = matrices.data[j]
            result.data[index].a14 += translations.data[i].x
            result.data[index].a24 += translations.data[i].y
            result.data[index].a34 += translations.data[i].z
    return result


# Multiply Matrices
##########################################

def vectorizedMatrixMultiplication(matricesA, matricesB):
    cdef bint isListA = isinstance(matricesA, Matrix4x4List)
    cdef bint isListB = isinstance(matricesB, Matrix4x4List)
    if isListA and isListB:
        return multiplyMatrixLists(matricesA, matricesB)
    elif isListA:
        return multiplyMatrixWithList(matricesA, matricesB, "RIGHT")
    elif isListB:
        return multiplyMatrixWithList(matricesB, matricesA, "LEFT")
    else:
        return matricesA * matricesB

def multiplyMatrixWithList(Matrix4x4List matrices, _transformation, str type):
    cdef Matrix4 transformation = toMatrix4(_transformation)
    cdef Matrix4x4List outMatrices = Matrix4x4List(length = len(matrices))
    cdef Py_ssize_t i
    if type == "LEFT":
        for i in range(len(outMatrices)):
            multMatrix4(outMatrices.data + i, &transformation, matrices.data + i)
    elif type == "RIGHT":
        for i in range(len(outMatrices)):
            multMatrix4(outMatrices.data + i, matrices.data + i, &transformation)
    else:
        raise Exception("type has to be 'LEFT' or 'RIGHT'")
    return outMatrices

def multiplyMatrixLists(Matrix4x4List listA, Matrix4x4List listB):
    assert listA.length == listB.length

    cdef Matrix4x4List outMatrices = Matrix4x4List(length = len(listA))
    cdef Py_ssize_t i
    for i in range(len(listA)):
        multMatrix4(outMatrices.data + i, listA.data + i, listB.data + i)
    return outMatrices


# Various
###########################################

def evaluateFalloffForMatrixList(Falloff falloff, Matrix4x4List matrices):
    cdef Py_ssize_t i
    cdef FalloffEvaluator evaluator
    cdef DoubleList influences = DoubleList(length = len(matrices))

    try: evaluator = falloff.getEvaluator("Transformation Matrix")
    except: return None

    for i in range(len(influences)):
        influences.data[i] = evaluator.evaluate(matrices.data + i, i)
    return influences

def reduceMatrixList(Matrix4x4List matrices, bint reversed):
    cdef:
        Py_ssize_t i
        Matrix4 tmp, target
        Py_ssize_t amount = len(matrices)

    if amount == 0:
        setIdentityMatrix(&target)
    elif amount == 1:
        target = matrices.data[0]
    else:
        if reversed:
            tmp = matrices.data[amount - 1]
            for i in range(amount - 2, -1, -1):
                multMatrix4(&target, &tmp, matrices.data + i)
                tmp = target
        else:
            tmp = matrices.data[0]
            for i in range(1, amount):
                multMatrix4(&target, &tmp, matrices.data + i)
                tmp = target

    return toPyMatrix4(&target)
