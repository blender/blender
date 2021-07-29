import functools
from ... utils.limits cimport INT_MAX
from ... data_structures cimport CList, PolygonIndicesList
from ... sockets.info import getSocketClass, isCopyable, toBaseDataType, getCopyFunction

def fill(str dataType, myList, str direction, length, element, bint makeElementCopies = True):
    length = min(max(length, 0), INT_MAX)
    return getFillFunction(dataType, makeElementCopies)(myList, direction, length, element)

def getFillFunction(str dataType, bint makeElementCopies = True):
    socketClass = getSocketClass(dataType)
    defaultValue = socketClass.getDefaultValue()

    if isinstance(defaultValue, list):
        baseDataType = toBaseDataType(dataType)
        if isCopyable(baseDataType) and makeElementCopies:
            return functools.partial(fill_PythonList_Copy, getCopyFunction(baseDataType))
        else:
            return fill_PythonList_NoCopy
    elif isinstance(defaultValue, CList):
        return fill_CList
    elif isinstance(defaultValue, PolygonIndicesList):
        return fill_PolygonIndicesList
    else:
        raise NotImplementedError()

def fill_PythonList_Copy(copyFunction, list myList, str direction, int length, element):
    cdef int extendAmount = max(0, length - len(myList))
    cdef list extension = [copyFunction(element) for i in range(extendAmount)]
    return joinListAndExtension(myList, extension, direction)

def fill_PythonList_NoCopy(list myList, str direction, int length, element):
    cdef int extendAmount = max(0, length - len(myList))
    cdef list extension = [element] * extendAmount
    return joinListAndExtension(myList, extension, direction)

def fill_CList(CList myList, str direction, int length, element):
    cdef int extendAmount = max(0, length - len(myList))
    cdef CList extension = type(myList).fromValues([element]).repeated(length = extendAmount)
    return joinListAndExtension(myList, extension, direction)

def fill_PolygonIndicesList(PolygonIndicesList myList, str direction, int length, element):
    cdef int extendAmount = max(0, length - len(myList))
    cdef PolygonIndicesList extension = PolygonIndicesList.fromValues([element]).repeated(length = extendAmount)
    return joinListAndExtension(myList, extension, direction)

cdef joinListAndExtension(myList, extension, direction):
    if len(myList) == 0:
        return extension
    if len(extension) == 0:
        return myList

    if direction == "LEFT":
        return extension + myList
    elif direction == "RIGHT":
        return myList + extension
    else:
        raise ValueError("direction has to be in {'LEFT', 'RIGHT'}")
