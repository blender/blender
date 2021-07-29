cimport cython
from libc.string cimport memcpy

@cython.freelist(10)
cdef class CList:
    cdef void* getPointer(self):
        raise NotImplementedError()

    cdef int getElementSize(self):
        raise NotImplementedError()

    cdef Py_ssize_t getLength(self):
        raise NotImplementedError()

    cdef Py_ssize_t getCapacity(self):
        raise NotImplementedError()

    def repeated(self, *, Py_ssize_t length = -1, Py_ssize_t amount = -1):
        if length < 0 and amount < 0:
            raise ValueError("'length' or 'amount' has to be non-negative")
        elif length > 0 and amount > 0:
            raise ValueError("Can only evaluate one parameter of 'length' and 'amount'")

        cdef Py_ssize_t oldLength = self.getLength()

        if amount >= 0:
            length = oldLength * amount

        if oldLength == 0 and length > 0:
            raise ValueError("Cannot repeat a list with zero elements to something longer")

        if length <= oldLength:
            return self[:length]

        cdef:
            CList newList = type(self)(length)
            void *newData = newList.getPointer()
            int elementSize = self.getElementSize()
            Py_ssize_t done = 0
            Py_ssize_t todo = length

        # Copy First Segment
        memcpy(newData, self.getPointer(), oldLength * elementSize)
        done = oldLength
        todo -= oldLength

        # Replicate already copied segments
        #  -> double the amount of elements that are copied at each step
        while todo > 0:
            if done < todo:
                memcpy(<char*>newData + done * elementSize, newData, done * elementSize)
                todo -= done
                done *= 2
            else:
                memcpy(<char*>newData + done * elementSize, newData, todo * elementSize)
                todo = 0
                done = length

        return newList

    def reversed(self):
        cdef:
            Py_ssize_t length = self.getLength()
            void *oldData = self.getPointer()
            int elementSize = self.getElementSize()

            CList newList = type(self)(length)
            void *newData = newList.getPointer()
            Py_ssize_t i, offset

        offset = length - 1
        for i in range(length):
            memcpy(<char*>newData + i * elementSize,
                   <char*>oldData + (offset - i) * elementSize,
                   elementSize)
        return newList
