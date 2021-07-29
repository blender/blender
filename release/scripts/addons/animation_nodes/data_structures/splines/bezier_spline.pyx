cimport cython
from libc.string cimport memcpy
from numpy.polynomial import Polynomial
from ... utils.lists cimport findListSegment_LowLevel
from ... math cimport (subVec3, normalizeVec3_InPlace, lengthVec3, toPyVector3)

from mathutils import Vector

# Great free online book about bezier curves:
# http://pomax.github.io/bezierinfo/

cdef class BezierSpline(Spline):

    def __cinit__(self, Vector3DList points = None,
                        Vector3DList leftHandles = None,
                        Vector3DList rightHandles = None,
                        FloatList radii = None,
                        bint cyclic = False):
        if points is None: points = Vector3DList()
        if leftHandles is None: leftHandles = points.copy()
        if rightHandles is None: rightHandles = points.copy()
        if radii is None: radii = FloatList.fromValues([0.1]) * len(points)

        if not (points.length == leftHandles.length == points.length == radii.length):
            raise ValueError("list lengths have to be equal")

        self.points = points
        self.leftHandles = leftHandles
        self.rightHandles = rightHandles
        self.radii = radii
        self.cyclic = cyclic
        self.type = "BEZIER"
        self.markChanged()

    def appendPoint(self, point, leftHandle, rightHandle, double radius = 0):
        self.points.append(point)
        self.leftHandles.append(leftHandle)
        self.rightHandles.append(rightHandle)
        self.radii.append(radius)
        self.markChanged()

    cpdef BezierSpline copy(self):
        return BezierSpline(self.points.copy(),
                            self.leftHandles.copy(),
                            self.rightHandles.copy(),
                            self.radii.copy(),
                            self.cyclic)

    cpdef transform(self, matrix):
        self.points.transform(matrix)
        self.leftHandles.transform(matrix)
        self.rightHandles.transform(matrix)
        self.markChanged()

    cdef float project_LowLevel(self, Vector3* _point):
        # TODO: Speedup using cython
        # slowest part here is the root finding using numpy
        # maybe implement another numerical method to find the best parameter
        # http://jazzros.blogspot.be/2011/03/projecting-point-on-bezier-curve.html
        cdef:
            int segmentAmount = self.getSegmentAmount()
            int i, leftIndex, rightIndex
            set possibleParameters = set()
            list coeffs = [0] * 6

        point = toPyVector3(_point)

        for i in range(segmentAmount):
            leftIndex = i
            rightIndex = (i + 1) % (segmentAmount + 1)

            p0 = self.points[leftIndex] - point
            p1 = self.rightHandles[leftIndex] - point
            p2 = self.leftHandles[rightIndex] - point
            p3 = self.points[rightIndex] - point

            a = p3 - 3 * p2 + 3 * p1 - p0
            b = 3 * p2 - 6 * p1 + 3 * p0
            c = 3 * (p1 - p0)

            coeffs[0] = c.dot(p0)
            coeffs[1] = c.dot(c) + b.dot(p0) * 2.0
            coeffs[2] = b.dot(c) * 3.0 + a.dot(p0) * 3.0
            coeffs[3] = a.dot(c) * 4.0 + b.dot(b) * 2.0
            coeffs[4] = a.dot(b) * 5.0
            coeffs[5] = a.dot(a) * 3.0

            poly = Polynomial(coeffs, [0.0, 1.0], [0.0, 1.0])
            roots = poly.roots()
            realRoots = [float(min(max(root.real, 0), 1)) for root in roots]
            segmentParameters = [(i + t) / segmentAmount for t in realRoots]
            possibleParameters.update(segmentParameters)

        sampledData = [(p, (point - self.evaluate(p)).length_squared) for p in possibleParameters]
        if len(sampledData) > 0:
            return min(sampledData, key = lambda item: item[1])[0]
        return 0

    cdef inline int getSegmentAmount(self):
        return self.points.length - 1 + self.cyclic

    cpdef bint isEvaluable(self):
        return self.points.length >= 2

    cdef void evaluate_LowLevel(self, float parameter, Vector3* result):
        cdef:
            float t1
            Vector3* w[4]
        self.getSegmentData(parameter, &t1, w)
        cdef:
            float t2 = t1 * t1
            float t3 = t2 * t1
            float mt1 = 1 - t1
            float mt2 = mt1 * mt1
            float mt3 = mt2 * mt1
            float coeff1 = 3 * mt2 * t1
            float coeff2 = 3 * mt1 * t2
        result.x = w[0].x*mt3 + w[1].x*coeff1 + w[2].x*coeff2 + w[3].x*t3
        result.y = w[0].y*mt3 + w[1].y*coeff1 + w[2].y*coeff2 + w[3].y*t3
        result.z = w[0].z*mt3 + w[1].z*coeff1 + w[2].z*coeff2 + w[3].z*t3

    cdef void evaluateTangent_LowLevel(self, float parameter, Vector3* result):
        cdef:
            float t1
            Vector3* w[4]
        self.getSegmentData(parameter, &t1, w)
        cdef:
            float t2 = t1 * t1
            float coeff0 = -3 +  6 * t1 - 3 * t2
            float coeff1 =  3 - 12 * t1 + 9 * t2
            float coeff2 =       6 * t1 - 9 * t2
            float coeff3 =                3 * t2
        result.x = w[0].x*coeff0 + w[1].x*coeff1 + w[2].x*coeff2 + w[3].x*coeff3
        result.y = w[0].y*coeff0 + w[1].y*coeff1 + w[2].y*coeff2 + w[3].y*coeff3
        result.z = w[0].z*coeff0 + w[1].z*coeff1 + w[2].z*coeff2 + w[3].z*coeff3

    cdef void getSegmentData(self, float parameter, float* t, Vector3** w):
        cdef long indices[2]
        findListSegment_LowLevel(self.points.length, self.cyclic, parameter, indices, t)
        w[0] = (self.points.data) + indices[0]
        w[1] = (self.rightHandles.data) + indices[0]
        w[2] = (self.leftHandles.data) + indices[1]
        w[3] = (self.points.data) + indices[1]

    cpdef calculateSmoothHandles(self, float strength = 1/3):
        cdef:
            Vector3* _points = self.points.data
            Vector3* _leftHandles = self.leftHandles.data
            Vector3* _rightHandles = self.rightHandles.data
            long indexLeft, i, indexRight
            long pointAmount = self.points.length

        if pointAmount < 2: return

        for i in range(1, pointAmount - 1):
            calculateSmoothControlPoints(
                _points + i, _points + i - 1, _points + i + 1, strength,
                _leftHandles + i, _rightHandles + i)

        # End points need extra consideration
        cdef long lastIndex = pointAmount - 1
        if self.cyclic:
            # Start Point
            calculateSmoothControlPoints(
                _points, _points + lastIndex, _points + 1, strength,
                _leftHandles, _rightHandles)
            # End Point
            calculateSmoothControlPoints(
                _points + lastIndex, _points + lastIndex - 1, _points, strength,
                _leftHandles + lastIndex, _rightHandles + lastIndex)
        else:
            # Start Point
            _leftHandles[0] = _points[0]
            _rightHandles[0] = _points[0]
            # End Point
            _leftHandles[lastIndex] = _points[lastIndex]
            _rightHandles[lastIndex] = _points[lastIndex]

        self.markChanged()

    cpdef BezierSpline getTrimmedCopy_LowLevel(self, float start, float end):
        cdef:
            long startIndices[2]
            long endIndices[2]
            float startT, endT

        findListSegment_LowLevel(self.points.length, self.cyclic, start, startIndices, &startT)
        findListSegment_LowLevel(self.points.length, self.cyclic, end, endIndices, &endT)

        cdef long newPointAmount
        if endIndices[1] == 0: # <- cyclic extension required
            newPointAmount = self.points.length - startIndices[0] + 1
        else:
            newPointAmount = endIndices[1] - startIndices[0] + 1

        cdef:
            Vector3DList newPoints = Vector3DList(length = newPointAmount)
            Vector3DList newLeftHandles = Vector3DList(length = newPointAmount)
            Vector3DList newRightHandles = Vector3DList(length = newPointAmount)
            FloatList newRadii = FloatList(length = newPointAmount)
            Vector3* _newPoints = newPoints.data
            Vector3* _newLeftHandles = newLeftHandles.data
            Vector3* _newRightHandles = newRightHandles.data
            float *_newRadii = newRadii.data
            Vector3* _oldPoints = self.points.data
            Vector3* _oldLeftHandles = self.leftHandles.data
            Vector3* _oldRightHandles = self.rightHandles.data
            float *_oldRadii = self.radii.data
            Vector3 tmp[4]

        if startIndices[0] == endIndices[0]: # <- result will contain only one segment
            if endT < 0.0001: # <- both parameters are (nearly) zero, avoid division by zero
                _newPoints[0] = _oldPoints[startIndices[0]]
                _newPoints[1] = _oldPoints[startIndices[0]]
                _newLeftHandles[0] = _oldPoints[startIndices[0]]
                _newLeftHandles[1] = _oldPoints[startIndices[0]]
                _newRightHandles[0] = _oldPoints[startIndices[0]]
                _newRightHandles[1] = _oldPoints[startIndices[0]]
            else: # <- trim segment from both ends
                calcRightTrimmedSegment(endT,
                                   _oldPoints + startIndices[0], _oldRightHandles + startIndices[0],
                                   _oldLeftHandles + startIndices[1], _oldPoints + startIndices[1],
                                   tmp + 0, tmp + 1, tmp + 2, tmp + 3)
                calcLeftTrimmedSegment(startT / endT,
                                   tmp + 0, tmp + 1, tmp + 2, tmp + 3,
                                   _newPoints + 0, _newRightHandles + 0,
                                   _newLeftHandles + 1, _newPoints + 1)
                _newLeftHandles[0] = _newPoints[0]
                _newRightHandles[1] = _newPoints[1]
        else: # <- resulting spline will contain multiple segments
            # Copy segments which stay the same
            memcpy(_newPoints + 1,
                   _oldPoints + startIndices[1],
                   sizeof(Vector3) * (newPointAmount - 2))
            memcpy(_newLeftHandles + 1,
                   _oldLeftHandles + startIndices[1],
                   sizeof(Vector3) * (newPointAmount - 2))
            memcpy(_newRightHandles + 1,
                   _oldRightHandles + startIndices[1],
                   sizeof(Vector3) * (newPointAmount - 2))
            memcpy(_newRadii + 1,
                   _oldRadii + startIndices[1],
                   sizeof(float) * (newPointAmount - 2))

            # Trim first segment
            calcLeftTrimmedSegment(startT,
                _oldPoints + startIndices[0], _oldRightHandles + startIndices[0],
                _oldLeftHandles + startIndices[1], _oldPoints + startIndices[1],
                _newPoints, _newRightHandles,
                _newLeftHandles + 1, _newPoints + 1)
            _newLeftHandles[0] = _newPoints[0]

            # Trim last segment
            calcRightTrimmedSegment(endT,
                _oldPoints + endIndices[0], _oldRightHandles + endIndices[0],
                _oldLeftHandles + endIndices[1], _oldPoints + endIndices[1],
                _newPoints + newPointAmount - 2, _newRightHandles + newPointAmount - 2,
                _newLeftHandles + newPointAmount - 1, _newPoints + newPointAmount - 1)
            _newRightHandles[newPointAmount - 1] = _newPoints[newPointAmount - 1]

        # calculate radius of first and last point
        _newRadii[0] = _oldRadii[startIndices[0]] * (1 - startT) + _oldRadii[startIndices[1]] * startT
        _newRadii[newPointAmount - 1] = _oldRadii[endIndices[0]] * (1 - endT) + _oldRadii[endIndices[1]] * endT

        return BezierSpline(newPoints, newLeftHandles, newRightHandles, newRadii)

@cython.cdivision(True)
cdef calculateSmoothControlPoints(
                Vector3* point, Vector3* left, Vector3* right, float strength,
                Vector3* leftHandle, Vector3* rightHandle):   # <- output
    # http://stackoverflow.com/questions/13037606/how-does-inkscape-calculate-the-coordinates-for-control-points-for-smooth-edges/13425159#13425159
    cdef:
        Vector3 vecLeft, vecRight
        float lenLeft, lenRight, factor
        Vector3 direction, directionLeft, directionRight

    subVec3(&vecLeft, left, point)
    subVec3(&vecRight, right, point)
    lenLeft = lengthVec3(&vecLeft)
    lenRight = lengthVec3(&vecRight)

    if lenLeft > 0 and lenRight > 0:
        factor = lenLeft / lenRight
        direction.x = factor * vecRight.x - vecLeft.x
        direction.y = factor * vecRight.y - vecLeft.y
        direction.z = factor * vecRight.z - vecLeft.z
        normalizeVec3_InPlace(&direction)

        factor = lenLeft * strength
        leftHandle.x = point.x - direction.x * factor
        leftHandle.y = point.y - direction.y * factor
        leftHandle.z = point.z - direction.z * factor

        factor = lenRight * strength
        rightHandle.x = point.x + direction.x * factor
        rightHandle.y = point.y + direction.y * factor
        rightHandle.z = point.z + direction.z * factor
    else:
        leftHandle[0] = point[0]
        rightHandle[0] = point[0]

cdef calcLeftTrimmedSegment(float t,
                    Vector3* P1, Vector3* P2, Vector3* P3, Vector3* P4,
                    Vector3* outP1, Vector3* outP2, Vector3* outP3, Vector3* outP4):
    calcRightTrimmedSegment(1 - t, P4, P3, P2, P1, outP4, outP3, outP2, outP1)

cdef calcRightTrimmedSegment(float t,
                    Vector3* P1, Vector3* P2, Vector3* P3, Vector3* P4,
                    Vector3* outP1, Vector3* outP2, Vector3* outP3, Vector3* outP4):
    '''
    t: how much of the curve will stay (0-1)
    P1: left point
    P2: right handle of left point
    P3: left handle of right point
    P4: right point
    outPX: new location of that point
    '''
    cdef float t2 = t * t
    cdef float t3 = t2 * t

    outP1[0] = P1[0]

    outP2.x = t * P2.x - (t-1) * P1.x
    outP2.y = t * P2.y - (t-1) * P1.y
    outP2.z = t * P2.z - (t-1) * P1.z

    outP3.x = t2 * P3.x - 2 * t * (t-1) * P2.x + (t-1) ** 2 * P1.x
    outP3.y = t2 * P3.y - 2 * t * (t-1) * P2.y + (t-1) ** 2 * P1.y
    outP3.z = t2 * P3.z - 2 * t * (t-1) * P2.z + (t-1) ** 2 * P1.z

    outP4.x = t3 * P4.x - 3 * t2 * (t-1) * P3.x + 3 * t * (t-1) ** 2 * P2.x - (t-1) ** 3 * P1.x
    outP4.y = t3 * P4.y - 3 * t2 * (t-1) * P3.y + 3 * t * (t-1) ** 2 * P2.y - (t-1) ** 3 * P1.y
    outP4.z = t3 * P4.z - 3 * t2 * (t-1) * P3.z + 3 * t * (t-1) ** 2 * P2.z - (t-1) ** 3 * P1.z
