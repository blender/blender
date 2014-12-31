/* Generated from config.h.in during build configuration using CMake. */

// Note: This header file is only used internally. It is not part of public interface!

// ---------------------------------------------------------------------------
// System checks

// Define if you build this library for a MS Windows OS.
#ifdef WIN32
#  define OS_WINDOWS
#endif

// Define if you have the <stdint.h> header file.
#define HAVE_STDINT_H

// Define if you have the <sys/types.h> header file.
#define HAVE_SYS_TYPES_H

// Define if you have the <inttypes.h> header file.
#define HAVE_INTTYPES_H

// Define if you have the <sys/stat.h> header file.
#define HAVE_SYS_STAT_H

// Define if you have the <unistd.h> header file.
#define HAVE_UNISTD_H

// Define if you have the <fnmatch.h> header file.
/* #undef HAVE_FNMATCH_H */

// Define if you have the <shlwapi.h> header file (Windows 2000/XP).
#ifdef WIN32
#  define HAVE_SHLWAPI_H
#endif

// Define if you have the strtoll function.
#define HAVE_STRTOLL

// Define if you have the strtoq function.
/* #undef HAVE_STRTOQ */

// Define if you have the <pthread.h> header file.
#define HAVE_PTHREAD

// Define if your pthread library defines the type pthread_rwlock_t
#define HAVE_RWLOCK

// gcc requires this to get PRId64, etc.
#if defined(HAVE_INTTYPES_H) && !defined(__STDC_FORMAT_MACROS)
#  define __STDC_FORMAT_MACROS 1
#endif

// ---------------------------------------------------------------------------
// Package information

// Name of package.
#define PACKAGE gflags

// Define to the full name of this package.
#define PACKAGE_NAME gflags

// Define to the full name and version of this package.
#define PACKAGE_STRING gflags 2.1.1

// Define to the one symbol short name of this package.
#define PACKAGE_TARNAME gflags-2.1.1

// Define to the version of this package.
#define PACKAGE_VERSION 2.1.1

// Version number of package.
#define VERSION PACKAGE_VERSION

// Define to the address where bug reports for this package should be sent.
#define PACKAGE_BUGREPORT https://code.google.com/p/gflags/issues/

// Namespace of gflags library symbols.
#define GFLAGS_NAMESPACE gflags

// ---------------------------------------------------------------------------
// Path separator
#ifndef PATH_SEPARATOR
#  ifdef OS_WINDOWS
#    define PATH_SEPARATOR  '\\'
#  else
#    define PATH_SEPARATOR  '/'
#  endif
#endif

// ---------------------------------------------------------------------------
// Windows

// Whether gflags library is a DLL.
#ifndef GFLAGS_IS_A_DLL
#  define GFLAGS_IS_A_DLL 0
#endif

// Always export symbols when compiling a shared library as this file is only
// included by internal modules when building the gflags library itself.
// The gflags_declare.h header file will set it to import these symbols otherwise.
#ifndef GFLAGS_DLL_DECL
#  if GFLAGS_IS_A_DLL && defined(_MSC_VER)
#    define GFLAGS_DLL_DECL __declspec(dllexport)
#  else
#    define GFLAGS_DLL_DECL
#  endif
#endif
// Flags defined by the gflags library itself must be exported
#ifndef GFLAGS_DLL_DEFINE_FLAG
#  define GFLAGS_DLL_DEFINE_FLAG GFLAGS_DLL_DECL
#endif

#ifdef OS_WINDOWS
// The unittests import the symbols of the shared gflags library
#  if GFLAGS_IS_A_DLL && defined(_MSC_VER)
#    define GFLAGS_DLL_DECL_FOR_UNITTESTS __declspec(dllimport)
#  endif
#  include "windows_port.h"
#endif
