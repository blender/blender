from libc.stdint cimport uint8_t, uint32_t

cpdef uint32_t strToInt(str text, uint32_t seed = ?)
cdef uint32_t murmur3_32(char* key, uint32_t len, uint32_t seed)
