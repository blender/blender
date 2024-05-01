/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_BokehImageOperation.h"

#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_numbers.hh"
#include "BLI_math_vector.hh"

namespace blender::compositor {

BokehImageOperation::BokehImageOperation()
{
  this->add_output_socket(DataType::Color);
  delete_data_ = false;
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
  const float offset = math::numbers::pi / 2.0f - compute_exterior_angle(sides);
  return angle - offset;
}

void BokehImageOperation::init_execution()
{
  exterior_angle_ = compute_exterior_angle(data_->flaps);
  rotation_ = compute_rotation(data_->angle, data_->flaps);
  roundness_ = data_->rounding;
  catadioptric_ = data_->catadioptric;
  lens_shift_ = data_->lensshift;
}

/* Get the 2D vertex position of the vertex with the given index in the regular polygon
 * representing this bokeh. The polygon is rotated by the rotation amount and have a unit
 * circumradius. The regular polygon is one whose vertices' exterior angles are given by
 * exterior_angle. See the bokeh function for more information. */
float2 BokehImageOperation::get_regular_polygon_vertex_position(int vertex_index)
{
  float angle = exterior_angle_ * vertex_index - rotation_;
  return float2(math::cos(angle), math::sin(angle));
}

/* Find the closest point to the given point on the given line. This assumes the length of the
 * given line is not zero. */
float2 BokehImageOperation::closest_point_on_line(float2 point, float2 line_start, float2 line_end)
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
float BokehImageOperation::bokeh(float2 point, float circumradius)
{
  /* Get the index of the vertex of the regular polygon whose polar angle is maximum but less than
   * the polar angle of the given point, taking rotation into account. This essentially finds the
   * vertex closest to the given point in the clock-wise direction. */
  float angle = floored_fmod(math::atan2(point.y, point.x) + rotation_,
                             math::numbers::pi_v<float> * 2.0f);
  int vertex_index = int(angle / exterior_angle_);

  /* Compute the shortest distance between the origin and the polygon edge composed from the
   * previously selected vertex and the one following it. */
  float2 first_vertex = this->get_regular_polygon_vertex_position(vertex_index) * circumradius;
  float2 second_vertex = this->get_regular_polygon_vertex_position(vertex_index + 1) *
                         circumradius;
  float2 closest_point = this->closest_point_on_line(point, first_vertex, second_vertex);
  float distance_to_edge = math::length(closest_point);

  /* Mix the distance to the edge with the circumradius, making it tend to the distance to a
   * circle when roundness tends to 1. */
  float distance_to_edge_round = math::interpolate(distance_to_edge, circumradius, roundness_);

  /* The point is outside of the bokeh, so we return 0. */
  float distance = math::length(point);
  if (distance > distance_to_edge_round) {
    return 0.0f;
  }

  /* The point is inside the catadioptric hole and is not part of the bokeh, so we return 0. */
  float catadioptric_distance = distance_to_edge_round * catadioptric_;
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
  if (catadioptric_ != 0.0f && distance - catadioptric_distance < 1.0f) {
    return distance - catadioptric_distance;
  }

  /* Otherwise, the point is part of the bokeh and we return 1. */
  return 1.0f;
}

void BokehImageOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> /*inputs*/)
{
  /* Since we need the regular polygon to occupy the entirety of the output image, the circumradius
   * of the regular polygon is half the width of the output image. */
  float circumradius = float(resolution_) / 2.0f;

  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    int2 texel = int2(it.x, it.y);

    /* Move the texel coordinates such that the regular polygon is centered. */
    float2 point = float2(texel) + float2(0.5f) - circumradius;

    /* Each of the color channels of the output image contains a bokeh with a different
     * circumradius. The largest one occupies the whole image as stated above, while the other two
     * have circumradii that are shifted by an amount that is proportional to the "lens_shift"
     * value. The alpha channel of the output is the average of all three values. */
    float min_shift = math::abs(lens_shift_ * circumradius);
    float min = min_shift == circumradius ? 0.0f : this->bokeh(point, circumradius - min_shift);

    float median_shift = min_shift / 2.0f;
    float median = this->bokeh(point, circumradius - median_shift);

    float max = this->bokeh(point, circumradius);
    float4 bokeh = float4(min, median, max, (max + median + min) / 3.0f);

    /* If the lens shift is negative, swap the min and max bokeh values, which are stored in the
     * red and blue channels respectively. Note that we take the absolute value of the lens shift
     * above, so the sign of the lens shift only controls this swap. */
    if (lens_shift_ < 0.0f) {
      std::swap(bokeh.x, bokeh.z);
    }

    copy_v4_v4(it.out, bokeh);
  }
}

void BokehImageOperation::deinit_execution()
{
  if (delete_data_) {
    if (data_) {
      delete data_;
      data_ = nullptr;
    }
  }
}

void BokehImageOperation::determine_canvas(const rcti & /*preferred_area*/, rcti &r_area)
{
  BLI_rcti_init(&r_area, 0, resolution_, 0, resolution_);
}

}  // namespace blender::compositor
