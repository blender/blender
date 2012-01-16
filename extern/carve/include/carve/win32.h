// Copyright 2006 Tobias Sargeant (toby@permuted.net)
// All rights reserved.
#pragma once

#pragma warning (disable : 4996)
#pragma warning (disable : 4786)

#include <string.h>
#include <stdlib.h>

inline int strcasecmp(const char *a, const char *b) {
  return _stricmp(a,b);
}

inline void srandom(unsigned long input) {
  srand(input);
}

inline long random() {
  return rand();
}

#if defined(_MSC_VER)
#  include <carve/cbrt.h>

#if _MSC_VER < 1300
// intptr_t is an integer type that is big enough to hold a pointer
// It is not defined in VC6 so include a definition here for the older compiler
typedef long intptr_t;
typedef unsigned long uintptr_t;
#endif

#  if _MSC_VER < 1600
// stdint.h is not available before VS2010
#if defined(_WIN32) && !defined(__MINGW32__)
/* The __intXX are built-in types of the visual complier! So we don't
   need to include anything else here.
   This typedefs should be in sync with types from MEM_sys_types.h */

typedef signed __int8  int8_t;
typedef signed __int16 int16_t;
typedef signed __int32 int32_t;

typedef unsigned __int8  uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
#endif
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#  else
#    include <stdint.h>
#  endif
#endif
