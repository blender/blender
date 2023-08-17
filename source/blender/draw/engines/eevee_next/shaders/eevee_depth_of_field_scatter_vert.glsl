
/**
 * Scatter pass: Use sprites to scatter the color of very bright pixel to have higher quality blur.
 *
 * We only scatter one triangle per sprite and one sprite per 4 pixels to reduce vertex shader
 * invocations and overdraw.
 **/

#pragma BLENDER_REQUIRE(eevee_depth_of_field_lib.glsl)

void main()
{
  ScatterRect rect = scatter_list_buf[gl_InstanceID];

  interp_flat.color_and_coc1 = rect.color_and_coc[0];
  interp_flat.color_and_coc2 = rect.color_and_coc[1];
  interp_flat.color_and_coc3 = rect.color_and_coc[2];
  interp_flat.color_and_coc4 = rect.color_and_coc[3];

  vec2 uv = vec2(gl_VertexID & 1, gl_VertexID >> 1) * 2.0 - 1.0;
  uv = uv * rect.half_extent;

  gl_Position = vec4(uv + rect.offset, 0.0, 1.0);
  /* NDC range [-1..1]. */
  gl_Position.xy = (gl_Position.xy / vec2(textureSize(occlusion_tx, 0).xy)) * 2.0 - 1.0;

  if (use_bokeh_lut) {
    /* Bias scale to avoid sampling at the texture's border. */
    interp_flat.distance_scale = (float(DOF_BOKEH_LUT_SIZE) / float(DOF_BOKEH_LUT_SIZE - 1));
    vec2 uv_div = 1.0 / (interp_flat.distance_scale * abs(rect.half_extent));
    interp_noperspective.rect_uv1 = ((uv + quad_offsets[0]) * uv_div) * 0.5 + 0.5;
    interp_noperspective.rect_uv2 = ((uv + quad_offsets[1]) * uv_div) * 0.5 + 0.5;
    interp_noperspective.rect_uv3 = ((uv + quad_offsets[2]) * uv_div) * 0.5 + 0.5;
    interp_noperspective.rect_uv4 = ((uv + quad_offsets[3]) * uv_div) * 0.5 + 0.5;
    /* Only for sampling. */
    interp_flat.distance_scale *= max_v2(abs(rect.half_extent));
  }
  else {
    interp_flat.distance_scale = 1.0;
    interp_noperspective.rect_uv1 = uv + quad_offsets[0];
    interp_noperspective.rect_uv2 = uv + quad_offsets[1];
    interp_noperspective.rect_uv3 = uv + quad_offsets[2];
    interp_noperspective.rect_uv4 = uv + quad_offsets[3];
  }
}
