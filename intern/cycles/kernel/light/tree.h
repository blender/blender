/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* This code implements a modified version of the paper [Importance Sampling of Many Lights with
 * Adaptive Tree Splitting](http://www.aconty.com/pdf/many-lights-hpg2018.pdf) by Alejandro Conty
 * Estevez, Christopher Kulla.
 * The original paper traverses both children when the variance of a node is too high (called
 * splitting). However, Cycles does not support multiple lights per shading point. Therefore, we
 * adjust the importance computation: instead of using a conservative measure (i.e., the maximal
 * possible contribution a node could make to a shading point) as in the paper, we additionally
 * compute the minimal possible contribution and choose uniformly between these two measures. Also,
 * support for distant lights is added, which is not included in the paper.
 */

#pragma once

#include "kernel/light/area.h"
#include "kernel/light/common.h"
#include "kernel/light/light.h"
#include "kernel/light/spot.h"
#include "kernel/light/triangle.h"

CCL_NAMESPACE_BEGIN

/* TODO: this seems like a relative expensive computation. We can make it a lot cheaper by using a
 * bounding sphere instead of a bounding box, but this will reduce the accuracy sometimes. */
ccl_device float light_tree_cos_bounding_box_angle(const BoundingBox bbox,
                                                   const float3 P,
                                                   const float3 point_to_centroid)
{
  if (P.x > bbox.min.x && P.y > bbox.min.y && P.z > bbox.min.z && P.x < bbox.max.x &&
      P.y < bbox.max.y && P.z < bbox.max.z)
  {
    /* If P is inside the bbox, `theta_u` covers the whole sphere. */
    return -1.0f;
  }
  float cos_theta_u = 1.0f;
  /* Iterate through all 8 possible points of the bounding box. */
  for (int i = 0; i < 8; ++i) {
    const float3 corner = make_float3((i & 1) ? bbox.max.x : bbox.min.x,
                                      (i & 2) ? bbox.max.y : bbox.min.y,
                                      (i & 4) ? bbox.max.z : bbox.min.z);

    /* Calculate the bounding box angle. */
    float3 point_to_corner = normalize(corner - P);
    cos_theta_u = fminf(cos_theta_u, dot(point_to_centroid, point_to_corner));
  }
  return cos_theta_u;
}

/* Compute vector v as in Fig .8. P_v is the corresponding point along the ray. */
ccl_device float3 compute_v(
    const float3 centroid, const float3 P, const float3 D, const float3 bcone_axis, const float t)
{
  const float3 unnormalized_v0 = P - centroid;
  const float3 unnormalized_v1 = unnormalized_v0 + D * fminf(t, 1e12f);
  const float3 v0 = normalize(unnormalized_v0);
  const float3 v1 = normalize(unnormalized_v1);

  const float3 o0 = v0;
  float3 o1, o2;
  make_orthonormals_tangent(o0, v1, &o1, &o2);

  const float dot_o0_a = dot(o0, bcone_axis);
  const float dot_o1_a = dot(o1, bcone_axis);
  const float inv_len = inversesqrtf(sqr(dot_o0_a) + sqr(dot_o1_a));
  const float cos_phi0 = dot_o0_a * inv_len;

  return (dot_o1_a < 0 || dot(v0, v1) > cos_phi0) ? (dot_o0_a > dot(v1, bcone_axis) ? v0 : v1) :
                                                    cos_phi0 * o0 + dot_o1_a * inv_len * o1;
}

ccl_device_inline bool is_light(const ccl_global KernelLightTreeEmitter *kemitter)
{
  return kemitter->light.id < 0;
}

ccl_device_inline bool is_mesh(const ccl_global KernelLightTreeEmitter *kemitter)
{
  return !is_light(kemitter) && kemitter->mesh_light.object_id == OBJECT_NONE;
}

ccl_device_inline bool is_triangle(const ccl_global KernelLightTreeEmitter *kemitter)
{
  return !is_light(kemitter) && kemitter->mesh_light.object_id != OBJECT_NONE;
}

ccl_device_inline bool is_leaf(const ccl_global KernelLightTreeNode *knode)
{
  /* The distant node is also considered o leaf node. */
  return knode->type >= LIGHT_TREE_LEAF;
}

template<bool in_volume_segment>
ccl_device void light_tree_to_local_space(KernelGlobals kg,
                                          const int object_id,
                                          ccl_private float3 &P,
                                          ccl_private float3 &N_or_D,
                                          ccl_private float &t)
{
  const int object_flag = kernel_data_fetch(object_flag, object_id);
  if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
#ifdef __OBJECT_MOTION__
    Transform itfm;
    object_fetch_transform_motion_test(kg, object_id, 0.5f, &itfm);
#else
    const Transform itfm = object_fetch_transform(kg, object_id, OBJECT_INVERSE_TRANSFORM);
#endif
    P = transform_point(&itfm, P);
    if (in_volume_segment) {
      /* Transform direction. */
      float3 D_local = transform_direction(&itfm, N_or_D);
      float scale;
      N_or_D = normalize_len(D_local, &scale);

      t *= scale;
    }
    else if (!is_zero(N_or_D)) {
      /* Transform normal. */
      const Transform tfm = object_fetch_transform(kg, object_id, OBJECT_TRANSFORM);
      N_or_D = normalize(transform_direction_transposed(&tfm, N_or_D));
    }
  }
}

/* This is the general function for calculating the importance of either a cluster or an emitter.
 * Both of the specialized functions obtain the necessary data before calling this function. */
template<bool in_volume_segment>
ccl_device void light_tree_importance(const float3 N_or_D,
                                      const bool has_transmission,
                                      const float3 point_to_centroid,
                                      const float cos_theta_u,
                                      const BoundingCone bcone,
                                      const float max_distance,
                                      const float min_distance,
                                      const float t,
                                      const float energy,
                                      ccl_private float &max_importance,
                                      ccl_private float &min_importance)
{
  max_importance = 0.0f;
  min_importance = 0.0f;

  const float sin_theta_u = sin_from_cos(cos_theta_u);

  /* cos(theta_i') in the paper, omitted for volume. */
  float cos_min_incidence_angle = 1.0f;
  float cos_max_incidence_angle = 1.0f;

  if (!in_volume_segment) {
    const float3 N = N_or_D;
    const float cos_theta_i = has_transmission ? fabsf(dot(point_to_centroid, N)) :
                                                 dot(point_to_centroid, N);
    const float sin_theta_i = sin_from_cos(cos_theta_i);

    /* cos_min_incidence_angle = cos(max{theta_i - theta_u, 0}) = cos(theta_i') in the paper */
    cos_min_incidence_angle = cos_theta_i >= cos_theta_u ?
                                  1.0f :
                                  cos_theta_i * cos_theta_u + sin_theta_i * sin_theta_u;

    /* If the node is guaranteed to be behind the surface we're sampling, and the surface is
     * opaque, then we can give the node an importance of 0 as it contributes nothing to the
     * surface. This is more accurate than the bbox test if we are calculating the importance of
     * an emitter with radius. */
    if (!has_transmission && cos_min_incidence_angle < 0) {
      return;
    }

    /* cos_max_incidence_angle = cos(min{theta_i + theta_u, pi}) */
    cos_max_incidence_angle = fmaxf(cos_theta_i * cos_theta_u - sin_theta_i * sin_theta_u, 0.0f);
  }

  float cos_theta, sin_theta;
  if (isequal(bcone.axis, -point_to_centroid)) {
    /* When `bcone.axis == -point_to_centroid`, dot(bcone.axis, -point_to_centroid) doesn't always
     * return 1 due to floating point precision issues. We account for that case here. */
    cos_theta = 1.0f;
    sin_theta = 0.0f;
  }
  else {
    cos_theta = dot(bcone.axis, -point_to_centroid);
    sin_theta = sin_from_cos(cos_theta);
  }

  /* cos(theta - theta_u) */
  const float cos_theta_minus_theta_u = cos_theta * cos_theta_u + sin_theta * sin_theta_u;

  float cos_theta_o, sin_theta_o;
  fast_sincosf(bcone.theta_o, &sin_theta_o, &cos_theta_o);

  /* Minimum angle an emitter’s axis would form with the direction to the shading point,
   * cos(theta') in the paper. */
  float cos_min_outgoing_angle;
  if ((cos_theta >= cos_theta_u) || (cos_theta_minus_theta_u >= cos_theta_o)) {
    /* theta - theta_o - theta_u <= 0 */
    kernel_assert((fast_acosf(cos_theta) - bcone.theta_o - fast_acosf(cos_theta_u)) < 5e-4f);
    cos_min_outgoing_angle = 1.0f;
  }
  else if ((bcone.theta_o + bcone.theta_e > M_PI_F) ||
           (cos_theta_minus_theta_u > cosf(bcone.theta_o + bcone.theta_e)))
  {
    /* theta' = theta - theta_o - theta_u < theta_e */
    kernel_assert(
        (fast_acosf(cos_theta) - bcone.theta_o - fast_acosf(cos_theta_u) - bcone.theta_e) < 5e-4f);
    const float sin_theta_minus_theta_u = sin_from_cos(cos_theta_minus_theta_u);
    cos_min_outgoing_angle = cos_theta_minus_theta_u * cos_theta_o +
                             sin_theta_minus_theta_u * sin_theta_o;
  }
  else {
    /* Cluster is invisible. */
    return;
  }

  /* TODO: find a good approximation for f_a. */
  const float f_a = 1.0f;
  /* TODO: also consider t (or theta_a, theta_b) for volume */
  max_importance = fabsf(f_a * cos_min_incidence_angle * energy * cos_min_outgoing_angle /
                         (in_volume_segment ? min_distance : sqr(min_distance)));

  /* TODO: compute proper min importance for volume. */
  if (in_volume_segment) {
    min_importance = 0.0f;
    return;
  }

  /* cos(theta + theta_o + theta_u) if theta + theta_o + theta_u < theta_e, 0 otherwise */
  float cos_max_outgoing_angle;
  const float cos_theta_plus_theta_u = cos_theta * cos_theta_u - sin_theta * sin_theta_u;
  if (bcone.theta_e - bcone.theta_o < 0 || cos_theta < 0 || cos_theta_u < 0 ||
      cos_theta_plus_theta_u < cosf(bcone.theta_e - bcone.theta_o))
  {
    min_importance = 0.0f;
  }
  else {
    const float sin_theta_plus_theta_u = sin_from_cos(cos_theta_plus_theta_u);
    cos_max_outgoing_angle = cos_theta_plus_theta_u * cos_theta_o -
                             sin_theta_plus_theta_u * sin_theta_o;
    min_importance = fabsf(f_a * cos_max_incidence_angle * energy * cos_max_outgoing_angle /
                           sqr(max_distance));
  }
}

template<bool in_volume_segment>
ccl_device bool compute_emitter_centroid_and_dir(KernelGlobals kg,
                                                 ccl_global const KernelLightTreeEmitter *kemitter,
                                                 const float3 P,
                                                 ccl_private float3 &centroid,
                                                 ccl_private packed_float3 &dir)
{
  if (is_light(kemitter)) {
    const ccl_global KernelLight *klight = &kernel_data_fetch(lights, ~(kemitter->light.id));
    centroid = klight->co;

    switch (klight->type) {
      case LIGHT_SPOT:
        dir = klight->spot.dir;
        break;
      case LIGHT_POINT:
        /* Disk-oriented normal. */
        dir = safe_normalize(P - centroid);
        break;
      case LIGHT_AREA:
        dir = klight->area.dir;
        break;
      case LIGHT_BACKGROUND:
        /* Arbitrary centroid and direction. */
        centroid = make_float3(0.0f, 0.0f, 1.0f);
        dir = make_float3(0.0f, 0.0f, -1.0f);
        break;
      case LIGHT_DISTANT:
        dir = centroid;
        break;
      default:
        return false;
    }
  }
  else {
    kernel_assert(is_triangle(kemitter));
    const int object = kemitter->mesh_light.object_id;
    float3 vertices[3];
    triangle_vertices(kg, kemitter->triangle.id, vertices);
    centroid = (vertices[0] + vertices[1] + vertices[2]) / 3.0f;

    const bool is_front_only = (kemitter->triangle.emission_sampling == EMISSION_SAMPLING_FRONT);
    const bool is_back_only = (kemitter->triangle.emission_sampling == EMISSION_SAMPLING_BACK);
    if (is_front_only || is_back_only) {
      dir = safe_normalize(cross(vertices[1] - vertices[0], vertices[2] - vertices[0]));
      if (is_back_only) {
        dir = -dir;
      }
      const int object_flag = kernel_data_fetch(object_flag, object);
      if ((object_flag & SD_OBJECT_TRANSFORM_APPLIED) && (object_flag & SD_OBJECT_NEGATIVE_SCALE))
      {
        dir = -dir;
      }
    }
    else {
      /* Double-sided: any vector in the plane. */
      dir = safe_normalize(vertices[0] - vertices[1]);
    }
  }
  return true;
}

template<bool in_volume_segment>
ccl_device void light_tree_node_importance(KernelGlobals kg,
                                           const float3 P,
                                           const float3 N_or_D,
                                           const float t,
                                           const bool has_transmission,
                                           const ccl_global KernelLightTreeNode *knode,
                                           ccl_private float &max_importance,
                                           ccl_private float &min_importance)
{
  const BoundingCone bcone = knode->bcone;
  const BoundingBox bbox = knode->bbox;

  float3 point_to_centroid;
  float cos_theta_u;
  float distance;
  if (knode->type == LIGHT_TREE_DISTANT) {
    point_to_centroid = -bcone.axis;
    cos_theta_u = fast_cosf(bcone.theta_o + bcone.theta_e);
    distance = 1.0f;
    if (t == FLT_MAX) {
      /* In world volume, distant light has no contribution. */
      return;
    }
  }
  else {
    const float3 centroid = 0.5f * (bbox.min + bbox.max);

    if (in_volume_segment) {
      const float3 D = N_or_D;
      const float3 closest_point = P + dot(centroid - P, D) * D;
      /* Minimal distance of the ray to the cluster. */
      distance = len(centroid - closest_point);
      point_to_centroid = -compute_v(centroid, P, D, bcone.axis, t);
      /* FIXME(weizhen): it is not clear from which point the `cos_theta_u` should be computed in
       * volume segment. We could use `closest_point` as a conservative measure, but then
       * `point_to_centroid` should also use `closest_point`. */
      cos_theta_u = light_tree_cos_bounding_box_angle(bbox, closest_point, point_to_centroid);
    }
    else {
      const float3 N = N_or_D;
      const float3 bbox_extent = bbox.max - centroid;
      const bool bbox_is_visible = has_transmission |
                                   (dot(N, centroid - P) + dot(fabs(N), fabs(bbox_extent)) > 0);

      /* If the node is guaranteed to be behind the surface we're sampling, and the surface is
       * opaque, then we can give the node an importance of 0 as it contributes nothing to the
       * surface. */
      if (!bbox_is_visible) {
        return;
      }

      point_to_centroid = normalize_len(centroid - P, &distance);
      cos_theta_u = light_tree_cos_bounding_box_angle(bbox, P, point_to_centroid);
    }
    /* Clamp distance to half the radius of the cluster when splitting is disabled. */
    distance = fmaxf(0.5f * len(centroid - bbox.max), distance);
  }
  /* TODO: currently max_distance = min_distance, max_importance = min_importance for the
   * nodes. Do we need better weights for complex scenes? */
  light_tree_importance<in_volume_segment>(N_or_D,
                                           has_transmission,
                                           point_to_centroid,
                                           cos_theta_u,
                                           bcone,
                                           distance,
                                           distance,
                                           t,
                                           knode->energy,
                                           max_importance,
                                           min_importance);
}

template<bool in_volume_segment>
ccl_device void light_tree_emitter_importance(KernelGlobals kg,
                                              const float3 P,
                                              const float3 N_or_D,
                                              const float t,
                                              const bool has_transmission,
                                              int emitter_index,
                                              ccl_private float &max_importance,
                                              ccl_private float &min_importance)
{
  max_importance = 0.0f;
  min_importance = 0.0f;

  const ccl_global KernelLightTreeEmitter *kemitter = &kernel_data_fetch(light_tree_emitters,
                                                                         emitter_index);

  if (is_mesh(kemitter)) {
    const ccl_global KernelLightTreeNode *knode = &kernel_data_fetch(light_tree_nodes,
                                                                     kemitter->mesh.node_id);

    light_tree_node_importance<in_volume_segment>(
        kg, P, N_or_D, t, has_transmission, knode, max_importance, min_importance);
    return;
  }

  BoundingCone bcone;
  bcone.theta_o = kemitter->theta_o;
  bcone.theta_e = kemitter->theta_e;
  float cos_theta_u;
  float2 distance; /* distance.x = max_distance, distance.y = mix_distance */
  float3 centroid, point_to_centroid, P_c;

  if (!compute_emitter_centroid_and_dir<in_volume_segment>(kg, kemitter, P, centroid, bcone.axis))
  {
    return;
  }

  if (in_volume_segment) {
    const float3 D = N_or_D;
    /* Closest point. */
    P_c = P + dot(centroid - P, D) * D;
    /* Minimal distance of the ray to the cluster. */
    distance.x = len(centroid - P_c);
    distance.y = distance.x;
    point_to_centroid = -compute_v(centroid, P, D, bcone.axis, t);
  }
  else {
    P_c = P;
  }

  /* Early out if the emitter is guaranteed to be invisible. */
  bool is_visible;
  if (is_triangle(kemitter)) {
    is_visible = triangle_light_tree_parameters<in_volume_segment>(
        kg, kemitter, centroid, P_c, N_or_D, bcone, cos_theta_u, distance, point_to_centroid);
  }
  else {
    kernel_assert(is_light(kemitter));
    const ccl_global KernelLight *klight = &kernel_data_fetch(lights, ~(kemitter->light.id));
    switch (klight->type) {
      /* Function templates only modifies cos_theta_u when in_volume_segment = true. */
      case LIGHT_SPOT:
        is_visible = spot_light_tree_parameters<in_volume_segment>(
            klight, centroid, P_c, cos_theta_u, distance, point_to_centroid);
        break;
      case LIGHT_POINT:
        is_visible = point_light_tree_parameters<in_volume_segment>(
            klight, centroid, P_c, cos_theta_u, distance, point_to_centroid);
        bcone.theta_o = 0.0f;
        break;
      case LIGHT_AREA:
        is_visible = area_light_tree_parameters<in_volume_segment>(
            klight, centroid, P_c, N_or_D, bcone.axis, cos_theta_u, distance, point_to_centroid);
        break;
      case LIGHT_BACKGROUND:
        is_visible = background_light_tree_parameters(
            centroid, cos_theta_u, distance, point_to_centroid);
        break;
      case LIGHT_DISTANT:
        is_visible = distant_light_tree_parameters(
            centroid, bcone.theta_e, cos_theta_u, distance, point_to_centroid);
        break;
      default:
        return;
    }
  }

  is_visible |= has_transmission;
  if (!is_visible) {
    return;
  }

  light_tree_importance<in_volume_segment>(N_or_D,
                                           has_transmission,
                                           point_to_centroid,
                                           cos_theta_u,
                                           bcone,
                                           distance.x,
                                           distance.y,
                                           t,
                                           kemitter->energy,
                                           max_importance,
                                           min_importance);
}

template<bool in_volume_segment>
ccl_device void light_tree_child_importance(KernelGlobals kg,
                                            const float3 P,
                                            const float3 N_or_D,
                                            const float t,
                                            const bool has_transmission,
                                            const ccl_global KernelLightTreeNode *knode,
                                            ccl_private float &max_importance,
                                            ccl_private float &min_importance)
{
  max_importance = 0.0f;
  min_importance = 0.0f;

  if (knode->num_emitters == 1) {
    light_tree_emitter_importance<in_volume_segment>(kg,
                                                     P,
                                                     N_or_D,
                                                     t,
                                                     has_transmission,
                                                     knode->leaf.first_emitter,
                                                     max_importance,
                                                     min_importance);
  }
  else if (knode->num_emitters != 0) {
    light_tree_node_importance<in_volume_segment>(
        kg, P, N_or_D, t, has_transmission, knode, max_importance, min_importance);
  }
}

/* Select an element from the reservoir with probability proportional to its weight.
 * Expect `selected_index` to be initialized to -1, and stays -1 if all the weights are invalid. */
ccl_device void sample_reservoir(const int current_index,
                                 const float current_weight,
                                 ccl_private int &selected_index,
                                 ccl_private float &selected_weight,
                                 ccl_private float &total_weight,
                                 ccl_private float &rand)
{
  if (!(current_weight > 0.0f)) {
    return;
  }
  total_weight += current_weight;

  /* When `-ffast-math` is used it is possible that the threshold is almost 1 but not quite.
   * For this case we check the first valid element explicitly (instead of relying on the threshold
   * to be 1, giving it certain probability). */
  if (selected_index == -1) {
    selected_index = current_index;
    selected_weight = current_weight;
    /* The threshold is expected to be 1 in this case with strict mathematics, so no need to divide
     * the rand. In fact, division in such case could lead the rand to exceed 1 because of division
     * by something smaller than 1. */
    return;
  }

  float thresh = current_weight / total_weight;
  if (rand <= thresh) {
    selected_index = current_index;
    selected_weight = current_weight;
    rand = rand / thresh;
  }
  else {
    rand = (rand - thresh) / (1.0f - thresh);
  }

  /* Ensure the `rand` is always within 0..1 range, which could be violated above when
   * `-ffast-math` is used. */
  rand = saturatef(rand);
}

/* Pick an emitter from a leaf node using reservoir sampling, keep two reservoirs for upper and
 * lower bounds. */
template<bool in_volume_segment>
ccl_device int light_tree_cluster_select_emitter(KernelGlobals kg,
                                                 ccl_private float &rand,
                                                 ccl_private float3 &P,
                                                 ccl_private float3 &N_or_D,
                                                 ccl_private float &t,
                                                 const bool has_transmission,
                                                 ccl_private int *node_index,
                                                 ccl_private float *pdf_factor)
{
  float selected_importance[2] = {0.0f, 0.0f};
  float total_importance[2] = {0.0f, 0.0f};
  int selected_index = -1;
  const ccl_global KernelLightTreeNode *knode = &kernel_data_fetch(light_tree_nodes, *node_index);
  *node_index = -1;

  kernel_assert(knode->num_emitters <= sizeof(uint) * 8);
  /* Mark emitters with valid importance. Used for reservoir when total minimum importance = 0. */
  uint has_importance = 0;

  const bool sample_max = (rand > 0.5f); /* Sampling using the maximum importance. */
  if (knode->num_emitters > 1) {
    rand = rand * 2.0f - float(sample_max);
  }

  for (int i = 0; i < knode->num_emitters; i++) {
    int current_index = knode->leaf.first_emitter + i;
    /* maximum importance = importance[0], minimum importance = importance[1] */
    float importance[2];
    light_tree_emitter_importance<in_volume_segment>(
        kg, P, N_or_D, t, has_transmission, current_index, importance[0], importance[1]);

    sample_reservoir(current_index,
                     importance[!sample_max],
                     selected_index,
                     selected_importance[!sample_max],
                     total_importance[!sample_max],
                     rand);
    if (selected_index == current_index) {
      selected_importance[sample_max] = importance[sample_max];
    }
    total_importance[sample_max] += importance[sample_max];

    has_importance |= ((importance[0] > 0) << i);
  }

  if (!has_importance) {
    return -1;
  }

  if (total_importance[1] == 0.0f) {
    /* Uniformly sample emitters with positive maximum importance. */
    if (sample_max) {
      selected_importance[1] = 1.0f;
      total_importance[1] = float(popcount(has_importance));
    }
    else {
      selected_index = -1;
      for (int i = 0; i < knode->num_emitters; i++) {
        int current_index = knode->leaf.first_emitter + i;
        sample_reservoir(current_index,
                         float(has_importance & 1),
                         selected_index,
                         selected_importance[1],
                         total_importance[1],
                         rand);
        has_importance >>= 1;
      }

      float discard;
      light_tree_emitter_importance<in_volume_segment>(
          kg, P, N_or_D, t, has_transmission, selected_index, selected_importance[0], discard);
    }
  }

  *pdf_factor *= 0.5f * (selected_importance[0] / total_importance[0] +
                         selected_importance[1] / total_importance[1]);

  const ccl_global KernelLightTreeEmitter *kemitter = &kernel_data_fetch(light_tree_emitters,
                                                                         selected_index);

  if (is_mesh(kemitter)) {
    /* Transform ray from world to local space. */
    light_tree_to_local_space<in_volume_segment>(kg, kemitter->mesh.object_id, P, N_or_D, t);

    *node_index = kemitter->mesh.node_id;
    const ccl_global KernelLightTreeNode *knode = &kernel_data_fetch(light_tree_nodes,
                                                                     *node_index);
    if (knode->type == LIGHT_TREE_INSTANCE) {
      /* Switch to the node with the subtree. */
      *node_index = knode->instance.reference;
    }
  }

  return selected_index;
}

template<bool in_volume_segment>
ccl_device bool get_left_probability(KernelGlobals kg,
                                     const float3 P,
                                     const float3 N_or_D,
                                     const float t,
                                     const bool has_transmission,
                                     const int left_index,
                                     const int right_index,
                                     ccl_private float &left_probability)
{
  const ccl_global KernelLightTreeNode *left = &kernel_data_fetch(light_tree_nodes, left_index);
  const ccl_global KernelLightTreeNode *right = &kernel_data_fetch(light_tree_nodes, right_index);

  float min_left_importance, max_left_importance, min_right_importance, max_right_importance;
  light_tree_child_importance<in_volume_segment>(
      kg, P, N_or_D, t, has_transmission, left, max_left_importance, min_left_importance);
  light_tree_child_importance<in_volume_segment>(
      kg, P, N_or_D, t, has_transmission, right, max_right_importance, min_right_importance);

  const float total_max_importance = max_left_importance + max_right_importance;
  if (total_max_importance == 0.0f) {
    return false;
  }
  const float total_min_importance = min_left_importance + min_right_importance;

  /* Average two probabilities of picking the left child node using lower and upper bounds. */
  const float probability_max = max_left_importance / total_max_importance;
  const float probability_min = total_min_importance > 0 ?
                                    min_left_importance / total_min_importance :
                                    0.5f * (float(max_left_importance > 0) +
                                            float(max_right_importance == 0.0f));
  left_probability = 0.5f * (probability_max + probability_min);
  return true;
}

ccl_device int light_tree_root_node_index(KernelGlobals kg, const int object_receiver)
{
  if (kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_LINKING) {
    const uint receiver_light_set =
        (object_receiver != OBJECT_NONE) ?
            kernel_data_fetch(objects, object_receiver).receiver_light_set :
            0;
    return kernel_data.light_link_sets[receiver_light_set].light_tree_root;
  }

  return 0;
}

/* Pick a random light from the light tree from a given shading point P, write to the picked light
 * index and the probability of picking the light. */
template<bool in_volume_segment>
ccl_device_noinline bool light_tree_sample(KernelGlobals kg,
                                           const float rand,
                                           const float3 P,
                                           float3 N_or_D,
                                           float t,
                                           const int object_receiver,
                                           const int shader_flags,
                                           ccl_private LightSample *ls)
{
  if (!kernel_data.integrator.use_direct_light) {
    return false;
  }

  const bool has_transmission = (shader_flags & SD_BSDF_HAS_TRANSMISSION);
  float pdf_leaf = 1.0f;
  float pdf_selection = 1.0f;
  int selected_emitter = -1;
  int node_index = light_tree_root_node_index(kg, object_receiver);
  float rand_selection = rand;

  float3 local_P = P;

  /* Traverse the light tree until a leaf node is reached. */
  while (true) {
    const ccl_global KernelLightTreeNode *knode = &kernel_data_fetch(light_tree_nodes, node_index);

    if (is_leaf(knode)) {
      /* At a leaf node, we pick an emitter. */
      selected_emitter = light_tree_cluster_select_emitter<in_volume_segment>(
          kg, rand_selection, local_P, N_or_D, t, has_transmission, &node_index, &pdf_selection);

      if (selected_emitter < 0) {
        return false;
      }

      if (node_index < 0) {
        break;
      }

      /* Continue with the picked mesh light. */
      ls->object = kernel_data_fetch(light_tree_emitters, selected_emitter).mesh.object_id;
      continue;
    }

    /* Inner node. */
    const int left_index = knode->inner.left_child;
    const int right_index = knode->inner.right_child;

    float left_prob;
    if (!get_left_probability<in_volume_segment>(
            kg, local_P, N_or_D, t, has_transmission, left_index, right_index, left_prob))
    {
      return false; /* Both child nodes have zero importance. */
    }

    float discard;
    float total_prob = left_prob;
    node_index = left_index;
    sample_reservoir(
        right_index, 1.0f - left_prob, node_index, discard, total_prob, rand_selection);
    pdf_leaf *= (node_index == left_index) ? left_prob : (1.0f - left_prob);
  }

  ls->emitter_id = selected_emitter;
  ls->pdf_selection = pdf_selection * pdf_leaf;

  return true;
}

/* We need to be able to find the probability of selecting a given light for MIS. */
template<bool in_volume_segment>
ccl_device float light_tree_pdf(KernelGlobals kg,
                                float3 P,
                                float3 N,
                                const float dt,
                                const int path_flag,
                                const int object_emitter,
                                const uint index_emitter,
                                const int object_receiver)
{
  const bool has_transmission = (path_flag & PATH_RAY_MIS_HAD_TRANSMISSION);

  ccl_global const KernelLightTreeEmitter *kemitter = &kernel_data_fetch(light_tree_emitters,
                                                                         index_emitter);
  int subtree_root_index;
  uint bit_trail, target_emitter;

  if (is_triangle(kemitter)) {
    /* If the target is an emissive triangle, first traverse the top level tree to find the mesh
     * light emitter, then traverse the subtree. */
    target_emitter = kernel_data_fetch(object_to_tree, object_emitter);
    ccl_global const KernelLightTreeEmitter *kmesh = &kernel_data_fetch(light_tree_emitters,
                                                                        target_emitter);
    subtree_root_index = kmesh->mesh.node_id;
    ccl_global const KernelLightTreeNode *kroot = &kernel_data_fetch(light_tree_nodes,
                                                                     subtree_root_index);
    bit_trail = kroot->bit_trail;

    if (kroot->type == LIGHT_TREE_INSTANCE) {
      subtree_root_index = kroot->instance.reference;
    }
  }
  else {
    subtree_root_index = -1;
    bit_trail = kemitter->bit_trail;
    target_emitter = index_emitter;
  }

  float pdf = 1.0f;
  int node_index = light_tree_root_node_index(kg, object_receiver);

  /* Traverse the light tree until we reach the target leaf node. */
  while (true) {
    const ccl_global KernelLightTreeNode *knode = &kernel_data_fetch(light_tree_nodes, node_index);

    if (is_leaf(knode)) {
      /* Iterate through leaf node to find the probability of sampling the target emitter. */
      float target_max_importance = 0.0f;
      float target_min_importance = 0.0f;
      float total_max_importance = 0.0f;
      float total_min_importance = 0.0f;
      int num_has_importance = 0;
      for (int i = 0; i < knode->num_emitters; i++) {
        const int emitter = knode->leaf.first_emitter + i;
        float max_importance, min_importance;
        light_tree_emitter_importance<in_volume_segment>(
            kg, P, N, dt, has_transmission, emitter, max_importance, min_importance);
        num_has_importance += (max_importance > 0);
        if (emitter == target_emitter) {
          target_max_importance = max_importance;
          target_min_importance = min_importance;
        }
        total_max_importance += max_importance;
        total_min_importance += min_importance;
      }

      if (target_max_importance > 0.0f) {
        pdf *= 0.5f * (target_max_importance / total_max_importance +
                       (total_min_importance > 0 ? target_min_importance / total_min_importance :
                                                   1.0f / num_has_importance));
      }
      else {
        return 0.0f;
      }

      if (subtree_root_index != -1) {
        /* Arrived at the mesh light. Continue with the subtree. */
        float unused;
        light_tree_to_local_space<in_volume_segment>(kg, object_emitter, P, N, unused);

        node_index = subtree_root_index;
        subtree_root_index = -1;
        target_emitter = index_emitter;
        bit_trail = kemitter->bit_trail;
        continue;
      }
      else {
        return pdf;
      }
    }

    /* Inner node. */
    const int left_index = knode->inner.left_child;
    const int right_index = knode->inner.right_child;

    float left_prob;
    if (!get_left_probability<in_volume_segment>(
            kg, P, N, dt, has_transmission, left_index, right_index, left_prob))
    {
      return 0.0f;
    }

    bit_trail >>= kernel_data_fetch(light_tree_nodes, node_index).bit_skip;
    const bool go_left = (bit_trail & 1) == 0;
    bit_trail >>= 1;

    node_index = go_left ? left_index : right_index;
    pdf *= go_left ? left_prob : (1.0f - left_prob);

    if (pdf == 0) {
      return 0.0f;
    }
  }
}

/* If the function is called in volume, retrieve the previous point in volume segment, and compute
 * pdf from there. Otherwise compute from the current shading point. */
ccl_device_inline float light_tree_pdf(KernelGlobals kg,
                                       float3 P,
                                       float3 N,
                                       const float dt,
                                       const int path_flag,
                                       const int emitter_object,
                                       const uint emitter_id,
                                       const int object_receiver)
{
  if (path_flag & PATH_RAY_VOLUME_SCATTER) {
    const float3 D_times_t = N;
    const float3 D = normalize(D_times_t);
    P = P - D_times_t;
    return light_tree_pdf<true>(
        kg, P, D, dt, path_flag, emitter_object, emitter_id, object_receiver);
  }

  return light_tree_pdf<false>(
      kg, P, N, 0.0f, path_flag, emitter_object, emitter_id, object_receiver);
}

CCL_NAMESPACE_END
