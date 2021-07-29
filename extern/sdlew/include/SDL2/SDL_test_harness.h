
#ifndef _SDL_test_harness_h
#define _SDL_test_harness_h

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

//! Definitions for test case structures
#define TEST_ENABLED  1
#define TEST_DISABLED 0

//! Definition of all the possible test return values of the test case method
#define TEST_ABORTED        -1
#define TEST_STARTED         0
#define TEST_COMPLETED       1
#define TEST_SKIPPED         2

//! Definition of all the possible test results for the harness
#define TEST_RESULT_PASSED              0
#define TEST_RESULT_FAILED              1
#define TEST_RESULT_NO_ASSERT           2
#define TEST_RESULT_SKIPPED             3
#define TEST_RESULT_SETUP_FAILURE       4

//!< Function pointer to a test case setup function (run before every test)
typedef void (*SDLTest_TestCaseSetUpFp)(void *arg);

//!< Function pointer to a test case function
typedef int (*SDLTest_TestCaseFp)(void *arg);

//!< Function pointer to a test case teardown function (run after every test)
typedef void  (*SDLTest_TestCaseTearDownFp)(void *arg);

typedef struct SDLTest_TestCaseReference {

    SDLTest_TestCaseFp testCase;

    char *name;

    char *description;

    int enabled;
} SDLTest_TestCaseReference;

typedef struct SDLTest_TestSuiteReference {

    char *name;

    SDLTest_TestCaseSetUpFp testSetUp;

    const SDLTest_TestCaseReference **testCases;

    SDLTest_TestCaseTearDownFp testTearDown;
} SDLTest_TestSuiteReference;

int SDLTest_RunSuites(SDLTest_TestSuiteReference *testSuites[], const char *userRunSeed, Uint64 userExecKey, const char *filter, int testIterations);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

