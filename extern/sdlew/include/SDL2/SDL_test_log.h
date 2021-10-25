
#ifndef _SDL_test_log_h
#define _SDL_test_log_h

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

void SDLTest_Log(const char *fmt, ...);

void SDLTest_LogError(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

