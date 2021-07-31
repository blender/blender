cdef struct NotExistentType:
    char tmp

cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class FloatList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <float*>PyMem_Malloc(sizeof(float) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <float*>PyMem_Realloc(self.data, sizeof(float) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <float*>PyMem_Realloc(self.data, sizeof(float) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, float* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(float)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef float element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef float _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef FloatList newList
        try:
            newList = FloatList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, FloatList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(FloatList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return FloatListIterator(self)

    def __contains__(self, value):
        cdef float _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == _value):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<FloatList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<FloatList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, FloatList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i] == other.data[i]): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = FloatList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef float _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(float))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, FloatList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef float _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, float value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == value):
                return i
        return -1

    def count(self, value):
        cdef float _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i] == _value):
                amount += 1
        return amount

    def remove(self, value):
        cdef float _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef float _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(float) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, float value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(float))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef FloatList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            FloatList newList

        getValuesInSlice(self.data, self.length, sizeof(float),
                         &newArray, &newLength, sliceObject)

        newList = FloatList()
        newList.replaceArray(<float*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(float) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef float _value
        if isinstance(values, FloatList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(float),
                      elementSize = sizeof(float),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef FloatList newList = FloatList()
        cdef long index
        cdef float element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, float* target):
        target[0] = value

    cdef toPyObject(self, float* value):
        return value[0]

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, FloatList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(float))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, float* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(float))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "float" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef float[:] memview
        if self.length > 0:
            memview = <float[:self.length * sizeof(float) / sizeof(float)]><float*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<float[:1]><float*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef FloatList source

        for source in sourceLists:
            newLength += len(source)
        newList = FloatList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toFloatList
            return toFloatList(values)
        except (ImportError, TypeError): pass

        cdef FloatList newList = FloatList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return FloatList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef FloatList newList = FloatList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<FloatList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<FloatList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(float))


    # Type Specific Methods
    ###############################################

    def getMinValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a min value in a list with zero elements")

        cdef float minValue = self.data[0]
        for i in range(self.length):
            if self.data[i] < minValue:
                minValue = self.data[i]
        return minValue

    def getMaxValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a max value in a list with zero elements")

        cdef float maxValue = self.data[0]
        for i in range(self.length):
            if self.data[i] > maxValue:
                maxValue = self.data[i]
        return maxValue

    def getSumOfElements(self):
        cdef float sum = 0
        for i in range(self.length):
            sum += self.data[i]
        return sum

    def getProductOfElements(self):
        cdef float sum = 1
        for i in range(self.length):
            sum *= self.data[i]
        return sum

    def getAverageOfElements(self):
        return <double>self.getSumOfElements() / <double>self.length

    def containsValueLowerThan(self, float value):
        for i in range(self.length):
            if self.data[i] < value:
                return True
        return False

    def containsValueGreaterThan(self, float value):
        for i in range(self.length):
            if self.data[i] > value:
                return True
        return False



cdef class FloatListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        FloatList source
        Py_ssize_t current

    def __cinit__(self, FloatList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef float currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class DoubleList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <double*>PyMem_Malloc(sizeof(double) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <double*>PyMem_Realloc(self.data, sizeof(double) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <double*>PyMem_Realloc(self.data, sizeof(double) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, double* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(double)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef double element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef double _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef DoubleList newList
        try:
            newList = DoubleList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, DoubleList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(DoubleList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return DoubleListIterator(self)

    def __contains__(self, value):
        cdef double _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == _value):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<DoubleList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<DoubleList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, DoubleList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i] == other.data[i]): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = DoubleList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef double _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(double))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, DoubleList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef double _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, double value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == value):
                return i
        return -1

    def count(self, value):
        cdef double _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i] == _value):
                amount += 1
        return amount

    def remove(self, value):
        cdef double _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef double _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(double) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, double value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(double))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef DoubleList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            DoubleList newList

        getValuesInSlice(self.data, self.length, sizeof(double),
                         &newArray, &newLength, sliceObject)

        newList = DoubleList()
        newList.replaceArray(<double*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(double) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef double _value
        if isinstance(values, DoubleList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(double),
                      elementSize = sizeof(double),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef DoubleList newList = DoubleList()
        cdef long index
        cdef double element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, double* target):
        target[0] = value

    cdef toPyObject(self, double* value):
        return value[0]

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, DoubleList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(double))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, double* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(double))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "double" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef double[:] memview
        if self.length > 0:
            memview = <double[:self.length * sizeof(double) / sizeof(double)]><double*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<double[:1]><double*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef DoubleList source

        for source in sourceLists:
            newLength += len(source)
        newList = DoubleList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toDoubleList
            return toDoubleList(values)
        except (ImportError, TypeError): pass

        cdef DoubleList newList = DoubleList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return DoubleList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef DoubleList newList = DoubleList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<DoubleList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<DoubleList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(double))


    # Type Specific Methods
    ###############################################

    def getMinValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a min value in a list with zero elements")

        cdef double minValue = self.data[0]
        for i in range(self.length):
            if self.data[i] < minValue:
                minValue = self.data[i]
        return minValue

    def getMaxValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a max value in a list with zero elements")

        cdef double maxValue = self.data[0]
        for i in range(self.length):
            if self.data[i] > maxValue:
                maxValue = self.data[i]
        return maxValue

    def getSumOfElements(self):
        cdef double sum = 0
        for i in range(self.length):
            sum += self.data[i]
        return sum

    def getProductOfElements(self):
        cdef double sum = 1
        for i in range(self.length):
            sum *= self.data[i]
        return sum

    def getAverageOfElements(self):
        return <double>self.getSumOfElements() / <double>self.length

    def containsValueLowerThan(self, double value):
        for i in range(self.length):
            if self.data[i] < value:
                return True
        return False

    def containsValueGreaterThan(self, double value):
        for i in range(self.length):
            if self.data[i] > value:
                return True
        return False



cdef class DoubleListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        DoubleList source
        Py_ssize_t current

    def __cinit__(self, DoubleList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef double currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class CharList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <char*>PyMem_Malloc(sizeof(char) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <char*>PyMem_Realloc(self.data, sizeof(char) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <char*>PyMem_Realloc(self.data, sizeof(char) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, char* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(char)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef char element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef char _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef CharList newList
        try:
            newList = CharList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, CharList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(CharList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return CharListIterator(self)

    def __contains__(self, value):
        cdef char _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == _value):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<CharList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<CharList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, CharList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i] == other.data[i]): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = CharList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef char _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(char))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, CharList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef char _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, char value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == value):
                return i
        return -1

    def count(self, value):
        cdef char _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i] == _value):
                amount += 1
        return amount

    def remove(self, value):
        cdef char _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef char _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(char) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, char value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(char))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef CharList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            CharList newList

        getValuesInSlice(self.data, self.length, sizeof(char),
                         &newArray, &newLength, sliceObject)

        newList = CharList()
        newList.replaceArray(<char*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(char) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef char _value
        if isinstance(values, CharList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(char),
                      elementSize = sizeof(char),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef CharList newList = CharList()
        cdef long index
        cdef char element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, char* target):
        target[0] = value

    cdef toPyObject(self, char* value):
        return value[0]

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, CharList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(char))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, char* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(char))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "char" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef char[:] memview
        if self.length > 0:
            memview = <char[:self.length * sizeof(char) / sizeof(char)]><char*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<char[:1]><char*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef CharList source

        for source in sourceLists:
            newLength += len(source)
        newList = CharList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toCharList
            return toCharList(values)
        except (ImportError, TypeError): pass

        cdef CharList newList = CharList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return CharList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef CharList newList = CharList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<CharList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<CharList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(char))


    # Type Specific Methods
    ###############################################

    def getMinValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a min value in a list with zero elements")

        cdef char minValue = self.data[0]
        for i in range(self.length):
            if self.data[i] < minValue:
                minValue = self.data[i]
        return minValue

    def getMaxValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a max value in a list with zero elements")

        cdef char maxValue = self.data[0]
        for i in range(self.length):
            if self.data[i] > maxValue:
                maxValue = self.data[i]
        return maxValue

    def getSumOfElements(self):
        cdef char sum = 0
        for i in range(self.length):
            sum += self.data[i]
        return sum

    def getProductOfElements(self):
        cdef char sum = 1
        for i in range(self.length):
            sum *= self.data[i]
        return sum

    def getAverageOfElements(self):
        return <double>self.getSumOfElements() / <double>self.length

    def containsValueLowerThan(self, char value):
        for i in range(self.length):
            if self.data[i] < value:
                return True
        return False

    def containsValueGreaterThan(self, char value):
        for i in range(self.length):
            if self.data[i] > value:
                return True
        return False



cdef class CharListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        CharList source
        Py_ssize_t current

    def __cinit__(self, CharList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef char currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class UCharList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <unsigned char*>PyMem_Malloc(sizeof(unsigned char) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <unsigned char*>PyMem_Realloc(self.data, sizeof(unsigned char) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <unsigned char*>PyMem_Realloc(self.data, sizeof(unsigned char) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, unsigned char* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(unsigned char)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef unsigned char element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef unsigned char _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef UCharList newList
        try:
            newList = UCharList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, UCharList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(UCharList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return UCharListIterator(self)

    def __contains__(self, value):
        cdef unsigned char _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == _value):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<UCharList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<UCharList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, UCharList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i] == other.data[i]): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = UCharList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef unsigned char _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(unsigned char))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, UCharList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef unsigned char _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, unsigned char value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == value):
                return i
        return -1

    def count(self, value):
        cdef unsigned char _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i] == _value):
                amount += 1
        return amount

    def remove(self, value):
        cdef unsigned char _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef unsigned char _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(unsigned char) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, unsigned char value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(unsigned char))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef UCharList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            UCharList newList

        getValuesInSlice(self.data, self.length, sizeof(unsigned char),
                         &newArray, &newLength, sliceObject)

        newList = UCharList()
        newList.replaceArray(<unsigned char*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(unsigned char) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef unsigned char _value
        if isinstance(values, UCharList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(unsigned char),
                      elementSize = sizeof(unsigned char),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef UCharList newList = UCharList()
        cdef long index
        cdef unsigned char element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, unsigned char* target):
        target[0] = value

    cdef toPyObject(self, unsigned char* value):
        return value[0]

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, UCharList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(unsigned char))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, unsigned char* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(unsigned char))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "unsigned char" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef unsigned char[:] memview
        if self.length > 0:
            memview = <unsigned char[:self.length * sizeof(unsigned char) / sizeof(unsigned char)]><unsigned char*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<unsigned char[:1]><unsigned char*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef UCharList source

        for source in sourceLists:
            newLength += len(source)
        newList = UCharList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toUCharList
            return toUCharList(values)
        except (ImportError, TypeError): pass

        cdef UCharList newList = UCharList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return UCharList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef UCharList newList = UCharList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<UCharList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<UCharList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(unsigned char))


    # Type Specific Methods
    ###############################################

    def getMinValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a min value in a list with zero elements")

        cdef unsigned char minValue = self.data[0]
        for i in range(self.length):
            if self.data[i] < minValue:
                minValue = self.data[i]
        return minValue

    def getMaxValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a max value in a list with zero elements")

        cdef unsigned char maxValue = self.data[0]
        for i in range(self.length):
            if self.data[i] > maxValue:
                maxValue = self.data[i]
        return maxValue

    def getSumOfElements(self):
        cdef unsigned char sum = 0
        for i in range(self.length):
            sum += self.data[i]
        return sum

    def getProductOfElements(self):
        cdef unsigned char sum = 1
        for i in range(self.length):
            sum *= self.data[i]
        return sum

    def getAverageOfElements(self):
        return <double>self.getSumOfElements() / <double>self.length

    def containsValueLowerThan(self, unsigned char value):
        for i in range(self.length):
            if self.data[i] < value:
                return True
        return False

    def containsValueGreaterThan(self, unsigned char value):
        for i in range(self.length):
            if self.data[i] > value:
                return True
        return False



cdef class UCharListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        UCharList source
        Py_ssize_t current

    def __cinit__(self, UCharList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef unsigned char currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class LongList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <long*>PyMem_Malloc(sizeof(long) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <long*>PyMem_Realloc(self.data, sizeof(long) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <long*>PyMem_Realloc(self.data, sizeof(long) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, long* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(long)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef long element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef long _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef LongList newList
        try:
            newList = LongList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, LongList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(LongList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return LongListIterator(self)

    def __contains__(self, value):
        cdef long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == _value):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<LongList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<LongList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, LongList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i] == other.data[i]): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = LongList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef long _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(long))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, LongList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, long value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == value):
                return i
        return -1

    def count(self, value):
        cdef long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i] == _value):
                amount += 1
        return amount

    def remove(self, value):
        cdef long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef long _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(long) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, long value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(long))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef LongList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            LongList newList

        getValuesInSlice(self.data, self.length, sizeof(long),
                         &newArray, &newLength, sliceObject)

        newList = LongList()
        newList.replaceArray(<long*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(long) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef long _value
        if isinstance(values, LongList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(long),
                      elementSize = sizeof(long),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef LongList newList = LongList()
        cdef long index
        cdef long element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, long* target):
        target[0] = value

    cdef toPyObject(self, long* value):
        return value[0]

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, LongList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(long))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, long* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(long))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "long" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef long[:] memview
        if self.length > 0:
            memview = <long[:self.length * sizeof(long) / sizeof(long)]><long*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<long[:1]><long*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef LongList source

        for source in sourceLists:
            newLength += len(source)
        newList = LongList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toLongList
            return toLongList(values)
        except (ImportError, TypeError): pass

        cdef LongList newList = LongList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return LongList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef LongList newList = LongList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<LongList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<LongList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(long))


    # Type Specific Methods
    ###############################################

    def getMinValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a min value in a list with zero elements")

        cdef long minValue = self.data[0]
        for i in range(self.length):
            if self.data[i] < minValue:
                minValue = self.data[i]
        return minValue

    def getMaxValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a max value in a list with zero elements")

        cdef long maxValue = self.data[0]
        for i in range(self.length):
            if self.data[i] > maxValue:
                maxValue = self.data[i]
        return maxValue

    def getSumOfElements(self):
        cdef long sum = 0
        for i in range(self.length):
            sum += self.data[i]
        return sum

    def getProductOfElements(self):
        cdef long sum = 1
        for i in range(self.length):
            sum *= self.data[i]
        return sum

    def getAverageOfElements(self):
        return <double>self.getSumOfElements() / <double>self.length

    def containsValueLowerThan(self, long value):
        for i in range(self.length):
            if self.data[i] < value:
                return True
        return False

    def containsValueGreaterThan(self, long value):
        for i in range(self.length):
            if self.data[i] > value:
                return True
        return False



cdef class LongListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        LongList source
        Py_ssize_t current

    def __cinit__(self, LongList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef long currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class ULongList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <unsigned long*>PyMem_Malloc(sizeof(unsigned long) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <unsigned long*>PyMem_Realloc(self.data, sizeof(unsigned long) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <unsigned long*>PyMem_Realloc(self.data, sizeof(unsigned long) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, unsigned long* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(unsigned long)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef unsigned long element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef unsigned long _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef ULongList newList
        try:
            newList = ULongList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, ULongList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(ULongList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return ULongListIterator(self)

    def __contains__(self, value):
        cdef unsigned long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == _value):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<ULongList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<ULongList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, ULongList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i] == other.data[i]): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = ULongList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef unsigned long _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(unsigned long))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, ULongList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef unsigned long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, unsigned long value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == value):
                return i
        return -1

    def count(self, value):
        cdef unsigned long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i] == _value):
                amount += 1
        return amount

    def remove(self, value):
        cdef unsigned long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef unsigned long _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(unsigned long) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, unsigned long value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(unsigned long))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef ULongList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            ULongList newList

        getValuesInSlice(self.data, self.length, sizeof(unsigned long),
                         &newArray, &newLength, sliceObject)

        newList = ULongList()
        newList.replaceArray(<unsigned long*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(unsigned long) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef unsigned long _value
        if isinstance(values, ULongList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(unsigned long),
                      elementSize = sizeof(unsigned long),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef ULongList newList = ULongList()
        cdef long index
        cdef unsigned long element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, unsigned long* target):
        target[0] = value

    cdef toPyObject(self, unsigned long* value):
        return value[0]

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, ULongList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(unsigned long))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, unsigned long* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(unsigned long))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "unsigned long" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef unsigned long[:] memview
        if self.length > 0:
            memview = <unsigned long[:self.length * sizeof(unsigned long) / sizeof(unsigned long)]><unsigned long*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<unsigned long[:1]><unsigned long*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef ULongList source

        for source in sourceLists:
            newLength += len(source)
        newList = ULongList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toULongList
            return toULongList(values)
        except (ImportError, TypeError): pass

        cdef ULongList newList = ULongList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return ULongList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef ULongList newList = ULongList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<ULongList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<ULongList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(unsigned long))


    # Type Specific Methods
    ###############################################

    def getMinValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a min value in a list with zero elements")

        cdef unsigned long minValue = self.data[0]
        for i in range(self.length):
            if self.data[i] < minValue:
                minValue = self.data[i]
        return minValue

    def getMaxValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a max value in a list with zero elements")

        cdef unsigned long maxValue = self.data[0]
        for i in range(self.length):
            if self.data[i] > maxValue:
                maxValue = self.data[i]
        return maxValue

    def getSumOfElements(self):
        cdef unsigned long sum = 0
        for i in range(self.length):
            sum += self.data[i]
        return sum

    def getProductOfElements(self):
        cdef unsigned long sum = 1
        for i in range(self.length):
            sum *= self.data[i]
        return sum

    def getAverageOfElements(self):
        return <double>self.getSumOfElements() / <double>self.length

    def containsValueLowerThan(self, unsigned long value):
        for i in range(self.length):
            if self.data[i] < value:
                return True
        return False

    def containsValueGreaterThan(self, unsigned long value):
        for i in range(self.length):
            if self.data[i] > value:
                return True
        return False



cdef class ULongListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        ULongList source
        Py_ssize_t current

    def __cinit__(self, ULongList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef unsigned long currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class IntegerList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <int*>PyMem_Malloc(sizeof(int) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <int*>PyMem_Realloc(self.data, sizeof(int) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <int*>PyMem_Realloc(self.data, sizeof(int) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, int* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(int)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef int element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef int _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef IntegerList newList
        try:
            newList = IntegerList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, IntegerList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(IntegerList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return IntegerListIterator(self)

    def __contains__(self, value):
        cdef int _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == _value):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<IntegerList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<IntegerList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, IntegerList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i] == other.data[i]): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = IntegerList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef int _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(int))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, IntegerList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef int _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, int value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == value):
                return i
        return -1

    def count(self, value):
        cdef int _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i] == _value):
                amount += 1
        return amount

    def remove(self, value):
        cdef int _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef int _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(int) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, int value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(int))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef IntegerList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            IntegerList newList

        getValuesInSlice(self.data, self.length, sizeof(int),
                         &newArray, &newLength, sliceObject)

        newList = IntegerList()
        newList.replaceArray(<int*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(int) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef int _value
        if isinstance(values, IntegerList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(int),
                      elementSize = sizeof(int),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef IntegerList newList = IntegerList()
        cdef long index
        cdef int element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, int* target):
        target[0] = value

    cdef toPyObject(self, int* value):
        return value[0]

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, IntegerList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(int))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, int* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(int))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "int" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef int[:] memview
        if self.length > 0:
            memview = <int[:self.length * sizeof(int) / sizeof(int)]><int*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<int[:1]><int*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef IntegerList source

        for source in sourceLists:
            newLength += len(source)
        newList = IntegerList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toIntegerList
            return toIntegerList(values)
        except (ImportError, TypeError): pass

        cdef IntegerList newList = IntegerList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return IntegerList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef IntegerList newList = IntegerList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<IntegerList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<IntegerList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(int))


    # Type Specific Methods
    ###############################################

    def getMinValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a min value in a list with zero elements")

        cdef int minValue = self.data[0]
        for i in range(self.length):
            if self.data[i] < minValue:
                minValue = self.data[i]
        return minValue

    def getMaxValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a max value in a list with zero elements")

        cdef int maxValue = self.data[0]
        for i in range(self.length):
            if self.data[i] > maxValue:
                maxValue = self.data[i]
        return maxValue

    def getSumOfElements(self):
        cdef int sum = 0
        for i in range(self.length):
            sum += self.data[i]
        return sum

    def getProductOfElements(self):
        cdef int sum = 1
        for i in range(self.length):
            sum *= self.data[i]
        return sum

    def getAverageOfElements(self):
        return <double>self.getSumOfElements() / <double>self.length

    def containsValueLowerThan(self, int value):
        for i in range(self.length):
            if self.data[i] < value:
                return True
        return False

    def containsValueGreaterThan(self, int value):
        for i in range(self.length):
            if self.data[i] > value:
                return True
        return False



cdef class IntegerListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        IntegerList source
        Py_ssize_t current

    def __cinit__(self, IntegerList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef int currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class UIntegerList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <unsigned int*>PyMem_Malloc(sizeof(unsigned int) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <unsigned int*>PyMem_Realloc(self.data, sizeof(unsigned int) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <unsigned int*>PyMem_Realloc(self.data, sizeof(unsigned int) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, unsigned int* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(unsigned int)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef unsigned int element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef unsigned int _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef UIntegerList newList
        try:
            newList = UIntegerList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, UIntegerList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(UIntegerList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return UIntegerListIterator(self)

    def __contains__(self, value):
        cdef unsigned int _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == _value):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<UIntegerList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<UIntegerList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, UIntegerList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i] == other.data[i]): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = UIntegerList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef unsigned int _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(unsigned int))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, UIntegerList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef unsigned int _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, unsigned int value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == value):
                return i
        return -1

    def count(self, value):
        cdef unsigned int _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i] == _value):
                amount += 1
        return amount

    def remove(self, value):
        cdef unsigned int _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef unsigned int _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(unsigned int) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, unsigned int value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(unsigned int))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef UIntegerList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            UIntegerList newList

        getValuesInSlice(self.data, self.length, sizeof(unsigned int),
                         &newArray, &newLength, sliceObject)

        newList = UIntegerList()
        newList.replaceArray(<unsigned int*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(unsigned int) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef unsigned int _value
        if isinstance(values, UIntegerList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(unsigned int),
                      elementSize = sizeof(unsigned int),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef UIntegerList newList = UIntegerList()
        cdef long index
        cdef unsigned int element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, unsigned int* target):
        target[0] = value

    cdef toPyObject(self, unsigned int* value):
        return value[0]

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, UIntegerList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(unsigned int))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, unsigned int* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(unsigned int))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "unsigned int" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef unsigned int[:] memview
        if self.length > 0:
            memview = <unsigned int[:self.length * sizeof(unsigned int) / sizeof(unsigned int)]><unsigned int*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<unsigned int[:1]><unsigned int*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef UIntegerList source

        for source in sourceLists:
            newLength += len(source)
        newList = UIntegerList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toUIntegerList
            return toUIntegerList(values)
        except (ImportError, TypeError): pass

        cdef UIntegerList newList = UIntegerList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return UIntegerList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef UIntegerList newList = UIntegerList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<UIntegerList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<UIntegerList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(unsigned int))


    # Type Specific Methods
    ###############################################

    def getMinValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a min value in a list with zero elements")

        cdef unsigned int minValue = self.data[0]
        for i in range(self.length):
            if self.data[i] < minValue:
                minValue = self.data[i]
        return minValue

    def getMaxValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a max value in a list with zero elements")

        cdef unsigned int maxValue = self.data[0]
        for i in range(self.length):
            if self.data[i] > maxValue:
                maxValue = self.data[i]
        return maxValue

    def getSumOfElements(self):
        cdef unsigned int sum = 0
        for i in range(self.length):
            sum += self.data[i]
        return sum

    def getProductOfElements(self):
        cdef unsigned int sum = 1
        for i in range(self.length):
            sum *= self.data[i]
        return sum

    def getAverageOfElements(self):
        return <double>self.getSumOfElements() / <double>self.length

    def containsValueLowerThan(self, unsigned int value):
        for i in range(self.length):
            if self.data[i] < value:
                return True
        return False

    def containsValueGreaterThan(self, unsigned int value):
        for i in range(self.length):
            if self.data[i] > value:
                return True
        return False



cdef class UIntegerListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        UIntegerList source
        Py_ssize_t current

    def __cinit__(self, UIntegerList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef unsigned int currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class ShortList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <short*>PyMem_Malloc(sizeof(short) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <short*>PyMem_Realloc(self.data, sizeof(short) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <short*>PyMem_Realloc(self.data, sizeof(short) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, short* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(short)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef short element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef short _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef ShortList newList
        try:
            newList = ShortList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, ShortList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(ShortList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return ShortListIterator(self)

    def __contains__(self, value):
        cdef short _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == _value):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<ShortList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<ShortList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, ShortList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i] == other.data[i]): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = ShortList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef short _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(short))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, ShortList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef short _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, short value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == value):
                return i
        return -1

    def count(self, value):
        cdef short _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i] == _value):
                amount += 1
        return amount

    def remove(self, value):
        cdef short _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef short _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(short) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, short value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(short))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef ShortList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            ShortList newList

        getValuesInSlice(self.data, self.length, sizeof(short),
                         &newArray, &newLength, sliceObject)

        newList = ShortList()
        newList.replaceArray(<short*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(short) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef short _value
        if isinstance(values, ShortList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(short),
                      elementSize = sizeof(short),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef ShortList newList = ShortList()
        cdef long index
        cdef short element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, short* target):
        target[0] = value

    cdef toPyObject(self, short* value):
        return value[0]

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, ShortList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(short))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, short* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(short))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "short" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef short[:] memview
        if self.length > 0:
            memview = <short[:self.length * sizeof(short) / sizeof(short)]><short*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<short[:1]><short*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef ShortList source

        for source in sourceLists:
            newLength += len(source)
        newList = ShortList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toShortList
            return toShortList(values)
        except (ImportError, TypeError): pass

        cdef ShortList newList = ShortList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return ShortList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef ShortList newList = ShortList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<ShortList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<ShortList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(short))


    # Type Specific Methods
    ###############################################

    def getMinValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a min value in a list with zero elements")

        cdef short minValue = self.data[0]
        for i in range(self.length):
            if self.data[i] < minValue:
                minValue = self.data[i]
        return minValue

    def getMaxValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a max value in a list with zero elements")

        cdef short maxValue = self.data[0]
        for i in range(self.length):
            if self.data[i] > maxValue:
                maxValue = self.data[i]
        return maxValue

    def getSumOfElements(self):
        cdef short sum = 0
        for i in range(self.length):
            sum += self.data[i]
        return sum

    def getProductOfElements(self):
        cdef short sum = 1
        for i in range(self.length):
            sum *= self.data[i]
        return sum

    def getAverageOfElements(self):
        return <double>self.getSumOfElements() / <double>self.length

    def containsValueLowerThan(self, short value):
        for i in range(self.length):
            if self.data[i] < value:
                return True
        return False

    def containsValueGreaterThan(self, short value):
        for i in range(self.length):
            if self.data[i] > value:
                return True
        return False



cdef class ShortListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        ShortList source
        Py_ssize_t current

    def __cinit__(self, ShortList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef short currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class UShortList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <unsigned short*>PyMem_Malloc(sizeof(unsigned short) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <unsigned short*>PyMem_Realloc(self.data, sizeof(unsigned short) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <unsigned short*>PyMem_Realloc(self.data, sizeof(unsigned short) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, unsigned short* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(unsigned short)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef unsigned short element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef unsigned short _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef UShortList newList
        try:
            newList = UShortList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, UShortList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(UShortList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return UShortListIterator(self)

    def __contains__(self, value):
        cdef unsigned short _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == _value):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<UShortList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<UShortList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, UShortList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i] == other.data[i]): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = UShortList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef unsigned short _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(unsigned short))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, UShortList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef unsigned short _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, unsigned short value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == value):
                return i
        return -1

    def count(self, value):
        cdef unsigned short _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i] == _value):
                amount += 1
        return amount

    def remove(self, value):
        cdef unsigned short _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef unsigned short _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(unsigned short) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, unsigned short value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(unsigned short))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef UShortList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            UShortList newList

        getValuesInSlice(self.data, self.length, sizeof(unsigned short),
                         &newArray, &newLength, sliceObject)

        newList = UShortList()
        newList.replaceArray(<unsigned short*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(unsigned short) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef unsigned short _value
        if isinstance(values, UShortList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(unsigned short),
                      elementSize = sizeof(unsigned short),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef UShortList newList = UShortList()
        cdef long index
        cdef unsigned short element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, unsigned short* target):
        target[0] = value

    cdef toPyObject(self, unsigned short* value):
        return value[0]

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, UShortList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(unsigned short))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, unsigned short* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(unsigned short))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "unsigned short" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef unsigned short[:] memview
        if self.length > 0:
            memview = <unsigned short[:self.length * sizeof(unsigned short) / sizeof(unsigned short)]><unsigned short*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<unsigned short[:1]><unsigned short*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef UShortList source

        for source in sourceLists:
            newLength += len(source)
        newList = UShortList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toUShortList
            return toUShortList(values)
        except (ImportError, TypeError): pass

        cdef UShortList newList = UShortList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return UShortList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef UShortList newList = UShortList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<UShortList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<UShortList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(unsigned short))


    # Type Specific Methods
    ###############################################

    def getMinValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a min value in a list with zero elements")

        cdef unsigned short minValue = self.data[0]
        for i in range(self.length):
            if self.data[i] < minValue:
                minValue = self.data[i]
        return minValue

    def getMaxValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a max value in a list with zero elements")

        cdef unsigned short maxValue = self.data[0]
        for i in range(self.length):
            if self.data[i] > maxValue:
                maxValue = self.data[i]
        return maxValue

    def getSumOfElements(self):
        cdef unsigned short sum = 0
        for i in range(self.length):
            sum += self.data[i]
        return sum

    def getProductOfElements(self):
        cdef unsigned short sum = 1
        for i in range(self.length):
            sum *= self.data[i]
        return sum

    def getAverageOfElements(self):
        return <double>self.getSumOfElements() / <double>self.length

    def containsValueLowerThan(self, unsigned short value):
        for i in range(self.length):
            if self.data[i] < value:
                return True
        return False

    def containsValueGreaterThan(self, unsigned short value):
        for i in range(self.length):
            if self.data[i] > value:
                return True
        return False



cdef class UShortListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        UShortList source
        Py_ssize_t current

    def __cinit__(self, UShortList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef unsigned short currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class LongLongList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <long long*>PyMem_Malloc(sizeof(long long) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <long long*>PyMem_Realloc(self.data, sizeof(long long) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <long long*>PyMem_Realloc(self.data, sizeof(long long) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, long long* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(long long)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef long long element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef long long _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef LongLongList newList
        try:
            newList = LongLongList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, LongLongList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(LongLongList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return LongLongListIterator(self)

    def __contains__(self, value):
        cdef long long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == _value):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<LongLongList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<LongLongList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, LongLongList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i] == other.data[i]): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = LongLongList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef long long _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(long long))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, LongLongList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef long long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, long long value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == value):
                return i
        return -1

    def count(self, value):
        cdef long long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i] == _value):
                amount += 1
        return amount

    def remove(self, value):
        cdef long long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef long long _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(long long) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, long long value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(long long))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef LongLongList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            LongLongList newList

        getValuesInSlice(self.data, self.length, sizeof(long long),
                         &newArray, &newLength, sliceObject)

        newList = LongLongList()
        newList.replaceArray(<long long*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(long long) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef long long _value
        if isinstance(values, LongLongList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(long long),
                      elementSize = sizeof(long long),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef LongLongList newList = LongLongList()
        cdef long index
        cdef long long element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, long long* target):
        target[0] = value

    cdef toPyObject(self, long long* value):
        return value[0]

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, LongLongList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(long long))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, long long* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(long long))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "long long" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef long long[:] memview
        if self.length > 0:
            memview = <long long[:self.length * sizeof(long long) / sizeof(long long)]><long long*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<long long[:1]><long long*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef LongLongList source

        for source in sourceLists:
            newLength += len(source)
        newList = LongLongList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toLongLongList
            return toLongLongList(values)
        except (ImportError, TypeError): pass

        cdef LongLongList newList = LongLongList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return LongLongList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef LongLongList newList = LongLongList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<LongLongList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<LongLongList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(long long))


    # Type Specific Methods
    ###############################################

    def getMinValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a min value in a list with zero elements")

        cdef long long minValue = self.data[0]
        for i in range(self.length):
            if self.data[i] < minValue:
                minValue = self.data[i]
        return minValue

    def getMaxValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a max value in a list with zero elements")

        cdef long long maxValue = self.data[0]
        for i in range(self.length):
            if self.data[i] > maxValue:
                maxValue = self.data[i]
        return maxValue

    def getSumOfElements(self):
        cdef long long sum = 0
        for i in range(self.length):
            sum += self.data[i]
        return sum

    def getProductOfElements(self):
        cdef long long sum = 1
        for i in range(self.length):
            sum *= self.data[i]
        return sum

    def getAverageOfElements(self):
        return <double>self.getSumOfElements() / <double>self.length

    def containsValueLowerThan(self, long long value):
        for i in range(self.length):
            if self.data[i] < value:
                return True
        return False

    def containsValueGreaterThan(self, long long value):
        for i in range(self.length):
            if self.data[i] > value:
                return True
        return False



cdef class LongLongListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        LongLongList source
        Py_ssize_t current

    def __cinit__(self, LongLongList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef long long currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class ULongLongList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <unsigned long long*>PyMem_Malloc(sizeof(unsigned long long) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <unsigned long long*>PyMem_Realloc(self.data, sizeof(unsigned long long) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <unsigned long long*>PyMem_Realloc(self.data, sizeof(unsigned long long) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, unsigned long long* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(unsigned long long)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef unsigned long long element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef unsigned long long _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef ULongLongList newList
        try:
            newList = ULongLongList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, ULongLongList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(ULongLongList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return ULongLongListIterator(self)

    def __contains__(self, value):
        cdef unsigned long long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == _value):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<ULongLongList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<ULongLongList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, ULongLongList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i] == other.data[i]): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = ULongLongList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef unsigned long long _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(unsigned long long))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, ULongLongList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef unsigned long long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, unsigned long long value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i] == value):
                return i
        return -1

    def count(self, value):
        cdef unsigned long long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i] == _value):
                amount += 1
        return amount

    def remove(self, value):
        cdef unsigned long long _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef unsigned long long _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(unsigned long long) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, unsigned long long value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(unsigned long long))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef ULongLongList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            ULongLongList newList

        getValuesInSlice(self.data, self.length, sizeof(unsigned long long),
                         &newArray, &newLength, sliceObject)

        newList = ULongLongList()
        newList.replaceArray(<unsigned long long*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(unsigned long long) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef unsigned long long _value
        if isinstance(values, ULongLongList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(unsigned long long),
                      elementSize = sizeof(unsigned long long),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef ULongLongList newList = ULongLongList()
        cdef long index
        cdef unsigned long long element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, unsigned long long* target):
        target[0] = value

    cdef toPyObject(self, unsigned long long* value):
        return value[0]

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, ULongLongList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(unsigned long long))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, unsigned long long* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(unsigned long long))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "unsigned long long" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef unsigned long long[:] memview
        if self.length > 0:
            memview = <unsigned long long[:self.length * sizeof(unsigned long long) / sizeof(unsigned long long)]><unsigned long long*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<unsigned long long[:1]><unsigned long long*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef ULongLongList source

        for source in sourceLists:
            newLength += len(source)
        newList = ULongLongList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toULongLongList
            return toULongLongList(values)
        except (ImportError, TypeError): pass

        cdef ULongLongList newList = ULongLongList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return ULongLongList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef ULongLongList newList = ULongLongList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<ULongLongList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<ULongLongList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(unsigned long long))


    # Type Specific Methods
    ###############################################

    def getMinValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a min value in a list with zero elements")

        cdef unsigned long long minValue = self.data[0]
        for i in range(self.length):
            if self.data[i] < minValue:
                minValue = self.data[i]
        return minValue

    def getMaxValue(self):
        if self.length == 0:
            raise ValueError("Cannot find a max value in a list with zero elements")

        cdef unsigned long long maxValue = self.data[0]
        for i in range(self.length):
            if self.data[i] > maxValue:
                maxValue = self.data[i]
        return maxValue

    def getSumOfElements(self):
        cdef unsigned long long sum = 0
        for i in range(self.length):
            sum += self.data[i]
        return sum

    def getProductOfElements(self):
        cdef unsigned long long sum = 1
        for i in range(self.length):
            sum *= self.data[i]
        return sum

    def getAverageOfElements(self):
        return <double>self.getSumOfElements() / <double>self.length

    def containsValueLowerThan(self, unsigned long long value):
        for i in range(self.length):
            if self.data[i] < value:
                return True
        return False

    def containsValueGreaterThan(self, unsigned long long value):
        for i in range(self.length):
            if self.data[i] > value:
                return True
        return False



cdef class ULongLongListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        ULongLongList source
        Py_ssize_t current

    def __cinit__(self, ULongLongList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef unsigned long long currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class BooleanList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <char*>PyMem_Malloc(sizeof(char) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <char*>PyMem_Realloc(self.data, sizeof(char) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <char*>PyMem_Realloc(self.data, sizeof(char) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, char* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(char)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef char element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef char _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef BooleanList newList
        try:
            newList = BooleanList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, BooleanList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(BooleanList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return BooleanListIterator(self)

    def __contains__(self, value):
        cdef char _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if ((self.data[i] == 0) == (_value == 0)):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<BooleanList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<BooleanList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, BooleanList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not ((self.data[i] == 0) == (other.data[i] == 0)): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = BooleanList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef char _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(char))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, BooleanList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef char _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, char value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if ((self.data[i] == 0) == (value == 0)):
                return i
        return -1

    def count(self, value):
        cdef char _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if ((self.data[i] == 0) == (_value == 0)):
                amount += 1
        return amount

    def remove(self, value):
        cdef char _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef char _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(char) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, char value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(char))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef BooleanList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            BooleanList newList

        getValuesInSlice(self.data, self.length, sizeof(char),
                         &newArray, &newLength, sliceObject)

        newList = BooleanList()
        newList.replaceArray(<char*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(char) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef char _value
        if isinstance(values, BooleanList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(char),
                      elementSize = sizeof(char),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef BooleanList newList = BooleanList()
        cdef long index
        cdef char element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, char* target):
        target[0] = value

    cdef toPyObject(self, char* value):
        return bool(value[0])

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, BooleanList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(char))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, char* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(char))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "char" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef char[:] memview
        if self.length > 0:
            memview = <char[:self.length * sizeof(char) / sizeof(char)]><char*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<char[:1]><char*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef BooleanList source

        for source in sourceLists:
            newLength += len(source)
        newList = BooleanList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toBooleanList
            return toBooleanList(values)
        except (ImportError, TypeError): pass

        cdef BooleanList newList = BooleanList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return BooleanList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef BooleanList newList = BooleanList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<BooleanList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<BooleanList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(char))


    # Type Specific Methods
    ###############################################

    def allTrue(self):
        cdef Py_ssize_t i
        for i in range(self.length):
            if self.data[i] == 0: return False
        return True

    def allFalse(self):
        cdef Py_ssize_t i
        for i in range(self.length):
            if self.data[i] != 0: return False
        return True

    def countTrue(self):
        return self.length - self.countFalse()

    def countFalse(self):
        cdef Py_ssize_t counter = 0
        for i in range(self.length):
            if self.data[i] == 0: counter += 1
        return counter

    def invertAll(self):
        cdef Py_ssize_t i
        for i in range(self.length):
            self.data[i] = not self.data[i]



cdef class BooleanListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        BooleanList source
        Py_ssize_t current

    def __cinit__(self, BooleanList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef char currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class Vector3DList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <Vector3*>PyMem_Malloc(sizeof(Vector3) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <Vector3*>PyMem_Realloc(self.data, sizeof(Vector3) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <Vector3*>PyMem_Realloc(self.data, sizeof(Vector3) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, Vector3* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(Vector3)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef Vector3 element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef Vector3 _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef Vector3DList newList
        try:
            newList = Vector3DList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, Vector3DList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(Vector3DList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return Vector3DListIterator(self)

    def __contains__(self, value):
        cdef Vector3 _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i].x == _value.x and self.data[i].y == _value.y and self.data[i].z == _value.z):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<Vector3DList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<Vector3DList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, Vector3DList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i].x == other.data[i].x and self.data[i].y == other.data[i].y and self.data[i].z == other.data[i].z): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = Vector3DList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef Vector3 _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(Vector3))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, Vector3DList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef Vector3 _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, Vector3 value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i].x == value.x and self.data[i].y == value.y and self.data[i].z == value.z):
                return i
        return -1

    def count(self, value):
        cdef Vector3 _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i].x == _value.x and self.data[i].y == _value.y and self.data[i].z == _value.z):
                amount += 1
        return amount

    def remove(self, value):
        cdef Vector3 _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef Vector3 _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(Vector3) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, Vector3 value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(Vector3))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef Vector3DList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            Vector3DList newList

        getValuesInSlice(self.data, self.length, sizeof(Vector3),
                         &newArray, &newLength, sliceObject)

        newList = Vector3DList()
        newList.replaceArray(<Vector3*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(Vector3) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef Vector3 _value
        if isinstance(values, Vector3DList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(Vector3),
                      elementSize = sizeof(Vector3),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef Vector3DList newList = Vector3DList()
        cdef long index
        cdef Vector3 element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, Vector3* target):
        setVector3(target, value)

    cdef toPyObject(self, Vector3* value):
        return toPyVector3(value)

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, Vector3DList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(Vector3))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, Vector3* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(Vector3))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "float" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef float[:] memview
        if self.length > 0:
            memview = <float[:self.length * sizeof(Vector3) / sizeof(float)]><float*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<float[:1]><float*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef Vector3DList source

        for source in sourceLists:
            newLength += len(source)
        newList = Vector3DList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toVector3DList
            return toVector3DList(values)
        except (ImportError, TypeError): pass

        cdef Vector3DList newList = Vector3DList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return Vector3DList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef Vector3DList newList = Vector3DList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<Vector3DList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<Vector3DList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(Vector3))


    # Type Specific Methods
    ###############################################

    @classmethod
    def fromFloatList(self, FloatList source):
        if source.length % 3 != 0:
            raise ValueError("length of float list has to be divisable by 3")
        cdef Vector3DList newList = Vector3DList(length = source.length / 3)
        memcpy(newList.data, source.data, source.length * sizeof(float))
        return newList

    def transform(self, matrix, bint ignoreTranslation = False):
        from ... math import transformVector3DList
        transformVector3DList(self, matrix, ignoreTranslation)

    def getSumOfElements(self):
        cdef Vector3 sum = {"x": 0, "y" : 0, "z" : 0}
        cdef Py_ssize_t i
        for i in range(self.length):
            sum.x += self.data[i].x
            sum.y += self.data[i].y
            sum.z += self.data[i].z
        return toPyVector3(&sum)

    def getAverageOfElements(self):
        cdef Vector3 zero = {"x" : 0, "y" : 0, "z" : 0}
        if self.length == 0:
            return toPyVector3(&zero)
        else:
            return self.getSumOfElements() / self.length



cdef class Vector3DListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        Vector3DList source
        Py_ssize_t current

    def __cinit__(self, Vector3DList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef Vector3 currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class EulerList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <Euler3*>PyMem_Malloc(sizeof(Euler3) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <Euler3*>PyMem_Realloc(self.data, sizeof(Euler3) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <Euler3*>PyMem_Realloc(self.data, sizeof(Euler3) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, Euler3* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(Euler3)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef Euler3 element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef Euler3 _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef EulerList newList
        try:
            newList = EulerList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, EulerList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(EulerList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return EulerListIterator(self)

    def __contains__(self, value):
        cdef Euler3 _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i].x == _value.x and self.data[i].y == _value.y and self.data[i].z == _value.z and self.data[i].order == _value.order):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<EulerList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<EulerList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, EulerList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i].x == other.data[i].x and self.data[i].y == other.data[i].y and self.data[i].z == other.data[i].z and self.data[i].order == other.data[i].order): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = EulerList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef Euler3 _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(Euler3))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, EulerList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef Euler3 _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, Euler3 value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i].x == value.x and self.data[i].y == value.y and self.data[i].z == value.z and self.data[i].order == value.order):
                return i
        return -1

    def count(self, value):
        cdef Euler3 _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i].x == _value.x and self.data[i].y == _value.y and self.data[i].z == _value.z and self.data[i].order == _value.order):
                amount += 1
        return amount

    def remove(self, value):
        cdef Euler3 _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef Euler3 _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(Euler3) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, Euler3 value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(Euler3))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef EulerList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            EulerList newList

        getValuesInSlice(self.data, self.length, sizeof(Euler3),
                         &newArray, &newLength, sliceObject)

        newList = EulerList()
        newList.replaceArray(<Euler3*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(Euler3) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef Euler3 _value
        if isinstance(values, EulerList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(Euler3),
                      elementSize = sizeof(Euler3),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef EulerList newList = EulerList()
        cdef long index
        cdef Euler3 element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, Euler3* target):
        setEuler3(target, value)

    cdef toPyObject(self, Euler3* value):
        return toPyEuler3(value)

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, EulerList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(Euler3))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, Euler3* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(Euler3))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "NotExistentType" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef NotExistentType[:] memview
        if self.length > 0:
            memview = <NotExistentType[:self.length * sizeof(Euler3) / sizeof(NotExistentType)]><NotExistentType*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<NotExistentType[:1]><NotExistentType*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef EulerList source

        for source in sourceLists:
            newLength += len(source)
        newList = EulerList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toEulerList
            return toEulerList(values)
        except (ImportError, TypeError): pass

        cdef EulerList newList = EulerList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return EulerList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef EulerList newList = EulerList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<EulerList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<EulerList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(Euler3))


    # Type Specific Methods
    ###############################################




cdef class EulerListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        EulerList source
        Py_ssize_t current

    def __cinit__(self, EulerList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef Euler3 currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class Matrix4x4List(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <Matrix4*>PyMem_Malloc(sizeof(Matrix4) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <Matrix4*>PyMem_Realloc(self.data, sizeof(Matrix4) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <Matrix4*>PyMem_Realloc(self.data, sizeof(Matrix4) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, Matrix4* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(Matrix4)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef Matrix4 element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef Matrix4 _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef Matrix4x4List newList
        try:
            newList = Matrix4x4List(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, Matrix4x4List):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(Matrix4x4List self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return Matrix4x4ListIterator(self)

    def __contains__(self, value):
        cdef Matrix4 _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i].a11 == _value.a11 and self.data[i].a12 == _value.a12 and self.data[i].a13 == _value.a13 and self.data[i].a14 == _value.a14 and self.data[i].a21 == _value.a21 and self.data[i].a22 == _value.a22 and self.data[i].a23 == _value.a23 and self.data[i].a24 == _value.a24 and self.data[i].a31 == _value.a31 and self.data[i].a32 == _value.a32 and self.data[i].a33 == _value.a33 and self.data[i].a34 == _value.a34 and self.data[i].a41 == _value.a41 and self.data[i].a42 == _value.a42 and self.data[i].a43 == _value.a43 and self.data[i].a44 == _value.a44):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<Matrix4x4List>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<Matrix4x4List>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, Matrix4x4List other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i].a11 == other.data[i].a11 and self.data[i].a12 == other.data[i].a12 and self.data[i].a13 == other.data[i].a13 and self.data[i].a14 == other.data[i].a14 and self.data[i].a21 == other.data[i].a21 and self.data[i].a22 == other.data[i].a22 and self.data[i].a23 == other.data[i].a23 and self.data[i].a24 == other.data[i].a24 and self.data[i].a31 == other.data[i].a31 and self.data[i].a32 == other.data[i].a32 and self.data[i].a33 == other.data[i].a33 and self.data[i].a34 == other.data[i].a34 and self.data[i].a41 == other.data[i].a41 and self.data[i].a42 == other.data[i].a42 and self.data[i].a43 == other.data[i].a43 and self.data[i].a44 == other.data[i].a44): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = Matrix4x4List(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef Matrix4 _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(Matrix4))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, Matrix4x4List):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef Matrix4 _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, Matrix4 value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i].a11 == value.a11 and self.data[i].a12 == value.a12 and self.data[i].a13 == value.a13 and self.data[i].a14 == value.a14 and self.data[i].a21 == value.a21 and self.data[i].a22 == value.a22 and self.data[i].a23 == value.a23 and self.data[i].a24 == value.a24 and self.data[i].a31 == value.a31 and self.data[i].a32 == value.a32 and self.data[i].a33 == value.a33 and self.data[i].a34 == value.a34 and self.data[i].a41 == value.a41 and self.data[i].a42 == value.a42 and self.data[i].a43 == value.a43 and self.data[i].a44 == value.a44):
                return i
        return -1

    def count(self, value):
        cdef Matrix4 _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i].a11 == _value.a11 and self.data[i].a12 == _value.a12 and self.data[i].a13 == _value.a13 and self.data[i].a14 == _value.a14 and self.data[i].a21 == _value.a21 and self.data[i].a22 == _value.a22 and self.data[i].a23 == _value.a23 and self.data[i].a24 == _value.a24 and self.data[i].a31 == _value.a31 and self.data[i].a32 == _value.a32 and self.data[i].a33 == _value.a33 and self.data[i].a34 == _value.a34 and self.data[i].a41 == _value.a41 and self.data[i].a42 == _value.a42 and self.data[i].a43 == _value.a43 and self.data[i].a44 == _value.a44):
                amount += 1
        return amount

    def remove(self, value):
        cdef Matrix4 _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef Matrix4 _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(Matrix4) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, Matrix4 value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(Matrix4))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef Matrix4x4List getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            Matrix4x4List newList

        getValuesInSlice(self.data, self.length, sizeof(Matrix4),
                         &newArray, &newLength, sliceObject)

        newList = Matrix4x4List()
        newList.replaceArray(<Matrix4*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(Matrix4) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef Matrix4 _value
        if isinstance(values, Matrix4x4List):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(Matrix4),
                      elementSize = sizeof(Matrix4),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef Matrix4x4List newList = Matrix4x4List()
        cdef long index
        cdef Matrix4 element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, Matrix4* target):
        setMatrix4(target, value)

    cdef toPyObject(self, Matrix4* value):
        return toPyMatrix4(value)

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, Matrix4x4List other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(Matrix4))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, Matrix4* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(Matrix4))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "float" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef float[:] memview
        if self.length > 0:
            memview = <float[:self.length * sizeof(Matrix4) / sizeof(float)]><float*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<float[:1]><float*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef Matrix4x4List source

        for source in sourceLists:
            newLength += len(source)
        newList = Matrix4x4List(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toMatrix4x4List
            return toMatrix4x4List(values)
        except (ImportError, TypeError): pass

        cdef Matrix4x4List newList = Matrix4x4List()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return Matrix4x4List.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef Matrix4x4List newList = Matrix4x4List(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<Matrix4x4List [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<Matrix4x4List [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(Matrix4))


    # Type Specific Methods
    ###############################################

    def toEulers(self, bint isNormalized = False):
        from ... math import matrix4x4ListToEulerList
        return matrix4x4ListToEulerList(self, isNormalized)

    def toQuaternions(self, bint isNormalized = False):
        from ... math import matrix4x4ListToQuaternionList
        return matrix4x4ListToQuaternionList(self, isNormalized)

    def transpose(self):
        cdef Py_ssize_t i
        for i in range(self.length):
            transposeMatrix_Inplace(self.data + i)

    def transform(self, matrix):
        cdef Matrix4 transformation
        cdef Matrix4 temp
        cdef Py_ssize_t i
        setMatrix4(&transformation, matrix)
        for i in range(self.length):
            multMatrix4(&temp, &transformation, self.data + i)
            self.data[i] = temp



cdef class Matrix4x4ListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        Matrix4x4List source
        Py_ssize_t current

    def __cinit__(self, Matrix4x4List source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef Matrix4 currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class QuaternionList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <Quaternion*>PyMem_Malloc(sizeof(Quaternion) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <Quaternion*>PyMem_Realloc(self.data, sizeof(Quaternion) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <Quaternion*>PyMem_Realloc(self.data, sizeof(Quaternion) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, Quaternion* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(Quaternion)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef Quaternion element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef Quaternion _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef QuaternionList newList
        try:
            newList = QuaternionList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, QuaternionList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(QuaternionList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return QuaternionListIterator(self)

    def __contains__(self, value):
        cdef Quaternion _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i].w == _value.w and self.data[i].x == _value.x and self.data[i].y == _value.y and self.data[i].z == _value.z):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<QuaternionList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<QuaternionList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, QuaternionList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i].w == other.data[i].w and self.data[i].x == other.data[i].x and self.data[i].y == other.data[i].y and self.data[i].z == other.data[i].z): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = QuaternionList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef Quaternion _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(Quaternion))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, QuaternionList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef Quaternion _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, Quaternion value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i].w == value.w and self.data[i].x == value.x and self.data[i].y == value.y and self.data[i].z == value.z):
                return i
        return -1

    def count(self, value):
        cdef Quaternion _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i].w == _value.w and self.data[i].x == _value.x and self.data[i].y == _value.y and self.data[i].z == _value.z):
                amount += 1
        return amount

    def remove(self, value):
        cdef Quaternion _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef Quaternion _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(Quaternion) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, Quaternion value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(Quaternion))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef QuaternionList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            QuaternionList newList

        getValuesInSlice(self.data, self.length, sizeof(Quaternion),
                         &newArray, &newLength, sliceObject)

        newList = QuaternionList()
        newList.replaceArray(<Quaternion*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(Quaternion) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef Quaternion _value
        if isinstance(values, QuaternionList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(Quaternion),
                      elementSize = sizeof(Quaternion),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef QuaternionList newList = QuaternionList()
        cdef long index
        cdef Quaternion element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, Quaternion* target):
        setQuaternion(target, value)

    cdef toPyObject(self, Quaternion* value):
        return toPyQuaternion(value)

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, QuaternionList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(Quaternion))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, Quaternion* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(Quaternion))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "float" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef float[:] memview
        if self.length > 0:
            memview = <float[:self.length * sizeof(Quaternion) / sizeof(float)]><float*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<float[:1]><float*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef QuaternionList source

        for source in sourceLists:
            newLength += len(source)
        newList = QuaternionList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toQuaternionList
            return toQuaternionList(values)
        except (ImportError, TypeError): pass

        cdef QuaternionList newList = QuaternionList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return QuaternionList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef QuaternionList newList = QuaternionList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<QuaternionList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<QuaternionList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(Quaternion))


    # Type Specific Methods
    ###############################################




cdef class QuaternionListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        QuaternionList source
        Py_ssize_t current

    def __cinit__(self, QuaternionList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef Quaternion currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)


cimport cython
from libc.string cimport memcpy, memmove, memcmp, memset
from cpython cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from . utils cimport predictSliceLength, makeStepPositive, removeValuesInSlice, getValuesInSlice

cdef class EdgeIndicesList(CList):

    # Initialization and Memory Management
    ###############################################

    def __cinit__(self, Py_ssize_t length = 0, Py_ssize_t capacity = -1):
        '''
        Initialize a new object with the given length.
        You can also directly allocate more memory from the beginning
        to allow faster appending/extending without memory reallocation.
        '''
        if length < 0:
            raise ValueError("Length has to be >= 0")
        if capacity < length:
            capacity = length
        self.data = <EdgeIndices*>PyMem_Malloc(sizeof(EdgeIndices) * capacity)
        if self.data == NULL:
            raise MemoryError()

        self.length = length
        self.capacity = capacity

    def __dealloc__(self):
        if self.data != NULL:
            PyMem_Free(self.data)

    cdef grow(self, Py_ssize_t minCapacity):
        if minCapacity < self.capacity:
            return

        cdef Py_ssize_t newCapacity = (self.capacity * 3) / 2 + 1
        if newCapacity < minCapacity:
            newCapacity = minCapacity

        self.data = <EdgeIndices*>PyMem_Realloc(self.data, sizeof(EdgeIndices) * newCapacity)
        if self.data == NULL:
            self.length = 0
            self.capacity = 0
            raise MemoryError()
        self.capacity = newCapacity

    cdef void shrinkToLength(self):
        cdef Py_ssize_t newCapacity = max(1, self.length)
        self.data = <EdgeIndices*>PyMem_Realloc(self.data, sizeof(EdgeIndices) * newCapacity)
        self.capacity = newCapacity

    cdef replaceArray(self, EdgeIndices* newData, Py_ssize_t newLength, Py_ssize_t newCapacity):
        PyMem_Free(self.data)
        self.data = newData
        self.length = newLength
        self.capacity = newCapacity



    # Parent Class Methods
    ###############################################

    cdef void* getPointer(self):
        return self.data

    cdef int getElementSize(self):
        return sizeof(EdgeIndices)

    cdef Py_ssize_t getLength(self):
        return self.length

    cdef Py_ssize_t getCapacity(self):
        return self.capacity



    # Special Methods for Python
    ###############################################

    def __len__(self):
        return self.length

    def __getitem__(self, key):
        cdef EdgeIndices element
        if isinstance(key, int):
            element = self.getValueAtIndex(key)
            return self.toPyObject(&element)
        elif isinstance(key, slice):
            return self.getValuesInSlice(key)
        elif hasattr(key, "__iter__"):
            return self.getValuesInIndexList(key)
        else:
            raise TypeError("Expected int, slice or index list object")

    def __setitem__(self, key, value):
        cdef EdgeIndices _value
        if isinstance(key, int):
            self.tryConversion(value, &_value)
            self.setValueAtIndex(key, _value)
        elif isinstance(key, slice):
            self.setValuesInSlice(key, value)
        else:
            raise TypeError("Expected int or slice object")

    def __delitem__(self, key):
        if isinstance(key, int):
            self.removeValueAtIndex(key)
        elif isinstance(key, slice):
            self.removeValuesInSlice(key)
        else:
            raise TypeError("Expected int or slice object")

    def __add__(a, b):
        cdef EdgeIndicesList newList
        try:
            newList = EdgeIndicesList(capacity = len(a) + len(b))
            newList.extend(a)
            newList.extend(b)
        except:
            raise NotImplementedError()
        return newList

    def __mul__(a, b):
        if isinstance(a, EdgeIndicesList):
            return a.repeated(amount = max(0, b))
        else:
            return b.repeated(amount = max(0, a))

    def __iadd__(EdgeIndicesList self, other):
        try:
            self.extend(other)
        except:
            raise NotImplementedError()
        return self

    def __iter__(self):
        return EdgeIndicesListIterator(self)

    def __contains__(self, value):
        cdef EdgeIndices _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i].v1 == _value.v1 and self.data[i].v2 == _value.v2):
                return True
        return False

    def __richcmp__(x, y, int operation):
        if operation == 2: # ==
            if type(x) == type(y):
                return (<EdgeIndicesList>x).equals_SameType(y)
            if len(x) == len(y):
                return all(a == b for a, b in zip(x, y))
            return False
        elif operation == 3: # !=
            if type(x) == type(y):
                return not (<EdgeIndicesList>x).equals_SameType(y)
            if len(x) == len(y):
                return any(a != b for a, b in zip(x, y))
            return True

        raise NotImplementedError()

    cdef equals_SameType(self, EdgeIndicesList other):
        if self.length != other.length:
            return False
        cdef Py_ssize_t i
        for i in range(self.length):
            if not (self.data[i].v1 == other.data[i].v1 and self.data[i].v2 == other.data[i].v2): return False
        return True


    # Base operations for lists - mimic python list
    ###############################################

    def copy(self):
        newList = EdgeIndicesList(self.length)
        newList.overwrite(self)
        return newList

    def clear(self):
        self.length = 0
        self.shrinkToLength()

    def fill(self, value):
        cdef Py_ssize_t i
        cdef EdgeIndices _value
        if value == 0:
            memset(self.data, 0, self.length * sizeof(EdgeIndices))
        else:
            self.tryConversion(value, &_value)
            for i in range(self.length):
                self.data[i] = _value

    def append(self, value):
        if self.length >= self.capacity:
            self.grow(self.length + 1)
        self.tryConversion(value, self.data + self.length)
        self.length += 1

    def extend(self, values):
        cdef Py_ssize_t oldLength, newLength, i
        if isinstance(values, EdgeIndicesList):
            self.overwrite(values, self.length)
        elif isinstance(values, list):
            self.extendList(values)
        elif isinstance(values, tuple):
            self.extendTuple(values)
        elif hasattr(values, "__len__"):
            newLength = self.length + len(values)
            self.grow(newLength)
            for i, value in enumerate(values, start = self.length):
                self.tryConversion(value, self.data + i)
            self.length = newLength
        else:
            try:
                oldLength = self.length
                for value in values:
                    self.append(value)
            except:
                self.length = oldLength
                raise TypeError("invalid input")

    cdef extendList(self, list values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    cdef extendTuple(self, tuple values):
        cdef Py_ssize_t newLength, i
        newLength = self.length + len(values)
        self.grow(newLength)
        for i in range(len(values)):
            self.tryConversion(values[i], self.data + self.length + i)
        self.length = newLength

    def index(self, value):
        cdef EdgeIndices _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index >= 0: return index
        raise ValueError("value not in list")

    cdef Py_ssize_t searchIndex(self, EdgeIndices value):
        cdef Py_ssize_t i
        for i in range(self.length):
            if (self.data[i].v1 == value.v1 and self.data[i].v2 == value.v2):
                return i
        return -1

    def count(self, value):
        cdef EdgeIndices _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t i
        cdef Py_ssize_t amount = 0
        for i in range(self.length):
            if (self.data[i].v1 == _value.v1 and self.data[i].v2 == _value.v2):
                amount += 1
        return amount

    def remove(self, value):
        cdef EdgeIndices _value
        self.tryConversion(value, &_value)
        cdef Py_ssize_t index = self.searchIndex(_value)
        if index == -1:
            raise ValueError("value not in list")
        else:
            self.removeValueAtIndex(index)

    def insert(self, Py_ssize_t index, value):
        cdef EdgeIndices _value
        if index >= self.length:
            self.append(value)
        else:
            self.tryConversion(value, &_value)
            self.grow(self.length + 1)
            if index < 0: index += self.length
            if index < 0: index = 0
            memmove(self.data + index + 1,
                    self.data + index,
                    sizeof(EdgeIndices) * (self.length - index))
            self.data[index] = _value
            self.length += 1



    # Get/Set/Remove single element
    ################################################

    cdef getValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        return self.data[index]

    cdef setValueAtIndex(self, Py_ssize_t index, EdgeIndices value):
        index = self.tryCorrectIndex(index)
        self.data[index] = value

    cdef removeValueAtIndex(self, Py_ssize_t index):
        index = self.tryCorrectIndex(index)
        memmove(self.data + index,
                self.data + index + 1,
                (self.length - index) * sizeof(EdgeIndices))
        self.length -= 1


    # Get/Set/Remove elements in slice
    ################################################

    cdef EdgeIndicesList getValuesInSlice(self, slice sliceObject):
        cdef:
            void* newArray
            Py_ssize_t newLength
            EdgeIndicesList newList

        getValuesInSlice(self.data, self.length, sizeof(EdgeIndices),
                         &newArray, &newLength, sliceObject)

        newList = EdgeIndicesList()
        newList.replaceArray(<EdgeIndices*>newArray, newLength, newLength)
        return newList

    cdef setValuesInSlice(self, slice sliceObject, values):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))

        if step == 1:
            self.setValuesInSimpleSlice(start, stop, values)
        else:
            self.setValuesInExtendedSlice(start, stop, step, values)

    cdef setValuesInSimpleSlice(self, Py_ssize_t start, Py_ssize_t stop, values):
        cdef:
            Py_ssize_t replacementLength = len(values)
            Py_ssize_t sliceLength = predictSliceLength(start, stop, 1)

        if replacementLength > sliceLength:
            self.grow(self.length + (replacementLength - sliceLength))
        if replacementLength != sliceLength:
            memmove(self.data + start + replacementLength,
                    self.data + stop,
                    sizeof(EdgeIndices) * (self.length - stop))
            self.length += replacementLength - sliceLength

        cdef Py_ssize_t i
        cdef EdgeIndices _value
        if isinstance(values, EdgeIndicesList):
            self.overwrite(values, start)
        else:
            for i in range(replacementLength):
                self.tryConversion(values[i], self.data + start + i)

    cdef setValuesInExtendedSlice(self, Py_ssize_t start, Py_ssize_t stop, Py_ssize_t step, values):
        cdef Py_ssize_t sliceLength = predictSliceLength(start, stop, step)
        if sliceLength != len(values):
            raise ValueError("attempt to assign sequence of size {} to extended slice of size {}"
                             .format(len(values), sliceLength))

        # TODO: Speedup for specific list types + use while loop
        # range does not efficiently work with a variable step
        cdef Py_ssize_t i
        for i, value in zip(range(start, stop, step), values):
            self.tryConversion(value, self.data + i)

    cdef removeValuesInSlice(self, slice sliceObject):
        cdef Py_ssize_t start, stop, step
        start, stop, step = sliceObject.indices(len(self))
        cdef Py_ssize_t removeAmount = removeValuesInSlice(
                      arrayStart = <char*>self.data,
                      arrayLength = self.length * sizeof(EdgeIndices),
                      elementSize = sizeof(EdgeIndices),
                      start = start, stop = stop, step = step)
        self.length -= removeAmount


    # Get/Set/Remove elements in index list
    ################################################

    cdef getValuesInIndexList(self, indices):
        # TODO: optimize for some data types
        cdef EdgeIndicesList newList = EdgeIndicesList()
        cdef long index
        cdef EdgeIndices element
        for index in indices:
            element = self.getValueAtIndex(index)
            newList.append(self.toPyObject(&element))
        return newList


    # Low level utilities
    ###############################################

    cdef tryConversion(self, value, EdgeIndices* target):
        if len(value) == 2: target.v1, target.v2 = value[0], value[1]
        else: raise TypeError("length has to be 2")

    cdef toPyObject(self, EdgeIndices* value):
        return (value.v1, value.v2)

    cdef tryCorrectIndex(self, Py_ssize_t index):
        if index < 0:
            index += self.length
        if index < 0 or index >= self.length:
            raise IndexError("list index out of range")
        return index

    cdef overwrite(self, EdgeIndicesList other, Py_ssize_t index = 0):
        if self.capacity < index + other.length:
            self.grow(index + other.length)
        memcpy(self.data + index, other.data, other.length * sizeof(EdgeIndices))
        self.length = max(self.length, index + other.length)

    cdef overwriteArray(self, EdgeIndices* array, Py_ssize_t arrayLength, Py_ssize_t index):
        if self.capacity <= index + arrayLength:
            self.grow(index + arrayLength)
        memcpy(self.data + index, array, arrayLength * sizeof(EdgeIndices))
        self.length = max(self.length, index + arrayLength)


    # Memory Views
    ###############################################

    def asMemoryView(self):
        if "unsigned int" == "NotExistentType":
            raise NotImplementedError("Cannot create memoryview for this type")

        cdef unsigned int[:] memview
        if self.length > 0:
            memview = <unsigned int[:self.length * sizeof(EdgeIndices) / sizeof(unsigned int)]><unsigned int*>self.data
        else:
            # hack to make zero-length memview possible
            memview = (<unsigned int[:1]><unsigned int*>self.data)[1:]
        return memview

    def asNumpyArray(self):
        import numpy
        return numpy.asarray(self.asMemoryView())


    # Classmethods for List Creation
    ###############################################

    @classmethod
    def join(cls, *sourceLists):
        cdef Py_ssize_t newLength = 0
        cdef Py_ssize_t offset = 0
        cdef EdgeIndicesList source

        for source in sourceLists:
            newLength += len(source)
        newList = EdgeIndicesList(newLength)
        for source in sourceLists:
            newList.overwrite(source, offset)
            offset += source.length

        return newList

    @classmethod
    def fromValues(cls, values):
        if isinstance(values, (list, tuple)):
            return cls.fromListOrTuple(values)

        try:
            from . convert import toEdgeIndicesList
            return toEdgeIndicesList(values)
        except (ImportError, TypeError): pass

        cdef EdgeIndicesList newList = EdgeIndicesList()
        newList.extend(values)
        return newList

    @classmethod
    def fromValue(cls, value, length = 1):
        return EdgeIndicesList.fromValues([value]) * length

    @classmethod
    def fromListOrTuple(cls, list_or_tuple values):
        cdef EdgeIndicesList newList = EdgeIndicesList(len(values))
        cdef Py_ssize_t i
        for i, value in enumerate(values):
            newList.tryConversion(value, newList.data + i)
        return newList


    # String Representations
    ###############################################

    def __repr__(self):
        if self.length < 20:
            return "<EdgeIndicesList [{}]>".format(", ".join(str(self[i]) for i in range(self.length)))
        else:
            return "<EdgeIndicesList [{}, ...]>".format(", ".join(str(self[i]) for i in range(20)))

    def status(self):
        return "Length: {}, Capacity: {}, Size: {} bytes".format(
            self.length, self.capacity, self.capacity * sizeof(EdgeIndices))


    # Type Specific Methods
    ###############################################

    def getMinIndex(self):
        if self.length == 0:
            raise ValueError("Cannot find a min value in a list with zero elements")

        cdef unsigned int* data = <unsigned int*>self.data
        cdef unsigned int minValue = data[0]
        for i in range(self.length * sizeof(EdgeIndices) / sizeof(unsigned int)):
            if data[i] < minValue:
                minValue = data[i]
        return minValue

    def getMaxIndex(self):
        if self.length == 0:
            raise ValueError("Cannot find a max value in a list with zero elements")

        cdef unsigned int* data = <unsigned int*>self.data
        cdef unsigned int maxValue = data[0]
        for i in range(self.length * sizeof(EdgeIndices) / sizeof(unsigned int)):
            if data[i] > maxValue:
                maxValue = data[i]
        return maxValue



cdef class EdgeIndicesListIterator:
    '''
    Implements the 'Iterator Protocol' that is used to allow iteration
    over a custom list object (eg with a for loop).
    An instance of this class is only created in the __iter__ method
    of the corresponding list type.
    https://docs.python.org/3.5/library/stdtypes.html#iterator-types
    '''
    cdef:
        EdgeIndicesList source
        Py_ssize_t current

    def __cinit__(self, EdgeIndicesList source):
        self.source = source
        self.current = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.current >= self.source.length:
            raise StopIteration()
        cdef EdgeIndices currentValue = self.source.data[self.current]
        self.current += 1
        return self.source.toPyObject(&currentValue)
