from libc.string cimport memcpy
from ... utils.limits cimport INT_MAX
from .. random cimport uniformRandomInteger
from cpython.mem cimport PyMem_Malloc, PyMem_Free
from ... data_structures cimport CList, PolygonIndicesList, LongList
random = __import__("random") # to avoid cython name clashes

from ... sockets.info import getSocketClass

def shuffle(str dataType, myList, seed):
    return getShuffleFunction(dataType)(myList, seed)

def getShuffleFunction(str dataType):
    socketClass = getSocketClass(dataType)
    defaultValue = socketClass.getDefaultValue()
    if isinstance(defaultValue, list):
        return shuffle_PythonList
    elif isinstance(defaultValue, CList):
        return shuffle_CList
    elif isinstance(defaultValue, PolygonIndicesList):
        return shuffle_PolygonIndicesList
    else:
        raise NotImplementedError()

def shuffle_PythonList(list myList, seed):
    random.seed(seed)
    random.shuffle(myList)
    return myList

def shuffle_CList(CList myList, seed):
    cdef:
        int _seed = (seed * 345722341) % INT_MAX
        char* data = <char*>myList.getPointer()
        Py_ssize_t length = myList.getLength()
        int elementSize = myList.getElementSize()
        void* tmp = PyMem_Malloc(elementSize)
        int i, offset

    if tmp == NULL:
        raise MemoryError()

    for i in range(length - 1, -1, -1):
        offset = uniformRandomInteger(_seed + i, 0, i) * elementSize

        memcpy(tmp, data + offset, elementSize)
        memcpy(data + offset, data + i * elementSize, elementSize)
        memcpy(data + i * elementSize, tmp, elementSize)

    PyMem_Free(tmp)
    return myList

def shuffle_PolygonIndicesList(PolygonIndicesList myList, seed):
    cdef:
        LongList newOrder = LongList(length = len(myList))
        Py_ssize_t i

    for i in range(len(myList)):
        newOrder.data[i] = i

    shuffle_CList(newOrder, seed)

    return myList.copyWithNewOrder(newOrder, checkIndices = False)
