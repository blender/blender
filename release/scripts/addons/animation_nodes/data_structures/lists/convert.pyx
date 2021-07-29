cpdef FloatList toFloatList(NumericList sourceList):
    cdef FloatList newList = FloatList(sourceList.length)
    cdef unsigned long i
    for i in range(len(sourceList)):
            newList.data[i] = <float>sourceList.data[i]
    return newList


cpdef DoubleList toDoubleList(NumericList sourceList):
    cdef DoubleList newList = DoubleList(sourceList.length)
    cdef unsigned long i
    for i in range(len(sourceList)):
            newList.data[i] = <double>sourceList.data[i]
    return newList


cpdef CharList toCharList(NumericList sourceList):
    cdef CharList newList = CharList(sourceList.length)
    cdef unsigned long i
    for i in range(len(sourceList)):
            newList.data[i] = <char>sourceList.data[i]
    return newList


cpdef UCharList toUCharList(NumericList sourceList):
    cdef UCharList newList = UCharList(sourceList.length)
    cdef unsigned long i
    for i in range(len(sourceList)):
            newList.data[i] = <unsigned char>sourceList.data[i]
    return newList


cpdef LongList toLongList(NumericList sourceList):
    cdef LongList newList = LongList(sourceList.length)
    cdef unsigned long i
    for i in range(len(sourceList)):
            newList.data[i] = <long>sourceList.data[i]
    return newList


cpdef ULongList toULongList(NumericList sourceList):
    cdef ULongList newList = ULongList(sourceList.length)
    cdef unsigned long i
    for i in range(len(sourceList)):
            newList.data[i] = <unsigned long>sourceList.data[i]
    return newList


cpdef IntegerList toIntegerList(NumericList sourceList):
    cdef IntegerList newList = IntegerList(sourceList.length)
    cdef unsigned long i
    for i in range(len(sourceList)):
            newList.data[i] = <int>sourceList.data[i]
    return newList


cpdef UIntegerList toUIntegerList(NumericList sourceList):
    cdef UIntegerList newList = UIntegerList(sourceList.length)
    cdef unsigned long i
    for i in range(len(sourceList)):
            newList.data[i] = <unsigned int>sourceList.data[i]
    return newList


cpdef ShortList toShortList(NumericList sourceList):
    cdef ShortList newList = ShortList(sourceList.length)
    cdef unsigned long i
    for i in range(len(sourceList)):
            newList.data[i] = <short>sourceList.data[i]
    return newList


cpdef UShortList toUShortList(NumericList sourceList):
    cdef UShortList newList = UShortList(sourceList.length)
    cdef unsigned long i
    for i in range(len(sourceList)):
            newList.data[i] = <unsigned short>sourceList.data[i]
    return newList


cpdef LongLongList toLongLongList(NumericList sourceList):
    cdef LongLongList newList = LongLongList(sourceList.length)
    cdef unsigned long i
    for i in range(len(sourceList)):
            newList.data[i] = <long long>sourceList.data[i]
    return newList


cpdef ULongLongList toULongLongList(NumericList sourceList):
    cdef ULongLongList newList = ULongLongList(sourceList.length)
    cdef unsigned long i
    for i in range(len(sourceList)):
            newList.data[i] = <unsigned long long>sourceList.data[i]
    return newList
