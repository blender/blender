
/**
 * Scatter pass: Use sprites to scatter the color of very bright pixel to have higher quality blur.
 *
 * We only scatter one quad per sprite and one sprite per 4 pixels to reduce vertex shader
 * invocations and overdraw.
 */

#pragma BLENDER_REQUIRE(eevee_depth_of_field_lib.glsl)

#define linearstep(p0, p1, v) (clamp(((v) - (p0)) / abs((p1) - (p0)), 0.0, 1.0))

void main()
{
  vec4 coc4 = vec4(interp.color_and_coc1.w,
                   interp.color_and_coc2.w,
                   interp.color_and_coc3.w,
                   interp.color_and_coc4.w);
  vec4 shapes;
  if (use_bokeh_lut) {
    shapes = vec4(texture(bokeh_lut_tx, interp.rect_uv1).r,
                  texture(bokeh_lut_tx, interp.rect_uv2).r,
                  texture(bokeh_lut_tx, interp.rect_uv3).r,
                  texture(bokeh_lut_tx, interp.rect_uv4).r);
  }
  else {
    shapes = vec4(length(interp.rect_uv1),
                  length(interp.rect_uv2),
                  length(interp.rect_uv3),
                  length(interp.rect_uv4));
  }
  shapes *= interp.distance_scale;
  /* Becomes signed distance field in pixel units. */
  shapes -= coc4;
  /* Smooth the edges a bit to fade out the undersampling artifacts. */
  shapes = saturate(1.0 - linearstep(-0.8, 0.8, shapes));
  /* Outside of bokeh shape. Try to avoid overloading ROPs. */
  if (max_v4(shapes) == 0.0) {
    discard;
    return;
  }

  if (!no_scatter_occlusion) {
    /* Works because target is the same size as occlusion_tx. */
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(occlusion_tx, 0).xy);
    vec2 occlusion_data = texture(occlusion_tx, uv).rg;
    /* Fix tilling artifacts. (Slide 90) */
    const float correction_fac = 1.0 - DOF_FAST_GATHER_COC_ERROR;
    /* Occlude the sprite with geometry from the same field using a chebychev test (slide 85). */
    float mean = occlusion_data.x;
    float variance = occlusion_data.y;
    shapes *= variance * safe_rcp(variance + sqr(max(coc4 * correction_fac - mean, 0.0)));
  }

  out_color = (interp.color_and_coc1 * shapes[0] + interp.color_and_coc2 * shapes[1] +
               interp.color_and_coc3 * shapes[2] + interp.color_and_coc4 * shapes[3]);
  /* Do not accumulate alpha. This has already been accumulated by the gather pass. */
  out_color.a = 0.0;

  if (debug_scatter_perf) {
    out_color.rgb = avg(out_color.rgb) * vec3(1.0, 0.0, 0.0);
  }
}
