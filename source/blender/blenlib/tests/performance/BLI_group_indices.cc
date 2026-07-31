/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <algorithm>
#include <cinttypes>
#include <cmath>

#include "testing/testing.h"

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_function_ref.hh"
#include "BLI_offset_indices.hh"
#include "BLI_rand.hh"
#include "BLI_task_c.hh"
#include "BLI_time.hh"

namespace blender {

static const int64_t GROUP_COUNTS[] = {8, 64, 512, 4096, 32768, 262144, 1048576, 4194304};
static const int64_t INDEX_COUNTS[] = {100000, 1000000, 10000000};

static Array<int> make_random_indices(const int64_t groups_num, const int64_t indices_num)
{
  RandomNumberGenerator random;
  Array<int> indices(indices_num);
  for (const int64_t i : indices.index_range()) {
    indices[i] = random.get_int32(int32_t(groups_num));
  }
  return indices;
}

static Array<int> make_clustered_indices(const int64_t groups_num, const int64_t indices_num)
{
  /* Approximately sorted with a small amount of jittering, more similar to real
   * topology than fully random indices. */
  RandomNumberGenerator random;
  Array<int> indices(indices_num);
  const int64_t jitter = std::max<int64_t>(sqrt(groups_num), 1);
  for (const int64_t i : indices.index_range()) {
    const int64_t center = i * groups_num / indices_num;
    const int64_t value = center + random.get_int32(int32_t(2 * jitter)) - jitter;
    indices[i] = int(std::clamp(value, int64_t(0), groups_num - 1));
  }
  return indices;
}

static double run_iterations(const int64_t indices_num, const FunctionRef<void()> function)
{
  const int iterations = (indices_num >= 10000000) ? 4 : 8;
  double min_time = 1e30;
  for (int repetition = 0; repetition < iterations; repetition++) {
    const double time_before = BLI_time_now_seconds();
    function();
    const double time_after = BLI_time_now_seconds();
    min_time = std::min(min_time, time_after - time_before);
  }
  return min_time;
}

static void print_time(const double seconds)
{
  if (seconds < 0.001) {
    printf(" %8.1f µs |", seconds * 1000000.0);
  }
  else {
    printf(" %8.2f ms |", seconds * 1000.0);
  }
}

static void print_table_header(const char *title)
{
  printf("\n#### %s\n\n| groups |", title);
  for (const int64_t indices_num : INDEX_COUNTS) {
    printf(" %" PRId64 " indices |", indices_num);
  }
  printf("\n|---:|");
  for ([[maybe_unused]] const int64_t indices_num : INDEX_COUNTS) {
    printf("---:|");
  }
  printf("\n");
}

using MakeIndicesFn = Array<int> (*)(int64_t, int64_t);

static void run_count_table(const char *title, const MakeIndicesFn make_indices)
{
  print_table_header(title);
  for (const int64_t groups_num : GROUP_COUNTS) {
    printf("| %" PRId64 " |", groups_num);
    for (const int64_t indices_num : INDEX_COUNTS) {
      const Array<int> indices = make_indices(groups_num, indices_num);
      Array<int> counts(groups_num, 0);
      print_time(run_iterations(indices_num, [&]() {
        counts.as_mutable_span().fill(0);
        array_utils::count_indices(indices, counts);
      }));
      fflush(stdout);
    }
    printf("\n");
  }
}

static void run_build_groups_table(const char *title, const MakeIndicesFn make_indices)
{
  print_table_header(title);
  for (const int64_t groups_num : GROUP_COUNTS) {
    printf("| %" PRId64 " |", groups_num);
    for (const int64_t indices_num : INDEX_COUNTS) {
      const Array<int> indices = make_indices(groups_num, indices_num);
      Array<int> offset_data;
      Array<int> index_data;
      print_time(run_iterations(indices_num, [&]() {
        offset_indices::build_groups_from_indices(
            indices, int(groups_num), offset_data, index_data);
      }));
      fflush(stdout);
    }
    printf("\n");
  }
}

TEST(group_indices_performance, CountIndices)
{
  BLI_task_scheduler_init();
  run_count_table("`array_utils::count_indices`, random", make_random_indices);
  run_count_table("`array_utils::count_indices`, clustered", make_clustered_indices);
}

TEST(group_indices_performance, BuildGroupsFromIndices)
{
  BLI_task_scheduler_init();
  run_build_groups_table("`offset_indices::build_groups_from_indices`, random",
                         make_random_indices);
  run_build_groups_table("`offset_indices::build_groups_from_indices`, clustered",
                         make_clustered_indices);
}

}  // namespace blender
