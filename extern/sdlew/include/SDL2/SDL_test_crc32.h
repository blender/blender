
#ifndef _SDL_test_crc32_h
#define _SDL_test_crc32_h

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CrcUint32
 #define CrcUint32  unsigned int
#endif
#ifndef CrcUint8
 #define CrcUint8   unsigned char
#endif

#ifdef ORIGINAL_METHOD
 #define CRC32_POLY 0x04c11db7
#else
 #define CRC32_POLY 0xEDB88320
#endif

  typedef struct {
    CrcUint32    crc32_table[256];
  } SDLTest_Crc32Context;

 int SDLTest_Crc32Init(SDLTest_Crc32Context * crcContext);

int SDLTest_crc32Calc(SDLTest_Crc32Context * crcContext, CrcUint8 *inBuf, CrcUint32 inLen, CrcUint32 *crc32);

int SDLTest_Crc32CalcStart(SDLTest_Crc32Context * crcContext, CrcUint32 *crc32);
int SDLTest_Crc32CalcEnd(SDLTest_Crc32Context * crcContext, CrcUint32 *crc32);
int SDLTest_Crc32CalcBuffer(SDLTest_Crc32Context * crcContext, CrcUint8 *inBuf, CrcUint32 inLen, CrcUint32 *crc32);

int SDLTest_Crc32Done(SDLTest_Crc32Context * crcContext);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

