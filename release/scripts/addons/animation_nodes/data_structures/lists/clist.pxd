cdef class CList:
    cdef void* getPointer(self)
    cdef int getElementSize(self)
    cdef Py_ssize_t getLength(self)
    cdef Py_ssize_t getCapacity(self)
