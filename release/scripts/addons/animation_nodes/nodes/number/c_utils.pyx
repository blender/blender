from ... data_structures cimport (
    DoubleList,
    LongList,
    Interpolation
)

from ... utils.limits cimport INT_MAX
from ... utils.clamp cimport clamp, clampLong
from ... algorithms.random cimport uniformRandomNumber

def clamp_DoubleList(DoubleList values, double minValue, double maxValue):
    cdef Py_ssize_t i
    for i in range(len(values)):
        if values.data[i] < minValue:
            values.data[i] = minValue
        elif values.data[i] > maxValue:
            values.data[i] = maxValue

def range_LongList_StartStep(amount, start, step):
    cdef long long _amount = clampLong(amount)
    cdef long long _start = clampLong(start)
    cdef long long _step = clampLong(step)

    cdef LongList newList = LongList(length = max(_amount, 0))
    cdef Py_ssize_t i
    for i in range(len(newList)):
        newList.data[i] = _start + i * _step
    return newList

def range_DoubleList_StartStep(amount, double start, double step):
    cdef DoubleList newList
    cdef Py_ssize_t i
    if step == 0:
        newList = DoubleList.fromValues([start]) * max(amount, 0)
    else:
        newList = DoubleList(length = max(amount, 0))
        for i in range(len(newList)):
            newList.data[i] = start + i * step
    return newList

def range_DoubleList_StartStop(amount, double start, double stop):
    if amount == 1:
        return DoubleList.fromValues([start])
    else:
        return range_DoubleList_StartStep(amount, start, (stop - start) / (amount - 1))

def random_DoubleList(seed, amount, double minValue, double maxValue):
    cdef DoubleList newList = DoubleList(length = max(0, amount))
    cdef int _seed = (seed * 234235) % INT_MAX
    cdef Py_ssize_t i

    for i in range(len(newList)):
        newList.data[i] = uniformRandomNumber(_seed + i, minValue, maxValue)
    return newList

def mapRange_DoubleList(DoubleList values, bint clamped,
                        double inMin, double inMax,
                        double outMin, double outMax):
    if inMin == inMax:
        return DoubleList.fromValues([0]) * len(values)

    cdef:
        DoubleList newValues = DoubleList(length = len(values))
        double factor = (outMax - outMin) / (inMax - inMin)
        double x
        long i

    for i in range(len(newValues)):
        x = values.data[i]
        if clamped: x = clamp(x, inMin, inMax)
        newValues.data[i] = outMin + (x - inMin) * factor

    return newValues

def mapRange_DoubleList_Interpolated(DoubleList values, Interpolation interpolation,
                                     double inMin, double inMax,
                                     double outMin, double outMax):
     if inMin == inMax:
         return DoubleList.fromValues([0]) * values.length

     cdef:
         DoubleList newValues = DoubleList(length = len(values))
         double factor1 = 1 / (inMax - inMin)
         double factor2 = outMax - outMin
         double x
         long i

     for i in range(len(newValues)):
         x = clamp(values.data[i], inMin, inMax)
         newValues.data[i] = outMin + interpolation.evaluate((x - inMin) * factor1) * factor2

     return newValues
