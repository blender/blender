from libc.string cimport memcpy, memcmp, memmove

cdef class PolygonIndicesList:
    def __cinit__(self, long indicesAmount = 0, long polygonAmount = 0):
        self.indices = UIntegerList(length = indicesAmount)
        self.polyStarts = UIntegerList(length = polygonAmount)
        self.polyLengths = UIntegerList(length = polygonAmount)


    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.getLength()

    cdef long getLength(self):
        return self.polyStarts.length

    def __getitem__(self, key):
        if isinstance(key, int):
            return self.getElementAtIndex(key)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        raise TypeError("expected int or slice")

    def __setitem__(self, key, value):
        if not self.isValueValid(value):
            raise TypeError("not valid polygon indices")
        if isinstance(key, int):
            self.setElementAtIndex(key, value)
        else:
            raise TypeError("expected int")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeElementAtIndex(key)
        else:
            raise TypeError("expected int")

    def __add__(a, b):
        cdef PolygonIndicesList newList = PolygonIndicesList()
        try:
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, PolygonIndicesList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __contains__(self, value):
        try: return self.index(value) >= 0
        except: return False

    def __iter__(self):
        return PolygonIndicesListIterator(self)


    # Base operations for lists - mimic python list
    ###############################################

    cpdef append(self, value):
        if not self.isValueValid(value):
            raise TypeError("cannot append value to this list")

        self.polyStarts.append(self.indices.length)
        self.polyLengths.append(len(value))
        self.indices.extend(value)

    cdef isValueValid(self, value):
        return len(value) >= 3 and all(v >= 0 and isinstance(v, int) for v in value)

    cpdef extend(self, values):
        if isinstance(values, PolygonIndicesList):
            self.extend_SameType(values)
        else:
            for value in values:
                self.append(value)

    cdef extend_SameType(self, PolygonIndicesList otherList):
        cdef long oldLength = self.getLength()
        cdef long oldIndicesLength = self.indices.length
        self.indices.extend(otherList.indices)
        self.polyStarts.extend(otherList.polyStarts)
        self.polyLengths.extend(otherList.polyLengths)

        cdef long i
        for i in range(otherList.getLength()):
            self.polyStarts.data[oldLength + i] += oldIndicesLength

    cpdef copy(self):
        cdef PolygonIndicesList newList = PolygonIndicesList()
        newList.indices.overwrite(self.indices)
        newList.polyStarts.overwrite(self.polyStarts)
        newList.polyLengths.overwrite(self.polyLengths)
        return newList

    cpdef index(self, value):
        cdef:
            UIntegerList _value = UIntegerList.fromValues(value)
            unsigned int inputLength = _value.length
            unsigned int *indices = self.indices.data
            unsigned int *polyStarts = self.polyStarts.data
            unsigned int *polyLengths = self.polyLengths.data
            Py_ssize_t i
        for i in range(self.getLength()):
            if inputLength == polyLengths[i]:
                if 0 == memcmp(_value.data, indices + polyStarts[i],
                               polyLengths[i] * sizeof(unsigned int)):
                    return i
        return -1

    cpdef count(self, value):
        cdef:
            UIntegerList _value = UIntegerList.fromValues(value)
            unsigned int inputLength = _value.length
            unsigned int *indices = self.indices.data
            unsigned int *polyStarts = self.polyStarts.data
            unsigned int *polyLengths = self.polyLengths.data
            int counter = 0
            Py_ssize_t i
        for i in range(self.getLength()):
            if inputLength == polyLengths[i]:
                if 0 == memcmp(_value.data, indices + polyStarts[i],
                               polyLengths[i] * sizeof(unsigned int)):
                    counter += 1
        return counter

    cpdef remove(self, value):
        cdef long index
        try: index = self.index(value)
        except (TypeError, OverflowError): index = -1
        if index == -1:
            raise ValueError("value is not in list")
        self.removeElementAtIndex(index)


    # Utilities for setting and getting
    ###############################################

    cdef getElementAtIndex(self, long index):
        index = self.tryCorrectIndex(index)
        cdef long start = self.polyStarts.data[index]
        cdef long length = self.polyLengths.data[index]
        return tuple(self.indices.data[i] for i in range(start, start + length))

    cdef setElementAtIndex(self, long index, value):
        # value has to be valid at this point
        index = self.tryCorrectIndex(index)
        if len(value) == self.polyLengths.data[index]:
            self.setElementAtIndex_SameLength(index, value)
        else:
            self.setElementAtIndex_DifferentLength(index, value)

    cdef setElementAtIndex_SameLength(self, long index, value):
        cdef long i
        cdef long polyStart = self.polyStarts.data[index]
        for i in range(self.polyLengths.data[index]):
            self.indices.data[polyStart + i] = value[i]

    cdef setElementAtIndex_DifferentLength(self, long index, value):
        cdef:
            int polyStart = self.polyStarts.data[index]
            int oldLength = self.polyLengths.data[index]
            int newLength = len(value)
            int lengthDifference = newLength - oldLength

        self.indices.grow(self.indices.length + lengthDifference)

        memmove(self.indices.data + polyStart + newLength,
                self.indices.data + polyStart + oldLength,
                (self.indices.length - polyStart - oldLength) * sizeof(unsigned int))

        self.indices.length += lengthDifference

        cdef long i
        for i in range(index + 1, self.getLength()):
            self.polyStarts.data[i] += lengthDifference
        self.polyLengths.data[index] = newLength
        self.setElementAtIndex_SameLength(index, value)

    cdef removeElementAtIndex(self, long index):
        index = self.tryCorrectIndex(index)
        cdef:
            int polyStart = self.polyStarts.data[index]
            int polyLength = self.polyLengths.data[index]

        del self.indices[polyStart:polyStart + polyLength]
        del self.polyStarts[index]
        del self.polyLengths[index]

        cdef int i
        for i in range(index, self.getLength()):
            self.polyStarts.data[i] -= polyLength

    cdef tryCorrectIndex(self, long index):
        if index < 0:
            index += self.getLength()
        if index < 0 or index >= self.getLength():
            raise IndexError("list index out of range")
        return index

    cdef getValuesInSlice(self, slice sliceObject):
        cdef LongList order = LongList.fromValues(range(*sliceObject.indices(self.getLength())))
        return self.copyWithNewOrder(order, checkIndices = False)

    cdef getValuesInIndexList(self, keyList):
        cdef LongList keys
        if isinstance(keyList, LongList):
            keys = keyList
        else:
            keys = LongList.fromValues(keyList)
        return self.copyWithNewOrder(keys, checkIndices = True)

    cpdef copyWithNewOrder(self, LongList newOrder, checkIndices = True):
        cdef long i
        cdef LongList _newOrder
        if newOrder.length == 0:
            return PolygonIndicesList()
        if self.getLength() == 0:
            raise IndexError("Not all indices in the new order exist")
        if checkIndices:
            if newOrder.getMaxValue() >= self.getLength():
                raise IndexError("Not all indices in the new order exist")
            if newOrder.getMinValue() < 0:
                _newOrder = LongList(length = newOrder.length)
                for i in range(newOrder.length):
                    if newOrder.data[i] >= 0:
                        _newOrder.data[i] = newOrder.data[i]
                    else:
                        _newOrder.data[i] = self.getLength() + newOrder.data[i]
                        if _newOrder.data[i] < 0:
                            raise IndexError("Not all indices in the new order exist")
                newOrder = _newOrder

        cdef long indicesAmount = 0
        for i in range(newOrder.length):
            indicesAmount += self.polyLengths.data[newOrder.data[i]]

        cdef PolygonIndicesList newList = PolygonIndicesList(
                indicesAmount = indicesAmount,
                polygonAmount = newOrder.length)

        cdef long index, length, start, accumulatedLength = 0
        for i in range(newOrder.length):
            index = newOrder.data[i]

            length = self.polyLengths.data[index]
            start = self.polyStarts.data[index]

            newList.polyLengths.data[i] = length
            newList.polyStarts.data[i] = accumulatedLength
            memcpy(newList.indices.data + accumulatedLength,
                   self.indices.data + start,
                   sizeof(unsigned int) * length)
            accumulatedLength += length
        return newList


    # Create new lists based on an existing list
    ###############################################

    def reversed(self):
        cdef long i, length = self.getLength()
        cdef LongList newOrder = LongList(length = self.getLength())
        for i in range(length):
            newOrder.data[i] = length - i - 1
        return self.copyWithNewOrder(newOrder, checkIndices = False)

    def repeated(self, *, length = -1, amount = -1):
        cdef long i
        cdef LongList preNewOrder = LongList(length = self.getLength())
        for i in range(self.getLength()):
            preNewOrder.data[i] = i
        cdef LongList newOrder = preNewOrder.repeated(length = length, amount = amount)
        return self.copyWithNewOrder(newOrder, checkIndices = False)


    # Helper functions
    ###############################################

    def getMinIndex(self):
        return self.indices.getMinValue()

    def getMaxIndex(self):
        return self.indices.getMaxValue()


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def fromValues(cls, values):
        cdef PolygonIndicesList newList = PolygonIndicesList()
        newList.extend(values)
        return newList

    @classmethod
    def join(cls, *lists):
        cdef PolygonIndicesList newList = PolygonIndicesList()
        for elements in lists:
            newList.extend(elements)
        return newList

    def __repr__(self):
        return "<PolygonIndicesList {}>".format(list(self[i] for i in range(self.getLength())))


cdef class PolygonIndicesListIterator:
    cdef:
        PolygonIndicesList source
        long current

    def __cinit__(self, PolygonIndicesList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.getLength():
            raise StopIteration()
        self.current += 1
        return self.source[self.current - 1]
