/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

/* TODO: ray intersection, overlap ... etc. */

#include "MEM_guardedalloc.h"

#include "BLI_compiler_attrs.h"
#include "BLI_kdopbvh.hh"
#include "BLI_math_vector.h"
#include "BLI_rand.h"

/* -------------------------------------------------------------------- */
/* Helper Functions */

static void rng_v3_round(float *coords, int coords_len, RNG *rng, int round, float scale)
{
  for (int i = 0; i < coords_len; i++) {
    float f = BLI_rng_get_float(rng) * 2.0f - 1.0f;
    coords[i] = (float(int(f * round)) / float(round)) * scale;
  }
}

/* -------------------------------------------------------------------- */
/* Tests */

TEST(kdopbvh, Empty)
{
  BVHTree *tree = BLI_bvhtree_new(0, 0.0, 8, 8);
  BLI_bvhtree_balance(tree);
  EXPECT_EQ(0, BLI_bvhtree_get_len(tree));
  BLI_bvhtree_free(tree);
}

TEST(kdopbvh, Single)
{
  BVHTree *tree = BLI_bvhtree_new(1, 0.0, 8, 8);
  {
    float co[3] = {0};
    BLI_bvhtree_insert(tree, 0, co, 1);
  }

  EXPECT_EQ(BLI_bvhtree_get_len(tree), 1);

  BLI_bvhtree_balance(tree);
  BLI_bvhtree_free(tree);
}

static void optimal_check_callback(void *userdata,
                                   int index,
                                   const float co[3],
                                   BVHTreeNearest *nearest)
{
  float (*points)[3] = (float (*)[3])userdata;

  /* BVH_NEAREST_OPTIMAL_ORDER should hit the right node on the first try */
  EXPECT_EQ(nearest->index, -1);
  EXPECT_EQ_ARRAY(co, points[index], 3);

  nearest->index = index;
  nearest->dist_sq = len_squared_v3v3(co, points[index]);
}

/**
 * Note that a small epsilon is added to the BVH nodes bounds, even if we pass in zero.
 * Use rounding to ensure very close nodes don't cause the wrong node to be found as nearest.
 */
static void find_nearest_points_test(
    int points_len, float scale, int round, int random_seed, bool optimal = false)
{
  RNG *rng = BLI_rng_new(random_seed);
  BVHTree *tree = BLI_bvhtree_new(points_len, 0.0, 8, 8);

  void *mem = MEM_malloc_arrayN<float[3]>(size_t(points_len), __func__);
  float (*points)[3] = (float (*)[3])mem;

  for (int i = 0; i < points_len; i++) {
    rng_v3_round(points[i], 3, rng, round, scale);
    BLI_bvhtree_insert(tree, i, points[i], 1);
  }
  BLI_bvhtree_balance(tree);

  /* first find each point */
  BVHTree_NearestPointCallback callback = optimal ? optimal_check_callback : nullptr;
  int flags = optimal ? BVH_NEAREST_OPTIMAL_ORDER : 0;

  for (int i = 0; i < points_len; i++) {
    const int j = BLI_bvhtree_find_nearest_ex(tree, points[i], nullptr, callback, points, flags);
    if (j != i) {
#if 0
      const float dist = len_v3v3(points[i], points[j]);
      if (dist > (1.0f / float(round))) {
        printf("%.15f (%d %d)\n", dist, i, j);
        print_v3_id(points[i]);
        print_v3_id(points[j]);
        fflush(stdout);
      }
#endif
      EXPECT_GE(j, 0);
      EXPECT_LT(j, points_len);
      EXPECT_EQ_ARRAY(points[i], points[j], 3);
    }
  }
  BLI_bvhtree_free(tree);
  BLI_rng_free(rng);
  MEM_freeN(points);
}

TEST(kdopbvh, FindNearest_1)
{
  find_nearest_points_test(1, 1.0, 1000, 1234);
}
TEST(kdopbvh, FindNearest_2)
{
  find_nearest_points_test(2, 1.0, 1000, 123);
}
TEST(kdopbvh, FindNearest_500)
{
  find_nearest_points_test(500, 1.0, 1000, 12);
}

TEST(kdopbvh, OptimalFindNearest_1)
{
  find_nearest_points_test(1, 1.0, 1000, 1234, true);
}
TEST(kdopbvh, OptimalFindNearest_2)
{
  find_nearest_points_test(2, 1.0, 1000, 123, true);
}
TEST(kdopbvh, OptimalFindNearest_500)
{
  find_nearest_points_test(500, 1.0, 1000, 12, true);
}
