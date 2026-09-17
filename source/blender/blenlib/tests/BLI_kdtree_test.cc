/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <algorithm>
#include <string>

#include "testing/testing.h"

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_kdtree_new.hh"
#include "BLI_math_vector.hh"
#include "BLI_rand.hh"
#include "BLI_task_c.hh"
#include "BLI_threads.hh"
#include "BLI_vector.hh"

namespace blender::tests {

using Nearest = KDTreeNew<float3>::Nearest;

static Array<float3> random_points(const int points_num, const int seed = 0)
{
  RandomNumberGenerator random(seed);
  Array<float3> positions(points_num);
  for (const int i : positions.index_range()) {
    positions[i] = float3(random.get_float(), random.get_float(), random.get_float());
  }
  return positions;
}

/** Reference implementation used to check the results of the tree. */
static Vector<Nearest> brute_force_sorted(const Span<float3> positions,
                                          const Span<int> indices,
                                          const float3 &position)
{
  Vector<Nearest> result;
  for (const int i : indices) {
    result.append({i, math::distance_squared(positions[i], position)});
  }
  std::ranges::sort(result, [](const Nearest &a, const Nearest &b) {
    return a.distance_sq < b.distance_sq || (a.distance_sq == b.distance_sq && a.index < b.index);
  });
  return result;
}

/** A grid of unit spaced points, so that distances land exactly on whole numbers. */
static Array<float3> grid_points(const int side)
{
  Array<float3> positions(side * side * side);
  int i = 0;
  for (const int x : IndexRange(side)) {
    for (const int y : IndexRange(side)) {
      for (const int z : IndexRange(side)) {
        positions[i++] = float3(float(x), float(y), float(z));
      }
    }
  }
  return positions;
}

static Vector<int> all_indices(const int points_num)
{
  Vector<int> indices(points_num);
  array_utils::fill_index_range(indices.as_mutable_span());
  return indices;
}

TEST(kdtree, Empty)
{
  const Array<float3> positions(0);
  const KDTreeNew<float3> tree(positions);
  EXPECT_TRUE(tree.is_empty());
  EXPECT_EQ(tree.size(), 0);
  EXPECT_EQ(tree.find_nearest(float3(0.0f)), -1);
  EXPECT_EQ(tree.count_in_radius(float3(0.0f), 100.0f), 0);

  Array<Nearest> nearest(4);
  EXPECT_EQ(tree.find_nearest_n(float3(0.0f), nearest), 0);

  Vector<Nearest> in_radius;
  EXPECT_EQ(tree.find_in_radius(float3(0.0f), 100.0f, in_radius), 0);
}

TEST(kdtree, SinglePoint)
{
  const Array<float3> positions = {float3(1.0f, 2.0f, 3.0f)};
  const KDTreeNew<float3> tree(positions);
  EXPECT_EQ(tree.size(), 1);

  float distance_sq = -1.0f;
  EXPECT_EQ(tree.find_nearest(float3(1.0f, 2.0f, 4.0f), &distance_sq), 0);
  EXPECT_FLOAT_EQ(distance_sq, 1.0f);

  /* The only point is filtered out. */
  EXPECT_EQ(tree.find_nearest_filtered(float3(0.0f), [](const int /*index*/) { return false; }),
            -1);
}

TEST(kdtree, FindNearest)
{
  const Array<float3> positions = random_points(1000);
  const KDTreeNew<float3> tree(positions);
  EXPECT_EQ(tree.size(), 1000);

  const Vector<int> indices = all_indices(positions.size());
  const Array<float3> queries = random_points(50, 1);
  for (const float3 &query : queries) {
    const Vector<Nearest> expected = brute_force_sorted(positions, indices, query);
    float distance_sq = -1.0f;
    const int index = tree.find_nearest(query, &distance_sq);
    EXPECT_EQ(index, expected.first().index);
    EXPECT_FLOAT_EQ(distance_sq, expected.first().distance_sq);
  }
}

TEST(kdtree, FindNearestFiltered)
{
  const Array<float3> positions = random_points(500);
  const KDTreeNew<float3> tree(positions);
  const Vector<int> indices = all_indices(positions.size());

  /* Skipping the search point itself is the common use of the filter. */
  for (const int i : positions.index_range()) {
    const Vector<Nearest> expected = brute_force_sorted(positions, indices, positions[i]);
    ASSERT_EQ(expected.first().index, i);
    const int index = tree.find_nearest_filtered(positions[i],
                                                 [&](const int other) { return other != i; });
    EXPECT_EQ(index, expected[1].index);
  }

  /* Only even indices are accepted. */
  const int index = tree.find_nearest_filtered(positions[3],
                                               [](const int other) { return other % 2 == 0; });
  ASSERT_GE(index, 0);
  EXPECT_EQ(index % 2, 0);
  const Vector<Nearest> expected = brute_force_sorted(positions, indices, positions[3]);
  for (const Nearest &nearest : expected) {
    if (nearest.index % 2 == 0) {
      EXPECT_EQ(nearest.index, index);
      break;
    }
  }
}

TEST(kdtree, FindNearestN)
{
  const Array<float3> positions = random_points(400);
  const KDTreeNew<float3> tree(positions);
  const Vector<int> indices = all_indices(positions.size());
  const Array<float3> queries = random_points(20, 2);

  for (const float3 &query : queries) {
    const Vector<Nearest> expected = brute_force_sorted(positions, indices, query);
    Array<Nearest> nearest(7);
    const int found = tree.find_nearest_n(query, nearest);
    ASSERT_EQ(found, 7);
    for (const int i : IndexRange(found)) {
      EXPECT_EQ(nearest[i].index, expected[i].index);
      EXPECT_FLOAT_EQ(nearest[i].distance_sq, expected[i].distance_sq);
      if (i > 0) {
        EXPECT_LE(nearest[i - 1].distance_sq, nearest[i].distance_sq);
      }
    }
  }
}

TEST(kdtree, FindNearestNMoreThanSize)
{
  const Array<float3> positions = random_points(5);
  const KDTreeNew<float3> tree(positions);
  Array<Nearest> nearest(10);
  EXPECT_EQ(tree.find_nearest_n(float3(0.5f), nearest), 5);
}

TEST(kdtree, Radius)
{
  const Array<float3> positions = random_points(600);
  const KDTreeNew<float3> tree(positions);
  const Vector<int> indices = all_indices(positions.size());
  const Array<float3> queries = random_points(20, 3);

  for (const float3 &query : queries) {
    for (const float radius : {0.05f, 0.2f, 2.0f}) {
      Vector<Nearest> expected;
      for (const Nearest &nearest : brute_force_sorted(positions, indices, query)) {
        if (nearest.distance_sq <= radius * radius) {
          expected.append(nearest);
        }
      }

      Vector<Nearest> found;
      EXPECT_EQ(tree.find_in_radius(query, radius, found), expected.size());
      for (const int i : expected.index_range()) {
        EXPECT_EQ(found[i].index, expected[i].index);
        EXPECT_FLOAT_EQ(found[i].distance_sq, expected[i].distance_sq);
      }

      EXPECT_EQ(tree.count_in_radius(query, radius), expected.size());

      Vector<int> visited;
      tree.foreach_in_radius(query, radius, [&](const int index, const float distance_sq) {
        EXPECT_FLOAT_EQ(distance_sq, math::distance_squared(positions[index], query));
        visited.append(index);
      });
      std::ranges::sort(visited);
      ASSERT_EQ(visited.size(), expected.size());
      Vector<int> expected_indices;
      for (const Nearest &nearest : expected) {
        expected_indices.append(nearest.index);
      }
      std::ranges::sort(expected_indices);
      EXPECT_EQ(visited, expected_indices);
    }
  }
}

TEST(kdtree, RadiusIsInclusive)
{
  /* A point exactly the radius away is inside it, matching #blender::kdtree_range_search_cb. */
  const Array<float3> positions = grid_points(3);
  const KDTreeNew<float3> tree(positions);
  const float3 center(1.0f, 1.0f, 1.0f);

  /* Only the point on the center itself. */
  EXPECT_EQ(tree.count_in_radius(center, 0.0f), 1);
  /* The center and the six points one unit away along an axis. */
  EXPECT_EQ(tree.count_in_radius(center, 1.0f), 7);
  /* Every point, the furthest being a corner a bit over 1.7 away. */
  EXPECT_EQ(tree.count_in_radius(center, 2.0f), 27);
  /* A radius just short of one leaves everything but the center out. */
  EXPECT_EQ(tree.count_in_radius(center, std::nextafter(1.0f, 0.0f)), 1);

  Vector<Nearest> found;
  EXPECT_EQ(tree.find_in_radius(center, 1.0f, found), 7);
  for (const Nearest &nearest : found.as_span().drop_front(1)) {
    EXPECT_FLOAT_EQ(nearest.distance_sq, 1.0f);
  }
}

TEST(kdtree, FindNearestWithEqualDistances)
{
  /* Points equally far away never displace the one already found, however the search is pruned. */
  const Array<float3> positions = grid_points(3);
  const KDTreeNew<float3> tree(positions);

  float distance_sq;
  const int nearest = tree.find_nearest(float3(1.5f, 1.0f, 1.0f), &distance_sq);
  EXPECT_FLOAT_EQ(distance_sq, 0.25f);
  /* Two points are exactly as close, so it must be one of them. */
  EXPECT_TRUE(nearest == 4 || nearest == 13) << "found point " << nearest;

  Array<Nearest> nearest_n(2);
  EXPECT_EQ(tree.find_nearest_n(float3(1.5f, 1.0f, 1.0f), nearest_n), 2);
  EXPECT_FLOAT_EQ(nearest_n[0].distance_sq, 0.25f);
  EXPECT_FLOAT_EQ(nearest_n[1].distance_sq, 0.25f);
  EXPECT_NE(nearest_n[0].index, nearest_n[1].index);
}

TEST(kdtree, MaskedBuild)
{
  const Array<float3> positions = random_points(300);
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_predicate(
      positions.index_range(), memory, [](const int64_t i) { return i % 3 == 0; });
  const KDTreeNew<float3> tree(positions, mask);
  EXPECT_EQ(tree.size(), mask.size());

  const Vector<int> indices = mask.to_indices<int>();
  const Array<float3> queries = random_points(20, 4);
  for (const float3 &query : queries) {
    const Vector<Nearest> expected = brute_force_sorted(positions, indices, query);

    /* Results are indices into the original array, not positions in the mask. */
    float distance_sq = -1.0f;
    const int index = tree.find_nearest(query, &distance_sq);
    EXPECT_EQ(index, expected.first().index);
    EXPECT_EQ(index % 3, 0);
    EXPECT_FLOAT_EQ(distance_sq, expected.first().distance_sq);

    Array<Nearest> nearest(4);
    ASSERT_EQ(tree.find_nearest_n(query, nearest), 4);
    for (const int i : IndexRange(4)) {
      EXPECT_EQ(nearest[i].index, expected[i].index);
    }

    Vector<Nearest> found;
    tree.find_in_radius(query, 0.3f, found);
    for (const Nearest &nearest : found) {
      EXPECT_EQ(nearest.index % 3, 0);
      EXPECT_LE(nearest.distance_sq, 0.3f * 0.3f);
    }
  }

  const int filtered = tree.find_nearest_filtered(positions[0],
                                                  [](const int index) { return index != 0; });
  EXPECT_NE(filtered, 0);
  EXPECT_EQ(filtered % 3, 0);
}

TEST(kdtree, MaskedBuildFullRange)
{
  const Array<float3> positions = random_points(100);
  const KDTreeNew<float3> tree(positions, IndexMask(positions.index_range()));
  EXPECT_EQ(tree.size(), 100);
  EXPECT_EQ(tree.find_nearest(positions[42]), 42);
}

TEST(kdtree, EmptyMask)
{
  const Array<float3> positions = random_points(100);
  const KDTreeNew<float3> tree(positions, IndexMask(0));
  EXPECT_TRUE(tree.is_empty());
  EXPECT_EQ(tree.find_nearest(positions[0]), -1);
}

TEST(kdtree, DuplicatePoints)
{
  /* Many points at the same position is a common degenerate case. */
  Array<float3> positions(100);
  for (const int i : positions.index_range()) {
    positions[i] = i < 60 ? float3(1.0f, 1.0f, 1.0f) : float3(float(i), 0.0f, 0.0f);
  }
  const KDTreeNew<float3> tree(positions);

  EXPECT_LT(tree.find_nearest(float3(1.0f, 1.0f, 1.0f)), 60);
  EXPECT_EQ(tree.count_in_radius(float3(1.0f, 1.0f, 1.0f), 0.001f), 60);

  Array<Nearest> nearest(70);
  EXPECT_EQ(tree.find_nearest_n(float3(1.0f, 1.0f, 1.0f), nearest), 70);
  for (const int i : IndexRange(60)) {
    EXPECT_LT(nearest[i].index, 60);
    EXPECT_FLOAT_EQ(nearest[i].distance_sq, 0.0f);
  }

  Vector<Nearest> found;
  EXPECT_EQ(tree.find_in_radius(float3(1.0f, 1.0f, 1.0f), 0.5f, found), 60);
  /* Equal distances are broken by index, so the order is deterministic. */
  for (const int i : IndexRange(60)) {
    EXPECT_EQ(found[i].index, i);
  }
}

/**
 * Which point of a cluster the others merge into depends on the order the tree happens to store
 * its points in, which is not defined. These are the properties that hold whatever that order is.
 *
 * \param indices: The points in the tree.
 * \param pairwise: Also check that no two points in range were both left unmerged, which is
 * quadratic and so only worth doing for small point counts.
 */
static void check_duplicates(const Span<float3> positions,
                             const Span<int> indices,
                             const float range,
                             const Span<int> duplicates,
                             const int found,
                             const bool pairwise = true)
{
  const float range_sq = range * range;

  int merged_num = 0;
  for (const int i : indices) {
    const int target = duplicates[i];
    if (target == -1 || target == i) {
      continue;
    }
    merged_num++;
    /* A point is only ever merged into a point it is actually within the range of. */
    EXPECT_LE(math::distance_squared(positions[i], positions[target]), range_sq)
        << "point " << i << " merged into " << target << " beyond the range";
    /* Merging is a single step, so what a point merged into is never merged itself. */
    EXPECT_EQ(duplicates[target], target) << "point " << i << " merged into a merged point";
  }
  EXPECT_EQ(found, merged_num);

  if (!pairwise) {
    return;
  }
  /* Two points within the range of each other can never both be left out of a cluster: whichever
   * of them the tree visits first claims the other. */
  for (const int i : indices) {
    if (duplicates[i] != -1) {
      continue;
    }
    for (const int j : indices) {
      if (j == i || duplicates[j] != -1) {
        continue;
      }
      EXPECT_GT(math::distance_squared(positions[i], positions[j]), range_sq)
          << "points " << i << " and " << j << " are in range but neither was merged";
    }
  }
}

TEST(kdtree, CalcDuplicates)
{
  for (const int points_num : {10, 100, 2000}) {
    const Array<float3> positions = random_points(points_num, 5);
    const Vector<int> indices = all_indices(points_num);
    /* From "no point is close enough" to "everything is one cluster". */
    for (const float range : {0.001f, 0.02f, 0.05f, 0.2f, 5.0f}) {
      const KDTreeNew<float3> tree(positions);
      Array<int> duplicates(points_num, -1);
      const int found = kdtree::calc_duplicates(tree, range, duplicates);
      SCOPED_TRACE(std::to_string(points_num) + " points, range " + std::to_string(range));
      check_duplicates(positions, indices, range, duplicates, found);
    }
  }
}

TEST(kdtree, CalcDuplicatesWithDuplicatePoints)
{
  /* Exactly equal coordinates are the case this is used for in the first place. Ten groups of
   * twenty points, each group a unit apart, so the clusters are the same whatever the order. */
  Array<float3> positions(200);
  for (const int i : positions.index_range()) {
    positions[i] = float3(float(i % 10), 0.0f, 0.0f);
  }
  const Vector<int> indices = all_indices(positions.size());

  const KDTreeNew<float3> tree(positions);
  Array<int> duplicates(positions.size(), -1);
  const int found = kdtree::calc_duplicates(tree, 0.1f, duplicates);
  check_duplicates(positions, indices, 0.1f, duplicates, found);

  /* Every point ends up in the cluster of the points that share its coordinates, and each of the
   * ten clusters has exactly one target that the other nineteen merged into. */
  int targets_num = 0;
  for (const int i : positions.index_range()) {
    ASSERT_NE(duplicates[i], -1) << "point " << i << " was left out of a cluster";
    EXPECT_EQ(duplicates[i] % 10, i % 10);
    targets_num += duplicates[i] == i;
  }
  EXPECT_EQ(targets_num, 10);
  EXPECT_EQ(found, 190);
}

TEST(kdtree, CalcDuplicatesExactRange)
{
  /* The range is inclusive, so points exactly that far apart are merged. A zero range still
   * merges the points that share a position exactly. */
  Array<float3> positions(60);
  for (const int i : positions.index_range()) {
    positions[i] = float3(float(i % 3), 0.0f, 0.0f);
  }
  const Vector<int> indices = all_indices(positions.size());
  const KDTreeNew<float3> tree(positions);

  Array<int> duplicates(positions.size(), -1);
  EXPECT_EQ(kdtree::calc_duplicates(tree, 0.0f, duplicates), 57);
  check_duplicates(positions, indices, 0.0f, duplicates, 57);

  /* At exactly the spacing of the groups, the first point claims the first two of them, and the
   * first point of the third group claims what is left. */
  duplicates.fill(-1);
  EXPECT_EQ(kdtree::calc_duplicates(tree, 1.0f, duplicates), 58);
  check_duplicates(positions, indices, 1.0f, duplicates, 58);
  EXPECT_EQ(duplicates[0], 0);
  EXPECT_EQ(duplicates[1], 0);
  EXPECT_EQ(duplicates[2], 2);
}

TEST(kdtree, CalcDuplicatesMasked)
{
  const Array<float3> positions = random_points(500, 6);
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_predicate(
      positions.index_range(), memory, [](const int64_t i) { return i % 4 != 0; });
  const Vector<int> indices = mask.to_indices<int>();

  const KDTreeNew<float3> tree(positions, mask);
  Array<int> duplicates(positions.size(), -1);
  const int found = kdtree::calc_duplicates(tree, 0.1f, duplicates);
  check_duplicates(positions, indices, 0.1f, duplicates, found);

  /* Points outside the mask are never touched. */
  for (const int i : positions.index_range()) {
    if (i % 4 == 0) {
      EXPECT_EQ(duplicates[i], -1);
    }
  }

  /* Building from an index array must give the same tree as building from the mask, and so the
   * same clusters down to which point of each is the target. */
  const KDTreeNew<float3> tree_from_indices(positions, indices.as_span());
  Array<int> from_indices(positions.size(), -1);
  const int found_from_indices = kdtree::calc_duplicates(tree_from_indices, 0.1f, from_indices);
  EXPECT_EQ(Span<int>(from_indices), Span<int>(duplicates));
  EXPECT_EQ(found_from_indices, found);
}

TEST(kdtree, CalcDuplicatesPresetTargets)
{
  const Array<float3> positions = random_points(300, 7);
  const Vector<int> indices = all_indices(positions.size());

  /* Points preset to their own index are kept out of any cluster but can still be targets. */
  Array<int> duplicates(positions.size(), -1);
  for (const int i : IndexRange(0, 20)) {
    duplicates[i] = i;
  }
  const KDTreeNew<float3> tree(positions);
  const int found = kdtree::calc_duplicates(tree, 0.15f, duplicates);

  for (const int i : IndexRange(0, 20)) {
    EXPECT_EQ(duplicates[i], i) << "preset point " << i << " was merged into another point";
  }
  check_duplicates(positions, indices, 0.15f, duplicates, found);
}

/**
 * Visiting the points in index order keeps the tree's layout out of the result: the target of
 * every cluster is its lowest index, and the same points built into a different tree cluster the
 * same way.
 */
TEST(kdtree, CalcDuplicatesIndexOrder)
{
  const Array<float3> positions = random_points(2000, 10);
  const Vector<int> indices = all_indices(positions.size());

  for (const float range : {0.02f, 0.05f, 0.2f}) {
    SCOPED_TRACE("range " + std::to_string(range));
    const KDTreeNew<float3> tree(positions);
    Array<int> duplicates(positions.size(), -1);
    const int found = kdtree::calc_duplicates(tree, range, indices.as_span(), duplicates);
    check_duplicates(positions, indices, range, duplicates, found);

    /* Every cluster is targeted at its lowest index: a point with a lower index would have been
     * visited first and claimed the target rather than the other way around. */
    for (const int i : positions.index_range()) {
      if (duplicates[i] != -1) {
        EXPECT_GE(i, duplicates[i]) << "point " << i << " merged into a higher index";
      }
    }

    /* The mask overload visits the same points in the same order. */
    IndexMaskMemory memory;
    const IndexMask mask = IndexMask::from_indices<int>(indices, memory);
    Array<int> from_mask(positions.size(), -1);
    const int found_from_mask = kdtree::calc_duplicates(tree, range, mask, from_mask);
    EXPECT_EQ(Span<int>(from_mask), Span<int>(duplicates));
    EXPECT_EQ(found_from_mask, found);

    /* Nothing of the tree reaches the result, so a tree with a different leaf size, which stores
     * the points in a different order, gives exactly the same clusters. */
    const KDTreeNew<float3> other_tree(positions, 4);
    Array<int> other(positions.size(), -1);
    const int other_found = kdtree::calc_duplicates(other_tree, range, indices.as_span(), other);
    EXPECT_EQ(Span<int>(other), Span<int>(duplicates));
    EXPECT_EQ(other_found, found);
  }
}

/** #CalcDuplicatesIndexOrder for a tree covering a subset of the points. */
TEST(kdtree, CalcDuplicatesIndexOrderMasked)
{
  const Array<float3> positions = random_points(500, 11);
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_predicate(
      positions.index_range(), memory, [](const int64_t i) { return i % 4 != 0; });
  const Vector<int> indices = mask.to_indices<int>();

  const KDTreeNew<float3> tree(positions, mask);
  Array<int> duplicates(positions.size(), -1);
  const int found = kdtree::calc_duplicates(tree, 0.1f, mask, duplicates);
  check_duplicates(positions, indices, 0.1f, duplicates, found);

  for (const int i : indices) {
    if (duplicates[i] != -1) {
      EXPECT_GE(i, duplicates[i]) << "point " << i << " merged into a higher index";
    }
  }

  /* Points outside the mask are never touched. */
  for (const int i : positions.index_range()) {
    if (i % 4 == 0) {
      EXPECT_EQ(duplicates[i], -1);
    }
  }

  /* The span overload visits the same points in the same order. */
  Array<int> from_indices(positions.size(), -1);
  const int found_from_indices = kdtree::calc_duplicates(
      tree, 0.1f, indices.as_span(), from_indices);
  EXPECT_EQ(Span<int>(from_indices), Span<int>(duplicates));
  EXPECT_EQ(found_from_indices, found);
}

/**
 * Trees past a certain size are built with several threads, which has to give exactly the same
 * tree as building it on one. Compares against a brute force search over the same points, and
 * against the same points built with several thread counts.
 *
 * #calc_duplicates walks the points in the order the tree stores them, so its result is the
 * sharpest check available that the trees are identical rather than merely equivalent.
 */
TEST(kdtree, ThreadedBuild)
{
  /* Larger than the point count threading threshold. */
  const Array<float3> positions = random_points(50000, 8);
  const KDTreeNew<float3> tree(positions);
  EXPECT_EQ(tree.size(), 50000);

  const Array<float3> queries = random_points(50, 9);
  for (const float3 &query : queries) {
    int expected_index = -1;
    float expected_distance_sq = FLT_MAX;
    for (const int i : positions.index_range()) {
      const float distance_sq = math::distance_squared(positions[i], query);
      if (distance_sq < expected_distance_sq) {
        expected_distance_sq = distance_sq;
        expected_index = i;
      }
    }

    float distance_sq = -1.0f;
    EXPECT_EQ(tree.find_nearest(query, &distance_sq), expected_index);
    EXPECT_FLOAT_EQ(distance_sq, expected_distance_sq);
  }

  Array<int> duplicates(positions.size(), -1);
  const int found = kdtree::calc_duplicates(tree, 0.02f, duplicates);
  EXPECT_GT(found, 0);
  check_duplicates(positions, all_indices(positions.size()), 0.02f, duplicates, found, false);

  /* However many threads the build is given, including none, the tree and so the clusters have to
   * come out the same. */
  for (const int threads_num : {1, 2, 3, 8}) {
    BLI_system_num_threads_override_set(threads_num);
    BLI_task_scheduler_init();
    Array<int> other(positions.size(), -1);
    int other_found = 0;
    {
      const KDTreeNew<float3> other_tree(positions);
      other_found = kdtree::calc_duplicates(other_tree, 0.02f, other);
    }
    BLI_task_scheduler_exit();
    BLI_system_num_threads_override_set(0);

    EXPECT_EQ(Span<int>(other), Span<int>(duplicates)) << threads_num << " threads";
    EXPECT_EQ(other_found, found) << threads_num << " threads";
  }
}

TEST(kdtree, Float2)
{
  const Array<float2> positions = {
      float2(0.0f, 0.0f), float2(1.0f, 0.0f), float2(0.0f, 5.0f), float2(4.0f, 4.0f)};
  const KDTreeNew<float2> tree(positions);
  EXPECT_EQ(tree.find_nearest(float2(0.9f, 0.1f)), 1);
  EXPECT_EQ(tree.count_in_radius(float2(0.0f, 0.0f), 1.5f), 2);
}

}  // namespace blender::tests
