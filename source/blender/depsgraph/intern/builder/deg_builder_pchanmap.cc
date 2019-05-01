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

#include <stdio.h>
#include <string.h>

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

namespace DEG {

static void free_rootpchanmap_valueset(void *val)
{
  /* Just need to free the set itself - the names stored are all references. */
  GSet *values = (GSet *)val;
  BLI_gset_free(values, NULL);
}

RootPChanMap::RootPChanMap()
{
  /* Just create empty map. */
  map_ = BLI_ghash_str_new("RootPChanMap");
}

RootPChanMap::~RootPChanMap()
{
  /* Free the map, and all the value sets. */
  BLI_ghash_free(map_, NULL, free_rootpchanmap_valueset);
}

/* Debug contents of map */
void RootPChanMap::print_debug()
{
  GHashIterator it1;
  GSetIterator it2;

  printf("Root PChan Map:\n");
  GHASH_ITER (it1, map_) {
    const char *item = (const char *)BLI_ghashIterator_getKey(&it1);
    GSet *values = (GSet *)BLI_ghashIterator_getValue(&it1);

    printf("  %s : { ", item);
    GSET_ITER (it2, values) {
      const char *val = (const char *)BLI_gsetIterator_getKey(&it2);
      printf("%s, ", val);
    }
    printf("}\n");
  }
}

/* Add a mapping. */
void RootPChanMap::add_bone(const char *bone, const char *root)
{
  if (BLI_ghash_haskey(map_, bone)) {
    /* Add new entry, but only add the root if it doesn't already
     * exist in there. */
    GSet *values = (GSet *)BLI_ghash_lookup(map_, bone);
    BLI_gset_add(values, (void *)root);
  }
  else {
    /* Create new set and mapping. */
    GSet *values = BLI_gset_new(
        BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, "RootPChanMap Value Set");
    BLI_ghash_insert(map_, (void *)bone, (void *)values);

    /* Add new entry now. */
    BLI_gset_insert(values, (void *)root);
  }
}

/* Check if there's a common root bone between two bones. */
bool RootPChanMap::has_common_root(const char *bone1, const char *bone2)
{
  /* Ensure that both are in the map... */
  if (BLI_ghash_haskey(map_, bone1) == false) {
    // fprintf("RootPChanMap: bone1 '%s' not found (%s => %s)\n", bone1, bone1, bone2);
    // print_debug();
    return false;
  }

  if (BLI_ghash_haskey(map_, bone2) == false) {
    // fprintf("RootPChanMap: bone2 '%s' not found (%s => %s)\n", bone2, bone1, bone2);
    // print_debug();
    return false;
  }

  GSet *bone1_roots = (GSet *)BLI_ghash_lookup(map_, (void *)bone1);
  GSet *bone2_roots = (GSet *)BLI_ghash_lookup(map_, (void *)bone2);

  GSetIterator it1, it2;
  GSET_ITER (it1, bone1_roots) {
    GSET_ITER (it2, bone2_roots) {
      const char *v1 = (const char *)BLI_gsetIterator_getKey(&it1);
      const char *v2 = (const char *)BLI_gsetIterator_getKey(&it2);

      if (strcmp(v1, v2) == 0) {
        // fprintf("RootPchanMap: %s in common for %s => %s\n", v1, bone1, bone2);
        return true;
      }
    }
  }

  // fprintf("RootPChanMap: No common root found (%s => %s)\n", bone1, bone2);
  return false;
}

}  // namespace DEG
