from libc.string cimport memcpy
from ... sockets.info import getSocketClass
from ... data_structures cimport CList, BooleanList, PolygonIndicesList, LongList

def mask(dataType, myList, mask):
    function = getMaskFunction(dataType)
    return function(myList, mask)

def getMaskFunction(dataType):
    socketClass = getSocketClass(dataType)
    defaultValue = socketClass.getDefaultValue()

    if isinstance(defaultValue, list):
        return mask_PythonList
    elif isinstance(defaultValue, CList):
        return mask_CList
    elif isinstance(defaultValue, PolygonIndicesList):
        return mask_PolygonIndicesList
    else:
        raise NotImplementedError()

def mask_PythonList(list myList, BooleanList mask):
    if len(myList) != len(mask):
        raise ValueError("mask has a different length")

    cdef list newList = []
    cdef long i
    for i in range(len(myList)):
        if mask.data[i]:
            newList.append(myList[i])
    return newList

def mask_CList(CList myList, BooleanList mask):
    if len(myList) != len(mask):
        raise ValueError("mask has a different length")

    cdef:
        CList newList = type(myList)(length = mask.countTrue())

        int elementSize = myList.getElementSize()
        char* oldPointer = <char*>myList.getPointer()
        char* newPointer = <char*>newList.getPointer()
        char* maskPointer = mask.data
        long i, k

    k = 0
    for i in range(len(myList)):
        if maskPointer[i]:
            memcpy(newPointer + k * elementSize,
                   oldPointer + i * elementSize,
                   elementSize)
            k += 1

    return newList

def mask_PolygonIndicesList(PolygonIndicesList myList, BooleanList mask):
    if len(myList) != len(mask):
        raise ValueError("mask has a different length")

    cdef:
        LongList newIndices = LongList(length = mask.countTrue())
        long i, k
    k = 0
    for i in range(len(mask)):
        if mask.data[i]:
            newIndices.data[k] = i
            k += 1

    return myList.copyWithNewOrder(newIndices)
