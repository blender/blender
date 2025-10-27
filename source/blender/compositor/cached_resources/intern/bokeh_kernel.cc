/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_hash.hh"
#include "BLI_math_base.hh"
#include "BLI_math_numbers.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"

#include "COM_bokeh_kernel.hh"
#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Bokeh Kernel Key.
 */

BokehKernelKey::BokehKernelKey(
    int2 size, int sides, float rotation, float roundness, float catadioptric, float lens_shift)
    : size(size),
      sides(sides),
      rotation(rotation),
      roundness(roundness),
      catadioptric(catadioptric),
      lens_shift(lens_shift)
{
}

uint64_t BokehKernelKey::hash() const
{
  return get_default_hash(
      size, size, get_default_hash(float4(rotation, roundness, catadioptric, lens_shift)));
}

bool operator==(const BokehKernelKey &a, const BokehKernelKey &b)
{
  return a.size == b.size && a.sides == b.sides && a.rotation == b.rotation &&
         a.roundness == b.roundness && a.catadioptric == b.catadioptric &&
         a.lens_shift == b.lens_shift;
}

/* --------------------------------------------------------------------
 * Bokeh Kernel.
 */

BokehKernel::BokehKernel(Context &context,
                         int2 size,
                         int sides,
                         float rotation,
                         float roundness,
                         float catadioptric,
                         float lens_shift)
    : result(context.create_result(ResultType::Color))
{
  this->result.allocate_texture(Domain(size), false);

  if (context.use_gpu()) {
    this->compute_gpu(context, sides, rotation, roundness, catadioptric, lens_shift);
  }
  else {
    this->compute_cpu(sides, rotation, roundness, catadioptric, lens_shift);
  }
}

BokehKernel::~BokehKernel()
{
  this->result.release();
}

/* The exterior angle is the angle between each two consecutive vertices of the regular polygon
 * from its center. */
static float compute_exterior_angle(int sides)
{
  return (math::numbers::pi * 2.0f) / sides;
}

static float compute_rotation(float angle, int sides)
{
  /* Offset the rotation such that the second vertex of the regular polygon lies on the positive
   * y axis, which is 90 degrees minus the angle that it makes with the positive x axis assuming
   * the first vertex lies on the positive x axis. */
  const float offset = (math::numbers::pi / 2.0f) - compute_exterior_angle(sides);
  return angle - offset;
}

void BokehKernel::compute_gpu(Context &context,
                              const int sides,
                              const float rotation,
                              const float roundness,
                              const float catadioptric,
                              const float lens_shift)
{
  gpu::Shader *shader = context.get_shader("compositor_bokeh_image");
  GPU_shader_bind(shader);

  GPU_shader_uniform_1f(shader, "exterior_angle", compute_exterior_angle(sides));
  GPU_shader_uniform_1f(shader, "rotation", compute_rotation(rotation, sides));
  GPU_shader_uniform_1f(shader, "roundness", roundness);
  GPU_shader_uniform_1f(shader, "catadioptric", catadioptric);
  GPU_shader_uniform_1f(shader, "lens_shift", lens_shift);

  this->result.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, this->result.domain().size);

  this->result.unbind_as_image();
  GPU_shader_unbind();
}

/* Get the 2D vertex position of the vertex with the given index in the regular polygon
 * representing this bokeh. The polygon is rotated by the rotation amount and have a unit
 * circumradius. The regular polygon is one whose vertices' exterior angles are given by
 * exterior_angle. See the bokeh function for more information. */
static float2 get_regular_polygon_vertex_position(const int vertex_index,
                                                  const float exterior_angle,
                                                  const float rotation)
{
  float angle = exterior_angle * vertex_index - rotation;
  return float2(math::cos(angle), math::sin(angle));
}

/* Find the closest point to the given point on the given line. This assumes the length of the
 * given line is not zero. */
static float2 closest_point_on_line(const float2 point,
                                    const float2 line_start,
                                    const float2 line_end)
{
  float2 line_vector = line_end - line_start;
  float2 point_vector = point - line_start;
  float line_length_squared = math::dot(line_vector, line_vector);
  float parameter = math::dot(point_vector, line_vector) / line_length_squared;
  return line_start + line_vector * parameter;
}

/* Compute the value of the bokeh at the given point. The computed bokeh is essentially a regular
 * polygon centered in space having the given circumradius. The regular polygon is one whose
 * vertices' exterior angles are given by "exterior_angle", which relates to the number of vertices
 * n through the equation "exterior angle = 2 pi / n". The regular polygon may additionally morph
 * into a shape with the given properties:
 *
 * - The regular polygon may have a circular hole in its center whose radius is controlled by the
 *   "catadioptric" value.
 * - The regular polygon is rotated by the "rotation" value.
 * - The regular polygon can morph into a circle controlled by the "roundness" value, such that it
 *   becomes a full circle at unit roundness.
 *
 * The function returns 0 when the point lies inside the regular polygon and 1 otherwise. However,
 * at the edges, it returns a narrow band gradient as a form of anti-aliasing. */
static float bokeh(const float2 point,
                   const float circumradius,
                   const float exterior_angle,
                   const float rotation,
                   const float roundness,
                   const float catadioptric)
{
  if (circumradius == 0.0f) {
    return 0.0f;
  }

  /* Get the index of the vertex of the regular polygon whose polar angle is maximum but less than
   * the polar angle of the given point, taking rotation into account. This essentially finds the
   * vertex closest to the given point in the clock-wise direction. */
  float angle = math::mod_periodic(math::atan2(point.y, point.x) + rotation,
                                   2.0f * math::numbers::pi_v<float>);
  int vertex_index = int(angle / exterior_angle);

  /* Compute the shortest distance between the origin and the polygon edge composed from the
   * previously selected vertex and the one following it. */
  float2 first_vertex = get_regular_polygon_vertex_position(
                            vertex_index, exterior_angle, rotation) *
                        circumradius;
  float2 second_vertex = get_regular_polygon_vertex_position(
                             vertex_index + 1, exterior_angle, rotation) *
                         circumradius;
  float2 closest_point = closest_point_on_line(point, first_vertex, second_vertex);
  float distance_to_edge = math::length(closest_point);

  /* Mix the distance to the edge with the circumradius, making it tend to the distance to a
   * circle when roundness tends to 1. */
  float distance_to_edge_round = math::interpolate(distance_to_edge, circumradius, roundness);

  /* The point is outside of the bokeh, so we return 0. */
  float distance = math::length(point);
  if (distance > distance_to_edge_round) {
    return 0.0f;
  }

  /* The point is inside the catadioptric hole and is not part of the bokeh, so we return 0. */
  float catadioptric_distance = distance_to_edge_round * catadioptric;
  if (distance < catadioptric_distance) {
    return 0.0f;
  }

  /* The point is very close to the edge of the bokeh, so we return the difference between the
   * distance to the edge and the distance as a form of anti-aliasing. */
  if (distance_to_edge_round - distance < 1.0f) {
    return distance_to_edge_round - distance;
  }

  /* The point is very close to the edge of the catadioptric hole, so we return the difference
   * between the distance to the hole and the distance as a form of anti-aliasing. */
  if (catadioptric != 0.0f && distance - catadioptric_distance < 1.0f) {
    return distance - catadioptric_distance;
  }

  /* Otherwise, the point is part of the bokeh and we return 1. */
  return 1.0f;
}

static float4 spectral_bokeh(const int2 texel,
                             const int2 size,
                             const float exterior_angle,
                             const float rotation,
                             const float roundness,
                             const float catadioptric,
                             const float lens_shift)
{
  /* Since we need the regular polygon to occupy the entirety of the output image, the circumradius
   * of the regular polygon is half the width of the output image. */
  float circumradius = float(size.x) / 2.0f;

  /* Move the texel coordinates such that the regular polygon is centered. */
  float2 point = float2(texel) + float2(0.5f) - circumradius;

  /* Each of the color channels of the output image contains a bokeh with a different circumradius.
   * The largest one occupies the whole image as stated above, while the other two have circumradii
   * that are shifted by an amount that is proportional to the "lens_shift" value. The alpha
   * channel of the output is the average of all three values. */
  float min_shift = math::abs(lens_shift * circumradius);
  float min = bokeh(
      point, circumradius - min_shift, exterior_angle, rotation, roundness, catadioptric);

  float median_shift = min_shift / 2.0f;
  float median = bokeh(
      point, circumradius - median_shift, exterior_angle, rotation, roundness, catadioptric);

  float max = bokeh(point, circumradius, exterior_angle, rotation, roundness, catadioptric);
  float4 bokeh = float4(min, median, max, (max + median + min) / 3.0f);

  /* If the lens shift is negative, swap the min and max bokeh values, which are stored in the red
   * and blue channels respectively. Note that we take the absolute value of the lens shift above,
   * so the sign of the lens shift only controls this swap. */
  if (lens_shift < 0.0f) {
    bokeh = float4(bokeh.z, bokeh.y, bokeh.x, bokeh.w);
  }

  return bokeh;
}

void BokehKernel::compute_cpu(const int sides,
                              const float rotation,
                              const float roundness,
                              const float catadioptric,
                              const float lens_shift)
{
  const int2 size = this->result.domain().size;
  const float exterior_angle = compute_exterior_angle(sides);
  const float corrected_rotation = compute_rotation(rotation, sides);

  parallel_for(size, [&](const int2 texel) {
    const float4 bokeh_value = spectral_bokeh(
        texel, size, exterior_angle, corrected_rotation, roundness, catadioptric, lens_shift);
    this->result.store_pixel(texel, Color(bokeh_value));
  });
}

/* --------------------------------------------------------------------
 * Bokeh Kernel Container.
 */

void BokehKernelContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

Result &BokehKernelContainer::get(Context &context,
                                  int2 size,
                                  int sides,
                                  float rotation,
                                  float roundness,
                                  float catadioptric,
                                  float lens_shift)
{
  const BokehKernelKey key(size, sides, rotation, roundness, catadioptric, lens_shift);

  auto &bokeh_kernel = *map_.lookup_or_add_cb(key, [&]() {
    return std::make_unique<BokehKernel>(
        context, size, sides, rotation, roundness, catadioptric, lens_shift);
  });

  bokeh_kernel.needed = true;
  return bokeh_kernel.result;
}

}  // namespace blender::compositor
