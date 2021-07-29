from . constant_falloff import ConstantFalloff
from ... data_structures cimport (DoubleList, Falloff, BaseFalloff,
                                  CompoundFalloff, Interpolation)

def createIndexBasedFalloff(str extensionMode, DoubleList myList,
                double length, double offset, Interpolation interpolation):
    if len(myList) == 0:
        return ConstantFalloff(0)
    if len(myList) == 1 or length == 0:
        return ConstantFalloff(myList[0])

    if extensionMode == "LOOP":
        return Loop_InterpolateDoubleListFalloff(myList, length, offset, interpolation)
    elif extensionMode == "MIRROR":
        return Loop_InterpolateDoubleListFalloff(myList + myList.reversed(), length * 2, offset * 2, interpolation)
    elif extensionMode == "EXTEND":
        return Extend_InterpolateDoubleListFalloff(myList, length, offset, interpolation)
    else:
        raise Exception("invalid extension mode")

def createFalloffBasedFalloff(Falloff falloff, DoubleList myList, Interpolation interpolation):
    if len(myList) == 0:
        return ConstantFalloff(0)
    if len(myList) == 1:
        return ConstantFalloff(myList[0])

    return Falloff_InterpolateDoubleListFalloff(falloff, myList, interpolation)

cdef double evaluatePosition(double x, DoubleList myList, Interpolation interpolation):
    cdef long indexBefore = <int>x
    cdef float influence = interpolation.evaluate(x - <float>indexBefore)
    cdef long indexAfter
    if indexBefore < myList.length - 1:
        indexAfter = indexBefore + 1
    else:
        indexAfter = indexBefore
    return myList.data[indexBefore] * (1 - influence) + myList.data[indexAfter] * influence

cdef class BaseInterpolateDoubleListFalloff(BaseFalloff):
    cdef:
        DoubleList myList
        Interpolation interpolation
        double length, offset

    def __cinit__(self, DoubleList myList, double length, double offset, Interpolation interpolation):
        self.myList = myList
        self.length = length
        self.offset = offset
        self.interpolation = interpolation
        self.dataType = "All"
        self.clamped = False

cdef class Loop_InterpolateDoubleListFalloff(BaseInterpolateDoubleListFalloff):
    cdef double evaluate(self, void *object, long _index):
        cdef double index = (<double>_index + self.offset) % self.length
        cdef double x = index / (self.length - 1) * (self.myList.length - 1)
        return evaluatePosition(x, self.myList, self.interpolation)

cdef class Extend_InterpolateDoubleListFalloff(BaseInterpolateDoubleListFalloff):
    cdef double evaluate(self, void *object, long _index):
        cdef double index = <double>_index + self.offset
        index = min(max(index, 0), self.length - 1)
        cdef double x = index / (self.length - 1) * (self.myList.length - 1)
        return evaluatePosition(x, self.myList, self.interpolation)


cdef class Falloff_InterpolateDoubleListFalloff(CompoundFalloff):
    cdef:
        Falloff falloff
        DoubleList myList
        Interpolation interpolation

    def __cinit__(self, Falloff falloff, DoubleList myList, Interpolation interpolation):
        self.falloff = falloff
        self.myList = myList
        self.interpolation = interpolation
        self.clamped = False

    cdef list getDependencies(self):
        return [self.falloff]

    cdef list getClampingRequirements(self):
        return [True]

    cdef double evaluate(self, double *dependencyResults):
        cdef double x = dependencyResults[0] * (self.myList.length - 1)
        return evaluatePosition(x, self.myList, self.interpolation)
