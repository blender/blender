import random
cimport cython
from libc.math cimport log, pow
from libc.string cimport memcpy
from . shuffle import shuffle_CList
from ... utils.limits cimport INT_MAX
from ... sockets.info import getSocketClass
from .. random cimport randomNumber_Positive
from ... data_structures cimport CList, PolygonIndicesList, IntegerList, LongList

def sample(str dataType, myList, amount, seed):
    return getSampleFunction(dataType)(myList, amount, seed)

def getSampleFunction(str dataType):
    socketClass = getSocketClass(dataType)
    defaultValue = socketClass.getDefaultValue()

    if isinstance(defaultValue, list):
        return sample_PythonList
    elif isinstance(defaultValue, CList):
        return sample_CList
    elif isinstance(defaultValue, PolygonIndicesList):
        return sample_PolygonIndicesList
    else:
        raise NotImplementedError()

def sample_PythonList(list myList, amount, seed):
    if amount < 0 or amount > len(myList):
        raise ValueError("amount has to be in [0, len(list)-1]")

    cdef IntegerList indices = getUniqueIndices(len(myList), amount, seed)
    cdef list outList = [None] * amount

    cdef int i
    for i in range(amount):
        outList[i] = myList[indices.data[i]]
    return outList

def sample_CList(CList sourceList, amount, seed):
    if amount < 0 or amount > len(sourceList):
        raise ValueError("amount has to be in [0, len(list)-1]")

    cdef:
        int _amount = amount
        int _seed = (seed * 3566341) % INT_MAX

        char* _sourceList = <char*>sourceList.getPointer()
        int elementSize = sourceList.getElementSize()

        CList newList = type(sourceList)(length = _amount)
        char* _newList = <char*>newList.getPointer()

    cdef IntegerList selectedIndices = getUniqueIndices(len(sourceList), _amount, _seed)

    cdef int i
    for i in range(_amount):
        memcpy(_newList + i * elementSize,
               _sourceList + selectedIndices.data[i] * elementSize,
               elementSize)

    return newList

def sample_PolygonIndicesList(PolygonIndicesList sourceList, amount, seed):
    indices = getUniqueIndices(len(sourceList), amount, seed)
    return sourceList.copyWithNewOrder(LongList.fromValues(indices))



# Algorithms to get unique indices
########################################################

cdef getUniqueIndices(int listLength, int amount, int seed, bint shuffled = True):
    if amount < 0 or amount > listLength:
        raise ValueError("amount has to be >= 0 and < listLength")
    elif amount == 0:
        return IntegerList()

    cdef IntegerList indices = IntegerList(length = amount)

    if 2 * (log(amount) / log(2) - 2) < log(listLength) / log(2):
        selectUniqueIndices_Naive(listLength, amount, seed, indices.data)
    else:
        selectUniqueIndices_ReservoirSampling(listLength, amount, seed, indices.data)

    if shuffled:
        shuffle_CList(indices, seed * 432 + amount * 6345 + listLength * 5243)

    return indices

cdef void selectUniqueIndices_Naive(int listLength, int amount, int seed, int* indicesOut):
    '''
    O(amount^2)
    Randomly selects indices and reselects if it has been taken already.
    Efficient when the amount is very low compared to the list length.
    '''
    cdef:
        int index
        int i, k, j
        double _indexFactor = <double>listLength
        bint indexTaken

    k = 0
    for i in range(amount):
        while True:
            k += 1
            indexTaken = False
            index = <int>(randomNumber_Positive(seed + k * 242243 + i * 341345) * _indexFactor)

            for j in range(i):
                if index == indicesOut[j]:
                    indexTaken = True
                    break

            if not indexTaken:
                indicesOut[i] = index
                break

@cython.cdivision(True)
cdef void selectUniqueIndices_ReservoirSampling(int listLength, int amount, int seed, int* indicesOut):
    '''
    O(listLength)
    https://en.wikipedia.org/wiki/Reservoir_sampling
    '''
    cdef:
        int i = 0
        int k = 0
        double elementsToPick = amount
        double elementsLeft = listLength
        double propability

    while elementsToPick > 0:
        propability = elementsToPick / elementsLeft
        if randomNumber_Positive(seed + i * 6745629) < propability:
            indicesOut[k] = i
            elementsToPick -= 1
            k += 1

        i += 1
        elementsLeft -= 1
