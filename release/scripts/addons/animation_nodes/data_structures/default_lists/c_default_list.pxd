from . default_list cimport DefaultList
from .. lists.clist cimport CList

cdef class CDefaultList(DefaultList):
    cdef:
        object dataType
        CList realList
        CList defaultElementList

        char* arrayStart
        void* default
        Py_ssize_t realListLength
        Py_ssize_t elementSize

    cdef void* get(self, Py_ssize_t index)
