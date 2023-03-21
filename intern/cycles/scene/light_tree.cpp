/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "scene/light_tree.h"
#include "scene/mesh.h"
#include "scene/object.h"

CCL_NAMESPACE_BEGIN

float OrientationBounds::calculate_measure() const
{
  if (this->is_empty()) {
    return 0.0f;
  }

  float theta_w = fminf(M_PI_F, theta_o + theta_e);
  float cos_theta_o = cosf(theta_o);
  float sin_theta_o = sinf(theta_o);

  return M_2PI_F * (1 - cos_theta_o) +
         M_PI_2_F * (2 * theta_w * sin_theta_o - cosf(theta_o - 2 * theta_w) -
                     2 * theta_o * sin_theta_o + cos_theta_o);
}

OrientationBounds merge(const OrientationBounds &cone_a, const OrientationBounds &cone_b)
{
  if (cone_a.is_empty()) {
    return cone_b;
  }
  if (cone_b.is_empty()) {
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
    measure.energy = area * average(shader->emission_estimate);

    /* NOTE: the original implementation used the bounding box centroid, but primitive centroid
     * seems to work fine */
    centroid = (vertices[0] + vertices[1] + vertices[2]) / 3.0f;

    const bool is_front_only = (shader->emission_sampling == EMISSION_SAMPLING_FRONT);
    const bool is_back_only = (shader->emission_sampling == EMISSION_SAMPLING_BACK);
    if (is_front_only || is_back_only) {
      /* One-sided. */
      measure.bcone.axis = safe_normalize(
          cross(vertices[1] - vertices[0], vertices[2] - vertices[0]));
      if (is_back_only) {
        measure.bcone.axis = -measure.bcone.axis;
      }
      if (transform_negative_scale(object->get_tfm())) {
        measure.bcone.axis = -measure.bcone.axis;
      }
      measure.bcone.theta_o = 0;
    }
    else {
      /* Double sided: any vector in the plane. */
      measure.bcone.axis = safe_normalize(vertices[0] - vertices[1]);
      measure.bcone.theta_o = M_PI_2_F;
    }
    measure.bcone.theta_e = M_PI_2_F;

    for (int i = 0; i < 3; i++) {
      measure.bbox.grow(vertices[i]);
    }
  }
  else {
    Light *lamp = scene->lights[object_id];
    LightType type = lamp->get_light_type();
    const float size = lamp->get_size();
    float3 strength = lamp->get_strength();

    centroid = scene->lights[object_id]->get_co();
    measure.bcone.axis = normalize(lamp->get_dir());

    if (type == LIGHT_AREA) {
      measure.bcone.theta_o = 0;
      measure.bcone.theta_e = lamp->get_spread() * 0.5f;

      /* For an area light, sizeu and sizev determine the 2 dimensions of the area light,
       * while axisu and axisv determine the orientation of the 2 dimensions.
       * We want to add all 4 corners to our bounding box. */
      const float3 half_extentu = 0.5f * lamp->get_sizeu() * lamp->get_axisu() * size;
      const float3 half_extentv = 0.5f * lamp->get_sizev() * lamp->get_axisv() * size;
      measure.bbox.grow(centroid + half_extentu + half_extentv);
      measure.bbox.grow(centroid + half_extentu - half_extentv);
      measure.bbox.grow(centroid - half_extentu + half_extentv);
      measure.bbox.grow(centroid - half_extentu - half_extentv);

      strength *= 0.25f; /* eval_fac scaling in `area.h` */
    }
    else if (type == LIGHT_POINT) {
      measure.bcone.theta_o = M_PI_F;
      measure.bcone.theta_e = M_PI_2_F;

      /* Point and spot lights can emit light from any point within its radius. */
      const float3 radius = make_float3(size);
      measure.bbox.grow(centroid - radius);
      measure.bbox.grow(centroid + radius);

      strength *= 0.25f * M_1_PI_F; /* eval_fac scaling in `spot.h` and `point.h` */
    }
    else if (type == LIGHT_SPOT) {
      measure.bcone.theta_o = 0;

      const float unscaled_theta_e = lamp->get_spot_angle() * 0.5f;
      const float len_u = len(lamp->get_axisu());
      const float len_v = len(lamp->get_axisv());
      const float len_w = len(lamp->get_dir());

      measure.bcone.theta_e = fast_atanf(fast_tanf(unscaled_theta_e) * fmaxf(len_u, len_v) /
                                         len_w);

      /* Point and spot lights can emit light from any point within its radius. */
      const float3 radius = make_float3(size);
      measure.bbox.grow(centroid - radius);
      measure.bbox.grow(centroid + radius);

      strength *= 0.25f * M_1_PI_F; /* eval_fac scaling in `spot.h` and `point.h` */
    }
    else if (type == LIGHT_BACKGROUND) {
      /* Set an arbitrary direction for the background light. */
      measure.bcone.axis = make_float3(0.0f, 0.0f, 1.0f);
      /* TODO: this may depend on portal lights as well. */
      measure.bcone.theta_o = M_PI_F;
      measure.bcone.theta_e = 0;

      /* integrate over cosine-weighted hemisphere */
      strength *= lamp->get_average_radiance() * M_PI_F;
    }
    else if (type == LIGHT_DISTANT) {
      measure.bcone.theta_o = 0;
      measure.bcone.theta_e = 0.5f * lamp->get_angle();
    }

    if (lamp->get_shader()) {
      strength *= lamp->get_shader()->emission_estimate;
    }

    /* Use absolute value of energy so lights with negative strength are properly supported in the
     * light tree. */
    measure.energy = fabsf(average(strength));
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
  const int num_prims = prims.size();
  const int num_local_lights = num_prims - num_distant_lights;

  root_ = create_node(LightTreePrimitivesMeasure::empty, 0);

  /* All local lights are grouped to the left child as an inner node. */
  recursive_build(left, root_.get(), 0, num_local_lights, &prims, 0, 1);
  task_pool.wait_work();

  /* All distant lights are grouped to the right child as a leaf node. */
  root_->children[right] = create_node(LightTreePrimitivesMeasure::empty, 1);
  for (int i = num_local_lights; i < num_prims; i++) {
    root_->children[right]->add(prims[i]);
  }
  root_->children[right]->make_leaf(num_local_lights, num_distant_lights);
}

void LightTree::recursive_build(const Child child,
                                LightTreeNode *parent,
                                const int start,
                                const int end,
                                vector<LightTreePrimitive> *prims,
                                const uint bit_trail,
                                const int depth)
{
  BoundBox centroid_bounds = BoundBox::empty;
  for (int i = start; i < end; i++) {
    centroid_bounds.grow((*prims)[i].centroid);
  }

  parent->children[child] = create_node(LightTreePrimitivesMeasure::empty, bit_trail);
  LightTreeNode *node = parent->children[child].get();

  /* Find the best place to split the primitives into 2 nodes.
   * If the best split cost is no better than making a leaf node, make a leaf instead. */
  int split_dim = -1, middle;
  if (should_split(*prims, start, middle, end, node->measure, centroid_bounds, split_dim)) {

    if (split_dim != -1) {
      /* Partition the primitives between start and end based on the centroids.  */
      std::nth_element(prims->begin() + start,
                       prims->begin() + middle,
                       prims->begin() + end,
                       [split_dim](const LightTreePrimitive &l, const LightTreePrimitive &r) {
                         return l.centroid[split_dim] < r.centroid[split_dim];
                       });
    }

    /* Recursively build the left branch. */
    if (middle - start > MIN_PRIMS_PER_THREAD) {
      task_pool.push(
          [=] { recursive_build(left, node, start, middle, prims, bit_trail, depth + 1); });
    }
    else {
      recursive_build(left, node, start, middle, prims, bit_trail, depth + 1);
    }

    /* Recursively build the right branch. */
    if (end - middle > MIN_PRIMS_PER_THREAD) {
      task_pool.push([=] {
        recursive_build(right, node, middle, end, prims, bit_trail | (1u << depth), depth + 1);
      });
    }
    else {
      recursive_build(right, node, middle, end, prims, bit_trail | (1u << depth), depth + 1);
    }
  }
  else {
    node->make_leaf(start, end - start);
  }
}

bool LightTree::should_split(const vector<LightTreePrimitive> &prims,
                             const int start,
                             int &middle,
                             const int end,
                             LightTreePrimitivesMeasure &measure,
                             const BoundBox &centroid_bbox,
                             int &split_dim)
{
  middle = (start + end) / 2;
  const int num_prims = end - start;
  const float3 extent = centroid_bbox.size();
  const float max_extent = max4(extent.x, extent.y, extent.z, 0.0f);

  /* Check each dimension to find the minimum splitting cost. */
  float total_cost = 0.0f;
  float min_cost = FLT_MAX;
  for (int dim = 0; dim < 3; dim++) {
    /* If the centroid bounding box is 0 along a given dimension, skip it. */
    if (centroid_bbox.size()[dim] == 0.0f && dim != 0) {
      continue;
    }

    const float inv_extent = 1 / (centroid_bbox.size()[dim]);

    /* Fill in buckets with primitives. */
    std::array<LightTreeBucket, LightTreeBucket::num_buckets> buckets;
    for (int i = start; i < end; i++) {
      const LightTreePrimitive &prim = prims[i];

      /* Place primitive into the appropriate bucket, where the centroid box is split into equal
       * partitions. */
      int bucket_idx = LightTreeBucket::num_buckets *
                       (prim.centroid[dim] - centroid_bbox.min[dim]) * inv_extent;
      bucket_idx = clamp(bucket_idx, 0, LightTreeBucket::num_buckets - 1);

      buckets[bucket_idx].add(prim);
    }

    /* Precompute the left bucket measure cumulatively. */
    std::array<LightTreeBucket, LightTreeBucket::num_buckets - 1> left_buckets;
    left_buckets.front() = buckets.front();
    for (int i = 1; i < LightTreeBucket::num_buckets - 1; i++) {
      left_buckets[i] = left_buckets[i - 1] + buckets[i];
    }

    if (dim == 0) {
      /* Calculate node measure by summing up the bucket measure. */
      measure = left_buckets.back().measure + buckets.back().measure;

      /* Do not try to split if there are only one primitive. */
      if (num_prims < 2) {
        return false;
      }

      /* Degenerate case with co-located primitives. */
      if (is_zero(centroid_bbox.size())) {
        break;
      }

      total_cost = measure.calculate();
      if (total_cost == 0.0f) {
        break;
      }
    }

    /* Precompute the right bucket measure cumulatively. */
    std::array<LightTreeBucket, LightTreeBucket::num_buckets - 1> right_buckets;
    right_buckets.back() = buckets.back();
    for (int i = LightTreeBucket::num_buckets - 3; i >= 0; i--) {
      right_buckets[i] = right_buckets[i + 1] + buckets[i + 1];
    }

    /* Calculate the cost of splitting at each point between partitions. */
    const float regularization = max_extent * inv_extent;
    for (int split = 0; split < LightTreeBucket::num_buckets - 1; split++) {
      const float left_cost = left_buckets[split].measure.calculate();
      const float right_cost = right_buckets[split].measure.calculate();
      const float cost = regularization * (left_cost + right_cost);

      if (cost < total_cost && cost < min_cost) {
        min_cost = cost;
        split_dim = dim;
        middle = start + left_buckets[split].count;
      }
    }
  }
  return min_cost < total_cost || num_prims > max_lights_in_leaf_;
}

__forceinline LightTreePrimitivesMeasure operator+(const LightTreePrimitivesMeasure &a,
                                                   const LightTreePrimitivesMeasure &b)
{
  LightTreePrimitivesMeasure c(a);
  c.add(b);
  return c;
}

LightTreeBucket operator+(const LightTreeBucket &a, const LightTreeBucket &b)
{
  return LightTreeBucket(a.measure + b.measure, a.count + b.count);
}

CCL_NAMESPACE_END
