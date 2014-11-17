
#ifndef _SDL_test_random_h
#define _SDL_test_random_h

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDLTest_RandomInt(c)        ((int)SDLTest_Random(c))

  typedef struct {
    unsigned int a;
    unsigned int x;
    unsigned int c;
    unsigned int ah;
    unsigned int al;
  } SDLTest_RandomContext;

 void SDLTest_RandomInit(SDLTest_RandomContext * rndContext, unsigned int xi,
                  unsigned int ci);

 void SDLTest_RandomInitTime(SDLTest_RandomContext *rndContext);

 unsigned int SDLTest_Random(SDLTest_RandomContext *rndContext);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

