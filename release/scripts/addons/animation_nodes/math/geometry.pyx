cimport cython
from . vector cimport (subVec3, scaleVec3_Inplace, lengthVec3, dotVec3,
                       distanceVec3, normalizeVec3_InPlace)

@cython.cdivision(True)
cdef float findNearestLineParameter(Vector3* lineStart, Vector3* lineDirection, Vector3* point):
    cdef float a = (lineDirection.x * (point.x - lineStart.x)
                  + lineDirection.y * (point.y - lineStart.y)
                  + lineDirection.z * (point.z - lineStart.z))
    cdef float b = dotVec3(lineDirection, lineDirection)
    if b == 0: return 0.0
    return a / b

cdef double distancePointToPlane(Vector3* planePoint, Vector3* planeNormal, Vector3* point):
    cdef Vector3 normPlaneNormal = planeNormal[0]
    normalizeVec3_InPlace(&normPlaneNormal)
    return abs(signedDistancePointToPlane_Normalized(planePoint, &normPlaneNormal, point))

cdef double signedDistancePointToPlane_Normalized(Vector3* planePoint, Vector3* normalizedPlaneNormal, Vector3* point):
    cdef Vector3 diff
    diff.x = point.x - planePoint.x
    diff.y = point.y - planePoint.y
    diff.z = point.z - planePoint.z
    return dotVec3(normalizedPlaneNormal, &diff)

@cython.cdivision(True)
cdef void projectPointOnPlane(Vector3* planePoint, Vector3* planeNormal, Vector3* point, Vector3* projection):
    cdef:
        Vector3 diff
        float sb, sn, sd

    subVec3(&diff, planePoint, point)
    sn = -dotVec3(planeNormal, &diff)
    sd = dotVec3(planeNormal, planeNormal)
    if sd == 0: sd = 1
    sb = sn / sd
    projection.x = point.x + sb * planeNormal.x
    projection.y = point.y + sb * planeNormal.y
    projection.z = point.z + sb * planeNormal.z
