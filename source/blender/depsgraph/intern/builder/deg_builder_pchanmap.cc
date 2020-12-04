/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder_pchanmap.h"

#include <cstdio>
#include <cstring>

#include "BLI_utildefines.h"

namespace blender::deg {

RootPChanMap::RootPChanMap()
{
}

RootPChanMap::~RootPChanMap()
{
}

/* Debug contents of map */
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

/* Add a mapping. */
void RootPChanMap::add_bone(const char *bone, const char *root)
{
  map_.lookup_or_add_default(bone).add(root);
}

/* Check if there's a common root bone between two bones. */
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
