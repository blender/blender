/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_concurrent_map.hh"

#include "FN_multi_function_registry.hh"

namespace blender::fn::multi_function::registry {

using RegistryMap = ConcurrentMap<UString, const MultiFunction *>;

struct Registry {
  RegistryMap map;
};

static Registry &get_registry()
{
  static Registry registry;
  return registry;
}

void add_new(const MultiFunction &fn)
{
  Registry &registry = get_registry();
  RegistryMap::MutableAccessor accessor;
  const UString id = UString(fn.name());
  if (registry.map.add(accessor, id)) {
    accessor->second = &fn;
  }
  else {
    /* A function can only be registered once. */
    BLI_assert_unreachable();
  }
}

const MultiFunction &lookup(UString id)
{
  Registry &registry = get_registry();
  RegistryMap::ConstAccessor accessor;
  if (registry.map.lookup(accessor, id)) {
    return *accessor->second;
  }
  /* The function is expected to exist when using the #lookup function. */
  BLI_assert_unreachable();
  return *accessor->second;
}

}  // namespace blender::fn::multi_function::registry
