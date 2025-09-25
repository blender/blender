/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Scatter pass: Use sprites to scatter the color of very bright pixel to have higher quality blur.
 *
 * We only scatter one quad per sprite and one sprite per 4 pixels to reduce vertex shader
 * invocations and overdraw.
 */

#include "infos/eevee_depth_of_field_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_depth_of_field_scatter)

#include "eevee_depth_of_field_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

#define linearstep(p0, p1, v) (clamp(((v) - (p0)) / abs((p1) - (p0)), 0.0f, 1.0f))

void main()
{
  float4 coc4 = float4(interp_flat.color_and_coc1.w,
                       interp_flat.color_and_coc2.w,
                       interp_flat.color_and_coc3.w,
                       interp_flat.color_and_coc4.w);
  float4 shapes;
  if (use_bokeh_lut) {
    shapes = float4(texture(bokeh_lut_tx, interp_noperspective.rect_uv1).r,
                    texture(bokeh_lut_tx, interp_noperspective.rect_uv2).r,
                    texture(bokeh_lut_tx, interp_noperspective.rect_uv3).r,
                    texture(bokeh_lut_tx, interp_noperspective.rect_uv4).r);
  }
  else {
    shapes = float4(length(interp_noperspective.rect_uv1),
                    length(interp_noperspective.rect_uv2),
                    length(interp_noperspective.rect_uv3),
                    length(interp_noperspective.rect_uv4));
  }
  shapes *= interp_flat.distance_scale;
  /* Becomes signed distance field in pixel units. */
  shapes -= coc4;
  /* Smooth the edges a bit to fade out the undersampling artifacts. */
  shapes = saturate(1.0f - linearstep(-0.8f, 0.8f, shapes));
  /* Outside of bokeh shape. Try to avoid overloading ROPs. */
  if (reduce_max(shapes) == 0.0f) {
    gpu_discard_fragment();
    return;
  }

  if (!no_scatter_occlusion) {
    /* Works because target is the same size as occlusion_tx. */
    float2 uv = gl_FragCoord.xy / float2(textureSize(occlusion_tx, 0).xy);
    float2 occlusion_data = texture(occlusion_tx, uv).rg;
    /* Fix tilling artifacts. (Slide 90) */
    constexpr float correction_fac = 1.0f - DOF_FAST_GATHER_COC_ERROR;
    /* Occlude the sprite with geometry from the same field using a chebychev test (slide 85). */
    float mean = occlusion_data.x;
    float variance = occlusion_data.y;
    shapes *= variance * safe_rcp(variance + square(max(coc4 * correction_fac - mean, 0.0f)));
  }

  out_color = (interp_flat.color_and_coc1 * shapes[0] + interp_flat.color_and_coc2 * shapes[1] +
               interp_flat.color_and_coc3 * shapes[2] + interp_flat.color_and_coc4 * shapes[3]);
  /* Do not accumulate alpha. This has already been accumulated by the gather pass. */
  out_color.a = 0.0f;

  if (debug_scatter_perf) {
    out_color.rgb = average(out_color.rgb) * float3(1.0f, 0.0f, 0.0f);
  }
}
