
#ifndef _SDL_test_fuzzer_h
#define _SDL_test_fuzzer_h

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

void SDLTest_FuzzerInit(Uint64 execKey);

Uint8 SDLTest_RandomUint8();

Sint8 SDLTest_RandomSint8();

Uint16 SDLTest_RandomUint16();

Sint16 SDLTest_RandomSint16();

Sint32 SDLTest_RandomSint32();

Uint32 SDLTest_RandomUint32();

Uint64 SDLTest_RandomUint64();

Sint64 SDLTest_RandomSint64();

float SDLTest_RandomUnitFloat();

double SDLTest_RandomUnitDouble();

float SDLTest_RandomFloat();

double SDLTest_RandomDouble();

Uint8 SDLTest_RandomUint8BoundaryValue(Uint8 boundary1, Uint8 boundary2, SDL_bool validDomain);

Uint16 SDLTest_RandomUint16BoundaryValue(Uint16 boundary1, Uint16 boundary2, SDL_bool validDomain);

Uint32 SDLTest_RandomUint32BoundaryValue(Uint32 boundary1, Uint32 boundary2, SDL_bool validDomain);

Uint64 SDLTest_RandomUint64BoundaryValue(Uint64 boundary1, Uint64 boundary2, SDL_bool validDomain);

Sint8 SDLTest_RandomSint8BoundaryValue(Sint8 boundary1, Sint8 boundary2, SDL_bool validDomain);

Sint16 SDLTest_RandomSint16BoundaryValue(Sint16 boundary1, Sint16 boundary2, SDL_bool validDomain);

Sint32 SDLTest_RandomSint32BoundaryValue(Sint32 boundary1, Sint32 boundary2, SDL_bool validDomain);

Sint64 SDLTest_RandomSint64BoundaryValue(Sint64 boundary1, Sint64 boundary2, SDL_bool validDomain);

Sint32 SDLTest_RandomIntegerInRange(Sint32 min, Sint32 max);

char * SDLTest_RandomAsciiString();

char * SDLTest_RandomAsciiStringWithMaximumLength(int maxLength);

char * SDLTest_RandomAsciiStringOfSize(int size);

int SDLTest_GetFuzzerInvocationCount();

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

