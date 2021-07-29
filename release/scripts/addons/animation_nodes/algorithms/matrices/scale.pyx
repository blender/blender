from ... math cimport Matrix4, Vector3

cpdef scaleMatrixList(Matrix4x4List matrices, str type,
                      CDefaultList scales, DoubleList influences):
    if len(matrices) != len(influences):
        raise Exception("amount of matrices and influences has to be equal")

    cdef ScaleFunction scale = getScaleFunction(type)
    cdef Vector3 *fullScale
    cdef Vector3 influencedScale
    cdef double influence
    cdef Py_ssize_t i

    for i in range(len(matrices)):
        influence = influences.data[i]
        fullScale = <Vector3*>scales.get(i)
        influencedScale.x = fullScale.x * influence + (1 - influence)
        influencedScale.y = fullScale.y * influence + (1 - influence)
        influencedScale.z = fullScale.z * influence + (1 - influence)
        scale(matrices.data + i, &influencedScale)

cdef ScaleFunction getScaleFunction(str type) except *:
    if type == "LOCAL_AXIS":
        return scale_LocalAxis
    elif type == "GLOBAL_AXIS":
        return scale_GlobalAxis
    elif type == "TRANSLATION_ONLY":
        return scale_TranslationOnly
    elif type == "INCLUDE_TRANSLATION":
        return scale_IncludeTranslation
    else:
        raise Exception("invalid type")

cdef void scale_LocalAxis(Matrix4 *m, Vector3 *v):
    m.a11 *= v.x
    m.a21 *= v.x
    m.a31 *= v.x

    m.a12 *= v.y
    m.a22 *= v.y
    m.a32 *= v.y

    m.a13 *= v.z
    m.a23 *= v.z
    m.a33 *= v.z

cdef void scale_GlobalAxis(Matrix4 *m, Vector3 *v):
    m.a11 *= v.x
    m.a12 *= v.x
    m.a13 *= v.x

    m.a21 *= v.y
    m.a22 *= v.y
    m.a23 *= v.y

    m.a31 *= v.z
    m.a32 *= v.z
    m.a33 *= v.z

cdef void scale_TranslationOnly(Matrix4 *m, Vector3 *v):
    m.a14 *= v.x
    m.a24 *= v.y
    m.a34 *= v.z

cdef void scale_IncludeTranslation(Matrix4 *m, Vector3 *v):
    scale_GlobalAxis(m, v)
    scale_TranslationOnly(m, v)
