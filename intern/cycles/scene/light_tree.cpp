/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "scene/light_tree.h"
#include "scene/mesh.h"
#include "scene/object.h"

#include "util/progress.h"

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

  float cos_a_b = dot(a->axis, b->axis);
  float theta_d = safe_acosf(cos_a_b);
  float theta_e = fmaxf(a->theta_e, b->theta_e);

  /* Return axis and theta_o of a if it already contains b. */
  /* This should also be called when b is empty. */
  if (a->theta_o + 5e-4f >= fminf(M_PI_F, theta_d + b->theta_o)) {
    return OrientationBounds({a->axis, a->theta_o, theta_e});
  }

  /* Compute new theta_o that contains both a and b. */
  float theta_o = (theta_d + a->theta_o + b->theta_o) * 0.5f;

  if (theta_o >= M_PI_F) {
    return OrientationBounds({a->axis, M_PI_F, theta_e});
  }

  /* Slerp between a and b. */
  float3 new_axis;
  if (cos_a_b < -0.9995f) {
    /* Opposite direction, any orthogonal vector is fine. */
    float3 unused;
    make_orthonormals(a->axis, &new_axis, &unused);
  }
  else {
    float theta_r = theta_o - a->theta_o;
    float3 ortho = safe_normalize(b->axis - a->axis * cos_a_b);
    new_axis = a->axis * cosf(theta_r) + ortho * sinf(theta_r);
  }

  return OrientationBounds({new_axis, theta_o, theta_e});
}

LightTreeEmitter::LightTreeEmitter(Object *object, int object_id) : object_id(object_id)
{
  centroid = object->bounds.center();
  light_set_membership = object->get_light_set_membership();
}

LightTreeEmitter::LightTreeEmitter(Scene *scene,
                                   int prim_id,
                                   int object_id,
                                   bool need_transformation)
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

    if (need_transformation) {
      assert(!mesh->transform_applied);
      const Transform &tfm = object->get_tfm();
      for (int i = 0; i < 3; i++) {
        vertices[i] = transform_point(&tfm, vertices[i]);
      }
    }

    /* TODO: need a better way to handle this when textures are used. */
    float area = triangle_area(vertices[0], vertices[1], vertices[2]);
    measure.energy = area * average(shader->emission_estimate);

    /* NOTE: the original implementation used the bounding box centroid, but triangle centroid
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
      if ((need_transformation || mesh->transform_applied) &&
          transform_negative_scale(object->get_tfm()))
      {
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

    light_set_membership = object->get_light_set_membership();
  }
  else {
    assert(is_light());
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

    light_set_membership = lamp->get_light_set_membership();
  }
}

static void sort_leaf(const int start, const int end, LightTreeEmitter *emitters)
{
  /* Sort primitive by light link mask so that specialized trees can use a subset of these. */
  if (end > start) {
    std::sort(emitters + start,
              emitters + end,
              [](const LightTreeEmitter &a, const LightTreeEmitter &b) {
                return a.light_set_membership < b.light_set_membership;
              });
  }
}

bool LightTree::triangle_usable_as_light(Mesh *mesh, int prim_id)
{
  int shader_index = mesh->get_shader()[prim_id];
  if (shader_index < mesh->get_used_shaders().size()) {
    Shader *shader = static_cast<Shader *>(mesh->get_used_shaders()[shader_index]);
    if (shader->emission_sampling != EMISSION_SAMPLING_NONE) {
      return true;
    }
  }
  return false;
}

void LightTree::add_mesh(Scene *scene, Mesh *mesh, int object_id)
{
  size_t mesh_num_triangles = mesh->num_triangles();
  for (size_t i = 0; i < mesh_num_triangles; i++) {
    if (triangle_usable_as_light(mesh, i)) {
      emitters_.emplace_back(scene, i, object_id);
    }
  }
}

LightTree::LightTree(Scene *scene,
                     DeviceScene *dscene,
                     Progress &progress,
                     uint max_lights_in_leaf)
    : progress_(progress), max_lights_in_leaf_(max_lights_in_leaf)
{
  KernelIntegrator *kintegrator = &dscene->data.integrator;

  local_lights_.reserve(kintegrator->num_lights - kintegrator->num_distant_lights);
  distant_lights_.reserve(kintegrator->num_distant_lights);

  /* When we keep track of the light index, only contributing lights will be added to the device.
   * Therefore, we want to keep track of the light's index on the device.
   * However, we also need the light's index in the scene when we're constructing the tree. */
  int device_light_index = 0;
  int scene_light_index = 0;
  for (Light *light : scene->lights) {
    if (light->is_enabled) {
      if (light->light_type == LIGHT_BACKGROUND || light->light_type == LIGHT_DISTANT) {
        distant_lights_.emplace_back(scene, ~device_light_index, scene_light_index);
      }
      else {
        local_lights_.emplace_back(scene, ~device_light_index, scene_light_index);
      }

      device_light_index++;
    }

    scene_light_index++;
  }

  /* Similarly, we also want to keep track of the index of triangles of emissive objects. */
  int object_id = 0;
  for (Object *object : scene->objects) {
    if (progress_.get_cancel()) {
      return;
    }

    light_link_receiver_used |= (uint64_t(1) << object->get_receiver_light_set());

    if (!object->usable_as_light()) {
      object_id++;
      continue;
    }

    mesh_lights_.emplace_back(object, object_id);
    object_id++;

    /* Only count unique meshes. */
    Mesh *mesh = static_cast<Mesh *>(object->get_geometry());
    auto map_it = offset_map_.find(mesh);
    if (map_it == offset_map_.end()) {
      offset_map_[mesh] = num_triangles;
      num_triangles += mesh->num_triangles();
    }
  }
}

LightTreeNode *LightTree::build(Scene *scene, DeviceScene *dscene)
{
  if (local_lights_.empty() && distant_lights_.empty() && mesh_lights_.empty()) {
    return nullptr;
  }

  const int num_mesh_lights = mesh_lights_.size();
  int num_local_lights = local_lights_.size() + num_mesh_lights;
  const int num_distant_lights = distant_lights_.size();

  /* Create a node for each mesh light, and keep track of unique mesh lights. */
  std::unordered_map<Mesh *, std::tuple<LightTreeNode *, int, int>> unique_mesh;
  uint *object_offsets = dscene->object_lookup_offset.alloc(scene->objects.size());
  emitters_.reserve(num_triangles + num_local_lights + num_distant_lights);
  for (LightTreeEmitter &emitter : mesh_lights_) {
    Object *object = scene->objects[emitter.object_id];
    Mesh *mesh = static_cast<Mesh *>(object->get_geometry());
    emitter.root = create_node(LightTreeMeasure::empty, 0);

    auto map_it = unique_mesh.find(mesh);
    if (map_it == unique_mesh.end()) {
      const int start = emitters_.size();
      add_mesh(scene, mesh, emitter.object_id);
      const int end = emitters_.size();

      unique_mesh[mesh] = std::make_tuple(emitter.root.get(), start, end);
      emitter.root->object_id = emitter.object_id;
    }
    else {
      emitter.root->make_instance(std::get<0>(map_it->second), emitter.object_id);
    }
    object_offsets[emitter.object_id] = offset_map_[mesh];
  }

  /* Build a subtree for each unique mesh light. */
  parallel_for_each(unique_mesh, [this](auto &map_it) {
    LightTreeNode *node = std::get<0>(map_it.second);
    int start = std::get<1>(map_it.second);
    int end = std::get<2>(map_it.second);
    recursive_build(self, node, start, end, emitters_.data(), 0, 0);
    node->type |= LIGHT_TREE_INSTANCE;
  });
  task_pool.wait_work();

  /* Update measure. */
  parallel_for_each(mesh_lights_, [&](LightTreeEmitter &emitter) {
    Object *object = scene->objects[emitter.object_id];
    Mesh *mesh = static_cast<Mesh *>(object->get_geometry());

    LightTreeNode *reference = std::get<0>(unique_mesh.find(mesh)->second);
    emitter.measure = emitter.root->measure = reference->measure;

    /* Transform measure. The measure is only directly transformable if the transformation has
     * uniform scaling, otherwise recount all the triangles in the mesh with transformation. */
    /* NOTE: in theory only energy needs recalculating: #bbox is available via `object->bounds`,
     * transformation of #bcone is possible. However, the computation involves eigendecomposition
     * and solving a cubic equation (https://doi.org/10.1016/j.nima.2009.11.075 section 3.4), then
     * the angle is derived from the major axis of the resulted right elliptic cone's base, which
     * can be an overestimation. */
    if (!mesh->transform_applied && !emitter.measure.transform(object->get_tfm())) {
      emitter.measure.reset();
      size_t mesh_num_triangles = mesh->num_triangles();
      for (size_t i = 0; i < mesh_num_triangles; i++) {
        if (triangle_usable_as_light(mesh, i)) {
          emitter.measure.add(LightTreeEmitter(scene, i, emitter.object_id, true).measure);
        }
      }
    }
  });

  for (LightTreeEmitter &emitter : mesh_lights_) {
    emitter.root->measure = emitter.measure;
  }

  /* Could be different from `num_triangles` if only some triangles of an object are emissive. */
  const int num_emissive_triangles = emitters_.size();
  num_local_lights += num_emissive_triangles;

  /* Build the top level tree. */
  root_ = create_node(LightTreeMeasure::empty, 0);

  /* All local lights and mesh lights are grouped to the left child as an inner node. */
  std::move(local_lights_.begin(), local_lights_.end(), std::back_inserter(emitters_));
  std::move(mesh_lights_.begin(), mesh_lights_.end(), std::back_inserter(emitters_));
  recursive_build(
      left, root_.get(), num_emissive_triangles, num_local_lights, emitters_.data(), 0, 1);
  task_pool.wait_work();

  /* All distant lights are grouped to the right child as a leaf node. */
  root_->get_inner().children[right] = create_node(LightTreeMeasure::empty, 1);
  for (int i = 0; i < num_distant_lights; i++) {
    root_->get_inner().children[right]->add(distant_lights_[i]);
  }

  sort_leaf(0, num_distant_lights, distant_lights_.data());
  root_->get_inner().children[right]->make_distant(num_local_lights, num_distant_lights);

  root_->measure = root_->get_inner().children[left]->measure +
                   root_->get_inner().children[right]->measure;
  root_->light_link = root_->get_inner().children[left]->light_link +
                      root_->get_inner().children[right]->light_link;

  /* Root nodes are never meant to be be shared, even if the local and distant lights are from the
   * same light linking set. Attempting to sharing it will make it so the specialized tree will
   * try to use the same root as the default tree. */
  root_->light_link.shareable = false;

  std::move(distant_lights_.begin(), distant_lights_.end(), std::back_inserter(emitters_));

  return root_.get();
}

void LightTree::recursive_build(const Child child,
                                LightTreeNode *inner,
                                const int start,
                                const int end,
                                LightTreeEmitter *emitters,
                                const uint bit_trail,
                                const int depth)
{
  if (progress_.get_cancel()) {
    return;
  }

  LightTreeNode *node;
  if (child == self) {
    /* Building subtree. */
    node = inner;
  }
  else {
    inner->get_inner().children[child] = create_node(LightTreeMeasure::empty, bit_trail);
    node = inner->get_inner().children[child].get();
  }

  /* Find the best place to split the emitters into 2 nodes.
   * If the best split cost is no better than making a leaf node, make a leaf instead. */
  int split_dim = -1, middle;
  if (should_split(emitters, start, middle, end, node->measure, node->light_link, split_dim)) {

    if (split_dim != -1) {
      /* Partition the emitters between start and end based on the centroids.  */
      std::nth_element(emitters + start,
                       emitters + middle,
                       emitters + end,
                       [split_dim](const LightTreeEmitter &l, const LightTreeEmitter &r) {
                         return l.centroid[split_dim] < r.centroid[split_dim];
                       });
    }

    /* Recursively build the left branch. */
    if (middle - start > MIN_EMITTERS_PER_THREAD) {
      task_pool.push(
          [=] { recursive_build(left, node, start, middle, emitters, bit_trail, depth + 1); });
    }
    else {
      recursive_build(left, node, start, middle, emitters, bit_trail, depth + 1);
    }

    /* Recursively build the right branch. */
    if (end - middle > MIN_EMITTERS_PER_THREAD) {
      task_pool.push([=] {
        recursive_build(right, node, middle, end, emitters, bit_trail | (1u << depth), depth + 1);
      });
    }
    else {
      recursive_build(right, node, middle, end, emitters, bit_trail | (1u << depth), depth + 1);
    }
  }
  else {
    sort_leaf(start, end, emitters);
    node->make_leaf(start, end - start);
  }
}

bool LightTree::should_split(LightTreeEmitter *emitters,
                             const int start,
                             int &middle,
                             const int end,
                             LightTreeMeasure &measure,
                             LightTreeLightLink &light_link,
                             int &split_dim)
{
  const int num_emitters = end - start;
  if (num_emitters < 2) {
    if (num_emitters) {
      /* Do not try to split if there is only one emitter. */
      measure = emitters[start].measure;
      light_link = LightTreeLightLink(emitters[start].light_set_membership);
    }
    return false;
  }

  middle = (start + end) / 2;

  BoundBox centroid_bbox = BoundBox::empty;
  for (int i = start; i < end; i++) {
    centroid_bbox.grow((emitters + i)->centroid);
  }

  const float3 extent = centroid_bbox.size();
  const float max_extent = max4(extent.x, extent.y, extent.z, 0.0f);

  /* Check each dimension to find the minimum splitting cost. */
  float total_cost = 0.0f;
  float min_cost = FLT_MAX;
  for (int dim = 0; dim < 3; dim++) {
    /* If the centroid bounding box is 0 along a given dimension and the node measure is already
     * computed, skip it. */
    if (centroid_bbox.size()[dim] == 0.0f && dim != 0) {
      continue;
    }

    const float inv_extent = 1 / (centroid_bbox.size()[dim]);

    /* Fill in buckets with emitters. */
    std::array<LightTreeBucket, LightTreeBucket::num_buckets> buckets;
    for (int i = start; i < end; i++) {
      const LightTreeEmitter *emitter = emitters + i;

      /* Place emitter into the appropriate bucket, where the centroid box is split into equal
       * partitions. */
      int bucket_idx = LightTreeBucket::num_buckets *
                       (emitter->centroid[dim] - centroid_bbox.min[dim]) * inv_extent;
      bucket_idx = clamp(bucket_idx, 0, LightTreeBucket::num_buckets - 1);

      buckets[bucket_idx].add(*emitter);
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
      light_link = left_buckets.back().light_link + buckets.back().light_link;

      /* Degenerate case with co-located emitters. */
      if (is_zero(centroid_bbox.size())) {
        break;
      }

      /* If the centroid bounding box is 0 along a given dimension, skip it. */
      if (centroid_bbox.size()[dim] == 0.0f) {
        continue;
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
  return min_cost < total_cost || num_emitters > max_lights_in_leaf_;
}

__forceinline LightTreeMeasure operator+(const LightTreeMeasure &a, const LightTreeMeasure &b)
{
  LightTreeMeasure c(a);
  c.add(b);
  return c;
}

LightTreeBucket operator+(const LightTreeBucket &a, const LightTreeBucket &b)
{
  return LightTreeBucket(a.measure + b.measure, a.light_link + b.light_link, a.count + b.count);
}

LightTreeLightLink operator+(const LightTreeLightLink &a, const LightTreeLightLink &b)
{
  LightTreeLightLink c(a);
  c.add(b);
  return c;
}

CCL_NAMESPACE_END
