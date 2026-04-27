/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <algorithm>

#include "bvh/bvh.h"

#include "scene/curves.h"
#include "scene/hair.h"
#include "scene/object.h"
#include "scene/scene.h"

#include "integrator/shader_eval.h"

#include "util/progress.h"
#include "util/tbb.h"

CCL_NAMESPACE_BEGIN

/* Hair Curve */
void Hair::Curve::bounds_grow(const int k, const float4 *keys, BoundBox &bounds) const
{
  float3 P[4];

  P[0] = make_float3(keys[max(first_key + k - 1, first_key)]);
  P[1] = make_float3(keys[first_key + k]);
  P[2] = make_float3(keys[first_key + k + 1]);
  P[3] = make_float3(keys[min(first_key + k + 2, first_key + num_keys - 1)]);

  float3 lower;
  float3 upper;

  curvebounds(&lower.x, &upper.x, P, 0);
  curvebounds(&lower.y, &upper.y, P, 1);
  curvebounds(&lower.z, &upper.z, P, 2);

  const float mr = max(keys[1].w, keys[2].w);

  bounds.grow(lower, mr);
  bounds.grow(upper, mr);
}

void Hair::Curve::bounds_grow(const int k,
                              const packed_float3 *curve_keys,
                              const float *curve_radius,
                              BoundBox &bounds) const
{
  float3 P[4];

  P[0] = curve_keys[max(first_key + k - 1, first_key)];
  P[1] = curve_keys[first_key + k];
  P[2] = curve_keys[first_key + k + 1];
  P[3] = curve_keys[min(first_key + k + 2, first_key + num_keys - 1)];

  float3 lower;
  float3 upper;

  curvebounds(&lower.x, &upper.x, P, 0);
  curvebounds(&lower.y, &upper.y, P, 1);
  curvebounds(&lower.z, &upper.z, P, 2);

  const float mr = max(curve_radius[first_key + k], curve_radius[first_key + k + 1]);

  bounds.grow(lower, mr);
  bounds.grow(upper, mr);
}

void Hair::Curve::bounds_grow(const int k,
                              const packed_float3 *curve_keys,
                              const float *curve_radius,
                              const Transform &aligned_space,
                              BoundBox &bounds) const
{
  float3 P[4];

  P[0] = curve_keys[max(first_key + k - 1, first_key)];
  P[1] = curve_keys[first_key + k];
  P[2] = curve_keys[first_key + k + 1];
  P[3] = curve_keys[min(first_key + k + 2, first_key + num_keys - 1)];

  P[0] = transform_point(&aligned_space, P[0]);
  P[1] = transform_point(&aligned_space, P[1]);
  P[2] = transform_point(&aligned_space, P[2]);
  P[3] = transform_point(&aligned_space, P[3]);

  float3 lower;
  float3 upper;

  curvebounds(&lower.x, &upper.x, P, 0);
  curvebounds(&lower.y, &upper.y, P, 1);
  curvebounds(&lower.z, &upper.z, P, 2);

  const float mr = max(curve_radius[first_key + k], curve_radius[first_key + k + 1]);

  bounds.grow(lower, mr);
  bounds.grow(upper, mr);
}

void Hair::Curve::bounds_grow(const float4 keys[4], BoundBox &bounds) const
{
  float3 P[4] = {
      make_float3(keys[0]),
      make_float3(keys[1]),
      make_float3(keys[2]),
      make_float3(keys[3]),
  };

  float3 lower;
  float3 upper;

  curvebounds(&lower.x, &upper.x, P, 0);
  curvebounds(&lower.y, &upper.y, P, 1);
  curvebounds(&lower.z, &upper.z, P, 2);

  const float mr = max(keys[1].w, keys[2].w);

  bounds.grow(lower, mr);
  bounds.grow(upper, mr);
}

/* Get position and radius arrays for a given time-ordered motion step. */
static void hair_step_buffers(const Attribute *attr_P,
                              const Attribute *attr_R,
                              const size_t step,
                              const packed_float3 *&P,
                              const float *&R)
{
  /* Radius motion follows the position motion steps, if it has fewer steps the
   * read falls back to the center radius. */
  const int num_steps = attr_P->num_motion_steps();
  P = attr_P->data_at_time_step<packed_float3>(step, num_steps);
  R = attr_R->data_at_time_step<float>(step, num_steps);
}

void Hair::Curve::motion_keys(const Attribute *attr_P,
                              const Attribute *attr_R,
                              const size_t num_steps,
                              const float time,
                              size_t k0,
                              size_t k1,
                              float4 r_keys[2]) const
{
  /* Figure out which steps we need to fetch and their interpolation factor. */
  const size_t max_step = num_steps - 1;
  const size_t step = std::min((size_t)(time * max_step), max_step - 1);
  const float t = time * max_step - step;
  /* Fetch vertex coordinates. */
  float4 curr_keys[2];
  float4 next_keys[2];
  keys_for_step(attr_P, attr_R, step, k0, k1, curr_keys);
  keys_for_step(attr_P, attr_R, step + 1, k0, k1, next_keys);
  /* Interpolate between steps. */
  r_keys[0] = (1.0f - t) * curr_keys[0] + t * next_keys[0];
  r_keys[1] = (1.0f - t) * curr_keys[1] + t * next_keys[1];
}

void Hair::Curve::cardinal_motion_keys(const Attribute *attr_P,
                                       const Attribute *attr_R,
                                       const size_t num_steps,
                                       const float time,
                                       size_t k0,
                                       size_t k1,
                                       size_t k2,
                                       size_t k3,
                                       float4 r_keys[4]) const
{
  /* Figure out which steps we need to fetch and their interpolation factor. */
  const size_t max_step = num_steps - 1;
  const size_t step = min((size_t)(time * max_step), max_step - 1);
  const float t = time * max_step - step;
  /* Fetch vertex coordinates. */
  float4 curr_keys[4];
  float4 next_keys[4];
  cardinal_keys_for_step(attr_P, attr_R, step, k0, k1, k2, k3, curr_keys);
  cardinal_keys_for_step(attr_P, attr_R, step + 1, k0, k1, k2, k3, next_keys);
  /* Interpolate between steps. */
  r_keys[0] = (1.0f - t) * curr_keys[0] + t * next_keys[0];
  r_keys[1] = (1.0f - t) * curr_keys[1] + t * next_keys[1];
  r_keys[2] = (1.0f - t) * curr_keys[2] + t * next_keys[2];
  r_keys[3] = (1.0f - t) * curr_keys[3] + t * next_keys[3];
}

void Hair::Curve::keys_for_step(const Attribute *attr_P,
                                const Attribute *attr_R,
                                const size_t step,
                                size_t k0,
                                size_t k1,
                                float4 r_keys[2]) const
{
  k0 = max(k0, (size_t)0);
  k1 = min(k1, (size_t)(num_keys - 1));
  const packed_float3 *P;
  const float *R;
  hair_step_buffers(attr_P, attr_R, step, P, R);
  r_keys[0] = make_float4(P[first_key + k0], R[first_key + k0]);
  r_keys[1] = make_float4(P[first_key + k1], R[first_key + k1]);
}

void Hair::Curve::cardinal_keys_for_step(const Attribute *attr_P,
                                         const Attribute *attr_R,
                                         const size_t step,
                                         size_t k0,
                                         size_t k1,
                                         size_t k2,
                                         size_t k3,
                                         float4 r_keys[4]) const
{
  k0 = max(k0, (size_t)0);
  k3 = min(k3, (size_t)(num_keys - 1));
  const packed_float3 *P;
  const float *R;
  hair_step_buffers(attr_P, attr_R, step, P, R);
  r_keys[0] = make_float4(P[first_key + k0], R[first_key + k0]);
  r_keys[1] = make_float4(P[first_key + k1], R[first_key + k1]);
  r_keys[2] = make_float4(P[first_key + k2], R[first_key + k2]);
  r_keys[3] = make_float4(P[first_key + k3], R[first_key + k3]);
}

/* Hair */

NODE_DEFINE(Hair)
{
  NodeType *type = NodeType::add("hair", create, NodeType::NONE, Geometry::get_node_base_type());

  SOCKET_INT_ARRAY(curve_first_key, "Curve First Key", array<int>());
  SOCKET_INT_ARRAY(curve_shader, "Curve Shader", array<int>());

  return type;
}

Hair::Hair() : Geometry(get_node_type(), Geometry::HAIR)
{
  curve_segment_offset = 0;
  curve_shape = CURVE_RIBBON;

  add_builtin_attributes();
}

Hair::~Hair() = default;

void Hair::add_builtin_attributes()
{
  attributes.add(ATTR_STD_POSITION);
  attributes.add(ATTR_STD_RADIUS);
}

void Hair::resize_curves(const int numcurves, const int numkeys)
{
  Attribute *attr_P = attributes.add(ATTR_STD_POSITION);
  attr_P->resize(numkeys);
  Attribute *attr_R = attributes.add(ATTR_STD_RADIUS);
  attr_R->resize(numkeys);
  curve_first_key.resize(numcurves);
  curve_shader.resize(numcurves);

  attributes.resize();
}

void Hair::clear(bool preserve_shaders)
{
  Geometry::clear(preserve_shaders);

  curve_first_key.clear();
  curve_shader.clear();

  attributes.clear();
  add_builtin_attributes();
}

void Hair::copy_center_to_motion_step(const int motion_step)
{
  const int attr_step = motion_step + 1;
  const size_t numkeys = num_keys();

  Attribute *attr_P = attributes.find(ATTR_STD_POSITION);
  if (attr_P->has_motion()) {
    std::copy_n(get_position(), numkeys, attr_P->data_for_write<packed_float3>(attr_step));
  }

  Attribute *attr_R = attributes.find(ATTR_STD_RADIUS);
  if (attr_R->has_motion()) {
    std::copy_n(get_radius(), numkeys, attr_R->data_for_write<float>(attr_step));
  }

  Attribute *attr_vN = attributes.find(ATTR_STD_VERTEX_NORMAL);
  if (attr_vN && attr_vN->has_motion()) {
    std::copy_n(attr_vN->data<packed_normal>(),
                numkeys,
                attr_vN->data_for_write<packed_normal>(attr_step));
  }
}

void Hair::get_uv_tiles(ustring map, unordered_set<int> &tiles)
{
  Attribute *attr;

  if (map.empty()) {
    attr = attributes.find(ATTR_STD_UV);
  }
  else {
    attr = attributes.find(map);
  }

  if (attr) {
    attr->get_uv_tiles(this, ATTR_PRIM_GEOMETRY, tiles);
  }
}

void Hair::compute_bounds()
{
  BoundBox bnds = BoundBox::empty;
  const size_t curve_keys_size = num_keys();
  const packed_float3 *curve_keys_data = get_position();
  const float *curve_radius_data = get_radius();
  const size_t curve_num = num_curves();

  if (curve_keys_size > 0) {
    bnds.grow(parallel_reduce(
        blocked_range<size_t>(0, curve_num),
        BoundBox(BoundBox::empty),
        [&](const blocked_range<size_t> &range, const BoundBox &partial_bounds) {
          BoundBox current_bounds = partial_bounds;
          for (size_t i = range.begin(); i < range.end(); ++i) {
            const Curve curve = get_curve(i);
            const int num_segments = curve.num_segments();
            for (int k = 0; k < num_segments; k++) {
              curve.bounds_grow(k, curve_keys_data, curve_radius_data, current_bounds);
            }
          }
          return current_bounds;
        },
        [](const BoundBox &bounds_a, const BoundBox &bounds_b) {
          BoundBox combined_bounds = bounds_a;
          combined_bounds.grow(bounds_b);
          return combined_bounds;
        }));

    const Attribute *attr_P = attributes.find(ATTR_STD_POSITION);
    if (use_motion_blur && attr_P->has_motion()) {
      for (int attr_step = 1; attr_step < attr_P->num_motion_steps(); attr_step++) {
        const packed_float3 *key_step = attr_P->data<packed_float3>(attr_step);
        for (size_t i = 0; i < curve_keys_size; i++) {
          bnds.grow(key_step[i]);
        }
      }
    }

    if (!bnds.valid()) {
      bnds = BoundBox::empty;

      /* skip nan or inf coordinates */
      for (size_t i = 0; i < curve_keys_size; i++) {
        bnds.grow_safe(curve_keys_data[i], curve_radius_data[i]);
      }

      if (use_motion_blur && attr_P->has_motion()) {
        for (int attr_step = 1; attr_step < attr_P->num_motion_steps(); attr_step++) {
          const packed_float3 *key_step = attr_P->data<packed_float3>(attr_step);
          for (size_t i = 0; i < curve_keys_size; i++) {
            bnds.grow_safe(key_step[i]);
          }
        }
      }
    }
  }

  if (!bnds.valid()) {
    /* empty mesh */
    bnds.grow(zero_float3());
  }

  bounds = bnds;
}

void Hair::apply_transform(const Transform &tfm, const bool apply_to_motion)
{
  /* compute uniform scale */
  const float3 c0 = transform_get_column(&tfm, 0);
  const float3 c1 = transform_get_column(&tfm, 1);
  const float3 c2 = transform_get_column(&tfm, 2);
  const float scalar = powf(fabsf(dot(cross(c0, c1), c2)), 1.0f / 3.0f);

  /* apply transform to curve keys */
  packed_float3 *keys = get_position_for_write();
  float *radius = get_radius_for_write();
  const size_t numkeys = num_keys();
  for (size_t i = 0; i < numkeys; i++) {
    const float3 co = transform_point(&tfm, keys[i]);
    const float r = radius[i] * scalar;

    /* scale for curve radius is only correct for uniform scale */
    keys[i] = co;
    radius[i] = r;
  }

  tag_position_modified();
  tag_radius_modified();

  if (apply_to_motion) {
    Attribute *attr_P = attributes.find(ATTR_STD_POSITION);
    Attribute *attr_R = attributes.find(ATTR_STD_RADIUS);

    if (attr_P->has_motion()) {
      const bool has_motion_radius = attr_R->has_motion();
      const size_t nk = num_keys();
      for (int step = 1; step <= int(attr_P->motion.size()); step++) {
        packed_float3 *motion_P = attr_P->data_for_write<packed_float3>(step);
        float *motion_R = has_motion_radius ? attr_R->data_for_write<float>(step) : nullptr;
        for (size_t i = 0; i < nk; i++) {
          motion_P[i] = transform_point(&tfm, motion_P[i]);
          if (motion_R) {
            motion_R[i] *= scalar;
          }
        }
      }
    }
  }
}

void Hair::pack_curves(Scene *scene, KernelCurve *curves, KernelCurveSegment *curve_segments)
{
  /* pack curve segments */
  const PrimitiveType type = primitive_type();

  const size_t curve_num = num_curves();
  size_t index = 0;

  for (size_t i = 0; i < curve_num; i++) {
    const Curve curve = get_curve(i);
    int shader_id = curve_shader[i];
    Shader *shader = (shader_id < used_shaders.size()) ?
                         static_cast<Shader *>(used_shaders[shader_id]) :
                         scene->default_surface;
    shader_id = scene->shader_manager->get_shader_id(shader, false);

    curves[i].shader_id = shader_id;
    curves[i].first_key = curve.first_key;
    curves[i].num_keys = curve.num_keys;
    curves[i].type = type;

    for (int k = 0; k < curve.num_segments(); ++k, ++index) {
      curve_segments[index].prim = prim_offset + i;
      curve_segments[index].type = PRIMITIVE_PACK_SEGMENT(type, k);
    }
  }
}

PrimitiveType Hair::primitive_type() const
{
  return has_motion_blur() ?
             ((curve_shape == CURVE_RIBBON)       ? PRIMITIVE_MOTION_CURVE_RIBBON :
              (curve_shape == CURVE_THICK_LINEAR) ? PRIMITIVE_MOTION_CURVE_THICK_LINEAR :
                                                    PRIMITIVE_MOTION_CURVE_THICK) :
             ((curve_shape == CURVE_RIBBON)       ? PRIMITIVE_CURVE_RIBBON :
              (curve_shape == CURVE_THICK_LINEAR) ? PRIMITIVE_CURVE_THICK_LINEAR :
                                                    PRIMITIVE_CURVE_THICK);
}

/* Fill in coordinates for curve transparency shader evaluation on device. */
static int fill_shader_input(const Hair *hair,
                             const size_t object_index,
                             device_vector<KernelShaderEvalInput> &d_input)
{
  int d_input_size = 0;
  KernelShaderEvalInput *d_input_data = d_input.data();

  const int num_curves = hair->num_curves();
  for (int i = 0; i < num_curves; i++) {
    const Hair::Curve curve = hair->get_curve(i);
    const int num_segments = curve.num_segments();

    for (int j = 0; j < num_segments + 1; j++) {
      KernelShaderEvalInput in;
      in.object = object_index;
      in.prim = hair->prim_offset + i;
      in.u = (j < num_segments) ? 0.0f : 1.0f;
      in.v = (j < num_segments) ? __int_as_float(j) : __int_as_float(j - 1);
      d_input_data[d_input_size++] = in;
    }
  }

  return d_input_size;
}

/* Read back curve transparency shader output. */
static void read_shader_output(float *shadow_transparency,
                               bool &is_fully_opaque,
                               const device_vector<float> &d_output)
{
  const int num_keys = d_output.size();
  const float *output_data = d_output.data();
  bool is_opaque = true;

  for (int i = 0; i < num_keys; i++) {
    shadow_transparency[i] = output_data[i];
    if (shadow_transparency[i] > 0.0f) {
      is_opaque = false;
    }
  }

  is_fully_opaque = is_opaque;
}

bool Hair::need_shadow_transparency() const
{
  if (!is_traceable()) {
    return false;
  }

  for (const Node *node : used_shaders) {
    const Shader *shader = static_cast<const Shader *>(node);
    if (shader->has_surface_transparent && shader->get_use_transparent_shadow()) {
      return true;
    }
  }

  return false;
}

bool Hair::need_update_shadow_transparency() const
{
  if (attributes.find(ATTR_STD_SHADOW_TRANSPARENCY) == nullptr) {
    return true;
  }

  for (const Node *node : used_shaders) {
    const Shader *shader = static_cast<const Shader *>(node);
    if (shader->need_update_shadow_transparency) {
      return true;
    }
  }

  return false;
}

bool Hair::update_shadow_transparency(Device *device, Scene *scene, Progress &progress)
{
  if (!need_shadow_transparency()) {
    /* If no shaders with shadow transparency, remove attribute. */
    Attribute *attr = attributes.find(ATTR_STD_SHADOW_TRANSPARENCY);
    if (attr) {
      attributes.remove(attr);
      return true;
    }
    return false;
  }

  if (!is_modified() && !need_update_shadow_transparency()) {
    /* Neither geometry nor shader is modified, no need to update. */
    return false;
  }

  const string msg = string_printf("Computing Shadow Transparency %s", name.c_str());
  progress.set_status("Updating Hair", msg);

  /* Create shadow transparency attribute. */
  Attribute *attr = attributes.find(ATTR_STD_SHADOW_TRANSPARENCY);
  const bool attribute_exists = (attr != nullptr);
  if (!attribute_exists) {
    attr = attributes.add(ATTR_STD_SHADOW_TRANSPARENCY);
  }

  float *attr_data = attr->data_for_write<float>();

  /* Find object index. */
  size_t object_index = OBJECT_NONE;

  for (size_t i = 0; i < scene->objects.size(); i++) {
    if (scene->objects[i]->get_geometry() == this) {
      object_index = i;
      break;
    }
  }

  /* Evaluate shader on device. */
  ShaderEval shader_eval(device, progress);
  bool is_fully_opaque = false;
  shader_eval.eval(
      SHADER_EVAL_CURVE_SHADOW_TRANSPARENCY,
      num_keys(),
      1,
      [this, object_index](device_vector<KernelShaderEvalInput> &d_input) {
        return fill_shader_input(this, object_index, d_input);
      },
      [attr_data, &is_fully_opaque](const device_vector<float> &d_output) {
        read_shader_output(attr_data, is_fully_opaque, d_output);
      });

  if (is_fully_opaque) {
    attributes.remove(attr);
    return attribute_exists;
  }

  return true;
}

CCL_NAMESPACE_END
