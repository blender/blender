'''
This is a Murmur Hash 3 implementation. It was created by Austin Appleby.
https://github.com/aappleby/smhasher
'''

cpdef uint32_t strToInt(str text, uint32_t seed = 0):
    cdef bytes byteText = text.encode()
    return murmur3_32(byteText, len(byteText), seed)

cdef uint32_t murmur3_32(char* key, uint32_t len, uint32_t seed):
    cdef:
        uint32_t c1 = 0xcc9e2d51
        uint32_t c2 = 0x1b873593
        uint32_t r1 = 15
        uint32_t r2 = 13
        uint32_t m = 5
        uint32_t n = 0xe6546b64

        uint32_t hash = seed
        int nBlocks = len / 4
        uint32_t* blocks = <uint32_t*>key
        int i
        uint32_t k

    for i in range(nBlocks):
        k = blocks[i]
        k *= c1
        k = ROT32(k, r1)
        k *= c2

        hash ^= k
        hash = ROT32(hash, r2) * m + n

    cdef:
        uint8_t* tail = <uint8_t*>(key + nBlocks * 4)
        uint32_t k1 = 0

    if len & 3 >= 3:
        k1 ^= tail[2] << 16
    if len & 3 >= 2:
        k1 ^= tail[1] << 8
    if len & 3 >= 1:
        k1 ^= tail[0]

        k1 *= c1
        k1 = ROT32(k1, r1)
        k1 *= c2
        hash ^= k1

    hash ^= len
    return finalMix(hash)

cdef inline uint32_t ROT32(uint32_t x, uint32_t y):
    return (x << y) | (x >> (32 - y))

cdef inline uint32_t finalMix(uint32_t h):
    cdef:
        uint32_t fac1 = 0x85ebca6b
        uint32_t fac2 = 0xc2b2ae35
    h ^= (h >> 16)
    h *= fac1
    h ^= (h >> 13)
    h *= fac2
    h ^= (h >> 16)
    return h
