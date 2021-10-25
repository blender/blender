
#ifndef _SDL_assert_h
#define _SDL_assert_h

#include "SDL_config.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SDL_ASSERT_LEVEL
#ifdef SDL_DEFAULT_ASSERT_LEVEL
#define SDL_ASSERT_LEVEL SDL_DEFAULT_ASSERT_LEVEL
#elif defined(_DEBUG) || defined(DEBUG) || \
      (defined(__GNUC__) && !defined(__OPTIMIZE__))
#define SDL_ASSERT_LEVEL 2
#else
#define SDL_ASSERT_LEVEL 1
#endif
#endif

#if defined(_MSC_VER)

    extern void __cdecl __debugbreak(void);
    #define SDL_TriggerBreakpoint() __debugbreak()
#elif (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)))
    #define SDL_TriggerBreakpoint() __asm__ __volatile__ ( "int $3\n\t" )
#elif defined(HAVE_SIGNAL_H)
    #include <signal.h>
    #define SDL_TriggerBreakpoint() raise(SIGTRAP)
#else

    #define SDL_TriggerBreakpoint()
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#   define SDL_FUNCTION __func__
#elif ((__GNUC__ >= 2) || defined(_MSC_VER))
#   define SDL_FUNCTION __FUNCTION__
#else
#   define SDL_FUNCTION "???"
#endif
#define SDL_FILE    __FILE__
#define SDL_LINE    __LINE__

#define SDL_disabled_assert(condition) \
    do { (void) sizeof ((condition)); } while (0)

typedef enum
{
    SDL_ASSERTION_RETRY,
    SDL_ASSERTION_BREAK,
    SDL_ASSERTION_ABORT,
    SDL_ASSERTION_IGNORE,
    SDL_ASSERTION_ALWAYS_IGNORE
} SDL_assert_state;

typedef struct SDL_assert_data
{
    int always_ignore;
    unsigned int trigger_count;
    const char *condition;
    const char *filename;
    int linenum;
    const char *function;
    const struct SDL_assert_data *next;
} SDL_assert_data;

#if (SDL_ASSERT_LEVEL > 0)

typedef SDL_assert_state SDLCALL tSDL_ReportAssertion(SDL_assert_data *,
                                                             const char *,
                                                             const char *, int);

#define SDL_enabled_assert(condition) \
    do { \
        while ( !(condition) ) { \
            static struct SDL_assert_data assert_data = { \
                0, 0, #condition, 0, 0, 0, 0 \
            }; \
            const SDL_assert_state state = SDL_ReportAssertion(&assert_data, \
                                                               SDL_FUNCTION, \
                                                               SDL_FILE, \
                                                               SDL_LINE); \
            if (state == SDL_ASSERTION_RETRY) { \
                continue;  \
            } else if (state == SDL_ASSERTION_BREAK) { \
                SDL_TriggerBreakpoint(); \
            } \
            break;  \
        } \
    } while (0)

#endif

#if SDL_ASSERT_LEVEL == 0
#   define SDL_assert(condition) SDL_disabled_assert(condition)
#   define SDL_assert_release(condition) SDL_disabled_assert(condition)
#   define SDL_assert_paranoid(condition) SDL_disabled_assert(condition)
#elif SDL_ASSERT_LEVEL == 1
#   define SDL_assert(condition) SDL_disabled_assert(condition)
#   define SDL_assert_release(condition) SDL_enabled_assert(condition)
#   define SDL_assert_paranoid(condition) SDL_disabled_assert(condition)
#elif SDL_ASSERT_LEVEL == 2
#   define SDL_assert(condition) SDL_enabled_assert(condition)
#   define SDL_assert_release(condition) SDL_enabled_assert(condition)
#   define SDL_assert_paranoid(condition) SDL_disabled_assert(condition)
#elif SDL_ASSERT_LEVEL == 3
#   define SDL_assert(condition) SDL_enabled_assert(condition)
#   define SDL_assert_release(condition) SDL_enabled_assert(condition)
#   define SDL_assert_paranoid(condition) SDL_enabled_assert(condition)
#else
#   error Unknown assertion level.
#endif

typedef SDL_assert_state (SDLCALL *SDL_AssertionHandler)(
                                 const SDL_assert_data* data, void* userdata);

typedef void SDLCALL tSDL_SetAssertionHandler(
                                            SDL_AssertionHandler handler,
                                            void *userdata);

typedef const SDL_assert_data * SDLCALL tSDL_GetAssertionReport(void);

typedef void SDLCALL tSDL_ResetAssertionReport(void);

extern tSDL_ReportAssertion *SDL_ReportAssertion;
extern tSDL_SetAssertionHandler *SDL_SetAssertionHandler;
extern tSDL_GetAssertionReport *SDL_GetAssertionReport;
extern tSDL_ResetAssertionReport *SDL_ResetAssertionReport;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

