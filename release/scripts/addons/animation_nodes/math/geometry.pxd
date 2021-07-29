from . vector cimport Vector3

cdef float findNearestLineParameter(Vector3* lineStart, Vector3* lineDirection, Vector3* point)
cdef double signedDistancePointToPlane_Normalized(Vector3* planePoint, Vector3* normalizedPlaneNormal, Vector3* point)
cdef double distancePointToPlane(Vector3* planePoint, Vector3* planeNormal, Vector3* point)
