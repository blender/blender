/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view_infos.hh"
#include "infos/eevee_lightprobe_infos.hh"

COMPUTE_SHADER_CREATE_INFO(draw_view)
COMPUTE_SHADER_CREATE_INFO(eevee_surfel_common)

#include "draw_view_lib.glsl"
#include "gpu_shader_index_range_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"

namespace eevee::surfel {

/**
 * Return the corresponding list index in the `list_start_buf` for a given world position.
 * It will clamp any coordinate outside valid bounds to nearest list.
 * Also return the surfel sorting value as `r_ray_distance`.
 */
int list_index_get(int2 ray_grid_size, float3 P, float &r_ray_distance)
{
  float3 ssP = drw_point_world_to_screen(P);
  r_ray_distance = -ssP.z;
  int2 ray_coord_on_grid = int2(ssP.xy * float2(ray_grid_size));
  ray_coord_on_grid = clamp(ray_coord_on_grid, int2(0), ray_grid_size - 1);

  int list_index = ray_coord_on_grid.y * ray_grid_size.x + ray_coord_on_grid.x;
  return list_index;
}

/**
 * Return the corresponding cluster index in the `cluster_list_tx` for a given world position.
 * It will clamp any coordinate outside valid bounds to nearest cluster.
 */
int3 cluster_index_get(int3 cluster_grid_size, float4x4 irradiance_grid_world_to_local, float3 P)
{
  float3 lP = transform_point(irradiance_grid_world_to_local, P) * 0.5f + 0.5f;
  int3 cluster_index = int3(lP * float3(cluster_grid_size));
  cluster_index = clamp(cluster_index, int3(0), cluster_grid_size - 1);
  return cluster_index;
}

namespace list::prepare {

struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo eevee_surfel_common;

  [[storage(0, read_write)]] int (&list_counter_buf)[];
  [[storage(6, read_write)]] SurfelListInfoData &list_info_buf;
};

/**
 * Takes the scene surfel representation to build lists of surfels aligning with a given direction.
 *
 * The lists heads are allocated to fit the surfel granularity.
 *
 * Due to alignment the link and list head are split into several int arrays to avoid too much
 * memory waste.
 *
 * This steps only count the number of surfel per list.
 *
 * Dispatch 1 thread per surfel.
 */
[[compute]] [[local_size(SURFEL_GROUP_SIZE)]] [[texture_atomic]]
void prepare_comp([[resource_table]] Resources &srt,
                  [[global_invocation_id]] const uint3 global_id)
{
  const int surfel_id = int(global_id.x);
  if (surfel_id >= int(capture_info_buf.surfel_len)) {
    return;
  }
  float ray_distance;
  int list_id = list_index_get(
      srt.list_info_buf.ray_grid_size, surfel_buf[surfel_id].position, ray_distance);

  atomicAdd(srt.list_counter_buf[list_id], 1);
  /* Do separate assignment to avoid reference to buffer in arguments which is tricky to cross
   * compile. */
  surfel_buf[surfel_id].ray_distance = ray_distance;
  surfel_buf[surfel_id].list_id = list_id;

  /* Clear for next step. */
  if (global_id.x == 0u) {
    srt.list_info_buf.list_prefix_sum = 0;
  }
}

}  // namespace list::prepare

namespace list::prefix_sum {

struct Resources {
  [[legacy_info]] ShaderCreateInfo eevee_surfel_common;
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[storage(0, read)]] const int (&list_counter_buf)[];
  [[storage(2, write)]] int (&list_range_buf)[];
  [[storage(6, read_write)]] SurfelListInfoData &list_info_buf;
};

/**
 * Create a prefix sum of the surfels per list.
 * Outputs one IndexRange for each surfel list.
 *
 * Dispatched as 1 thread per surfel list.
 */
[[compute]] [[local_size(SURFEL_GROUP_SIZE)]] [[texture_atomic]]
void prefix_sum_comp([[resource_table]] Resources &srt)
{
  const int list_id = int(gl_GlobalInvocationID.x);
  if (list_id >= srt.list_info_buf.list_max) {
    return;
  }

  int list_item_count = srt.list_counter_buf[list_id];
  int list_item_start = atomicAdd(srt.list_info_buf.list_prefix_sum, list_item_count);

  srt.list_range_buf[list_id * 2 + 0] = list_item_start;
  srt.list_range_buf[list_id * 2 + 1] = list_item_count;
}

}  // namespace list::prefix_sum

namespace list::flatten {

struct Resources {
  [[legacy_info]] ShaderCreateInfo eevee_surfel_common;
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[storage(0, read_write)]] int (&list_counter_buf)[];
  [[storage(1, read)]] const int (&list_range_buf)[];
  [[storage(2, write)]] float (&list_item_distance_buf)[];
  [[storage(3, write)]] int (&list_item_surfel_id_buf)[];
  [[storage(6, read)]] const SurfelListInfoData &list_info_buf;
};

/**
 * Flatten surfel sorting data into a sequential structure.
 * The buffer structure follows the lists OffsetIndices.
 *
 * Dispatched as 1 thread per surfel.
 */
[[compute]] [[local_size(SURFEL_GROUP_SIZE)]] [[texture_atomic]]
void flatten_comp([[resource_table]] Resources &srt,
                  [[global_invocation_id]] const uint3 global_id)
{
  const int surfel_id = int(global_id.x);
  if (surfel_id >= int(capture_info_buf.surfel_len)) {
    return;
  }

  int list_id = surfel_buf[surfel_id].list_id;
  int item_id = atomicAdd(srt.list_counter_buf[list_id], -1) - 1;
  item_id += srt.list_range_buf[list_id * 2 + 0];

  srt.list_item_distance_buf[item_id] = surfel_buf[surfel_id].ray_distance;
  srt.list_item_surfel_id_buf[item_id] = surfel_id;
}

}  // namespace list::flatten

namespace list::sort {

struct Resources {
  [[legacy_info]] ShaderCreateInfo eevee_surfel_common;
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[storage(0, read)]] const int (&list_range_buf)[];
  [[storage(1, read)]] const int (&list_item_surfel_id_buf)[];
  [[storage(2, read)]] const float (&list_item_distance_buf)[];
  [[storage(3, write)]] int (&sorted_surfel_id_buf)[];
  [[storage(6, read)]] const SurfelListInfoData &list_info_buf;
};

/**
 * Sort ranges of surfel inside a list using radix sort.
 * The previous step flatten the list data into on big arrays, with a specific range of data for
 * each ray list. This stage operate on these ranges.
 *
 * For now the implementation is done in a single pass with brute force.
 * All elements of a list scan inside the whole list.
 *
 * Dispatched as 1 thread per surfel (array elem).
 */
[[compute]] [[local_size(SURFEL_GROUP_SIZE)]] [[texture_atomic]]
void sort_comp([[resource_table]] Resources &srt, [[global_invocation_id]] const uint3 global_id)
{
  const int item_id = int(global_id.x);
  if (item_id >= int(capture_info_buf.surfel_len)) {
    return;
  }

  int surfel_id = srt.list_item_surfel_id_buf[item_id];
  int list_id = surfel_buf[surfel_id].list_id;
  float ray_distance = srt.list_item_distance_buf[item_id];

  IndexRange list_range{srt.list_range_buf[list_id * 2 + 0], srt.list_range_buf[list_id * 2 + 1]};
  int prefix = 0;
  /* Prefix sum inside the list range. */
  for (int i = list_range.start(); i <= list_range.last(); i++) {
    if (srt.list_item_distance_buf[i] > ray_distance) {
      prefix++;
    }
    else if (srt.list_item_distance_buf[i] == ray_distance) {
      /* Resolve the case where 2 items have the same value. */
      if (i > item_id) {
        prefix++;
      }
    }
  }

  int sorted_id = list_range.start() + prefix;
  srt.sorted_surfel_id_buf[sorted_id] = surfel_id;
  surfel_buf[surfel_id].index_in_sorted_list = sorted_id;
}

}  // namespace list::sort

namespace list::build {

/**
 * Return true if link from `surfel[a]` to `surfel[b]` is valid.
 * WARNING: this function is not commutative : `f(a, b) != f(b, a)`
 */
bool is_valid_surfel_link(int a, int b)
{
  float3 link_vector = normalize(surfel_buf[b].position - surfel_buf[a].position);
  float link_angle_cos = dot(surfel_buf[a].normal, link_vector);
  bool is_coplanar = abs(link_angle_cos) < 0.05f;
  return !is_coplanar;
}

struct Resources {
  [[legacy_info]] ShaderCreateInfo eevee_surfel_common;
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[storage(0, write)]] int (&list_start_buf)[];
  [[storage(1, read)]] const int (&list_range_buf)[];
  [[storage(3, read)]] const int (&sorted_surfel_id_buf)[];
  [[storage(6, read_write)]] SurfelListInfoData &list_info_buf;
};

/**
 * Read the result of the sorted buffer and update the `prev` and `next` surfel id inside each
 * surfel structure. This step also transform the linked list into a graph in order to avoid lost
 * energy from almost coplanar surfaces.
 *
 * Dispatched as 1 thread per list.
 */
[[compute]] [[local_size(SURFEL_GROUP_SIZE)]] [[texture_atomic]]
void build_comp([[resource_table]] Resources &srt, [[global_invocation_id]] const uint3 global_id)
{
  const int list_id = int(global_id.x);
  if (list_id >= srt.list_info_buf.list_max) {
    return;
  }

  const IndexRange list_range{srt.list_range_buf[list_id * 2 + 0],
                              srt.list_range_buf[list_id * 2 + 1]};
  if (list_range.size() == 0) {
    srt.list_start_buf[list_id] = -1;
    return;
  }

  const int first_item = list_range.start();
  const int last_item = list_range.last();

  const int sorted_list_first = srt.sorted_surfel_id_buf[first_item];
  {
    /* Update surfels linked list. */
    int prev = -1;
    int curr = srt.sorted_surfel_id_buf[first_item];
    for (int i = first_item; i <= last_item; i++) {
      int next = (i == last_item) ? -1 : srt.sorted_surfel_id_buf[i + 1];
      surfel_buf[curr].next = next;
      surfel_buf[curr].prev = prev;
      prev = curr;
      curr = next;
    }
  }
  /* Update list start for irradiance sample capture. */
  srt.list_start_buf[list_id] = sorted_list_first;

  /* Now that we have a sorted list, try to avoid connection from coplanar surfels.
   * For that we disconnect them and link them to the first non-coplanar surfel.
   * Note that this changes the list to a tree, which doesn't affect the rest of the algorithm.
   *
   * This is a really important step since it allows to clump more surfels into one ray list and
   * avoid light leaking through surfaces. If we don't disconnect coplanar surfels, we loose many
   * good rays by evaluating null radiance transfer between the coplanar surfels for rays that
   * are not directly perpendicular to the surface. */

  /* Limiting the total number of search steps avoids TDRs, but may cause overshadowing if the
   * limit is reached. */
  const int max_search = 2000;
  uint search_count = 0;

  /* Mutable `foreach`. */
  for (int i = sorted_list_first, next = -1; i > -1; i = next) {
    next = surfel_buf[i].next;

    int valid_next = surfel_buf[i].next;
    int valid_prev = surfel_buf[i].prev;

    /* Search the list for the first valid next and previous surfel. */
    while (search_count < max_search) {
      if (valid_next == -1) {
        break;
      }
      if (is_valid_surfel_link(i, valid_next)) {
        break;
      }
      valid_next = surfel_buf[valid_next].next;
      search_count++;
    }
    while (search_count < max_search) {
      if (valid_prev == -1) {
        break;
      }
      if (is_valid_surfel_link(i, valid_prev)) {
        break;
      }
      valid_prev = surfel_buf[valid_prev].prev;
      search_count++;
    }

    surfel_buf[i].next = valid_next;
    surfel_buf[i].prev = valid_prev;
  }

#if 0 /* For debugging the sorted list. */
  for (int i = sorted_list_first, next = -1; i > -1; i = next) {
    next = surfel_buf[i].next;
    if (next != -1) {
      drw_debug_line(surfel_buf[next].position,
                     surfel_buf[i].position,
                     float4(1, 0, 0, 1),
                     drw_debug_persistent_lifetime);
    }
  }
#endif
}

}  // namespace list::build

}  // namespace eevee::surfel

PipelineCompute eevee_surfel_list_build(eevee::surfel::list::build::build_comp);
PipelineCompute eevee_surfel_list_flatten(eevee::surfel::list::flatten::flatten_comp);
PipelineCompute eevee_surfel_list_prefix(eevee::surfel::list::prefix_sum::prefix_sum_comp);
PipelineCompute eevee_surfel_list_prepare(eevee::surfel::list::prepare::prepare_comp);
PipelineCompute eevee_surfel_list_sort(eevee::surfel::list::sort::sort_comp);
