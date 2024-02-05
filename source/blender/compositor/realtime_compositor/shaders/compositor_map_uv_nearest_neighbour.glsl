/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec2 uv_coordinates = texture_load(uv_tx, texel).xy;

  vec4 sampled_color = texture(input_tx, uv_coordinates);

  /* The UV texture is assumed to contain an alpha channel as its third channel, since the UV
   * coordinates might be defined in only a subset area of the UV texture as mentioned. In that
   * case, the alpha is typically opaque at the subset area and transparent everywhere else, and
   * alpha pre-multiplication is then performed. This format of having an alpha channel in the UV
   * coordinates is the format used by UV passes in render engines, hence the mentioned logic. */
  float alpha = texture_load(uv_tx, texel).z;

  vec4 result = sampled_color * alpha;

  imageStore(output_img, texel, result);
}
