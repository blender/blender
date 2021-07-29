
#ifndef _SDL_test_assert_h
#define _SDL_test_assert_h

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ASSERT_FAIL     0

#define ASSERT_PASS     1

void SDLTest_Assert(int assertCondition, const char *assertDescription, ...);

int SDLTest_AssertCheck(int assertCondition, const char *assertDescription, ...);

void SDLTest_AssertPass(const char *assertDescription, ...);

void SDLTest_ResetAssertSummary();

void SDLTest_LogAssertSummary();

int SDLTest_AssertSummaryToTestResult();

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

