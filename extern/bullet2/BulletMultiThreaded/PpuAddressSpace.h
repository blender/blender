#ifndef __PPU_ADDRESS_SPACE_H
#define __PPU_ADDRESS_SPACE_H


#ifdef _WIN32
//stop those casting warnings until we have a better solution for ppu_address_t / void* / uint64 conversions
#pragma warning (disable: 4311)
#pragma warning (disable: 4312)
#endif //_WIN32

#if defined(_WIN64) || defined(__LP64__) || defined(__x86_64__) || defined(USE_ADDR64)
typedef uint64_t ppu_address_t;
#else

typedef uint32_t ppu_address_t;

#endif

#endif

