cdef class DefaultList:
    cdef Py_ssize_t getRealLength(self):
        return 0

    @classmethod
    def getMaxLength(cls, *args):
        cdef:
            DefaultList defaultList
            Py_ssize_t maxLength = 0
            Py_ssize_t length

        for defaultList in args:
            length = defaultList.getRealLength()
            if length > maxLength:
                maxLength = length
        return maxLength
