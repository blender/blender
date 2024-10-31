/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#define PROGRAM_NO_OPTI 0
// #define GPU_NO_USE_PY_REFERENCES

#include "BLI_sys_types.h"

/* GPU_INLINE */
#if defined(_MSC_VER)
#  define GPU_INLINE static __forceinline
#else
#  define GPU_INLINE static inline __attribute__((always_inline)) __attribute__((__unused__))
#endif
