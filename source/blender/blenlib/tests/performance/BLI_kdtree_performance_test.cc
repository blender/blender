/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cmath>
#include <ctime>

#include "testing/testing.h"

#include "BLI_array.hh"
#include "BLI_function_ref.hh"
#include "BLI_index_mask.hh"
#include "BLI_kdtree.hh"
#include "BLI_kdtree_new.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_rand.hh"
#include "BLI_task_c.hh"
#include "BLI_threads.hh"
#include "BLI_time.hh"

namespace blender {

static const std::array<int64_t, 4> POINT_COUNTS = {1000, 10000, 100000, 1000000};

/**
 * Trees this small are built one per group by nodes like Cluster by Distance and Index of
 * Nearest, so the cost of building the tree matters as much as the queries.
 */
static const std::array<int64_t, 5> TINY_POINT_COUNTS = {4, 16, 64, 256, 1024};

/** Number of query points used by the search benchmarks. */
static constexpr int64_t QUERY_COUNT = 100000;

/** Neighbors requested by the "find nearest n" benchmark. */
static constexpr int NEAREST_N = 8;

/**
 * Search radius chosen so that a uniform distribution has roughly this many points in range.
 * Using a fixed radius instead would make the small and large sizes test completely different
 * things (finding nothing vs. finding most of the tree).
 */
static constexpr double NEIGHBORS_IN_RANGE = 8.0;

static float radius_for_neighbor_count(const int64_t points_num)
{
  /* Volume of the sphere containing #NEIGHBORS_IN_RANGE points in the unit cube. */
  return float(std::cbrt(NEIGHBORS_IN_RANGE * 3.0 / (4.0 * M_PI * double(points_num))));
}

/* -------------------------------------------------------------------- */
/** \name Point Distributions
 *
 * All of these fill the unit cube, so a given radius means the same thing for each of them.
 * \{ */

/** Uniform noise. Neighbors in space are completely unrelated to neighbors in the array. */
static Array<float3> make_random_points(const int64_t points_num)
{
  RandomNumberGenerator random;
  Array<float3> positions(points_num);
  for (const int64_t i : positions.index_range()) {
    positions[i] = float3(random.get_float(), random.get_float(), random.get_float());
  }
  return positions;
}

/**
 * A jittered grid visited in scan-line order, so points close in the array are close in space.
 * This is much closer to real geometry (mesh vertices, hair roots, grid-aligned point clouds)
 * than uniform noise, and it's the case where memory access patterns can be exploited.
 */
static Array<float3> make_contiguous_points(const int64_t points_num)
{
  RandomNumberGenerator random;
  Array<float3> positions(points_num);
  /* Round up, so the grid is always large enough to contain every point. */
  const int64_t side = std::max<int64_t>(std::ceil(std::cbrt(double(points_num))), 1);
  const float step = 1.0f / float(side);
  for (const int64_t i : positions.index_range()) {
    const int64_t x = i % side;
    const int64_t y = (i / side) % side;
    const int64_t z = i / (side * side);
    const float3 corner = float3(float(x), float(y), float(z)) * step;
    const float3 jitter = float3(random.get_float(), random.get_float(), random.get_float()) *
                          step;
    positions[i] = corner + jitter;
  }
  return positions;
}

/**
 * Half of the points sit exactly on one of a few locations. Degenerate cases like this show up
 * whenever geometry is generated procedurally (instances at the same origin, collapsed edges,
 * points snapped to a grid), and they're the worst case for the median split: many coordinates
 * are equal along every axis, so the tree can become badly unbalanced.
 */
static Array<float3> make_duplicate_points(const int64_t points_num)
{
  constexpr int hotspots_num = 16;
  RandomNumberGenerator random;
  Array<float3> hotspots(hotspots_num);
  for (const int i : hotspots.index_range()) {
    hotspots[i] = float3(random.get_float(), random.get_float(), random.get_float());
  }

  Array<float3> positions(points_num);
  for (const int64_t i : positions.index_range()) {
    if (i % 2 == 0) {
      positions[i] = hotspots[random.get_int32(hotspots_num)];
    }
    else {
      positions[i] = float3(random.get_float(), random.get_float(), random.get_float());
    }
  }
  return positions;
}

using MakePointsFn = Array<float3> (*)(int64_t);

struct Distribution {
  const char *name;
  MakePointsFn make_points;
};

static const Distribution DISTRIBUTIONS[] = {
    {"random", make_random_points},
    {"contiguous", make_contiguous_points},
    {"duplicates", make_duplicate_points},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/** The two implementations that are compared. */
enum class Impl {
  BLI,
  New,
};

static const std::array<Impl, 2> IMPLEMENTATIONS = {Impl::BLI, Impl::New};

static const char *impl_name(const Impl impl)
{
  return impl == Impl::BLI ? "BLI_kdtree" : "New";
}

/**
 * Query points taken from the tree's own points, keeping their relative order so that a
 * spatially coherent input is also queried coherently. They're offset by a small amount so
 * queries don't degenerate into finding the exact same coordinate every time.
 */
static Array<float3> make_query_points(const Span<float3> positions)
{
  const int64_t queries_num = std::min<int64_t>(QUERY_COUNT, positions.size());
  const int64_t stride = positions.size() / queries_num;
  RandomNumberGenerator random;
  Array<float3> queries(queries_num);
  for (const int64_t i : queries.index_range()) {
    const float3 offset = float3(random.get_float(), random.get_float(), random.get_float());
    queries[i] = positions[i * stride] + (offset - 0.5f) * 0.01f;
  }
  return queries;
}

static KDTree<float3> *build_bli_tree(const Span<float3> positions)
{
  KDTree<float3> *tree = kdtree_new<float3>(positions.size());
  for (const int64_t i : positions.index_range()) {
    kdtree_insert(tree, int(i), positions[i]);
  }
  kdtree_balance(tree);
  return tree;
}

/**
 * Runs \a function often enough that the result isn't dominated by timer noise for small point
 * counts, without making the largest point counts take a long time. Returns the fastest run.
 */
static double run_iterations(const int64_t points_num, const FunctionRef<void()> function)
{
  const int iterations = int(std::clamp<int64_t>(2000000 / points_num, 3, 1000));
  double min_time = 1e30;
  for (int repetition = 0; repetition < iterations; repetition++) {
    const double time_before = BLI_time_now_seconds();
    function();
    const double time_after = BLI_time_now_seconds();
    min_time = std::min(min_time, time_after - time_before);
  }
  return min_time;
}

/**
 * Like #run_iterations, but for operations that are too fast to time on their own: a single run
 * of the smallest trees is only a few hundred nanoseconds, which is close enough to the cost of
 * reading the clock that it would dominate the result. Runs the function in batches instead.
 *
 * \return The average time of a single run, from the fastest batch.
 */
static double run_batched(const int64_t points_num, const FunctionRef<void()> function)
{
  const int64_t batch_size = std::max<int64_t>(20000 / points_num, 1);
  double min_time = 1e30;
  for (int repetition = 0; repetition < 20; repetition++) {
    const double time_before = BLI_time_now_seconds();
    for ([[maybe_unused]] const int64_t i : IndexRange(batch_size)) {
      function();
    }
    const double time_after = BLI_time_now_seconds();
    min_time = std::min(min_time, (time_after - time_before) / double(batch_size));
  }
  return min_time;
}

/**
 * Runs \a query_fn for query points in batches, wrapping around the array until the time budget
 * is used up. Since a single query can be orders of magnitude slower in the degenerate cases,
 * running a fixed number of queries would either be too noisy or far too slow.
 *
 * \return The average time of a single query.
 */
static double run_queries(const Span<float3> queries,
                          const FunctionRef<void(const float3 &query)> query_fn)
{
  /* Small enough that the slowest queries don't overshoot the budget by much, large enough that
   * the timer isn't a significant cost. */
  constexpr int64_t batch_size = 256;
  constexpr double time_budget = 0.25;
  constexpr int64_t min_queries = 2000;

  double total_time = 0.0;
  int64_t queries_num = 0;
  while (total_time < time_budget || queries_num < min_queries) {
    const int64_t start = queries_num % queries.size();
    const int64_t size = std::min(batch_size, queries.size() - start);
    const double time_before = BLI_time_now_seconds();
    for (const float3 &query : queries.slice(start, size)) {
      query_fn(query);
    }
    total_time += BLI_time_now_seconds() - time_before;
    queries_num += size;
  }
  return total_time / double(queries_num);
}

static void print_time(const double seconds)
{
  if (seconds < 0.000001) {
    printf(" %8.1f ns |", seconds * 1000000000.0);
  }
  else if (seconds < 0.001) {
    printf(" %8.2f µs |", seconds * 1000000.0);
  }
  else {
    printf(" %8.2f ms |", seconds * 1000.0);
  }
}

static void print_table_header(const char *title,
                               const char *value_description,
                               const Span<int64_t> point_counts)
{
  printf("\n#### %s\n\n(%s)\n\n| distribution | implementation |", title, value_description);
  for (const int64_t points_num : point_counts) {
    printf(" %" PRId64 " points |", points_num);
  }
  printf("\n|---:|---:|");
  for ([[maybe_unused]] const int64_t points_num : point_counts) {
    printf("---:|");
  }
  printf("\n");
}

/**
 * Runs \a run_cell for every combination of distribution, implementation and point count,
 * printing one row per distribution and implementation. The function returns the time of a
 * single run of whatever it measures.
 */
using RunCellFn = FunctionRef<double(Impl impl, const Span<float3> positions, int64_t points_num)>;

static void run_table(const char *title,
                      const char *value_description,
                      const RunCellFn run_cell,
                      const Span<int64_t> point_counts = POINT_COUNTS,
                      const Span<Impl> implementations = IMPLEMENTATIONS)
{
  /* Without this the scheduler reports a single thread, which makes #threading::max_threads_task
   * do nothing, so the tree's own limit on how many threads it builds with has no effect. */
  BLI_task_scheduler_init();

  print_table_header(title, value_description, point_counts);
  for (const Distribution &distribution : DISTRIBUTIONS) {
    for (const Impl impl : implementations) {
      printf("| %s | %s |", distribution.name, impl_name(impl));
      for (const int64_t points_num : point_counts) {
        const Array<float3> positions = distribution.make_points(points_num);
        print_time(run_cell(impl, positions, points_num));
        fflush(stdout);
      }
      printf("\n");
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Benchmarks
 * \{ */

TEST(kdtree_performance, Build)
{
  run_table("Building the tree",
            "`kdtree_insert` and `kdtree_balance` vs. the `KDTreeNew` constructor",
            [](const Impl impl, const Span<float3> positions, const int64_t points_num) {
              if (impl == Impl::BLI) {
                return run_iterations(points_num, [&]() {
                  KDTree<float3> *tree = build_bli_tree(positions);
                  kdtree_free(tree);
                });
              }
              return run_iterations(points_num, [&]() { KDTreeNew<float3> tree(positions); });
            });
}

TEST(kdtree_performance, FindNearest)
{
  run_table("Find the nearest point",
            "time per query",
            [](const Impl impl, const Span<float3> positions, const int64_t /*points_num*/) {
              const Array<float3> queries = make_query_points(positions);
              int64_t index_sum = 0;
              double time;
              if (impl == Impl::BLI) {
                KDTree<float3> *tree = build_bli_tree(positions);
                time = run_queries(queries, [&](const float3 &query) {
                  KDTreeNearest<float3> nearest;
                  index_sum += kdtree_find_nearest(tree, query, &nearest);
                });
                kdtree_free(tree);
              }
              else {
                const KDTreeNew<float3> tree(positions);
                time = run_queries(
                    queries, [&](const float3 &query) { index_sum += tree.find_nearest(query); });
              }
              EXPECT_GT(index_sum, 0);
              return time;
            });
}

TEST(kdtree_performance, FindNearestSkipSelf)
{
  run_table("Find the nearest point, skipping the search point itself",
            "time per query, this is what the Index of Nearest node does",
            [](const Impl impl, const Span<float3> positions, const int64_t /*points_num*/) {
              /* Search from the points themselves, since that's the only case where skipping
               * the search point matters. */
              const Span<float3> queries = positions.take_front(
                  std::min<int64_t>(QUERY_COUNT, positions.size()));
              int64_t index_sum = 0;
              double time;
              /* The queries are the points themselves, so the index to skip is the position of
               * the query in the array. Counting the calls instead would go wrong as soon as
               * #run_queries wraps around, and then the search would accept the point it is
               * searching from and return immediately. */
              const auto query_index = [&](const float3 &query) {
                return int(&query - queries.data());
              };
              if (impl == Impl::BLI) {
                KDTree<float3> *tree = build_bli_tree(positions);
                time = run_queries(queries, [&](const float3 &query) {
                  const int i = query_index(query);
                  index_sum += kdtree_find_nearest_cb<float3>(
                      tree,
                      query,
                      nullptr,
                      [i](const int other, const float3 & /*co*/, const float /*dist_sq*/) {
                        return other == i ? 0 : 1;
                      });
                });
                kdtree_free(tree);
              }
              else {
                const KDTreeNew<float3> tree(positions);
                time = run_queries(queries, [&](const float3 &query) {
                  const int i = query_index(query);
                  index_sum += tree.find_nearest_filtered(
                      query, [i](const int other) { return other != i; });
                });
              }
              EXPECT_GT(index_sum, 0);
              return time;
            });
}

TEST(kdtree_performance, FindNearestN)
{
  run_table("Find the 8 nearest points",
            "time per query",
            [](const Impl impl, const Span<float3> positions, const int64_t /*points_num*/) {
              const Array<float3> queries = make_query_points(positions);
              int64_t found_sum = 0;
              double time;
              if (impl == Impl::BLI) {
                KDTree<float3> *tree = build_bli_tree(positions);
                Array<KDTreeNearest<float3>> nearest(NEAREST_N);
                time = run_queries(queries, [&](const float3 &query) {
                  found_sum += kdtree_find_nearest_n(tree, query, nearest.data(), NEAREST_N);
                });
                kdtree_free(tree);
              }
              else {
                const KDTreeNew<float3> tree(positions);
                Array<KDTreeNew<float3>::Nearest> nearest(NEAREST_N);
                time = run_queries(queries, [&](const float3 &query) {
                  found_sum += tree.find_nearest_n(query, nearest);
                });
              }
              EXPECT_GT(found_sum, 0);
              return time;
            });
}

TEST(kdtree_performance, RangeSearch)
{
  run_table("Visit every point in a radius",
            "time per query, radius scaled for ~8 points in range",
            [](const Impl impl, const Span<float3> positions, const int64_t points_num) {
              const Array<float3> queries = make_query_points(positions);
              const float radius = radius_for_neighbor_count(points_num);
              int64_t found_sum = 0;
              double time;
              if (impl == Impl::BLI) {
                KDTree<float3> *tree = build_bli_tree(positions);
                time = run_queries(queries, [&](const float3 &query) {
                  kdtree_range_search_cb(
                      tree,
                      query,
                      radius,
                      [&](const int /*index*/, const float3 & /*co*/, const float /*dist_sq*/) {
                        found_sum++;
                        return true;
                      });
                });
                kdtree_free(tree);
              }
              else {
                const KDTreeNew<float3> tree(positions);
                time = run_queries(queries, [&](const float3 &query) {
                  tree.foreach_in_radius(
                      query, radius, [&](const int /*index*/, const float /*distance_sq*/) {
                        found_sum++;
                      });
                });
              }
              EXPECT_GT(found_sum, 0);
              return time;
            });
}

TEST(kdtree_performance, CalcDuplicates)
{
  run_table("Merging points by distance",
            "time for the whole tree, radius scaled for ~8 points in range, this is what the "
            "Cluster by Distance node does",
            [](const Impl impl, const Span<float3> positions, const int64_t points_num) {
              const float radius = radius_for_neighbor_count(points_num);
              Array<int> duplicates(points_num);
              if (impl == Impl::BLI) {
                KDTree<float3> *tree = build_bli_tree(positions);
                const double time = run_iterations(points_num, [&]() {
                  duplicates.as_mutable_span().fill(-1);
                  kdtree_calc_duplicates_fast(tree, radius, true, duplicates.data());
                });
                kdtree_free(tree);
                return time;
              }
              const KDTreeNew<float3> tree(positions);
              const IndexMask visit_order(points_num);
              return run_iterations(points_num, [&]() {
                duplicates.as_mutable_span().fill(-1);
                kdtree::calc_duplicates(tree, radius, visit_order, duplicates);
              });
            });
}

/* -------------------------------------------------------------------- */
/** \name Threaded Build
 * \{ */

TEST(kdtree_performance, ThreadedBuild)
{
  static const int thread_counts[] = {1, 2, 4, 8, 16, 32};
  static const int64_t point_counts[] = {1000, 10000, 100000, 1000000};

  printf(
      "\n#### Threaded build\n\n(`KDTreeNew` build time by thread count, random. The\n"
      "tree limits its own thread count by size as well, so the larger counts give the same\n"
      "time once that limit is the lower of the two.)\n\n");
  printf("| points |");
  for (const int threads_num : thread_counts) {
    printf(" %d threads |", threads_num);
  }
  printf(" speedup |\n|---:|");
  for ([[maybe_unused]] const int threads_num : thread_counts) {
    printf("---:|");
  }
  printf("---:|\n");

  Vector<double> cpu_times;
  for (const int64_t points_num : point_counts) {
    const Array<float3> positions = make_random_points(points_num);
    printf("| %" PRId64 " |", points_num);
    double single_thread_time = 0.0;
    double best_time = 1e30;
    for (const int threads_num : thread_counts) {
      BLI_system_num_threads_override_set(threads_num);
      BLI_task_scheduler_init();

      /* Processor time as well as elapsed time, to show how much of the machine the build
       * occupies. Threads spent here are not available for other work. */
      const std::clock_t cpu_before = std::clock();
      const double time = run_iterations(points_num, [&]() { KDTreeNew<float3> tree(positions); });
      const std::clock_t cpu_after = std::clock();
      cpu_times.append(double(cpu_after - cpu_before) / double(CLOCKS_PER_SEC));

      if (threads_num == 1) {
        single_thread_time = time;
      }
      best_time = std::min(best_time, time);
      print_time(time);
      fflush(stdout);

      BLI_task_scheduler_exit();
    }
    printf(" %.1fx |\n", single_thread_time / best_time);
  }

  printf("\n(processor time of the same builds, relative to the single threaded build)\n\n");
  printf("| points |");
  for (const int threads_num : thread_counts) {
    printf(" %d threads |", threads_num);
  }
  printf("\n|---:|");
  for ([[maybe_unused]] const int threads_num : thread_counts) {
    printf("---:|");
  }
  printf("\n");
  int64_t cpu_time_i = 0;
  for (const int64_t points_num : point_counts) {
    printf("| %" PRId64 " |", points_num);
    const double base = cpu_times[cpu_time_i];
    for ([[maybe_unused]] const int threads_num : thread_counts) {
      printf(" %.1fx |", cpu_times[cpu_time_i++] / base);
    }
    printf("\n");
  }

  BLI_system_num_threads_override_set(0);
  BLI_task_scheduler_init();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tiny Trees
 *
 * Nodes that group points by an ID build one tree per group, so trees with a handful of points
 * are common and the cost of building one is part of every query. These measure whole workloads
 * including the build, rather than a single query against a tree that already exists.
 * \{ */

TEST(kdtree_performance, TinyTreesBuild)
{
  run_table(
      "Building a tiny tree",
      "time to build and free one tree",
      [](const Impl impl, const Span<float3> positions, const int64_t points_num) {
        int64_t sum = 0;
        double time;
        if (impl == Impl::BLI) {
          time = run_batched(points_num, [&]() {
            KDTree<float3> *tree = build_bli_tree(positions);
            sum += tree->nodes_len;
            kdtree_free(tree);
          });
        }
        else {
          AlignedBuffer<8192, 8> buffer;
          time = run_batched(points_num, [&]() {
            LinearAllocator<> memory;
            memory.provide_buffer(buffer);
            const KDTreeNew<float3> tree(positions, memory);
            sum += tree.size();
          });
        }
        EXPECT_GT(sum, 0);
        return time;
      },
      TINY_POINT_COUNTS);
}

TEST(kdtree_performance, TinyTreesFindNearestOther)
{
  run_table(
      "Building a tiny tree and finding the nearest other point for every point",
      "time for the whole group, this is what the Index of Nearest node does per group",
      [](const Impl impl, const Span<float3> positions, const int64_t points_num) {
        int64_t sum = 0;
        double time;
        if (impl == Impl::BLI) {
          time = run_batched(points_num, [&]() {
            KDTree<float3> *tree = build_bli_tree(positions);
            for (const int64_t i : IndexRange(points_num)) {
              sum += kdtree_find_nearest_cb<float3>(
                  tree,
                  positions[i],
                  nullptr,
                  [i](const int other, const float3 & /*co*/, const float /*dist_sq*/) {
                    return other == int(i) ? 0 : 1;
                  });
            }
            kdtree_free(tree);
          });
        }
        else {
          AlignedBuffer<8192, 8> buffer;
          time = run_batched(points_num, [&]() {
            LinearAllocator<> memory;
            memory.provide_buffer(buffer);
            const KDTreeNew<float3> tree(positions, memory);
            for (const int64_t i : IndexRange(points_num)) {
              sum += tree.find_nearest_filtered(positions[i],
                                                [i](const int other) { return other != int(i); });
            }
          });
        }
        EXPECT_GT(sum, 0);
        return time;
      },
      TINY_POINT_COUNTS);
}

TEST(kdtree_performance, TinyTreesCalcDuplicates)
{
  run_table(
      "Building a tiny tree and merging its points by distance",
      "time for the whole group, this is what the Cluster by Distance node does per group",
      [](const Impl impl, const Span<float3> positions, const int64_t points_num) {
        const float radius = radius_for_neighbor_count(points_num);
        Array<int> duplicates(points_num);
        if (impl == Impl::BLI) {
          return run_batched(points_num, [&]() {
            KDTree<float3> *tree = build_bli_tree(positions);
            duplicates.as_mutable_span().fill(-1);
            kdtree_calc_duplicates_fast(tree, radius, true, duplicates.data());
            kdtree_free(tree);
          });
        }
        AlignedBuffer<8192, 8> buffer;
        const IndexMask visit_order(points_num);
        return run_batched(points_num, [&]() {
          LinearAllocator<> memory;
          memory.provide_buffer(buffer);
          const KDTreeNew<float3> tree(positions, memory);
          duplicates.as_mutable_span().fill(-1);
          kdtree::calc_duplicates(tree, radius, visit_order, duplicates);
        });
      },
      TINY_POINT_COUNTS);
}

/** \} */

TEST(kdtree_performance, KDTreeNewLeafSize)
{
  BLI_task_scheduler_init(); /* Without this, the build's own thread limit has no effect. */

  static const int leaf_sizes[] = {4, 8, 10, 16, 32, 64, 128};
  static const int64_t point_counts[] = {10000, 1000000};

  for (const int64_t points_num : point_counts) {
    printf("\n#### KDTreeNew leaf size\n\n(%" PRId64 " points)\n\n", points_num);
    printf("| distribution | leaf size | build | find nearest | 8 nearest | radius |\n");
    printf("|---:|---:|---:|---:|---:|---:|\n");

    const float radius = radius_for_neighbor_count(points_num);
    for (const Distribution &distribution : DISTRIBUTIONS) {
      const Array<float3> positions = distribution.make_points(points_num);
      const Array<float3> queries = make_query_points(positions);
      for (const int leaf_size : leaf_sizes) {
        printf("| %s | %d |", distribution.name, leaf_size);
        print_time(
            run_iterations(points_num, [&]() { KDTreeNew<float3> tree(positions, leaf_size); }));

        const KDTreeNew<float3> tree(positions, leaf_size);
        int64_t sum = 0;
        print_time(
            run_queries(queries, [&](const float3 &query) { sum += tree.find_nearest(query); }));

        Array<KDTreeNew<float3>::Nearest> nearest(NEAREST_N);
        print_time(run_queries(
            queries, [&](const float3 &query) { sum += tree.find_nearest_n(query, nearest); }));

        print_time(run_queries(queries, [&](const float3 &query) {
          tree.foreach_in_radius(
              query, radius, [&](const int /*index*/, const float /*distance_sq*/) { sum++; });
        }));
        EXPECT_GT(sum, 0);
        printf("\n");
        fflush(stdout);
      }
    }
  }
}

/** \} */

}  // namespace blender
