/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Given a depth texture, compute the radius of the circle of confusion in pixels based on equation
 * (8) of the paper:
 *
 *   Potmesil, Michael, and Indranil Chakravarty. "A lens and aperture camera model for synthetic
 *   image generation." ACM SIGGRAPH Computer Graphics 15.3 (1981): 297-305. */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  float depth = texture_load(depth_tx, texel).x;

  /* Compute `Vu` in equation (7). */
  const float distance_to_image_of_object = (focal_length * depth) / (depth - focal_length);

  /* Compute C in equation (8). Notice that the last multiplier was included in the absolute since
   * it is negative when the object distance is less than the focal length, as noted in equation
   * (7). */
  float diameter = abs((distance_to_image_of_object - distance_to_image_of_focus) *
                       (focal_length / (f_stop * distance_to_image_of_object)));

  /* The diameter is in meters, so multiply by the pixels per meter. */
  float radius = (diameter / 2.0) * pixels_per_meter;

  imageStore(radius_img, texel, vec4(min(max_radius, radius)));
}
