/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_utildefines.h"

namespace blender {

template<class Fn, size_t... I> void unroll_impl(Fn fn, std::index_sequence<I...> /*indices*/)
{
  (fn(I), ...);
}

/**
 * Variadic templates are used to unroll loops manually. This helps GCC avoid branching during math
 * operations and makes the code generation more explicit and predictable. Unrolling should always
 * be worth it because the vector size is expected to be small.
 */
template<int N, class Fn> void unroll(Fn fn)
{
  unroll_impl(fn, std::make_index_sequence<N>());
}

}  // namespace blender
