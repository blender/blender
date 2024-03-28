/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

/* Get the 2D vertex position of the vertex with the given index in the regular polygon
 * representing this bokeh. The polygon is rotated by the rotation amount and have a unit
 * circumradius. The regular polygon is one whose vertices' exterior angles are given by
 * exterior_angle. See the bokeh function for more information. */
vec2 get_regular_polygon_vertex_position(int vertex_index)
{
  float angle = exterior_angle * vertex_index - rotation;
  return vec2(cos(angle), sin(angle));
}

/* Find the closest point to the given point on the given line. This assumes the length of the
 * given line is not zero. */
vec2 closest_point_on_line(vec2 point, vec2 line_start, vec2 line_end)
{
  vec2 line_vector = line_end - line_start;
  vec2 point_vector = point - line_start;
  float line_length_squared = dot(line_vector, line_vector);
  float parameter = dot(point_vector, line_vector) / line_length_squared;
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
float bokeh(vec2 point, float circumradius)
{
  /* Get the index of the vertex of the regular polygon whose polar angle is maximum but less than
   * the polar angle of the given point, taking rotation into account. This essentially finds the
   * vertex closest to the given point in the clock-wise direction. */
  float angle = mod(atan(point.y, point.x) + rotation, M_2PI);
  int vertex_index = int(angle / exterior_angle);

  /* Compute the shortest distance between the origin and the polygon edge composed from the
   * previously selected vertex and the one following it. */
  vec2 first_vertex = get_regular_polygon_vertex_position(vertex_index) * circumradius;
  vec2 second_vertex = get_regular_polygon_vertex_position(vertex_index + 1) * circumradius;
  vec2 closest_point = closest_point_on_line(point, first_vertex, second_vertex);
  float distance_to_edge = length(closest_point);

  /* Mix the distance to the edge with the circumradius, making it tend to the distance to a
   * circle when roundness tends to 1. */
  float distance_to_edge_round = mix(distance_to_edge, circumradius, roundness);

  /* The point is outside of the bokeh, so we return 0. */
  float distance = length(point);
  if (distance > distance_to_edge_round) {
    return 0.0;
  }

  /* The point is inside the catadioptric hole and is not part of the bokeh, so we return 0. */
  float catadioptric_distance = distance_to_edge_round * catadioptric;
  if (distance < catadioptric_distance) {
    return 0.0;
  }

  /* The point is very close to the edge of the bokeh, so we return the difference between the
   * distance to the edge and the distance as a form of anti-aliasing. */
  if (distance_to_edge_round - distance < 1.0) {
    return distance_to_edge_round - distance;
  }

  /* The point is very close to the edge of the catadioptric hole, so we return the difference
   * between the distance to the hole and the distance as a form of anti-aliasing. */
  if (catadioptric != 0.0 && distance - catadioptric_distance < 1.0) {
    return distance - catadioptric_distance;
  }

  /* Otherwise, the point is part of the bokeh and we return 1. */
  return 1.0;
}

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Since we need the regular polygon to occupy the entirety of the output image, the circumradius
   * of the regular polygon is half the width of the output image. */
  float circumradius = float(imageSize(output_img).x) / 2.0;

  /* Move the texel coordinates such that the regular polygon is centered. */
  vec2 point = vec2(texel) + vec2(0.5) - circumradius;

  /* Each of the color channels of the output image contains a bokeh with a different circumradius.
   * The largest one occupies the whole image as stated above, while the other two have circumradii
   * that are shifted by an amount that is proportional to the "lens_shift" value. The alpha
   * channel of the output is the average of all three values. */
  float min_shift = abs(lens_shift * circumradius);
  float min = mix(bokeh(point, circumradius - min_shift), 0.0, min_shift == circumradius);

  float median_shift = min_shift / 2.0;
  float median = bokeh(point, circumradius - median_shift);

  float max = bokeh(point, circumradius);
  vec4 bokeh = vec4(min, median, max, (max + median + min) / 3.0);

  /* If the lens shift is negative, swap the min and max bokeh values, which are stored in the red
   * and blue channels respectively. Note that we take the absolute value of the lens shift above,
   * so the sign of the lens shift only controls this swap. */
  if (lens_shift < 0) {
    bokeh = bokeh.zyxw;
  }

  imageStore(output_img, texel, bokeh);
}
