from ... math cimport (
    lengthVec3,
    normalizeLengthVec3_Inplace,
    transformVec3AsDirection
)

cpdef translateMatrixList(Matrix4x4List matrices, str type,
                          CDefaultList translations, DoubleList influences):
    if len(matrices) != len(influences):
        raise Exception("amoutn of matrices and influences has to be equal")

    cdef TranslationFunction translate = getTranslationFunction(type)
    cdef Vector3 *fullTranslation
    cdef Vector3 influencedTranslation
    cdef double influence
    cdef Py_ssize_t i

    for i in range(len(matrices)):
        influence = influences.data[i]
        fullTranslation = <Vector3*>translations.get(i)
        influencedTranslation.x = fullTranslation.x * influence
        influencedTranslation.y = fullTranslation.y * influence
        influencedTranslation.z = fullTranslation.z * influence
        translate(matrices.data + i, &influencedTranslation)

cdef TranslationFunction getTranslationFunction(str type) except *:
    if type == "GLOBAL_AXIS":
        return translate_GlobalAxis
    elif type == "LOCAL_AXIS":
        return translate_LocalAxis
    else:
        raise Exception("invalid type")

cdef void translate_GlobalAxis(Matrix4 *m, Vector3 *v):
    m.a14 += v.x
    m.a24 += v.y
    m.a34 += v.z

cdef void translate_LocalAxis(Matrix4 *m, Vector3 *offset):
    cdef Vector3 realOffset
    transformVec3AsDirection(&realOffset, offset, m)
    normalizeLengthVec3_Inplace(&realOffset, lengthVec3(offset))
    m.a14 += realOffset.x
    m.a24 += realOffset.y
    m.a34 += realOffset.z
