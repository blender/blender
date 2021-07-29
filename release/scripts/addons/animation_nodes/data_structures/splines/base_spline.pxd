from ... math.vector cimport Vector3
from .. lists.base_lists cimport FloatList, Vector3DList

ctypedef void (*SplineEvaluationFunction)(Spline, float, Vector3*)

cdef class Spline:
    cdef:
        public bint cyclic
        readonly str type
        FloatList uniformParameters

    # Generic
    #############################################

    cpdef Spline copy(self)
    cpdef void markChanged(self)
    cpdef bint isEvaluable(self)
    cpdef transform(self, matrix)
    cpdef double getLength(self, int resolution = ?)
    cpdef double getPartialLength(self, float start, float end, int resolution = ?)

    cpdef project(self, point)
    cpdef projectExtended(self, point)
    cdef float project_LowLevel(self, Vector3* point)

    cpdef getTrimmedCopy(self, float start = ?, float end = ?)
    cdef Spline getTrimmedCopy_LowLevel(self, float start, float end)


    # Uniform Conversion
    #############################################

    cpdef toUniformParameter(self, float parameter)
    cdef float toUniformParameter_LowLevel(self, float parameter)
    cpdef ensureUniformConverter(self, long resolution)
    cdef updateUniformParameters(self, long totalResolution)
    cdef checkUniformConverter(self)


    # Get Multiple Samples
    #############################################

    cpdef getSamples(self, long amount, float start = ?, float end = ?)
    cpdef getTangentSamples(self, long amount, float start = ?, float end = ?)
    cpdef getUniformSamples(self, long amount, float start = ?, float end = ?)
    cpdef getUniformTangentSamples(self, long amount, float start = ?, float end = ?)

    cdef getSamples_LowLevel(self, long amount, float start, float end, Vector3* output)
    cdef getUniformSamples_LowLevel(self, long amount, float start, float end, Vector3* output)

    cdef sampleEvaluationFunction(self, SplineEvaluationFunction evaluate,
                                        long amount, float start, float end)

    cdef void sampleEvaluationFunction_LowLevel(self, SplineEvaluationFunction evaluate,
                                                long amount, float start, float end,
                                                Vector3* output)


    # Evaluate Single Parameter
    #############################################

    cpdef evaluate(self, float parameter)
    cpdef evaluateTangent(self, float parameter)
    cpdef evaluateUniform(self, float parameter)
    cpdef evaluateUniformTangent(self, float parameter)

    cdef evaluateEvaluationFunction(self, SplineEvaluationFunction evaluate, float parameter)

    cdef void evaluate_LowLevel(self, float parameter, Vector3* result)
    cdef void evaluateTangent_LowLevel(self, float parameter, Vector3* result)
    cdef void evaluateUniform_LowLevel(self, float parameter, Vector3* result)
    cdef void evaluateUniformTangent_LowLevel(self, float parameter, Vector3* result)
