
#ifndef _SDL_test_md5_h
#define _SDL_test_md5_h

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef unsigned long int MD5UINT4;

  typedef struct {
    MD5UINT4  i[2];
    MD5UINT4  buf[4];
    unsigned char in[64];
    unsigned char digest[16];
  } SDLTest_Md5Context;

 void SDLTest_Md5Init(SDLTest_Md5Context * mdContext);

 void SDLTest_Md5Update(SDLTest_Md5Context * mdContext, unsigned char *inBuf,
                 unsigned int inLen);

 void SDLTest_Md5Final(SDLTest_Md5Context * mdContext);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

