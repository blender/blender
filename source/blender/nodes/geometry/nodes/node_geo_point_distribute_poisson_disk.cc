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
 */

/*
 * Based on Cem Yuksel. 2015. Sample Elimination for Generating Poisson Disk Sample
 * ! Sets. Computer Graphics Forum 34, 2 (May 2015), 25-32.
 * ! http://www.cemyuksel.com/research/sampleelimination/
 * Copyright (c) 2016, Cem Yuksel <cem@cemyuksel.com>
 * All rights reserved.
 */

#include "BLI_inplace_priority_queue.hh"
#include "BLI_kdtree.h"

#include "node_geometry_util.hh"

#include <iostream>
#include <string.h>

namespace blender::nodes {

static void tile_point(Vector<float3> *tiled_points,
                       Vector<size_t> *indices,
                       const float maximum_distance,
                       const float3 boundbox,
                       float3 const &point,
                       size_t index,
                       int dimension = 0)
{
  for (int dimension_iter = dimension; dimension_iter < 3; dimension_iter++) {
    if (boundbox[dimension_iter] - point[dimension_iter] < maximum_distance) {
      float3 point_tiled = point;
      point_tiled[dimension_iter] -= boundbox[dimension_iter];

      tiled_points->append(point_tiled);
      indices->append(index);

      tile_point(tiled_points,
                 indices,
                 maximum_distance,
                 boundbox,
                 point_tiled,
                 index,
                 dimension_iter + 1);
    }

    if (point[dimension_iter] < maximum_distance) {
      float3 point_tiled = point;
      point_tiled[dimension_iter] += boundbox[dimension_iter];

      tiled_points->append(point_tiled);
      indices->append(index);

      tile_point(tiled_points,
                 indices,
                 maximum_distance,
                 boundbox,
                 point_tiled,
                 index,
                 dimension_iter + 1);
    }
  }
}

/**
 * Returns the weight the point gets based on the distance to another point.
 */
static float point_weight_influence_get(const float maximum_distance,
                                        const float minimum_distance,
                                        float distance)
{
  const float alpha = 8.0f;

  if (distance < minimum_distance) {
    distance = minimum_distance;
  }

  return std::pow(1.0f - distance / maximum_distance, alpha);
}

/**
 * Weight each point based on their proximity to its neighbors
 *
 * For each index in the weight array add a weight based on the proximity the
 * corresponding point has with its neighboors.
 **/
static void points_distance_weight_calculate(Vector<float> *weights,
                                             const size_t point_id,
                                             const float3 *input_points,
                                             const void *kd_tree,
                                             const float minimum_distance,
                                             const float maximum_distance,
                                             InplacePriorityQueue<float> *heap)
{
  KDTreeNearest_3d *nearest_point = nullptr;
  int neighbors = BLI_kdtree_3d_range_search(
      (KDTree_3d *)kd_tree, input_points[point_id], &nearest_point, maximum_distance);

  for (int i = 0; i < neighbors; i++) {
    size_t neighbor_point_id = nearest_point[i].index;

    if (neighbor_point_id >= weights->size()) {
      continue;
    }

    /* The point should not influence itself. */
    if (neighbor_point_id == point_id) {
      continue;
    }

    const float weight_influence = point_weight_influence_get(
        maximum_distance, minimum_distance, nearest_point[i].dist);

    /* In the first pass we just the weights. */
    if (heap == nullptr) {
      (*weights)[point_id] += weight_influence;
    }
    /* When we run again we need to update the weights and the heap. */
    else {
      (*weights)[neighbor_point_id] -= weight_influence;
      heap->priority_decreased(neighbor_point_id);
    }
  }

  if (nearest_point) {
    MEM_freeN(nearest_point);
  }
}

/**
 * Returns the minimum radius fraction used by the default weight function.
 */
static float weight_limit_fraction_get(const size_t input_size, const size_t output_size)
{
  const float beta = 0.65f;
  const float gamma = 1.5f;
  float ratio = float(output_size) / float(input_size);
  return (1.0f - std::pow(ratio, gamma)) * beta;
}

/**
 * Tile the input points.
 */
static void points_tiling(const float3 *input_points,
                          const size_t input_size,
                          void **kd_tree,
                          const float maximum_distance,
                          const float3 boundbox)

{
  Vector<float3> tiled_points(input_points, input_points + input_size);
  Vector<size_t> indices(input_size);

  for (size_t i = 0; i < input_size; i++) {
    indices[i] = i;
  }

  /* Tile the tree based on the boundbox. */
  for (size_t i = 0; i < input_size; i++) {
    tile_point(&tiled_points, &indices, maximum_distance, boundbox, input_points[i], i);
  }

  /* Build a new tree with the new indices and tiled points. */
  *kd_tree = BLI_kdtree_3d_new(tiled_points.size());
  for (size_t i = 0; i < tiled_points.size(); i++) {
    BLI_kdtree_3d_insert(*(KDTree_3d **)kd_tree, indices[i], tiled_points[i]);
  }
  BLI_kdtree_3d_balance(*(KDTree_3d **)kd_tree);
}

static void weighted_sample_elimination(const float3 *input_points,
                                        const size_t input_size,
                                        float3 *output_points,
                                        const size_t output_size,
                                        const float maximum_distance,
                                        const float3 boundbox,
                                        const bool do_copy_eliminated)
{
  const float minimum_distance = maximum_distance *
                                 weight_limit_fraction_get(input_size, output_size);

  void *kd_tree = nullptr;
  points_tiling(input_points, input_size, &kd_tree, maximum_distance, boundbox);

  /* Assign weights to each sample. */
  Vector<float> weights(input_size, 0.0f);
  for (size_t point_id = 0; point_id < weights.size(); point_id++) {
    points_distance_weight_calculate(
        &weights, point_id, input_points, kd_tree, minimum_distance, maximum_distance, nullptr);
  }

  /* Remove the points based on their weight. */
  InplacePriorityQueue<float> heap(weights);

  size_t sample_size = input_size;
  while (sample_size > output_size) {
    /* For each sample around it, remove its weight contribution and update the heap. */
    size_t point_id = heap.pop_index();
    points_distance_weight_calculate(
        &weights, point_id, input_points, kd_tree, minimum_distance, maximum_distance, &heap);
    sample_size--;
  }

  /* Copy the samples to the output array. */
  size_t target_size = do_copy_eliminated ? input_size : output_size;
  for (size_t i = 0; i < target_size; i++) {
    size_t index = heap.all_indices()[i];
    output_points[i] = input_points[index];
  }

  /* Cleanup. */
  BLI_kdtree_3d_free((KDTree_3d *)kd_tree);
}

static void progressive_sampling_reorder(Vector<float3> *output_points,
                                         float maximum_density,
                                         float3 boundbox)
{
  /* Re-order the points for progressive sampling. */
  Vector<float3> temporary_points(output_points->size());
  float3 *source_points = output_points->data();
  float3 *dest_points = temporary_points.data();
  size_t source_size = output_points->size();
  size_t dest_size = 0;

  while (source_size >= 3) {
    dest_size = source_size * 0.5f;

    /* Changes the weight function radius using half of the number of samples.
     * It is used for progressive sampling. */
    maximum_density *= std::sqrt(2.0f);
    weighted_sample_elimination(
        source_points, source_size, dest_points, dest_size, maximum_density, boundbox, true);

    if (dest_points != output_points->data()) {
      memcpy((*output_points)[dest_size],
             dest_points[dest_size],
             (source_size - dest_size) * sizeof(float3));
    }

    /* Swap the arrays around. */
    float3 *points_iter = source_points;
    source_points = dest_points;
    dest_points = points_iter;
    source_size = dest_size;
  }
  if (source_points != output_points->data()) {
    memcpy(output_points->data(), source_points, dest_size * sizeof(float3));
  }
}

void poisson_disk_point_elimination(Vector<float3> const *input_points,
                                    Vector<float3> *output_points,
                                    float maximum_density,
                                    float3 boundbox)
{
  weighted_sample_elimination(input_points->data(),
                              input_points->size(),
                              output_points->data(),
                              output_points->size(),
                              maximum_density,
                              boundbox,
                              false);

  progressive_sampling_reorder(output_points, maximum_density, boundbox);
}

}  // namespace blender::nodes
