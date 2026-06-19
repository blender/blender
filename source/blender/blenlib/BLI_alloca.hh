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

#include <cstdlib>
#include <type_traits> /* IWYU pragma: keep */

#define BLI_array_alloca(arr, realsize) \
  (std::remove_reference_t<decltype(arr)>)alloca(sizeof(*arr) * (realsize))
