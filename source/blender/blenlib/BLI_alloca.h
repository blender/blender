/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Defines alloca and utility macro BLI_array_alloca
 */

/* BLI_array_alloca / alloca */

#include <stdlib.h>

#if defined(__GNUC__) || defined(__clang__)
#  if defined(__cplusplus) && (__cplusplus > 199711L)
#    define BLI_array_alloca(arr, realsize) (decltype(arr))alloca(sizeof(*arr) * (realsize))
#  else
#    define BLI_array_alloca(arr, realsize) (typeof(arr))alloca(sizeof(*arr) * (realsize))
#  endif
#else
#  define BLI_array_alloca(arr, realsize) alloca(sizeof(*arr) * (realsize))
#endif
