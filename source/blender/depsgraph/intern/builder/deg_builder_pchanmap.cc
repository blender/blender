/* SPDX-FileCopyrightText: 2015 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder_pchanmap.h"

#include <cstdio>
#include <cstring>

#include "BLI_utildefines.h"

namespace blender::deg {

void RootPChanMap::print_debug()
{
  map_.foreach_item([](StringRefNull key, const Set<StringRefNull> &values) {
    printf("  %s : { ", key.data());
    for (StringRefNull val : values) {
      printf("%s, ", val.data());
    }
    printf("}\n");
  });
}

void RootPChanMap::add_bone(const char *bone, const char *root)
{
  map_.lookup_or_add_default(bone).add(root);
}

bool RootPChanMap::has_common_root(const char *bone1, const char *bone2) const
{
  const Set<StringRefNull> *bone1_roots = map_.lookup_ptr(bone1);
  const Set<StringRefNull> *bone2_roots = map_.lookup_ptr(bone2);

  if (bone1_roots == nullptr) {
    // fprintf("RootPChanMap: bone1 '%s' not found (%s => %s)\n", bone1, bone1, bone2);
    // print_debug();
    return false;
  }

  if (bone2_roots == nullptr) {
    // fprintf("RootPChanMap: bone2 '%s' not found (%s => %s)\n", bone2, bone1, bone2);
    // print_debug();
    return false;
  }

  return Set<StringRefNull>::Intersects(*bone1_roots, *bone2_roots);
}

}  // namespace blender::deg
