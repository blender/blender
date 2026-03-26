/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_ustring.hh"

#include "FN_multi_function.hh"

namespace blender::fn::multi_function::registry {

/**
 * Add a new multi-function to the registry. The #MultiFunction::name is used as identifier.
 * This multi-function is expected to have static storage duration.
 */
void add_new(const MultiFunction &fn);

/**
 * Utility to create a multi-function with static storage duration that is added to the registry.
 */
template<typename CreateFn> inline void add_new_cb(CreateFn &&create_fn)
{
  static auto fn = create_fn();
  registry::add_new(fn);
}

/**
 * Find the multi-function with the given identifier. The multi-function is expected to exist.
 */
const MultiFunction &lookup(UString id);

}  // namespace blender::fn::multi_function::registry
