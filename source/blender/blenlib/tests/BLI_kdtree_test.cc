/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_kdtree.hh"

#include <cmath>

/* -------------------------------------------------------------------- */
/* Tests */

static void standard_test()
{
  for (int tree_size = 30; tree_size < 500; tree_size++) {
    int tree_index = 0;
    blender::KDTree_1d *tree = blender::kdtree_1d_new(tree_size);
    int mask = tree_size & 31;
    bool occupied[32] = {false};

    for (int i = 0; i < tree_size; i++) {
      int index = i & mask;
      occupied[index] = true;
      float value = fmodf(index * 7.121f, 0.6037f); /* Co-prime. */
      float key[1] = {value};
      blender::kdtree_1d_insert(tree, tree_index++, key);
    }
    int expected = 0;
    for (int j = 0; j < 32; j++) {
      if (occupied[j]) {
        expected++;
      }
    }

    int dedup_count = blender::kdtree_1d_deduplicate(tree);
    EXPECT_EQ(dedup_count, expected);
    blender::kdtree_1d_free(tree);
  }
}

static void deduplicate_test()
{
  for (int tree_size = 1; tree_size < 40; tree_size++) {
    int tree_index = 0;
    blender::KDTree_1d *tree = blender::kdtree_1d_new(tree_size);
    for (int i = 0; i < tree_size; i++) {
      float key[1] = {1.0f};
      blender::kdtree_1d_insert(tree, tree_index++, key);
    }
    int dedup_count = blender::kdtree_1d_deduplicate(tree);
    EXPECT_EQ(dedup_count, 1);
    blender::kdtree_1d_free(tree);
  }
}

TEST(kdtree, Standard)
{
  standard_test();
}

TEST(kdtree, Deduplicate)
{
  deduplicate_test();
}
