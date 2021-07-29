import functools
from itertools import islice, cycle
from ... data_structures cimport CList, PolygonIndicesList
from ... sockets.info import getSocketClass, isCopyable, toBaseDataType, getCopyFunction

def repeat(str dataType, myList, length):
    function = getRepeatFunction(dataType)
    return function(myList, length)

def getRepeatFunction(str dataType, bint makeElementCopies = True):
    socketClass = getSocketClass(dataType)
    defaultValue = socketClass.getDefaultValue()

    if isinstance(defaultValue, list):
        baseDataType = toBaseDataType(dataType)
        if isCopyable(baseDataType) and makeElementCopies:
            return functools.partial(repeat_PythonList_Copy, getCopyFunction(baseDataType))
        else:
            return repeat_PythonList_NoCopy
    elif isinstance(defaultValue, CList):
        return repeat_CList
    elif isinstance(defaultValue, PolygonIndicesList):
        return repeat_PolygonIndicesList
    else:
        raise NotImplementedError()

def repeat_PythonList_NoCopy(list myList, length):
    return list(islice(cycle(myList), length))

def repeat_PythonList_Copy(copyFunction, list myList, length):
    indexGenerator = islice(cycle(range(len(myList))), length)
    return [copyFunction(myList[i]) for i in indexGenerator]

def repeat_CList(CList myList, length):
    return myList.repeated(length = length)

def repeat_PolygonIndicesList(PolygonIndicesList myList, length):
    return myList.repeated(length = length)
