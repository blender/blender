from cpython cimport PyMem_Malloc
from libc.math cimport ceil, floor
from libc.string cimport memmove, memcpy

cpdef Py_ssize_t predictSliceLength(Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step):
    assert step != 0
    cdef Py_ssize_t diff = abs(start - stop)
    if start < stop and step > 0: return <Py_ssize_t>max(1, ceil(diff / <double>step))
    elif start > stop and step < 0: return <Py_ssize_t>max(1, -floor(diff / <double>step))
    else: return 0

cpdef makeStepPositive(Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step):
    cdef Py_ssize_t newStart, newEnd, newStep
    if step >= 0:
         newStart, newEnd, newStep = start, stop, step
    elif step < 0:
        newStep = -step

        mod1 = start % newStep
        mod2 = (stop + 1) % newStep
        diff = max(0, mod1 - mod2)

        newStart = stop + diff + 1
        newEnd = start + 1
        newStep = -step
    return newStart, newEnd, newStep

cdef removeValuesInSlice(char* arrayStart, Py_ssize_t arrayLength, Py_ssize_t elementSize,
                         Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step):
    cdef Py_ssize_t removeAmount, i

    if step < 0: start, stop, step = makeStepPositive(start, stop, step)
    removeAmount = predictSliceLength(start, stop, step)

    if step == 1:
        memmove(arrayStart + start * elementSize,
                arrayStart + stop * elementSize,
                arrayLength - stop * elementSize)
    elif step > 1:
        # Move values between the steps
        for i in range(removeAmount - 1):
            memmove(arrayStart + (start + i * (step - 1)) * elementSize,
                    arrayStart + (start + i * step + 1) * elementSize,
                    (step - 1) * elementSize)

        # Move values behind the last step
        memmove(arrayStart + (start + (step - 1) * (removeAmount - 1)) * elementSize,
                arrayStart + (start + (removeAmount - 1) * step + 1) * elementSize,
                arrayLength - (start + (removeAmount - 1) * step + 1) * elementSize)

    return removeAmount


cdef getValuesInSlice(void* source, Py_ssize_t elementAmount, int elementSize,
                      void** target, Py_ssize_t* targetLength,
                      sliceObject):
    cdef:
        Py_ssize_t start, stop, step
        Py_ssize_t realStart, realStop, realStep
        Py_ssize_t outLength
        Py_ssize_t newIndex, oldIndex
        char* _source = <char*>source
        char* result

    start, stop, step = sliceObject.indices(elementAmount)
    outLength = predictSliceLength(start, stop, step)
    result = <char*>PyMem_Malloc(outLength * elementSize)

    realStart = start * elementSize
    realStop = stop * elementSize
    realStep = step * elementSize

    newIndex = 0
    oldIndex = realStart

    if step == 1:
        memcpy(result, _source + realStart, outLength * elementSize)
    elif step > 1:
        while oldIndex < realStop:
            memcpy(result + newIndex, _source + oldIndex, elementSize)
            oldIndex += realStep
            newIndex += elementSize
    elif step < 0:
        while oldIndex > realStop:
            memcpy(result + newIndex, _source + oldIndex, elementSize)
            oldIndex += realStep
            newIndex += elementSize

    target[0] = result
    targetLength[0] = outLength
