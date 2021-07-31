ctypedef fused list_or_tuple:
    list
    tuple

ctypedef fused NumericList:
    FloatList
    DoubleList
    CharList
    UCharList
    LongList
    ULongList
    IntegerList
    UIntegerList
    ShortList
    UShortList
    LongLongList
    ULongLongList


from . clist cimport CList

cdef class FloatList(CList):
    cdef:
        float* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, float* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, float value)

    cdef FloatList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, float value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, FloatList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, float* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, float* target)
    cdef toPyObject(self, float* value)
    cdef equals_SameType(self, FloatList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from . clist cimport CList

cdef class DoubleList(CList):
    cdef:
        double* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, double* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, double value)

    cdef DoubleList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, double value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, DoubleList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, double* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, double* target)
    cdef toPyObject(self, double* value)
    cdef equals_SameType(self, DoubleList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from . clist cimport CList

cdef class CharList(CList):
    cdef:
        char* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, char* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, char value)

    cdef CharList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, char value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, CharList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, char* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, char* target)
    cdef toPyObject(self, char* value)
    cdef equals_SameType(self, CharList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from . clist cimport CList

cdef class UCharList(CList):
    cdef:
        unsigned char* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, unsigned char* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, unsigned char value)

    cdef UCharList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, unsigned char value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, UCharList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, unsigned char* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, unsigned char* target)
    cdef toPyObject(self, unsigned char* value)
    cdef equals_SameType(self, UCharList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from . clist cimport CList

cdef class LongList(CList):
    cdef:
        long* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, long* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, long value)

    cdef LongList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, long value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, LongList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, long* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, long* target)
    cdef toPyObject(self, long* value)
    cdef equals_SameType(self, LongList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from . clist cimport CList

cdef class ULongList(CList):
    cdef:
        unsigned long* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, unsigned long* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, unsigned long value)

    cdef ULongList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, unsigned long value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, ULongList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, unsigned long* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, unsigned long* target)
    cdef toPyObject(self, unsigned long* value)
    cdef equals_SameType(self, ULongList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from . clist cimport CList

cdef class IntegerList(CList):
    cdef:
        int* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, int* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, int value)

    cdef IntegerList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, int value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, IntegerList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, int* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, int* target)
    cdef toPyObject(self, int* value)
    cdef equals_SameType(self, IntegerList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from . clist cimport CList

cdef class UIntegerList(CList):
    cdef:
        unsigned int* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, unsigned int* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, unsigned int value)

    cdef UIntegerList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, unsigned int value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, UIntegerList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, unsigned int* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, unsigned int* target)
    cdef toPyObject(self, unsigned int* value)
    cdef equals_SameType(self, UIntegerList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from . clist cimport CList

cdef class ShortList(CList):
    cdef:
        short* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, short* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, short value)

    cdef ShortList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, short value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, ShortList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, short* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, short* target)
    cdef toPyObject(self, short* value)
    cdef equals_SameType(self, ShortList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from . clist cimport CList

cdef class UShortList(CList):
    cdef:
        unsigned short* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, unsigned short* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, unsigned short value)

    cdef UShortList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, unsigned short value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, UShortList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, unsigned short* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, unsigned short* target)
    cdef toPyObject(self, unsigned short* value)
    cdef equals_SameType(self, UShortList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from . clist cimport CList

cdef class LongLongList(CList):
    cdef:
        long long* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, long long* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, long long value)

    cdef LongLongList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, long long value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, LongLongList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, long long* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, long long* target)
    cdef toPyObject(self, long long* value)
    cdef equals_SameType(self, LongLongList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from . clist cimport CList

cdef class ULongLongList(CList):
    cdef:
        unsigned long long* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, unsigned long long* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, unsigned long long value)

    cdef ULongLongList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, unsigned long long value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, ULongLongList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, unsigned long long* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, unsigned long long* target)
    cdef toPyObject(self, unsigned long long* value)
    cdef equals_SameType(self, ULongLongList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from . clist cimport CList

cdef class BooleanList(CList):
    cdef:
        char* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, char* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, char value)

    cdef BooleanList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, char value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, BooleanList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, char* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, char* target)
    cdef toPyObject(self, char* value)
    cdef equals_SameType(self, BooleanList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from ... math.vector cimport Vector3

from ... math.conversion cimport setVector3, toPyVector3

from . clist cimport CList

cdef class Vector3DList(CList):
    cdef:
        Vector3* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, Vector3* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, Vector3 value)

    cdef Vector3DList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, Vector3 value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, Vector3DList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, Vector3* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, Vector3* target)
    cdef toPyObject(self, Vector3* value)
    cdef equals_SameType(self, Vector3DList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from ... math.euler cimport Euler3

from ... math.conversion cimport setEuler3, toPyEuler3

from . clist cimport CList

cdef class EulerList(CList):
    cdef:
        Euler3* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, Euler3* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, Euler3 value)

    cdef EulerList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, Euler3 value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, EulerList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, Euler3* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, Euler3* target)
    cdef toPyObject(self, Euler3* value)
    cdef equals_SameType(self, EulerList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from ... math.matrix cimport Matrix4, transposeMatrix_Inplace, multMatrix4

from ... math.conversion cimport setMatrix4, toPyMatrix4

from . clist cimport CList

cdef class Matrix4x4List(CList):
    cdef:
        Matrix4* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, Matrix4* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, Matrix4 value)

    cdef Matrix4x4List getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, Matrix4 value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, Matrix4x4List other, Py_ssize_t index = ?)
    cdef overwriteArray(self, Matrix4* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, Matrix4* target)
    cdef toPyObject(self, Matrix4* value)
    cdef equals_SameType(self, Matrix4x4List other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


from ... math.quaternion cimport Quaternion

from ... math.conversion cimport setQuaternion, toPyQuaternion

from . clist cimport CList

cdef class QuaternionList(CList):
    cdef:
        Quaternion* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, Quaternion* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, Quaternion value)

    cdef QuaternionList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, Quaternion value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, QuaternionList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, Quaternion* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, Quaternion* target)
    cdef toPyObject(self, Quaternion* value)
    cdef equals_SameType(self, QuaternionList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)


cdef struct EdgeIndices:
    unsigned int v1, v2

from . clist cimport CList

cdef class EdgeIndicesList(CList):
    cdef:
        EdgeIndices* data
        Py_ssize_t length
        Py_ssize_t capacity

    cdef grow(self, Py_ssize_t minCapacity)
    cdef void shrinkToLength(self)
    cdef replaceArray(self, EdgeIndices* newData, Py_ssize_t newLength, Py_ssize_t newCapacity)

    cdef extendList(self, list values)
    cdef extendTuple(self, tuple values)
    cdef Py_ssize_t searchIndex(self, EdgeIndices value)

    cdef EdgeIndicesList getValuesInSlice(self, slice sliceObject)
    cdef setValuesInSlice(self, slice sliceObject, values)
    cdef removeValuesInSlice(self, slice sliceObject)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values)
    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values)

    cdef getValueAtIndex(self, Py_ssize_t index)
    cdef setValueAtIndex(self, Py_ssize_t index, EdgeIndices value)
    cdef removeValueAtIndex(self, Py_ssize_t index)

    cdef getValuesInIndexList(self, indices)

    cdef overwrite(self, EdgeIndicesList other, Py_ssize_t index = ?)
    cdef overwriteArray(self, EdgeIndices* array, Py_ssize_t arrayLength, Py_ssize_t index)

    # Helpers
    cdef tryConversion(self, value, EdgeIndices* target)
    cdef toPyObject(self, EdgeIndices* value)
    cdef equals_SameType(self, EdgeIndicesList other)
    cdef tryCorrectIndex(self, Py_ssize_t index)
