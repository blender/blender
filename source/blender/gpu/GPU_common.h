/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#define PROGRAM_NO_OPTI 0
// #define GPU_NO_USE_PY_REFERENCES

#if defined(NDEBUG)
#  define TRUST_NO_ONE 0
#else
/* strict error checking, enabled for debug builds during early development */
#  define TRUST_NO_ONE 1
#endif

#include "BLI_sys_types.h"
#include <stdbool.h>
#include <stdint.h>

#if TRUST_NO_ONE
#  include <assert.h>
#endif

/* GPU_INLINE */
#if defined(_MSC_VER)
#  define GPU_INLINE static __forceinline
#else
#  define GPU_INLINE static inline __attribute__((always_inline)) __attribute__((__unused__))
#endif
