/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_kdtree.hh"

#include <cmath>

namespace blender {

/* -------------------------------------------------------------------- */
/* Tests */

static void standard_test()
{
  for (int tree_size = 30; tree_size < 500; tree_size++) {
    int tree_index = 0;
    KDTree<float> *tree = kdtree_new<float>(tree_size);
    int mask = tree_size & 31;
    bool occupied[32] = {false};

    for (int i = 0; i < tree_size; i++) {
      int index = i & mask;
      occupied[index] = true;
      float value = fmodf(index * 7.121f, 0.6037f); /* Co-prime. */
      kdtree_insert<float>(tree, tree_index++, value);
    }
    int expected = 0;
    for (int j = 0; j < 32; j++) {
      if (occupied[j]) {
        expected++;
      }
    }

    int dedup_count = kdtree_deduplicate<float>(tree);
    EXPECT_EQ(dedup_count, expected);
    kdtree_free<float>(tree);
  }
}

static void deduplicate_test()
{
  for (int tree_size = 1; tree_size < 40; tree_size++) {
    int tree_index = 0;
    KDTree<float> *tree = kdtree_new<float>(tree_size);
    for (int i = 0; i < tree_size; i++) {
      kdtree_insert<float>(tree, tree_index++, 1.0f);
    }
    int dedup_count = kdtree_deduplicate<float>(tree);
    EXPECT_EQ(dedup_count, 1);
    kdtree_free<float>(tree);
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

}  // namespace blender
