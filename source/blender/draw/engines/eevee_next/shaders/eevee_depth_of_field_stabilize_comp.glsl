
/**
 * Temporal Stabilization of the Depth of field input.
 * Corresponds to the TAA pass in the paper.
 *
 * TODO: This pass needs a cleanup / improvement using much better TAA.
 *
 * Inputs:
 * - Output of setup pass (halfres).
 * Outputs:
 * - Stabilized Color and CoC (halfres).
 **/

#pragma BLENDER_REQUIRE(eevee_depth_of_field_lib.glsl)

float fast_luma(vec3 color)
{
  return (2.0 * color.g) + color.r + color.b;
}

/* Lightweight version of neighborhood clamping found in TAA. */
vec3 dof_neighborhood_clamping(vec3 color)
{
  vec2 texel_size = 1.0 / vec2(textureSize(color_tx, 0));
  vec2 uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) * texel_size;
  vec4 ofs = vec4(-1, 1, -1, 1) * texel_size.xxyy;

  /* Luma clamping. 3x3 square neighborhood. */
  float c00 = fast_luma(textureLod(color_tx, uv + ofs.xz, 0.0).rgb);
  float c01 = fast_luma(textureLod(color_tx, uv + ofs.xz * vec2(1.0, 0.0), 0.0).rgb);
  float c02 = fast_luma(textureLod(color_tx, uv + ofs.xw, 0.0).rgb);

  float c10 = fast_luma(textureLod(color_tx, uv + ofs.xz * vec2(0.0, 1.0), 0.0).rgb);
  float c11 = fast_luma(color);
  float c12 = fast_luma(textureLod(color_tx, uv + ofs.xw * vec2(0.0, 1.0), 0.0).rgb);

  float c20 = fast_luma(textureLod(color_tx, uv + ofs.yz, 0.0).rgb);
  float c21 = fast_luma(textureLod(color_tx, uv + ofs.yz * vec2(1.0, 0.0), 0.0).rgb);
  float c22 = fast_luma(textureLod(color_tx, uv + ofs.yw, 0.0).rgb);

  float avg_luma = avg8(c00, c01, c02, c10, c12, c20, c21, c22);
  float max_luma = max8(c00, c01, c02, c10, c12, c20, c21, c22);

  float upper_bound = mix(max_luma, avg_luma, dof_buf.denoise_factor);
  upper_bound = mix(c11, upper_bound, dof_buf.denoise_factor);

  float clamped_luma = min(upper_bound, c11);

  return color * clamped_luma * safe_rcp(c11);
}

void main()
{
  vec2 uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) / vec2(textureSize(color_tx, 0).xy);
  vec4 out_color = textureLod(color_tx, uv, 0.0);
  float out_coc = textureLod(coc_tx, uv, 0.0).r;

  out_color.rgb = dof_neighborhood_clamping(out_color.rgb);
  /* TODO(fclem): Stabilize CoC. */

  ivec2 out_texel = ivec2(gl_GlobalInvocationID.xy);
  imageStore(out_color_img, out_texel, out_color);
  imageStore(out_coc_img, out_texel, vec4(out_coc));
}
