
/**
 * Setup pass: CoC and luma aware downsample to half resolution of the input scene color buffer.
 *
 * An addition to the downsample CoC, we output the maximum slight out of focus CoC to be
 * sure we don't miss a pixel.
 */

#pragma BLENDER_REQUIRE(effect_dof_lib.glsl)

void main()
{
  DEFINE_DOF_QUAD_OFFSETS
  vec2 fullres_texel_size = 1.0 / vec2(textureSize(colorBuffer, 0).xy);
  /* Center uv around the 4 fullres pixels. */
  vec2 quad_center = (floor(gl_FragCoord.xy) * 2.0 + 1.0) * fullres_texel_size;

  vec4 colors[4];
  vec4 depths;
  for (int i = 0; i < 4; i++) {
    vec2 sample_uv = quad_center + quad_offsets[i] * fullres_texel_size;
    colors[i] = safe_color(textureLod(colorBuffer, sample_uv, 0.0));
    depths[i] = textureLod(depthBuffer, sample_uv, 0.0).r;
  }

  vec4 cocs = dof_coc_from_zdepth(depths);

  cocs = clamp(cocs, -bokehMaxSize, bokehMaxSize);

  vec4 weights = dof_downsample_bilateral_coc_weights(cocs);
  weights *= dof_downsample_bilateral_color_weights(colors);
  /* Normalize so that the sum is 1. */
  weights *= safe_rcp(sum(weights));

  outColor = weighted_sum_array(colors, weights);
  outCoc.x = dot(cocs, weights);

  /* Max slight focus abs CoC. */

  /* Clamp to 0.5 if full in defocus to differentiate full focus tiles with coc == 0.0.
   * This enables an optimization in the resolve pass. */
  const vec4 threshold = vec4(layer_threshold + layer_offset);
  cocs = abs(cocs);
  bvec4 defocus = greaterThan(cocs, threshold);
  bvec4 focus = lessThanEqual(cocs, vec4(0.5));
  if (any(defocus) && any(focus)) {
    /* For the same reason as in the flatten pass. This is a case we cannot optimize for. */
    cocs = select(cocs, vec4(DOF_TILE_MIXED), focus);
    cocs = select(cocs, vec4(DOF_TILE_MIXED), defocus);
  }
  else {
    cocs = select(cocs, vec4(DOF_TILE_FOCUS), focus);
    cocs = select(cocs, vec4(DOF_TILE_DEFOCUS), defocus);
  }
  outCoc.y = max_v4(cocs);
}
