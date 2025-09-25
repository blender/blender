/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_widget_infos.hh"

#include "gpu_shader_colorspace_lib.glsl"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_widget_shared)

float3 compute_masks(float2 uv)
{
  bool upper_half = uv.y > outRectSize.y * 0.5f;
  bool right_half = uv.x > outRectSize.x * 0.5f;
  float corner_rad;

  /* Correct aspect ratio for 2D views not using uniform scaling.
   * uv is already in pixel space so a uniform scale should give us a ratio of 1. */
  float ratio = (butCo != -2.0f) ? abs(gpu_dfdy(uv.y) / gpu_dfdx(uv.x)) : 1.0f;
  float2 uv_sdf = uv;
  uv_sdf.x *= ratio;

  if (right_half) {
    uv_sdf.x = outRectSize.x * ratio - uv_sdf.x;
  }
  if (upper_half) {
    uv_sdf.y = outRectSize.y - uv_sdf.y;
    corner_rad = right_half ? outRoundCorners.z : outRoundCorners.w;
  }
  else {
    corner_rad = right_half ? outRoundCorners.y : outRoundCorners.x;
  }

  /* Fade emboss at the border. */
  float emboss_size = upper_half ? 0.0f : min(1.0f, uv_sdf.x / (corner_rad * ratio));

  /* Signed distance field from the corner (in pixel).
   * inner_sdf is sharp and outer_sdf is rounded. */
  uv_sdf -= corner_rad;
  float inner_sdf = max(0.0f, min(uv_sdf.x, uv_sdf.y));
  float outer_sdf = -length(min(uv_sdf, 0.0f));
  float sdf = inner_sdf + outer_sdf + corner_rad;

  /* Clamp line width to be at least 1px wide. This can happen if the projection matrix
   * has been scaled (i.e: Node editor)... */
  float line_width = (lineWidth > 0.0f) ? max(gpu_fwidth(uv.y), lineWidth) : 0.0f;

  constexpr float aa_radius = 0.5f;
  float3 masks;
  masks.x = smoothstep(-aa_radius, aa_radius, sdf);
  masks.y = smoothstep(-aa_radius, aa_radius, sdf - line_width);
  masks.z = smoothstep(-aa_radius, aa_radius, sdf + line_width * emboss_size);

  /* Compose masks together to avoid having too much alpha. */
  masks.zx = max(float2(0.0f), masks.zx - masks.xy);

  return masks;
}

float4 do_checkerboard()
{
  float size = checkerColorAndSize.z;
  float2 phase = mod(gl_FragCoord.xy, size * 2.0f);

  if ((phase.x > size && phase.y < size) || (phase.x < size && phase.y > size)) {
    return float4(checkerColorAndSize.xxx, 1.0f);
  }
  else {
    return float4(checkerColorAndSize.yyy, 1.0f);
  }
}

void main()
{
  if (min(1.0f, -butCo) > discardFac) {
    gpu_discard_fragment();
  }

  float3 masks = compute_masks(uvInterp);

  if (butCo > 0.0f) {
    /* Alpha checker widget. */
    if (butCo > 0.5f) {
      float4 checker = do_checkerboard();
      fragColor = mix(checker, innerColor, innerColor.a);
    }
    else {
      /* Set alpha to 1.0f. */
      fragColor = innerColor;
    }
    fragColor.a = 1.0f;
  }
  else {
    /* Pre-multiply here. */
    fragColor = innerColor * float4(innerColor.aaa, 1.0f);
  }
  fragColor *= masks.y;
  fragColor += masks.x * borderColor;
  fragColor += masks.z * embossColor;

  /* Un-pre-multiply because the blend equation is already doing the multiplication. */
  if (fragColor.a > 0.0f) {
    fragColor.rgb /= fragColor.a;
  }

  fragColor = blender_srgb_to_framebuffer_space(fragColor);
}
