
#ifndef _SDL_platform_h
#define _SDL_platform_h

#if defined(_AIX)
#undef __AIX__
#define __AIX__     1
#endif
#if defined(__BEOS__)
#undef __BEOS__
#define __BEOS__    1
#endif
#if defined(__HAIKU__)
#undef __HAIKU__
#define __HAIKU__   1
#endif
#if defined(bsdi) || defined(__bsdi) || defined(__bsdi__)
#undef __BSDI__
#define __BSDI__    1
#endif
#if defined(_arch_dreamcast)
#undef __DREAMCAST__
#define __DREAMCAST__   1
#endif
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
#undef __FREEBSD__
#define __FREEBSD__ 1
#endif
#if defined(hpux) || defined(__hpux) || defined(__hpux__)
#undef __HPUX__
#define __HPUX__    1
#endif
#if defined(sgi) || defined(__sgi) || defined(__sgi__) || defined(_SGI_SOURCE)
#undef __IRIX__
#define __IRIX__    1
#endif
#if defined(linux) || defined(__linux) || defined(__linux__)
#undef __LINUX__
#define __LINUX__   1
#endif
#if defined(ANDROID)
#undef __ANDROID__
#undef __LINUX__
#define __ANDROID__ 1
#endif

#if defined(__APPLE__)

#include "AvailabilityMacros.h"
#include "TargetConditionals.h"
#if TARGET_OS_IPHONE

#undef __IPHONEOS__
#define __IPHONEOS__ 1
#undef __MACOSX__
#else

#undef __MACOSX__
#define __MACOSX__  1
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050
# error SDL for Mac OS X only supports deploying on 10.5 and above.
#endif
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1060
# error SDL for Mac OS X must be built with a 10.6 SDK or above.
#endif
#endif
#endif

#if defined(__NetBSD__)
#undef __NETBSD__
#define __NETBSD__  1
#endif
#if defined(__OpenBSD__)
#undef __OPENBSD__
#define __OPENBSD__ 1
#endif
#if defined(__OS2__)
#undef __OS2__
#define __OS2__     1
#endif
#if defined(osf) || defined(__osf) || defined(__osf__) || defined(_OSF_SOURCE)
#undef __OSF__
#define __OSF__     1
#endif
#if defined(__QNXNTO__)
#undef __QNXNTO__
#define __QNXNTO__  1
#endif
#if defined(riscos) || defined(__riscos) || defined(__riscos__)
#undef __RISCOS__
#define __RISCOS__  1
#endif
#if defined(__SVR4)
#undef __SOLARIS__
#define __SOLARIS__ 1
#endif
#if defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__)
#undef __WIN32__
#define __WIN32__   1
#endif
#if defined(__PSP__)
#undef __PSP__
#define __PSP__ 1
#endif

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef const char * SDLCALL tSDL_GetPlatform (void);

extern tSDL_GetPlatform *SDL_GetPlatform;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

