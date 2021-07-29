cpdef Py_ssize_t predictSliceLength(Py_ssize_t start, Py_ssize_t end, Py_ssize_t step)
cpdef makeStepPositive(Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step)

cdef removeValuesInSlice(char* arrayStart, Py_ssize_t arrayLength, Py_ssize_t elementSize,
                         Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step)

cdef getValuesInSlice(void* source, Py_ssize_t elementAmount, int elementSize,
                      void** target, Py_ssize_t* targetLength,
                      sliceObject)
