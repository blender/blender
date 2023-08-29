/* SPDX-FileCopyrightText: 2023 Blender Authors
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

#if defined(__cplusplus)
#  include <type_traits>
#  define BLI_array_alloca(arr, realsize) \
    (std::remove_reference_t<decltype(arr)>)alloca(sizeof(*arr) * (realsize))
#else
#  if defined(__GNUC__) || defined(__clang__)
#    define BLI_array_alloca(arr, realsize) (typeof(arr))alloca(sizeof(*arr) * (realsize))
#  else
#    define BLI_array_alloca(arr, realsize) alloca(sizeof(*arr) * (realsize))
#  endif
#endif
