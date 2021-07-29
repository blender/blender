from cpython.ref cimport PyObject
from cpython.mem cimport PyMem_Malloc, PyMem_Free

from . falloff_base cimport Falloff
from . falloff_base cimport BaseFalloff, CompoundFalloff
from ... math cimport Matrix4, Vector3, toVector3, toMatrix4

ctypedef double (*BaseEvaluatorWithConversion)(BaseFalloff, void*, long index)
ctypedef double (*EvaluatorFunction)(void* settings, void* value, long index)


# Interface for other files
#########################################################

cdef class FalloffEvaluator:
    @staticmethod
    def create(Falloff falloff, str sourceType, bint clamped, bint onlyC):
        cdef FalloffEvaluator evaluator = createFalloffEvaluator(falloff, sourceType, clamped)
        if evaluator is None:
            raise Exception("Cannot create FalloffEvaluator for this source type")
        if not onlyC and evaluator.pyEvaluator is None:
            raise Exception("Cannot evaluate this source type from Python. "
                            "Consider to set the 'onlyC' parameter to True.")
        return evaluator

    cdef double evaluate(self, void* value, long index):
        raise NotImplementedError()

    def __call__(self, object value, long index):
        return self.pyEvaluator(self, value, index)

cdef createFalloffEvaluator(Falloff falloff, str sourceType, bint clamped = False):
    cdef:
        EvaluatorFunctionEvaluator evaluator
        EvaluatorFunction function
        void* settings

    assert falloff is not None

    createEvaluatorFunction(falloff, sourceType, clamped, &function, &settings)
    if function != NULL:
        evaluator = EvaluatorFunctionEvaluator()
        evaluator.function = function
        evaluator.settings = settings
        evaluator.pyEvaluator = getPyEvaluator(sourceType)
        return evaluator
    else:
        return None


# Create Falloff Evaluators
#########################################################

cdef createEvaluatorFunction(Falloff falloff, str sourceType, bint clamped, EvaluatorFunction* outFunction, void** outSettings):
    cdef:
        EvaluatorFunction function = NULL
        void* settings = NULL

    if isinstance(falloff, BaseFalloff):
        dataType = (<BaseFalloff>falloff).dataType
        if dataType == "All" or sourceType == dataType:
            createBaseEvaluator_NoConversion(falloff, &function, &settings)
        else:
            createBaseEvaluator_Conversion(falloff, sourceType, &function, &settings)
    elif isinstance(falloff, CompoundFalloff):
        dependencyAmount = len((<CompoundFalloff>falloff).getDependencies())
        if dependencyAmount == 1:
            createCompoundEvaluator_One(falloff, sourceType, &function, &settings)
        else:
            createCompoundEvaluator_Generic(falloff, sourceType, &function, &settings)

    if not falloff.clamped and clamped and function != NULL and settings != NULL:
        createClampingEvaluator(function, settings, outFunction, outSettings)
    else:
        outFunction[0] = function
        outSettings[0] = settings


cdef createBaseEvaluator_NoConversion(BaseFalloff falloff, EvaluatorFunction* outFunction, void** outSettings):
    cdef BaseSettings_NoConversion* settings
    settings = <BaseSettings_NoConversion*>PyMem_Malloc(sizeof(BaseSettings_NoConversion))
    settings.falloff = <PyObject*>falloff
    outFunction[0] = evaluateBaseFalloff_NoConversion
    outSettings[0] = settings

cdef createBaseEvaluator_Conversion(BaseFalloff falloff, str sourceType, EvaluatorFunction* outFunction, void** outSettings):
    cdef BaseEvaluatorWithConversion evaluator
    evaluator = getEvaluatorWithConversion(sourceType, falloff.dataType)
    if evaluator == NULL: return

    settings = <BaseSettings_Conversion*>PyMem_Malloc(sizeof(BaseSettings_Conversion))
    settings.falloff = <PyObject*>falloff
    settings.evaluator = evaluator
    outFunction[0] = evaluateBaseFalloff_Conversion
    outSettings[0] = settings

cdef createCompoundEvaluator_Generic(CompoundFalloff falloff, str sourceType, EvaluatorFunction* outFunction, void** outSettings):
    cdef:
        CompoundSettings_Generic* settings = <CompoundSettings_Generic*>PyMem_Malloc(sizeof(CompoundSettings_Generic))
        list dependencies = falloff.getDependencies()
        list clampingRequirements = falloff.getClampingRequirements()
        int amount = len(dependencies)
        int i
        bint isValid = True

    settings.falloff = <PyObject*>falloff
    settings.dependencyAmount = amount
    settings.dependencyResults = <double*>PyMem_Malloc(sizeof(double) * amount)
    settings.dependencySettings = <void**>PyMem_Malloc(sizeof(char*) * amount)
    settings.dependencyFunctions = <EvaluatorFunction*>PyMem_Malloc(sizeof(EvaluatorFunction) * amount)

    for i in range(amount):
        createEvaluatorFunction(dependencies[i], sourceType, clampingRequirements[i],
            settings.dependencyFunctions + i, settings.dependencySettings + i)
        if settings.dependencyFunctions[i] == NULL:
            isValid = False
            break

    if isValid:
        outFunction[0] = evaluateCompoundFalloff_Generic
        outSettings[0] = settings
    else:
        freeCompoundSettings_Generic(settings)
        outFunction[0] = outSettings[0] = NULL

cdef createCompoundEvaluator_One(CompoundFalloff falloff, str sourceType, EvaluatorFunction* outFunction, void** outSettings):
    cdef CompoundSettings_One* settings = <CompoundSettings_One*>PyMem_Malloc(sizeof(CompoundSettings_One))
    settings.falloff = <PyObject*>falloff
    createEvaluatorFunction(falloff.getDependencies()[0], sourceType, falloff.getClampingRequirements()[0],
        &settings.dependencyFunction, &settings.dependencySettings)

    if settings.dependencyFunction == NULL:
        PyMem_Free(settings)
        outFunction[0] = outSettings[0] = NULL
    else:
        outFunction[0] = evaluateCompoundFalloff_One
        outSettings[0] = settings

cdef createClampingEvaluator(EvaluatorFunction realFunction, void* realSettings, EvaluatorFunction* outFunction, void** outSettings):
    cdef ClampingSettings* settings = <ClampingSettings*>PyMem_Malloc(sizeof(ClampingSettings))
    settings.realFunction = realFunction
    settings.realSettings = realSettings
    outFunction[0] = evaluateClamping
    outSettings[0] = settings


# Free Falloff Evaluators
#########################################################

cdef freeEvaluatorFunction(EvaluatorFunction function, void* settings):
    if function == evaluateBaseFalloff_NoConversion:
        PyMem_Free(settings)
    elif function == evaluateBaseFalloff_Conversion:
        PyMem_Free(settings)
    elif function == evaluateCompoundFalloff_Generic:
        freeCompoundSettings_Generic(<CompoundSettings_Generic*>settings)
    elif function == evaluateCompoundFalloff_One:
        PyMem_Free(settings)
    elif function == evaluateClamping:
        freeClampingSettings(<ClampingSettings*>settings)

cdef freeCompoundSettings_Generic(CompoundSettings_Generic* settings):
    cdef int i
    for i in range(settings.dependencyAmount):
        freeEvaluatorFunction(settings.dependencyFunctions[i], settings.dependencySettings[i])

    PyMem_Free(settings.dependencyResults)
    PyMem_Free(settings.dependencyFunctions)
    PyMem_Free(settings.dependencySettings)
    PyMem_Free(settings)

cdef freeClampingSettings(ClampingSettings* settings):
    freeEvaluatorFunction(settings.realFunction, settings.realSettings)
    PyMem_Free(settings)


# Evaluator Settings
#########################################################

cdef struct BaseSettings_NoConversion:
    PyObject* falloff

cdef struct BaseSettings_Conversion:
    PyObject* falloff
    BaseEvaluatorWithConversion evaluator

cdef struct CompoundSettings_Generic:
    PyObject* falloff
    EvaluatorFunction* dependencyFunctions
    void** dependencySettings
    double* dependencyResults
    int dependencyAmount

cdef struct CompoundSettings_One:
    PyObject* falloff
    EvaluatorFunction dependencyFunction
    void* dependencySettings

cdef struct ClampingSettings:
    EvaluatorFunction realFunction
    void* realSettings


# Execute Falloff Evaluators
#########################################################

cdef double evaluateBaseFalloff_NoConversion(void* settings, void* value, long index):
    cdef BaseSettings_NoConversion* _settings = <BaseSettings_NoConversion*>settings
    return (<BaseFalloff>_settings.falloff).evaluate(value, index)

cdef double evaluateBaseFalloff_Conversion(void* settings, void* value, long index):
    cdef BaseSettings_Conversion* _settings = <BaseSettings_Conversion*>settings
    return _settings.evaluator(<BaseFalloff>_settings.falloff, value, index)

cdef double evaluateCompoundFalloff_Generic(void* settings, void* value, long index):
    cdef CompoundSettings_Generic* _settings = <CompoundSettings_Generic*>settings
    cdef int i
    for i in range(_settings.dependencyAmount):
        _settings.dependencyResults[i] = _settings.dependencyFunctions[i](_settings.dependencySettings[i], value, index)
    return (<CompoundFalloff>_settings.falloff).evaluate(_settings.dependencyResults)

cdef double evaluateCompoundFalloff_One(void* settings, void* value, long index):
    cdef CompoundSettings_One* _settings = <CompoundSettings_One*>settings
    cdef double dependencyResult = _settings.dependencyFunction(_settings.dependencySettings, value, index)
    return (<CompoundFalloff>_settings.falloff).evaluate(&dependencyResult)

cdef double evaluateClamping(void* settings, void* value, long index):
    cdef ClampingSettings* _settings = <ClampingSettings*>settings
    cdef double result = _settings.realFunction(_settings.realSettings, value, index)
    if result > 1: return 1
    if result < 0: return 0
    return result


# Evaluator Function Wrapper
#########################################################

cdef class EvaluatorFunctionEvaluator(FalloffEvaluator):
    cdef void* settings
    cdef EvaluatorFunction function

    def __dealloc__(self):
        freeEvaluatorFunction(self.function, self.settings)

    cdef double evaluate(self, void* value, long index):
        return self.function(self.settings, value, index)


# Value Conversion
###########################################################

cdef BaseEvaluatorWithConversion getEvaluatorWithConversion(str sourceType, str targetType):
    if sourceType == "Transformation Matrix" and targetType == "Location":
        return convert_TransformationMatrix_Location
    return NULL

cdef double convert_TransformationMatrix_Location(BaseFalloff falloff, void* value, long index):
    cdef Matrix4* matrix = <Matrix4*>value
    cdef Vector3 vector
    vector.x = matrix.a14
    vector.y = matrix.a24
    vector.z = matrix.a34
    return falloff.evaluate(&vector, index)


# Evaluate falloff with Python objects
###########################################################

cdef getPyEvaluator(str sourceType):
    if sourceType == "": return evaluate_None
    if sourceType == "Location": return evaluate_PyVector
    if sourceType == "Transformation Matrix": return evaluate_PyMatrix
    return None

def evaluate_None(FalloffEvaluator evaluator, object noObject, long index):
    return evaluator.evaluate(NULL, index)

def evaluate_PyVector(FalloffEvaluator evaluator, object pyVector, long index):
    cdef Vector3 vector = toVector3(pyVector)
    return evaluator.evaluate(&vector, index)

def evaluate_PyMatrix(FalloffEvaluator evaluator, object pyMatrix, long index):
    cdef Matrix4 matrix = toMatrix4(pyMatrix)
    return evaluator.evaluate(&matrix, index)
