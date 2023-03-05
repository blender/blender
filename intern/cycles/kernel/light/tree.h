/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

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
      P.y < bbox.max.y && P.z < bbox.max.z) {
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

  /* When sampling the light tree for the second time in `shade_volume.h` and when query the pdf in
   * `sample.h`. */
  const bool in_volume = is_zero(N_or_D);
  if (!in_volume_segment && !in_volume) {
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

  /* cos(theta - theta_u) */
  const float cos_theta = dot(bcone.axis, -point_to_centroid);
  const float sin_theta = sin_from_cos(cos_theta);
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
           (cos_theta_minus_theta_u > cos(bcone.theta_o + bcone.theta_e))) {
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

  /* TODO: also min importance for volume? */
  if (in_volume_segment) {
    min_importance = max_importance;
    return;
  }

  /* cos(theta + theta_o + theta_u) if theta + theta_o + theta_u < theta_e, 0 otherwise */
  float cos_max_outgoing_angle;
  const float cos_theta_plus_theta_u = cos_theta * cos_theta_u - sin_theta * sin_theta_u;
  if (bcone.theta_e - bcone.theta_o < 0 || cos_theta < 0 || cos_theta_u < 0 ||
      cos_theta_plus_theta_u < cos(bcone.theta_e - bcone.theta_o)) {
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
  const int prim_id = kemitter->prim;
  if (prim_id < 0) {
    const ccl_global KernelLight *klight = &kernel_data_fetch(lights, ~prim_id);
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
        return !in_volume_segment;
      case LIGHT_DISTANT:
        dir = centroid;
        return !in_volume_segment;
      default:
        return false;
    }
  }
  else {
    const int object = kemitter->mesh_light.object_id;
    float3 vertices[3];
    triangle_world_space_vertices(kg, object, prim_id, -1.0f, vertices);
    centroid = (vertices[0] + vertices[1] + vertices[2]) / 3.0f;

    const bool is_front_only = (kemitter->emission_sampling == EMISSION_SAMPLING_FRONT);
    const bool is_back_only = (kemitter->emission_sampling == EMISSION_SAMPLING_BACK);
    if (is_front_only || is_back_only) {
      dir = safe_normalize(cross(vertices[1] - vertices[0], vertices[2] - vertices[0]));
      if (is_back_only) {
        dir = -dir;
      }
      if (kernel_data_fetch(object_flag, object) & SD_OBJECT_NEGATIVE_SCALE) {
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
ccl_device void light_tree_emitter_importance(KernelGlobals kg,
                                              const float3 P,
                                              const float3 N_or_D,
                                              const float t,
                                              const bool has_transmission,
                                              int emitter_index,
                                              ccl_private float &max_importance,
                                              ccl_private float &min_importance)
{
  const ccl_global KernelLightTreeEmitter *kemitter = &kernel_data_fetch(light_tree_emitters,
                                                                         emitter_index);

  max_importance = 0.0f;
  min_importance = 0.0f;
  BoundingCone bcone;
  bcone.theta_o = kemitter->theta_o;
  bcone.theta_e = kemitter->theta_e;
  float cos_theta_u;
  float2 distance; /* distance.x = max_distance, distance.y = mix_distance */
  float3 centroid, point_to_centroid, P_c;

  if (!compute_emitter_centroid_and_dir<in_volume_segment>(
          kg, kemitter, P, centroid, bcone.axis)) {
    return;
  }

  const int prim_id = kemitter->prim;

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

  bool is_visible;
  if (prim_id < 0) {
    const ccl_global KernelLight *klight = &kernel_data_fetch(lights, ~prim_id);
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
  else { /* Mesh light. */
    is_visible = triangle_light_tree_parameters<in_volume_segment>(
        kg, kemitter, centroid, P_c, N_or_D, bcone, cos_theta_u, distance, point_to_centroid);
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
ccl_device void light_tree_node_importance(KernelGlobals kg,
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
  if (knode->num_prims == 1) {
    /* At a leaf node with only one emitter. */
    light_tree_emitter_importance<in_volume_segment>(
        kg, P, N_or_D, t, has_transmission, -knode->child_index, max_importance, min_importance);
  }
  else if (knode->num_prims != 0) {
    const BoundingCone bcone = knode->bcone;
    const BoundingBox bbox = knode->bbox;

    float3 point_to_centroid;
    float cos_theta_u;
    float distance;
    if (knode->bit_trail == 1) {
      /* Distant light node. */
      if (in_volume_segment) {
        return;
      }
      point_to_centroid = -bcone.axis;
      cos_theta_u = fast_cosf(bcone.theta_o);
      distance = 1.0f;
    }
    else {
      const float3 centroid = 0.5f * (bbox.min + bbox.max);

      if (in_volume_segment) {
        const float3 D = N_or_D;
        const float3 closest_point = P + dot(centroid - P, D) * D;
        /* Minimal distance of the ray to the cluster. */
        distance = len(centroid - closest_point);
        point_to_centroid = -compute_v(centroid, P, D, bcone.axis, t);
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
}

ccl_device void sample_resevoir(const int current_index,
                                const float current_weight,
                                ccl_private int &selected_index,
                                ccl_private float &selected_weight,
                                ccl_private float &total_weight,
                                ccl_private float &rand)
{
  if (current_weight == 0.0f) {
    return;
  }
  total_weight += current_weight;
  float thresh = current_weight / total_weight;
  if (rand <= thresh) {
    selected_index = current_index;
    selected_weight = current_weight;
    rand = rand / thresh;
  }
  else {
    rand = (rand - thresh) / (1.0f - thresh);
  }
  kernel_assert(rand >= 0.0f && rand <= 1.0f);
  return;
}

/* Pick an emitter from a leaf node using resevoir sampling, keep two reservoirs for upper and
 * lower bounds. */
template<bool in_volume_segment>
ccl_device int light_tree_cluster_select_emitter(KernelGlobals kg,
                                                 ccl_private float &rand,
                                                 const float3 P,
                                                 const float3 N_or_D,
                                                 const float t,
                                                 const bool has_transmission,
                                                 const ccl_global KernelLightTreeNode *knode,
                                                 ccl_private float *pdf_factor)
{
  float selected_importance[2] = {0.0f, 0.0f};
  float total_importance[2] = {0.0f, 0.0f};
  int selected_index = -1;

  /* Mark emitters with zero importance. Used for resevoir when total minimum importance = 0. */
  kernel_assert(knode->num_prims <= sizeof(uint) * 8);
  uint has_importance = 0;

  const bool sample_max = (rand > 0.5f); /* Sampling using the maximum importance. */
  rand = rand * 2.0f - float(sample_max);

  for (int i = 0; i < knode->num_prims; i++) {
    int current_index = -knode->child_index + i;
    /* maximum importance = importance[0], minimum importance = importance[1] */
    float importance[2];
    light_tree_emitter_importance<in_volume_segment>(
        kg, P, N_or_D, t, has_transmission, current_index, importance[0], importance[1]);

    sample_resevoir(current_index,
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

  if (total_importance[0] == 0.0f) {
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
      for (int i = 0; i < knode->num_prims; i++) {
        int current_index = -knode->child_index + i;
        sample_resevoir(current_index,
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

  *pdf_factor = 0.5f * (selected_importance[0] / total_importance[0] +
                        selected_importance[1] / total_importance[1]);

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
  light_tree_node_importance<in_volume_segment>(
      kg, P, N_or_D, t, has_transmission, left, max_left_importance, min_left_importance);
  light_tree_node_importance<in_volume_segment>(
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

template<bool in_volume_segment>
ccl_device_noinline bool light_tree_sample(KernelGlobals kg,
                                           float randn,
                                           const float randu,
                                           const float randv,
                                           const float time,
                                           const float3 P,
                                           const float3 N_or_D,
                                           const float t,
                                           const int shader_flags,
                                           const int bounce,
                                           const uint32_t path_flag,
                                           ccl_private LightSample *ls)
{
  if (!kernel_data.integrator.use_direct_light) {
    return false;
  }

  const bool has_transmission = (shader_flags & SD_BSDF_HAS_TRANSMISSION);
  float pdf_leaf = 1.0f;
  float pdf_selection = 1.0f;
  int selected_emitter = -1;

  int node_index = 0; /* Root node. */

  /* Traverse the light tree until a leaf node is reached. */
  while (true) {
    const ccl_global KernelLightTreeNode *knode = &kernel_data_fetch(light_tree_nodes, node_index);

    if (knode->child_index <= 0) {
      /* At a leaf node, we pick an emitter. */
      selected_emitter = light_tree_cluster_select_emitter<in_volume_segment>(
          kg, randn, P, N_or_D, t, has_transmission, knode, &pdf_selection);
      break;
    }

    /* At an interior node, the left child is directly after the parent, while the right child is
     * stored as the child index. */
    const int left_index = node_index + 1;
    const int right_index = knode->child_index;

    float left_prob;
    if (!get_left_probability<in_volume_segment>(
            kg, P, N_or_D, t, has_transmission, left_index, right_index, left_prob)) {
      return false; /* Both child nodes have zero importance. */
    }

    float discard;
    float total_prob = left_prob;
    node_index = left_index;
    sample_resevoir(right_index, 1.0f - left_prob, node_index, discard, total_prob, randn);
    pdf_leaf *= (node_index == left_index) ? left_prob : (1.0f - left_prob);
  }

  if (selected_emitter < 0) {
    return false;
  }

  pdf_selection *= pdf_leaf;

  return light_sample<in_volume_segment>(
      kg, randu, randv, time, P, bounce, path_flag, selected_emitter, pdf_selection, ls);
}

/* We need to be able to find the probability of selecting a given light for MIS. */
ccl_device float light_tree_pdf(
    KernelGlobals kg, const float3 P, const float3 N, const int path_flag, const int prim)
{
  const bool has_transmission = (path_flag & PATH_RAY_MIS_HAD_TRANSMISSION);
  /* Target emitter info. */
  const int target_emitter = (prim >= 0) ? kernel_data_fetch(triangle_to_tree, prim) :
                                           kernel_data_fetch(light_to_tree, ~prim);
  ccl_global const KernelLightTreeEmitter *kemitter = &kernel_data_fetch(light_tree_emitters,
                                                                         target_emitter);
  const int target_leaf = kemitter->parent_index;
  ccl_global const KernelLightTreeNode *kleaf = &kernel_data_fetch(light_tree_nodes, target_leaf);
  uint bit_trail = kleaf->bit_trail;

  int node_index = 0; /* Root node. */

  float pdf = 1.0f;

  /* Traverse the light tree until we reach the target leaf node. */
  while (true) {
    const ccl_global KernelLightTreeNode *knode = &kernel_data_fetch(light_tree_nodes, node_index);

    if (knode->child_index <= 0) {
      break;
    }

    /* Interior node. */
    const int left_index = node_index + 1;
    const int right_index = knode->child_index;

    float left_prob;
    if (!get_left_probability<false>(
            kg, P, N, 0, has_transmission, left_index, right_index, left_prob)) {
      return 0.0f;
    }

    const bool go_left = (bit_trail & 1) == 0;
    bit_trail >>= 1;
    pdf *= go_left ? left_prob : (1.0f - left_prob);
    node_index = go_left ? left_index : right_index;

    if (pdf == 0) {
      return 0.0f;
    }
  }

  kernel_assert(node_index == target_leaf);

  /* Iterate through leaf node to find the probability of sampling the target emitter. */
  float target_max_importance = 0.0f;
  float target_min_importance = 0.0f;
  float total_max_importance = 0.0f;
  float total_min_importance = 0.0f;
  int num_has_importance = 0;
  for (int i = 0; i < kleaf->num_prims; i++) {
    const int emitter = -kleaf->child_index + i;
    float max_importance, min_importance;
    light_tree_emitter_importance<false>(
        kg, P, N, 0, has_transmission, emitter, max_importance, min_importance);
    num_has_importance += (max_importance > 0);
    if (emitter == target_emitter) {
      target_max_importance = max_importance;
      target_min_importance = min_importance;
    }
    total_max_importance += max_importance;
    total_min_importance += min_importance;
  }

  if (target_max_importance > 0.0f) {
    return pdf * 0.5f *
           (target_max_importance / total_max_importance +
            (total_min_importance > 0 ? target_min_importance / total_min_importance :
                                        1.0f / num_has_importance));
  }
  return 0.0f;
}

CCL_NAMESPACE_END
