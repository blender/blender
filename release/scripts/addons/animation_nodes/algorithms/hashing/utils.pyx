from . murmurhash3 cimport strToInt

cpdef int strToEnumItemID(str text):
    # not sure why but Blender has problems
    # when the unique number is too large
    return strToInt(text) % 10000000
