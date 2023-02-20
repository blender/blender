/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "scene/light_tree.h"
#include "scene/mesh.h"
#include "scene/object.h"

CCL_NAMESPACE_BEGIN

float OrientationBounds::calculate_measure() const
{
  float theta_w = fminf(M_PI_F, theta_o + theta_e);
  float cos_theta_o = cosf(theta_o);
  float sin_theta_o = sinf(theta_o);

  return M_2PI_F * (1 - cos_theta_o) +
         M_PI_2_F * (2 * theta_w * sin_theta_o - cosf(theta_o - 2 * theta_w) -
                     2 * theta_o * sin_theta_o + cos_theta_o);
}

OrientationBounds merge(const OrientationBounds &cone_a, const OrientationBounds &cone_b)
{
  if (is_zero(cone_a.axis)) {
    return cone_b;
  }
  if (is_zero(cone_b.axis)) {
    return cone_a;
  }

  /* Set cone a to always have the greater theta_o. */
  const OrientationBounds *a = &cone_a;
  const OrientationBounds *b = &cone_b;
  if (cone_b.theta_o > cone_a.theta_o) {
    a = &cone_b;
    b = &cone_a;
  }

  float theta_d = safe_acosf(dot(a->axis, b->axis));
  float theta_e = fmaxf(a->theta_e, b->theta_e);

  /* Return axis and theta_o of a if it already contains b. */
  /* This should also be called when b is empty. */
  if (a->theta_o >= fminf(M_PI_F, theta_d + b->theta_o)) {
    return OrientationBounds({a->axis, a->theta_o, theta_e});
  }

  /* Compute new theta_o that contains both a and b. */
  float theta_o = (theta_d + a->theta_o + b->theta_o) * 0.5f;

  if (theta_o >= M_PI_F) {
    return OrientationBounds({a->axis, M_PI_F, theta_e});
  }

  /* Rotate new axis to be between a and b. */
  float theta_r = theta_o - a->theta_o;
  float3 new_axis = rotate_around_axis(a->axis, cross(a->axis, b->axis), theta_r);
  new_axis = normalize(new_axis);

  return OrientationBounds({new_axis, theta_o, theta_e});
}

LightTreePrimitive::LightTreePrimitive(Scene *scene, int prim_id, int object_id)
    : prim_id(prim_id), object_id(object_id)
{
  bcone = OrientationBounds::empty;
  bbox = BoundBox::empty;

  if (is_triangle()) {
    float3 vertices[3];
    Object *object = scene->objects[object_id];
    Mesh *mesh = static_cast<Mesh *>(object->get_geometry());
    Mesh::Triangle triangle = mesh->get_triangle(prim_id);
    Shader *shader = static_cast<Shader *>(mesh->get_used_shaders()[mesh->get_shader()[prim_id]]);

    for (int i = 0; i < 3; i++) {
      vertices[i] = mesh->get_verts()[triangle.v[i]];
    }

    /* instanced mesh lights have not applied their transform at this point.
     * in this case, these points have to be transformed to get the proper
     * spatial bound. */
    if (!mesh->transform_applied) {
      const Transform &tfm = object->get_tfm();
      for (int i = 0; i < 3; i++) {
        vertices[i] = transform_point(&tfm, vertices[i]);
      }
    }

    /* TODO: need a better way to handle this when textures are used. */
    float area = triangle_area(vertices[0], vertices[1], vertices[2]);
    energy = area * average(shader->emission_estimate);

    /* NOTE: the original implementation used the bounding box centroid, but primitive centroid
     * seems to work fine */
    centroid = (vertices[0] + vertices[1] + vertices[2]) / 3.0f;

    const bool is_front_only = (shader->emission_sampling == EMISSION_SAMPLING_FRONT);
    const bool is_back_only = (shader->emission_sampling == EMISSION_SAMPLING_BACK);
    if (is_front_only || is_back_only) {
      /* One-sided. */
      bcone.axis = safe_normalize(cross(vertices[1] - vertices[0], vertices[2] - vertices[0]));
      if (is_back_only) {
        bcone.axis = -bcone.axis;
      }
      if (transform_negative_scale(object->get_tfm())) {
        bcone.axis = -bcone.axis;
      }
      bcone.theta_o = 0;
    }
    else {
      /* Double sided: any vector in the plane. */
      bcone.axis = safe_normalize(vertices[0] - vertices[1]);
      bcone.theta_o = M_PI_2_F;
    }
    bcone.theta_e = M_PI_2_F;

    for (int i = 0; i < 3; i++) {
      bbox.grow(vertices[i]);
    }
  }
  else {
    Light *lamp = scene->lights[object_id];
    LightType type = lamp->get_light_type();
    const float size = lamp->get_size();
    float3 strength = lamp->get_strength();

    centroid = scene->lights[object_id]->get_co();
    bcone.axis = normalize(lamp->get_dir());

    if (type == LIGHT_AREA) {
      bcone.theta_o = 0;
      bcone.theta_e = lamp->get_spread() * 0.5f;

      /* For an area light, sizeu and sizev determine the 2 dimensions of the area light,
       * while axisu and axisv determine the orientation of the 2 dimensions.
       * We want to add all 4 corners to our bounding box. */
      const float3 half_extentu = 0.5f * lamp->get_sizeu() * lamp->get_axisu() * size;
      const float3 half_extentv = 0.5f * lamp->get_sizev() * lamp->get_axisv() * size;
      bbox.grow(centroid + half_extentu + half_extentv);
      bbox.grow(centroid + half_extentu - half_extentv);
      bbox.grow(centroid - half_extentu + half_extentv);
      bbox.grow(centroid - half_extentu - half_extentv);

      strength *= 0.25f; /* eval_fac scaling in `area.h` */
    }
    else if (type == LIGHT_POINT) {
      bcone.theta_o = M_PI_F;
      bcone.theta_e = M_PI_2_F;

      /* Point and spot lights can emit light from any point within its radius. */
      const float3 radius = make_float3(size);
      bbox.grow(centroid - radius);
      bbox.grow(centroid + radius);

      strength *= 0.25f * M_1_PI_F; /* eval_fac scaling in `spot.h` and `point.h` */
    }
    else if (type == LIGHT_SPOT) {
      bcone.theta_o = 0;

      const float unscaled_theta_e = lamp->get_spot_angle() * 0.5f;
      const float len_u = len(lamp->get_axisu());
      const float len_v = len(lamp->get_axisv());
      const float len_w = len(lamp->get_dir());

      bcone.theta_e = fast_atanf(fast_tanf(unscaled_theta_e) * fmaxf(len_u, len_v) / len_w);

      /* Point and spot lights can emit light from any point within its radius. */
      const float3 radius = make_float3(size);
      bbox.grow(centroid - radius);
      bbox.grow(centroid + radius);

      strength *= 0.25f * M_1_PI_F; /* eval_fac scaling in `spot.h` and `point.h` */
    }
    else if (type == LIGHT_BACKGROUND) {
      /* Set an arbitrary direction for the background light. */
      bcone.axis = make_float3(0.0f, 0.0f, 1.0f);
      /* TODO: this may depend on portal lights as well. */
      bcone.theta_o = M_PI_F;
      bcone.theta_e = 0;

      /* integrate over cosine-weighted hemisphere */
      strength *= lamp->get_average_radiance() * M_PI_F;
    }
    else if (type == LIGHT_DISTANT) {
      bcone.theta_o = 0;
      bcone.theta_e = 0.5f * lamp->get_angle();
    }

    if (lamp->get_shader()) {
      strength *= lamp->get_shader()->emission_estimate;
    }

    /* Use absolute value of energy so lights with negative strength are properly
     * supported in the light tree. */
    energy = fabsf(average(strength));
  }
}

LightTree::LightTree(vector<LightTreePrimitive> &prims,
                     const int &num_distant_lights,
                     uint max_lights_in_leaf)
{
  if (prims.empty()) {
    return;
  }

  max_lights_in_leaf_ = max_lights_in_leaf;
  int num_prims = prims.size();
  int num_local_lights = num_prims - num_distant_lights;
  /* The amount of nodes is estimated to be twice the amount of primitives */
  nodes_.reserve(2 * num_prims);

  nodes_.emplace_back();                             /* root node */
  recursive_build(0, num_local_lights, prims, 0, 1); /* build tree */
  nodes_[0].make_interior(nodes_.size());

  /* All distant lights are grouped to one node (right child of the root node) */
  OrientationBounds bcone = OrientationBounds::empty;
  float energy_total = 0.0;
  for (int i = num_local_lights; i < num_prims; i++) {
    const LightTreePrimitive &prim = prims.at(i);
    bcone = merge(bcone, prim.bcone);
    energy_total += prim.energy;
  }
  nodes_.emplace_back(BoundBox::empty, bcone, energy_total, 1);
  nodes_.back().make_leaf(num_local_lights, num_distant_lights);

  nodes_.shrink_to_fit();
}

const vector<LightTreeNode> &LightTree::get_nodes() const
{
  return nodes_;
}

int LightTree::recursive_build(
    int start, int end, vector<LightTreePrimitive> &prims, uint bit_trail, int depth)
{
  BoundBox bbox = BoundBox::empty;
  OrientationBounds bcone = OrientationBounds::empty;
  BoundBox centroid_bounds = BoundBox::empty;
  float energy_total = 0.0;
  int num_prims = end - start;
  int current_index = nodes_.size();

  for (int i = start; i < end; i++) {
    const LightTreePrimitive &prim = prims.at(i);
    bbox.grow(prim.bbox);
    bcone = merge(bcone, prim.bcone);
    centroid_bounds.grow(prim.centroid);

    energy_total += prim.energy;
  }

  nodes_.emplace_back(bbox, bcone, energy_total, bit_trail);

  bool try_splitting = num_prims > 1 && len(centroid_bounds.size()) > 0.0f;
  int split_dim = -1, split_bucket = 0, num_left_prims = 0;
  bool should_split = false;
  if (try_splitting) {
    /* Find the best place to split the primitives into 2 nodes.
     * If the best split cost is no better than making a leaf node, make a leaf instead.*/
    float min_cost = min_split_saoh(
        centroid_bounds, start, end, bbox, bcone, split_dim, split_bucket, num_left_prims, prims);
    should_split = num_prims > max_lights_in_leaf_ || min_cost < energy_total;
  }
  if (should_split) {
    int middle;

    if (split_dim != -1) {
      /* Partition the primitives between start and end based on the split dimension and bucket
       * calculated by `split_saoh` */
      middle = start + num_left_prims;
      std::nth_element(prims.begin() + start,
                       prims.begin() + middle,
                       prims.begin() + end,
                       [split_dim](const LightTreePrimitive &l, const LightTreePrimitive &r) {
                         return l.centroid[split_dim] < r.centroid[split_dim];
                       });
    }
    else {
      /* Degenerate case with many lights in the same place. */
      middle = (start + end) / 2;
    }

    [[maybe_unused]] int left_index = recursive_build(start, middle, prims, bit_trail, depth + 1);
    int right_index = recursive_build(middle, end, prims, bit_trail | (1u << depth), depth + 1);
    assert(left_index == current_index + 1);
    nodes_[current_index].make_interior(right_index);
  }
  else {
    nodes_[current_index].make_leaf(start, num_prims);
  }
  return current_index;
}

float LightTree::min_split_saoh(const BoundBox &centroid_bbox,
                                int start,
                                int end,
                                const BoundBox &bbox,
                                const OrientationBounds &bcone,
                                int &split_dim,
                                int &split_bucket,
                                int &num_left_prims,
                                const vector<LightTreePrimitive> &prims)
{
  /* Even though this factor is used for every bucket, we use it to compare
   * the min_cost and total_energy (when deciding between creating a leaf or interior node. */
  const float bbox_area = bbox.area();
  const bool has_area = bbox_area != 0.0f;
  const float total_area = has_area ? bbox_area : len(bbox.size());
  const float total_cost = total_area * bcone.calculate_measure();
  if (total_cost == 0.0f) {
    return FLT_MAX;
  }

  const float inv_total_cost = 1.0f / total_cost;
  const float3 extent = centroid_bbox.size();
  const float max_extent = max4(extent.x, extent.y, extent.z, 0.0f);

  /* Check each dimension to find the minimum splitting cost. */
  float min_cost = FLT_MAX;
  for (int dim = 0; dim < 3; dim++) {
    /* If the centroid bounding box is 0 along a given dimension, skip it. */
    if (centroid_bbox.size()[dim] == 0.0f) {
      continue;
    }

    const float inv_extent = 1 / (centroid_bbox.size()[dim]);

    /* Fill in buckets with primitives. */
    vector<LightTreeBucketInfo> buckets(LightTreeBucketInfo::num_buckets);
    for (int i = start; i < end; i++) {
      const LightTreePrimitive &prim = prims[i];

      /* Place primitive into the appropriate bucket,
       * where the centroid box is split into equal partitions. */
      int bucket_idx = LightTreeBucketInfo::num_buckets *
                       (prim.centroid[dim] - centroid_bbox.min[dim]) * inv_extent;
      if (bucket_idx == LightTreeBucketInfo::num_buckets) {
        bucket_idx = LightTreeBucketInfo::num_buckets - 1;
      }

      buckets[bucket_idx].count++;
      buckets[bucket_idx].energy += prim.energy;
      buckets[bucket_idx].bbox.grow(prim.bbox);
      buckets[bucket_idx].bcone = merge(buckets[bucket_idx].bcone, prim.bcone);
    }

    /* Calculate the cost of splitting at each point between partitions. */
    vector<float> bucket_costs(LightTreeBucketInfo::num_buckets - 1);
    float energy_L, energy_R;
    BoundBox bbox_L, bbox_R;
    OrientationBounds bcone_L, bcone_R;
    for (int split = 0; split < LightTreeBucketInfo::num_buckets - 1; split++) {
      energy_L = 0;
      energy_R = 0;
      bbox_L = BoundBox::empty;
      bbox_R = BoundBox::empty;
      bcone_L = OrientationBounds::empty;
      bcone_R = OrientationBounds::empty;

      for (int left = 0; left <= split; left++) {
        if (buckets[left].bbox.valid()) {
          energy_L += buckets[left].energy;
          bbox_L.grow(buckets[left].bbox);
          bcone_L = merge(bcone_L, buckets[left].bcone);
        }
      }

      for (int right = split + 1; right < LightTreeBucketInfo::num_buckets; right++) {
        if (buckets[right].bbox.valid()) {
          energy_R += buckets[right].energy;
          bbox_R.grow(buckets[right].bbox);
          bcone_R = merge(bcone_R, buckets[right].bcone);
        }
      }

      /* Calculate the cost of splitting using the heuristic as described in the paper. */
      const float area_L = has_area ? bbox_L.area() : len(bbox_L.size());
      const float area_R = has_area ? bbox_R.area() : len(bbox_R.size());
      float left = (bbox_L.valid()) ? energy_L * area_L * bcone_L.calculate_measure() : 0.0f;
      float right = (bbox_R.valid()) ? energy_R * area_R * bcone_R.calculate_measure() : 0.0f;
      float regularization = max_extent * inv_extent;
      bucket_costs[split] = regularization * (left + right) * inv_total_cost;

      if (bucket_costs[split] < min_cost) {
        min_cost = bucket_costs[split];
        split_dim = dim;
        split_bucket = split;
        num_left_prims = 0;
        for (int i = 0; i <= split_bucket; i++) {
          num_left_prims += buckets[i].count;
        }
      }
    }
  }
  return min_cost;
}

CCL_NAMESPACE_END
