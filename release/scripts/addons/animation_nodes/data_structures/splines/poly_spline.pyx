cimport cython
from libc.math cimport floor
from libc.string cimport memcpy
from ... utils.lists cimport findListSegment_LowLevel
from ... math cimport (Vector3, mixVec3, distanceVec3, subVec3, lengthVec3,
                       distanceSquaredVec3, findNearestLineParameter,
                       distanceSumOfVector3DList)

from mathutils import Vector

cdef class PolySpline(Spline):

    def __cinit__(self, Vector3DList points = None, FloatList radii = None, bint cyclic = False):
        if points is None:
            points = Vector3DList()
        if radii is None:
            radii = FloatList.fromValues([0.1]) * len(points)
        if points.length != radii.length:
            raise Exception("Point and radius amount has to be equal")
        self.cyclic = cyclic
        self.type = "POLY"
        self.points = points
        self.radii = radii
        self.markChanged()

    def appendPoint(self, point, float radius = 0):
        self.points.append(point)
        self.radii.append(radius)
        self.markChanged()

    cpdef PolySpline copy(self):
        return PolySpline(self.points.copy(), self.radii.copy(), self.cyclic)

    cpdef transform(self, matrix):
        self.points.transform(matrix)
        self.markChanged()

    cpdef double getLength(self, int resolution = 0):
        cdef double length = distanceSumOfVector3DList(self.points)
        if self.cyclic and self.points.length >= 2:
            length += distanceVec3(self.points.data + 0,
                                   self.points.data + self.points.length - 1)
        return length

    @cython.cdivision(True)
    cdef float project_LowLevel(self, Vector3* point):
        cdef:
            float closestParameter = 0
            float smallestDistance = 1e9
            float lineParameter, lineDistance
            int segmentAmount = self.getSegmentAmount()
            int i, pointAmount = self.points.length
            Vector3 lineDirection, projectionOnLine
            Vector3* _points = self.points.data
            int endIndex

        for i in range(segmentAmount):
            endIndex = (i + 1) % pointAmount

            # find closest t value on current segment
            subVec3(&lineDirection, _points + endIndex, _points + i)
            lineParameter = findNearestLineParameter(_points + i, &lineDirection, point)
            lineParameter = min(max(lineParameter, 0.0), 1.0)

            # calculate closest point on the current segment
            mixVec3(&projectionOnLine, _points + i, _points + endIndex, lineParameter)

            # check if it is closer than all previously calculated points
            lineDistance = distanceSquaredVec3(point, &projectionOnLine)
            if lineDistance < smallestDistance:
                smallestDistance = lineDistance
                closestParameter = (lineParameter + i) / <float>segmentAmount

        return closestParameter

    cdef inline int getSegmentAmount(self):
        return self.points.length - 1 + self.cyclic

    cdef PolySpline getTrimmedCopy_LowLevel(self, float start, float end):
        cdef:
            long startIndices[2]
            long endIndices[2]
            float startT, endT

        findListSegment_LowLevel(self.points.length, self.cyclic, start, startIndices, &startT)
        findListSegment_LowLevel(self.points.length, self.cyclic, end, endIndices, &endT)

        cdef long newPointAmount
        if endIndices[1] > 0:
            newPointAmount = endIndices[1] - startIndices[0] + 1
        elif endIndices[1] == 0: # <- cyclic extension required
            newPointAmount = self.points.length - startIndices[0] + 1

        cdef:
            Vector3DList newPoints = Vector3DList(length = newPointAmount)
            Vector3 *_newPoints = newPoints.data
            Vector3 *_oldPoints = self.points.data
            FloatList newRadii = FloatList(length = newPointAmount)
            float *_newRadii = newRadii.data
            float *_oldRadii = self.radii.data

        mixVec3(_newPoints, _oldPoints + startIndices[0], _oldPoints + startIndices[1], startT)
        _newRadii[0] = _oldRadii[startIndices[0]] * (1 - startT) + _oldRadii[startIndices[1]] * startT

        mixVec3(_newPoints + newPointAmount - 1, _oldPoints + endIndices[0], _oldPoints + endIndices[1], endT)
        _newRadii[newPointAmount - 1] = _oldRadii[endIndices[0]] * (1 - endT) + _oldRadii[endIndices[1]] * endT

        memcpy(_newPoints + 1, _oldPoints + startIndices[1], sizeof(Vector3) * (newPointAmount - 2))
        memcpy(_newRadii + 1, _oldRadii + startIndices[1], sizeof(float) * (newPointAmount - 2))
        return PolySpline(newPoints, newRadii)

    cpdef bint isEvaluable(self):
        return self.points.length >= 2

    cdef void evaluate_LowLevel(self, float parameter, Vector3* result):
        cdef:
            Vector3* _points = self.points.data
            long indices[2]
            float t
        findListSegment_LowLevel(self.points.length, self.cyclic, parameter, indices, &t)
        mixVec3(result, _points + indices[0], _points + indices[1], t)

    cdef void evaluateTangent_LowLevel(self, float parameter, Vector3* result):
        cdef:
            Vector3* _points = self.points.data
            long indices[2]
            float t # not really needed here
        findListSegment_LowLevel(self.points.length, self.cyclic, parameter, indices, &t)
        subVec3(result, _points + indices[1], _points + indices[0])

    @cython.cdivision(True)
    cpdef FloatList getUniformParameters(self, long amount):
        cdef:
            long i
            FloatList parameters = FloatList(length = max(0, amount))
            Vector3* _points = self.points.data
            long pointAmount = self.points.length

        if amount <= 1 or pointAmount <= 1:
            parameters.fill(0)
            return parameters

        cdef FloatList distances = FloatList(length = pointAmount - 1 + int(self.cyclic))
        for i in range(pointAmount - 1):
            distances.data[i] = distanceVec3(_points + i, _points + i + 1)
        if self.cyclic:
            distances.data[pointAmount - 1] = distanceVec3(_points + pointAmount - 1, _points)

        cdef float totalLength = distances.getSumOfElements()
        if totalLength < 0.001: # <- necessary to remove the risk of running
            parameters.fill(0)  #    into endless loops or division by 0
            return parameters

        cdef:
            # Safe Division: amount > 1
            float stepSize = totalLength / (amount - 1)
            float factor = 1 / <float>distances.length
            float missingDistance = stepSize
            float residualDistance
            long currentIndex = 1

        for i in range(distances.length):
            residualDistance = distances.data[i]
            while residualDistance > missingDistance and currentIndex < amount:
                residualDistance -= missingDistance
                # Safe Division: distances.data[i] > 0
                parameters.data[currentIndex] = (i + 1 - residualDistance / distances.data[i]) * factor
                missingDistance = stepSize
                currentIndex += 1
            missingDistance -= residualDistance

        parameters.data[0] = 0
        # It can happen that more than one element is 1 due to float inaccuracy
        for i in range(currentIndex, amount):
            parameters.data[i] = 1
        parameters.data[amount - 1]
        return parameters
