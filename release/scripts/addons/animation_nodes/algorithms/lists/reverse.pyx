from ... sockets.info import getSocketClass
from ... data_structures cimport CList, PolygonIndicesList

def reverse(str dataType, myList):
    return getReverseFunction(dataType)(myList)

def getReverseFunction(str dataType):
    socketClass = getSocketClass(dataType)
    defaultValue = socketClass.getDefaultValue()

    if isinstance(defaultValue, list):
        return reverse_PythonList
    elif isinstance(defaultValue, (CList, PolygonIndicesList)):
        return reverse_ReversableList
    else:
        raise NotImplementedError()

def reverse_PythonList(list myList):
    return list(reversed(myList))

def reverse_ReversableList(myList):
    return myList.reversed()
