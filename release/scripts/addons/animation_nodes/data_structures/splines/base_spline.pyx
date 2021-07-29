cimport cython
from ... utils.lists cimport findListSegment_LowLevel
from ... math.vector cimport distanceSquaredVec3
from ... math.conversion cimport toPyVector3, toVector3
from ... math.geometry cimport findNearestLineParameter
from ... math.list_operations cimport distanceSumOfVector3DList

cdef class Spline:

    # Generic
    #############################################

    cpdef Spline copy(self):
        raise NotImplementedError()

    cpdef transform(self, matrix):
        raise NotImplementedError()

    cpdef double getLength(self, int resolution = 0):
        return self.getPartialLength(0.0, 1.0, resolution)

    cpdef double getPartialLength(self, float start, float end, int resolution = 100):
        if not self.isEvaluable(): return 0.0
        start = min(max(start, 0), 1)
        end = min(max(end, 0), 1)
        return distanceSumOfVector3DList(self.getSamples(resolution, start, end))

    cpdef project(self, point):
        if not self.isEvaluable():
            raise Exception("spline is not evaluable")
        cdef Vector3 _point = toVector3(point)
        return self.project_LowLevel(&_point)

    cpdef projectExtended(self, point):
        if not self.isEvaluable():
            raise Exception("spline is not evaluable")

        cdef:
            Vector3 _point = toVector3(point)
            float parameter, smallestDistance, distance
            Vector3 nearestProjection, projection
            Vector3 nearestTangent, tangent
            Vector3 startPoint, startTangent
            Vector3 endPoint, endTangent

        parameter = self.project_LowLevel(&_point)
        self.evaluate_LowLevel(parameter, &nearestProjection)
        self.evaluateTangent_LowLevel(parameter, &nearestTangent)

        if not self.cyclic:
            smallestDistance = distanceSquaredVec3(&_point, &nearestProjection)

            self.evaluate_LowLevel(0, &startPoint)
            self.evaluate_LowLevel(1, &endPoint)
            self.evaluateTangent_LowLevel(0, &startTangent)
            self.evaluateTangent_LowLevel(1, &endTangent)

            parameter = findNearestLineParameter(&startPoint, &startTangent, &_point)
            if parameter < 0:
                projection.x = startPoint.x + parameter * startTangent.x
                projection.y = startPoint.y + parameter * startTangent.y
                projection.z = startPoint.z + parameter * startTangent.z
                distance = distanceSquaredVec3(&_point, &projection)
                if distance < smallestDistance:
                    smallestDistance = distance
                    nearestProjection = projection
                    nearestTangent = startTangent

            parameter = findNearestLineParameter(&endPoint, &endTangent, &_point)
            if parameter > 0:
                projection.x = endPoint.x + parameter * endTangent.x
                projection.y = endPoint.y + parameter * endTangent.y
                projection.z = endPoint.z + parameter * endTangent.z
                distance = distanceSquaredVec3(&_point, &projection)
                if distance < smallestDistance:
                    smallestDistance = distance
                    nearestProjection = projection
                    nearestTangent = endTangent

        return toPyVector3(&nearestProjection), toPyVector3(&nearestTangent)

    cdef float project_LowLevel(self, Vector3* point):
        raise NotImplementedError()

    cpdef bint isEvaluable(self):
        raise NotImplementedError()

    cpdef void markChanged(self):
        self.uniformParameters = None

    cpdef getTrimmedCopy(self, float start = 0.0, float end = 1.0):
        if not self.isEvaluable():
            raise Exception("spline is not evaluable")
        if start < 0 or end < 0 or start > 1 or end > 1:
            raise ValueError("start and end have to be between 0 and 1")
        cdef float _start, _end
        if start < end:
            _start, _end = start, end
        else:
            _start, _end = start, start
        cdef Spline trimmedSpline = self.getTrimmedCopy_LowLevel(_start, _end)
        return trimmedSpline

    cdef Spline getTrimmedCopy_LowLevel(self, float start, float end):
        raise NotImplementedError()


    # Uniform Conversion
    #############################################

    cdef checkUniformConverter(self):
        if self.uniformParameters is None:
            raise Exception("cannot evaluate uniform parameters, call spline.ensureUniformConverter() first")

    cpdef ensureUniformConverter(self, long resolution):
        cdef long pointAmount = len(self.points)
        cdef long totalResolution = pointAmount + max((pointAmount - 1), 0) * max(0, resolution)
        if self.uniformParameters is None:
            self.updateUniformParameters(totalResolution)
        elif self.uniformParameters.length < resolution:
            self.updateUniformParameters(totalResolution)

    cdef updateUniformParameters(self, long totalResolution):
        from . poly_spline import PolySpline
        if self.type == "POLY": polySpline = self
        else: polySpline = PolySpline(self.getSamples(totalResolution))
        self.uniformParameters = polySpline.getUniformParameters(totalResolution)

    cpdef toUniformParameter(self, float t):
        if self.uniformParameters is None:
            raise Exception("cannot evaluate uniform parameters, call spline.ensureUniformConverter() first")
        if t < 0 or t > 1:
            raise ValueError("parameter has to be between 0 and 1")
        return self.toUniformParameter_LowLevel(t)

    cdef float toUniformParameter_LowLevel(self, float t):
        cdef float factor
        cdef long indices[2]
        findListSegment_LowLevel(self.uniformParameters.length, False, t, indices, &factor)
        return self.uniformParameters.data[indices[0]] * (1 - factor) + \
               self.uniformParameters.data[indices[1]] * factor


    # Get Multiple Samples
    #############################################

    cpdef getSamples(self, long amount, float start = 0, float end = 1):
        return self.sampleEvaluationFunction(self.evaluate_LowLevel, amount, start, end)

    cpdef getTangentSamples(self, long amount, float start = 0, float end = 1):
        return self.sampleEvaluationFunction(self.evaluateTangent_LowLevel, amount, start, end)

    cpdef getUniformSamples(self, long amount, float start = 0, float end = 1):
        self.checkUniformConverter()
        return self.sampleEvaluationFunction(self.evaluateUniform_LowLevel, amount, start, end)

    cpdef getUniformTangentSamples(self, long amount, float start = 0, float end = 1):
        self.checkUniformConverter()
        return self.sampleEvaluationFunction(self.evaluateUniformTangent_LowLevel, amount, start, end)

    cdef sampleEvaluationFunction(self, SplineEvaluationFunction evaluate,
                                   long amount, float start, float end):
        if not self.isEvaluable():
            raise Exception("spline is not evaluable")
        if start < 0 or end < 0 or start > 1 or end > 1:
            raise ValueError("start and end have to be between 0 and 1")
        if amount < 0:
            raise ValueError("amount has to be greator or equal to 0")

        cdef Vector3DList samples = Vector3DList(length = amount)
        self.sampleEvaluationFunction_LowLevel(evaluate, amount, start, end, samples.data)
        return samples

    cdef getSamples_LowLevel(self, long amount, float start, float end, Vector3* output):
        self.sampleEvaluationFunction_LowLevel(self.evaluate_LowLevel, amount, start, end, output)

    cdef getUniformSamples_LowLevel(self, long amount, float start, float end, Vector3* output):
        self.sampleEvaluationFunction_LowLevel(self.evaluateUniform_LowLevel, amount, start, end, output)

    @cython.cdivision(True)
    cdef void sampleEvaluationFunction_LowLevel(self, SplineEvaluationFunction evaluate,
                                           long amount, float start, float end,
                                           Vector3* output):
        '''amount >= 0; 0 <= start, end <= 1'''
        if amount == 1: evaluate(self, (start + end) / 2, output)
        if amount <= 1: return

        cdef float step

        if self.cyclic and start == 0 and end == 1:
            step = (end - start) / amount
        else:
            step = (end - start) / (amount - 1)

        cdef long i
        cdef float t
        for i in range(amount):
            t = start + i * step
            # needed due to limited float accuracy
            if t > 1: t = 1
            if t < 0: t = 0
            evaluate(self, t, output + i)


   # Evaluate Single Parameter
   #############################################

    cpdef evaluate(self, float parameter):
        return self.evaluateEvaluationFunction(self.evaluate_LowLevel, parameter)

    cpdef evaluateTangent(self, float parameter):
        return self.evaluateEvaluationFunction(self.evaluateTangent_LowLevel, parameter)

    cpdef evaluateUniform(self, float parameter):
        self.checkUniformConverter()
        return self.evaluateEvaluationFunction(self.evaluateUniform_LowLevel, parameter)

    cpdef evaluateUniformTangent(self, float parameter):
        self.checkUniformConverter()
        return self.evaluateEvaluationFunction(self.evaluateUniformTangent_LowLevel, parameter)

    cdef evaluateEvaluationFunction(self, SplineEvaluationFunction evaluate, float parameter):
        if parameter < 0 or parameter > 1:
            raise ValueError("parameter has to be between 0 and 1")
        if not self.isEvaluable():
            raise Exception("spline is not evaluable")
        cdef Vector3 result
        evaluate(self, parameter, &result)
        return toPyVector3(&result)

    cdef void evaluate_LowLevel(self, float parameter, Vector3* result):
        raise NotImplementedError()

    cdef void evaluateTangent_LowLevel(self, float parameter, Vector3* result):
        raise NotImplementedError()

    cdef void evaluateUniform_LowLevel(self, float parameter, Vector3* result):
        self.evaluate_LowLevel(self.toUniformParameter_LowLevel(parameter), result)

    cdef void evaluateUniformTangent_LowLevel(self, float parameter, Vector3* result):
        self.evaluateTangent_LowLevel(self.toUniformParameter_LowLevel(parameter), result)

    def __repr__(self):
        return "<{} object at {}>".format(type(self).__name__, hex(id(self)))
