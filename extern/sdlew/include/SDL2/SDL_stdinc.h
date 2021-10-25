
#ifndef _SDL_stdinc_h
#define _SDL_stdinc_h

#include "SDL_config.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#if defined(STDC_HEADERS)
# include <stdlib.h>
# include <stddef.h>
# include <stdarg.h>
#else
# if defined(HAVE_STDLIB_H)
#  include <stdlib.h>
# elif defined(HAVE_MALLOC_H)
#  include <malloc.h>
# endif
# if defined(HAVE_STDDEF_H)
#  include <stddef.h>
# endif
# if defined(HAVE_STDARG_H)
#  include <stdarg.h>
# endif
#endif
#ifdef HAVE_STRING_H
# if !defined(STDC_HEADERS) && defined(HAVE_MEMORY_H)
#  include <memory.h>
# endif
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#if defined(HAVE_INTTYPES_H)
# include <inttypes.h>
#elif defined(HAVE_STDINT_H)
# include <stdint.h>
#endif
#ifdef HAVE_CTYPE_H
# include <ctype.h>
#endif
#ifdef HAVE_MATH_H
# include <math.h>
#endif
#if defined(HAVE_ICONV) && defined(HAVE_ICONV_H)
# include <iconv.h>
#endif

#define SDL_arraysize(array)    (sizeof(array)/sizeof(array[0]))
#define SDL_TABLESIZE(table)    SDL_arraysize(table)

#ifdef __cplusplus
#define SDL_reinterpret_cast(type, expression) reinterpret_cast<type>(expression)
#define SDL_static_cast(type, expression) static_cast<type>(expression)
#define SDL_const_cast(type, expression) const_cast<type>(expression)
#else
#define SDL_reinterpret_cast(type, expression) ((type)(expression))
#define SDL_static_cast(type, expression) ((type)(expression))
#define SDL_const_cast(type, expression) ((type)(expression))
#endif

#define SDL_FOURCC(A, B, C, D) \
    ((SDL_static_cast(Uint32, SDL_static_cast(Uint8, (A))) << 0) | \
     (SDL_static_cast(Uint32, SDL_static_cast(Uint8, (B))) << 8) | \
     (SDL_static_cast(Uint32, SDL_static_cast(Uint8, (C))) << 16) | \
     (SDL_static_cast(Uint32, SDL_static_cast(Uint8, (D))) << 24))

typedef enum
{
    SDL_FALSE = 0,
    SDL_TRUE = 1
} SDL_bool;

typedef int8_t Sint8;

typedef uint8_t Uint8;

typedef int16_t Sint16;

typedef uint16_t Uint16;

typedef int32_t Sint32;

typedef uint32_t Uint32;

typedef int64_t Sint64;

typedef uint64_t Uint64;

#define SDL_COMPILE_TIME_ASSERT(name, x)               \
       typedef int SDL_dummy_ ## name[(x) * 2 - 1]

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
SDL_COMPILE_TIME_ASSERT(uint8, sizeof(Uint8) == 1);
SDL_COMPILE_TIME_ASSERT(sint8, sizeof(Sint8) == 1);
SDL_COMPILE_TIME_ASSERT(uint16, sizeof(Uint16) == 2);
SDL_COMPILE_TIME_ASSERT(sint16, sizeof(Sint16) == 2);
SDL_COMPILE_TIME_ASSERT(uint32, sizeof(Uint32) == 4);
SDL_COMPILE_TIME_ASSERT(sint32, sizeof(Sint32) == 4);
SDL_COMPILE_TIME_ASSERT(uint64, sizeof(Uint64) == 8);
SDL_COMPILE_TIME_ASSERT(sint64, sizeof(Sint64) == 8);
#endif

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
#if !defined(__ANDROID__)

typedef enum
{
    DUMMY_ENUM_VALUE
} SDL_DUMMY_ENUM;

SDL_COMPILE_TIME_ASSERT(enum, sizeof(SDL_DUMMY_ENUM) == sizeof(int));
#endif
#endif

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(HAVE_ALLOCA) && !defined(alloca)
# if defined(HAVE_ALLOCA_H)
#  include <alloca.h>
# elif defined(__GNUC__)
#  define alloca __builtin_alloca
# elif defined(_MSC_VER)
#  include <malloc.h>
#  define alloca _alloca
# elif defined(__WATCOMC__)
#  include <malloc.h>
# elif defined(__BORLANDC__)
#  include <malloc.h>
# elif defined(__DMC__)
#  include <stdlib.h>
# elif defined(__AIX__)
#pragma alloca
# elif defined(__MRC__)
void *alloca(unsigned);
# else
char *alloca();
# endif
#endif
#ifdef HAVE_ALLOCA
#define SDL_stack_alloc(type, count)    (type*)alloca(sizeof(type)*(count))
#define SDL_stack_free(data)
#else
#define SDL_stack_alloc(type, count)    (type*)SDL_malloc(sizeof(type)*(count))
#define SDL_stack_free(data)            SDL_free(data)
#endif

typedef void * SDLCALL tSDL_malloc(size_t size);
typedef void * SDLCALL tSDL_calloc(size_t nmemb, size_t size);
typedef void * SDLCALL tSDL_realloc(void *mem, size_t size);
typedef void SDLCALL tSDL_free(void *mem);

typedef char * SDLCALL tSDL_getenv(const char *name);
typedef int SDLCALL tSDL_setenv(const char *name, const char *value, int overwrite);

typedef void SDLCALL tSDL_qsort(void *base, size_t nmemb, size_t size, int (*compare) (const void *, const void *));

typedef int SDLCALL tSDL_abs(int x);

#define SDL_min(x, y) (((x) < (y)) ? (x) : (y))
#define SDL_max(x, y) (((x) > (y)) ? (x) : (y))

typedef int SDLCALL tSDL_isdigit(int x);
typedef int SDLCALL tSDL_isspace(int x);
typedef int SDLCALL tSDL_toupper(int x);
typedef int SDLCALL tSDL_tolower(int x);

typedef void * SDLCALL tSDL_memset(void *dst, int c, size_t len);

#define SDL_zero(x) SDL_memset(&(x), 0, sizeof((x)))
#define SDL_zerop(x) SDL_memset((x), 0, sizeof(*(x)))

SDL_FORCE_INLINE void SDL_memset4(void *dst, int val, size_t dwords)
{
#if defined(__GNUC__) && defined(i386)
    int u0, u1, u2;
    __asm__ __volatile__ (
        "cld \n\t"
        "rep ; stosl \n\t"
        : "=&D" (u0), "=&a" (u1), "=&c" (u2)
        : "0" (dst), "1" (val), "2" (SDL_static_cast(Uint32, dwords))
        : "memory"
    );
#else
    size_t _n = (dwords + 3) / 4;
    Uint32 *_p = SDL_static_cast(Uint32 *, dst);
    Uint32 _val = (val);
    if (dwords == 0)
        return;
    switch (dwords % 4)
    {
        case 0: do {    *_p++ = _val;
        case 3:         *_p++ = _val;
        case 2:         *_p++ = _val;
        case 1:         *_p++ = _val;
        } while ( --_n );
    }
#endif
}

typedef void * SDLCALL tSDL_memcpy(void *dst, const void *src, size_t len);

extern tSDL_memcpy *SDL_memcpy;
SDL_FORCE_INLINE void *SDL_memcpy4(void *dst, const void *src, size_t dwords)
{
    return SDL_memcpy(dst, src, dwords * 4);
}

typedef void * SDLCALL tSDL_memmove(void *dst, const void *src, size_t len);
typedef int SDLCALL tSDL_memcmp(const void *s1, const void *s2, size_t len);

typedef size_t SDLCALL tSDL_wcslen(const wchar_t *wstr);
typedef size_t SDLCALL tSDL_wcslcpy(wchar_t *dst, const wchar_t *src, size_t maxlen);
typedef size_t SDLCALL tSDL_wcslcat(wchar_t *dst, const wchar_t *src, size_t maxlen);

typedef size_t SDLCALL tSDL_strlen(const char *str);
typedef size_t SDLCALL tSDL_strlcpy(char *dst, const char *src, size_t maxlen);
typedef size_t SDLCALL tSDL_utf8strlcpy(char *dst, const char *src, size_t dst_bytes);
typedef size_t SDLCALL tSDL_strlcat(char *dst, const char *src, size_t maxlen);
typedef char * SDLCALL tSDL_strdup(const char *str);
typedef char * SDLCALL tSDL_strrev(char *str);
typedef char * SDLCALL tSDL_strupr(char *str);
typedef char * SDLCALL tSDL_strlwr(char *str);
typedef char * SDLCALL tSDL_strchr(const char *str, int c);
typedef char * SDLCALL tSDL_strrchr(const char *str, int c);
typedef char * SDLCALL tSDL_strstr(const char *haystack, const char *needle);

typedef char * SDLCALL tSDL_itoa(int value, char *str, int radix);
typedef char * SDLCALL tSDL_uitoa(unsigned int value, char *str, int radix);
typedef char * SDLCALL tSDL_ltoa(long value, char *str, int radix);
typedef char * SDLCALL tSDL_ultoa(unsigned long value, char *str, int radix);
typedef char * SDLCALL tSDL_lltoa(Sint64 value, char *str, int radix);
typedef char * SDLCALL tSDL_ulltoa(Uint64 value, char *str, int radix);

typedef int SDLCALL tSDL_atoi(const char *str);
typedef double SDLCALL tSDL_atof(const char *str);
typedef long SDLCALL tSDL_strtol(const char *str, char **endp, int base);
extern DECLSPEC unsigned long SDLCALL SDL_strtoul(const char *str, char **endp, int base);
typedef Sint64 SDLCALL tSDL_strtoll(const char *str, char **endp, int base);
typedef Uint64 SDLCALL tSDL_strtoull(const char *str, char **endp, int base);
typedef double SDLCALL tSDL_strtod(const char *str, char **endp);

typedef int SDLCALL tSDL_strcmp(const char *str1, const char *str2);
typedef int SDLCALL tSDL_strncmp(const char *str1, const char *str2, size_t maxlen);
typedef int SDLCALL tSDL_strcasecmp(const char *str1, const char *str2);
typedef int SDLCALL tSDL_strncasecmp(const char *str1, const char *str2, size_t len);

typedef int SDLCALL tSDL_sscanf(const char *text, const char *fmt, ...);
typedef int SDLCALL tSDL_snprintf(char *text, size_t maxlen, const char *fmt, ...);
typedef int SDLCALL tSDL_vsnprintf(char *text, size_t maxlen, const char *fmt, va_list ap);

#ifndef HAVE_M_PI
#ifndef M_PI
#define M_PI    3.14159265358979323846264338327950288
#endif
#endif

typedef double SDLCALL tSDL_atan(double x);
typedef double SDLCALL tSDL_atan2(double x, double y);
typedef double SDLCALL tSDL_ceil(double x);
typedef double SDLCALL tSDL_copysign(double x, double y);
typedef double SDLCALL tSDL_cos(double x);
typedef float SDLCALL tSDL_cosf(float x);
typedef double SDLCALL tSDL_fabs(double x);
typedef double SDLCALL tSDL_floor(double x);
typedef double SDLCALL tSDL_log(double x);
typedef double SDLCALL tSDL_pow(double x, double y);
typedef double SDLCALL tSDL_scalbn(double x, int n);
typedef double SDLCALL tSDL_sin(double x);
typedef float SDLCALL tSDL_sinf(float x);
typedef double SDLCALL tSDL_sqrt(double x);

#define SDL_ICONV_ERROR     (size_t)-1
#define SDL_ICONV_E2BIG     (size_t)-2
#define SDL_ICONV_EILSEQ    (size_t)-3
#define SDL_ICONV_EINVAL    (size_t)-4

typedef struct _SDL_iconv_t *SDL_iconv_t;
typedef SDL_iconv_t SDLCALL tSDL_iconv_open(const char *tocode,
                                                   const char *fromcode);
typedef int SDLCALL tSDL_iconv_close(SDL_iconv_t cd);
typedef size_t SDLCALL tSDL_iconv(SDL_iconv_t cd, const char **inbuf,
                                         size_t * inbytesleft, char **outbuf,
                                         size_t * outbytesleft);

typedef char * SDLCALL tSDL_iconv_string(const char *tocode,
                                               const char *fromcode,
                                               const char *inbuf,
                                               size_t inbytesleft);
#define SDL_iconv_utf8_locale(S)    SDL_iconv_string("", "UTF-8", S, SDL_strlen(S)+1)
#define SDL_iconv_utf8_ucs2(S)      (Uint16 *)SDL_iconv_string("UCS-2-INTERNAL", "UTF-8", S, SDL_strlen(S)+1)
#define SDL_iconv_utf8_ucs4(S)      (Uint32 *)SDL_iconv_string("UCS-4-INTERNAL", "UTF-8", S, SDL_strlen(S)+1)

#ifndef HAVE_ALLOCA
extern tSDL_malloc *SDL_malloc;
#endif
extern tSDL_calloc *SDL_calloc;
extern tSDL_realloc *SDL_realloc;
extern tSDL_free *SDL_free;
extern tSDL_getenv *SDL_getenv;
extern tSDL_setenv *SDL_setenv;
extern tSDL_qsort *SDL_qsort;
extern tSDL_abs *SDL_abs;
extern tSDL_isdigit *SDL_isdigit;
extern tSDL_isspace *SDL_isspace;
extern tSDL_toupper *SDL_toupper;
extern tSDL_tolower *SDL_tolower;
extern tSDL_memset *SDL_memset;
extern tSDL_memmove *SDL_memmove;
extern tSDL_memcmp *SDL_memcmp;
extern tSDL_wcslen *SDL_wcslen;
extern tSDL_wcslcpy *SDL_wcslcpy;
extern tSDL_wcslcat *SDL_wcslcat;
extern tSDL_strlen *SDL_strlen;
extern tSDL_strlcpy *SDL_strlcpy;
extern tSDL_utf8strlcpy *SDL_utf8strlcpy;
extern tSDL_strlcat *SDL_strlcat;
extern tSDL_strdup *SDL_strdup;
extern tSDL_strrev *SDL_strrev;
extern tSDL_strupr *SDL_strupr;
extern tSDL_strlwr *SDL_strlwr;
extern tSDL_strchr *SDL_strchr;
extern tSDL_strrchr *SDL_strrchr;
extern tSDL_strstr *SDL_strstr;
extern tSDL_itoa *SDL_itoa;
extern tSDL_uitoa *SDL_uitoa;
extern tSDL_ltoa *SDL_ltoa;
extern tSDL_ultoa *SDL_ultoa;
extern tSDL_lltoa *SDL_lltoa;
extern tSDL_ulltoa *SDL_ulltoa;
extern tSDL_atoi *SDL_atoi;
extern tSDL_atof *SDL_atof;
extern tSDL_strtol *SDL_strtol;
extern tSDL_strtoll *SDL_strtoll;
extern tSDL_strtoull *SDL_strtoull;
extern tSDL_strtod *SDL_strtod;
extern tSDL_strcmp *SDL_strcmp;
extern tSDL_strncmp *SDL_strncmp;
extern tSDL_strcasecmp *SDL_strcasecmp;
extern tSDL_strncasecmp *SDL_strncasecmp;
extern tSDL_sscanf *SDL_sscanf;
extern tSDL_snprintf *SDL_snprintf;
extern tSDL_vsnprintf *SDL_vsnprintf;
extern tSDL_atan *SDL_atan;
extern tSDL_atan2 *SDL_atan2;
extern tSDL_ceil *SDL_ceil;
extern tSDL_copysign *SDL_copysign;
extern tSDL_cos *SDL_cos;
extern tSDL_cosf *SDL_cosf;
extern tSDL_fabs *SDL_fabs;
extern tSDL_floor *SDL_floor;
extern tSDL_log *SDL_log;
extern tSDL_pow *SDL_pow;
extern tSDL_scalbn *SDL_scalbn;
extern tSDL_sin *SDL_sin;
extern tSDL_sinf *SDL_sinf;
extern tSDL_sqrt *SDL_sqrt;
extern tSDL_iconv_open *SDL_iconv_open;
extern tSDL_iconv_close *SDL_iconv_close;
extern tSDL_iconv *SDL_iconv;
extern tSDL_iconv_string *SDL_iconv_string;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

