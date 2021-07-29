def calcMin(bytes):
    return - 2 ** (bytes * 8 - 1)

def calcMax(bytes):
    return 2 ** (bytes * 8 - 1) - 1

cdef int INT_MIN = calcMin(sizeof(int))
cdef int INT_MAX = calcMax(sizeof(int))

cdef long LONG_MIN = calcMin(sizeof(long))
cdef long LONG_MAX = calcMax(sizeof(long))
