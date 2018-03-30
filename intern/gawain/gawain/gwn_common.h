
// Gawain common #defines and #includes
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#define PROGRAM_NO_OPTI 0

#if defined(NDEBUG)
  #define TRUST_NO_ONE 0
#else
  // strict error checking, enabled for debug builds during early development
  #define TRUST_NO_ONE 1
#endif

#include <GL/glew.h>
#include <stdbool.h>
#include <stdint.h>

#if TRUST_NO_ONE
  #include <assert.h>
#endif

/* GWN_INLINE */
#if defined(_MSC_VER)
#  define GWN_INLINE static __forceinline
#else
#  define GWN_INLINE static inline __attribute__((always_inline)) __attribute__((__unused__))
#endif