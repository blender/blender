/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Fill the inpainting region by sampling the color of the nearest boundary pixel. Additionally,
 * compute some information about the inpainting region, like the distance to the boundary, as well
 * as the blur radius to use to smooth out that region. */

#include "gpu_shader_compositor_jump_flooding_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_constants_lib.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float4 color = texture_load(input_tx, texel);

  /* An opaque pixel, not part of the inpainting region. */
  if (color.a == 1.0f) {
    imageStore(filled_region_img, texel, color);
    imageStore(smoothing_radius_img, texel, float4(0.0f));
    imageStore(distance_to_boundary_img, texel, float4(0.0f));
    return;
  }

  int2 closest_boundary_texel = texture_load(flooded_boundary_tx, texel).xy;
  float distance_to_boundary = distance(float2(texel), float2(closest_boundary_texel));
  imageStore(distance_to_boundary_img, texel, float4(distance_to_boundary));

  /* We follow this shader by a blur shader that smooths out the inpainting region, where the blur
   * radius is the radius of the circle that touches the boundary. We can imagine the blur window
   * to be inscribed in that circle and thus the blur radius is the distance to the boundary
   * divided by square root two. As a performance optimization, we limit the blurring to areas that
   * will affect the inpainting region, that is, whose distance to boundary is less than double the
   * inpainting distance. Additionally, we clamp to the distance to the inpainting distance since
   * areas outside of the clamp range only indirectly affect the inpainting region due to blurring
   * and thus needn't use higher blur radii. */
  float blur_window_size = min(float(max_distance), distance_to_boundary) / M_SQRT2;
  bool skip_smoothing = distance_to_boundary > (max_distance * 2.0f);
  float smoothing_radius = skip_smoothing ? 0.0f : blur_window_size;
  imageStore(smoothing_radius_img, texel, float4(smoothing_radius));

  /* Mix the boundary color with the original color using its alpha because semi-transparent areas
   * are considered to be partially inpainted. */
  float4 boundary_color = texture_load(input_tx, closest_boundary_texel);
  imageStore(filled_region_img, texel, mix(boundary_color, color, color.a));
}
