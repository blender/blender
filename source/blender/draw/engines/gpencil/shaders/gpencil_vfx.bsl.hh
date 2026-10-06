/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpencil_common_lib.glsl"
#include "gpu_shader_compat.hh"
#include "gpu_shader_fullscreen.bsl.hh"

namespace grease_pencil::vfx {

/* Match DNA enum. */
enum ColorizeShaderFxModes : int {
  eShaderFxColorizeMode_GrayScale = 0,
  eShaderFxColorizeMode_Sepia = 1,
  eShaderFxColorizeMode_Duotone = 2,
  eShaderFxColorizeMode_Custom = 3,
  eShaderFxColorizeMode_Transparent = 4,
};

enum FxMode : int {
  COMPOSITE = 0,
  COLORIZE = 1,
  BLUR = 2,
  TRANSFORM = 3,
  GLOW = 4,
  RIM = 5,
  SHADOW = 6,
  PIXELIZE = 7,
};

float gaussian_weight(float x)
{
  return exp(-x * x / (2.0f * 0.35f * 0.35f));
}

struct Vfx {
  [[compilation_constant]] int vfx_mode;
  [[sampler(0)]] sampler2D color_buf;
  [[sampler(1)]] sampler2D reveal_buf;
};

struct FragOut {
  /* Reminder: This is considered SRC color in blend equations.
   * Same operation on all buffers. */
  [[frag_color(0)]] float4 color;
  [[frag_color(1)]] float4 revealage;
};

struct Composite {
  [[push_constant]] bool is_first_pass;

  void process(const Vfx &srt, float2 screen_uv, FragOut &frag_out) const
  {
    if (is_first_pass) {
      /* Blend mode is multiply. */
      frag_out.color.rgb = frag_out.revealage.rgb = texture(srt.reveal_buf, screen_uv).rgb;
      frag_out.color.a = frag_out.revealage.a = 1.0f;
    }
    else {
      /* Blend mode is additive. */
      frag_out.revealage = float4(0.0f);
      frag_out.color.rgb = texture(srt.color_buf, screen_uv).rgb;
      frag_out.color.a = 0.0f;
    }
  }
};

struct Colorize {
  [[push_constant]] float3 low_color;
  [[push_constant]] float3 high_color;
  [[push_constant]] float factor;
  [[push_constant]] int mode;

  void process(const Vfx &srt, float2 screen_uv, FragOut &frag_out) const
  {
    frag_out.color = texture(srt.color_buf, screen_uv);
    frag_out.revealage = texture(srt.reveal_buf, screen_uv);

    float luma = dot(frag_out.color.rgb, float3(0.2126f, 0.7152f, 0.723f));

    float3x3 sepia_mat = float3x3(float3(0.393f, 0.349f, 0.272f),
                                  float3(0.769f, 0.686f, 0.534f),
                                  float3(0.189f, 0.168f, 0.131f));
    /* No blending. */
    switch (mode) {
      case eShaderFxColorizeMode_GrayScale:
        frag_out.color.rgb = mix(frag_out.color.rgb, float3(luma), factor);
        break;
      case eShaderFxColorizeMode_Sepia:
        frag_out.color.rgb = mix(frag_out.color.rgb, sepia_mat * frag_out.color.rgb, factor);
        break;
      case eShaderFxColorizeMode_Duotone:
        frag_out.color.rgb = luma * ((luma <= factor) ? low_color : high_color);
        break;
      case eShaderFxColorizeMode_Custom:
        frag_out.color.rgb = mix(frag_out.color.rgb, luma * low_color, factor);
        break;
      case eShaderFxColorizeMode_Transparent:
      default:
        frag_out.color.rgb *= factor;
        frag_out.revealage.rgb = mix(float3(1.0f), frag_out.revealage.rgb, factor);
        break;
    }
  }
};

struct Blur {
  [[push_constant]] float2 offset;
  [[push_constant]] int samp_count;

  void process(const Vfx &srt, float2 screen_uv, FragOut &frag_out) const
  {
    float2 pixel_size = 1.0f / float2(textureSize(srt.reveal_buf, 0).xy);
    float2 ofs = offset * pixel_size;

    frag_out.color = float4(0.0f);
    frag_out.revealage = float4(0.0f);

    /* No blending. */
    float weight_accum = 0.0f;
    for (int i = -samp_count; i <= samp_count; i++) {
      float x = float(i) / float(samp_count);
      float weight = gaussian_weight(x);
      weight_accum += weight;
      float2 uv = screen_uv + ofs * x;
      frag_out.color.rgb += texture(srt.color_buf, uv).rgb * weight;
      frag_out.revealage.rgb += texture(srt.reveal_buf, uv).rgb * weight;
    }

    frag_out.color /= weight_accum;
    frag_out.revealage /= weight_accum;
  }
};

struct Transform {
  [[push_constant]] float2 axis_flip;
  [[push_constant]] float2 wave_dir;
  [[push_constant]] float2 wave_offset;
  [[push_constant]] float wave_phase;
  [[push_constant]] float2 swirl_center;
  [[push_constant]] float swirl_angle;
  [[push_constant]] float swirl_radius;

  void process(const Vfx &srt, float2 screen_uv, FragOut &frag_out) const
  {
    float2 uv = (screen_uv - 0.5f) * axis_flip + 0.5f;

    /* Wave deform. */
    float wave_time = dot(uv, wave_dir.xy);
    uv += sin(wave_time + wave_phase) * wave_offset;
    /* Swirl deform. */
    if (swirl_radius > 0.0f) {
      float2 tex_size = float2(textureSize(srt.color_buf, 0).xy);
      float2 pix_coord = uv * tex_size - swirl_center;
      float dist = length(pix_coord);
      float percent = clamp((swirl_radius - dist) / swirl_radius, 0.0f, 1.0f);
      float theta = percent * percent * swirl_angle;
      float s = sin(theta);
      float c = cos(theta);
      float2x2 rot = float2x2(float2(c, -s), float2(s, c));
      uv = (rot * pix_coord + swirl_center) / tex_size;
    }

    frag_out.color = texture(srt.color_buf, uv);
    frag_out.revealage = texture(srt.reveal_buf, uv);
  }
};

struct Glow {
  [[push_constant]] float4 glow_color;
  [[push_constant]] float2 offset;
  [[push_constant]] int samp_count;
  [[push_constant]] float4 threshold;
  [[push_constant]] bool first_pass;
  [[push_constant]] bool glow_under;
  [[push_constant]] int blend_mode;

  void process(const Vfx &srt, float2 screen_uv, FragOut &frag_out) const
  {
    float2 pixel_size = 1.0f / float2(textureSize(srt.reveal_buf, 0).xy);
    float2 ofs = offset * pixel_size;

    frag_out.color = float4(0.0f);
    frag_out.revealage = float4(0.0f);

    float weight_accum = 0.0f;
    for (int i = -samp_count; i <= samp_count; i++) {
      float x = float(i) / float(samp_count);
      float weight = gaussian_weight(x);
      weight_accum += weight;
      float2 uv = screen_uv + ofs * x;
      float3 col = texture(srt.color_buf, uv).rgb;
      float3 rev = texture(srt.reveal_buf, uv).rgb;
      if (threshold.x > -1.0f) {
        if (threshold.y > -1.0f) {
          if (any(greaterThan(abs(col - threshold.xyz), float3(threshold.w)))) {
            weight = 0.0f;
          }
        }
        else {
          if (dot(col, float3(1.0f / 3.0f)) < threshold.x) {
            weight = 0.0f;
          }
        }
      }
      frag_out.color.rgb += col * weight;
      frag_out.revealage.rgb += (1.0f - rev) * weight;
    }

    if (weight_accum > 0.0f) {
      frag_out.color *= glow_color.rgbb / weight_accum;
      frag_out.revealage = frag_out.revealage / weight_accum;
    }
    frag_out.revealage = 1.0f - frag_out.revealage;

    if (glow_under) {
      if (first_pass) {
        /* In first pass we copy the revealage buffer in the alpha channel.
         * This let us do the alpha under in second pass. */
        float3 original_revealage = texture(srt.reveal_buf, screen_uv).rgb;
        frag_out.revealage.a = clamp(dot(original_revealage.rgb, float3(0.333334f)), 0.0f, 1.0f);
      }
      else {
        /* Recover original revealage. */
        frag_out.revealage.a = texture(srt.reveal_buf, screen_uv).a;
      }
    }
    if (!first_pass) {
      frag_out.color.a = clamp(1.0f - dot(frag_out.revealage.rgb, float3(0.333334f)), 0.0f, 1.0f);
      frag_out.revealage.a *= glow_color.a;
      blend_mode_output(
          blend_mode, frag_out.color, frag_out.revealage.a, frag_out.color, frag_out.revealage);
    }
  }
};

struct Rim {
  [[push_constant]] float2 blur_dir;
  [[push_constant]] float2 uv_offset;
  [[push_constant]] float3 rim_color;
  [[push_constant]] float3 mask_color;
  [[push_constant]] int samp_count;
  [[push_constant]] int blend_mode;
  [[push_constant]] bool is_first_pass;

  void process(const Vfx &srt, float2 screen_uv, FragOut &frag_out) const
  {
    /* Blur revealage buffer. */
    frag_out.revealage = float4(0.0f);
    float weight_accum = 0.0f;
    for (int i = -samp_count; i <= samp_count; i++) {
      float x = float(i) / float(samp_count);
      float weight = gaussian_weight(x);
      weight_accum += weight;
      float2 uv = screen_uv + blur_dir * x + uv_offset;
      float3 col = texture(srt.reveal_buf, uv).rgb;
      if (any(not(equal(float2(0.0f), floor(uv))))) {
        col = float3(0.0f);
      }
      frag_out.revealage.rgb += col * weight;
    }
    frag_out.revealage /= weight_accum;

    if (is_first_pass) {
      /* In first pass we copy the reveal buffer. This let us do alpha masking in second pass. */
      frag_out.color = texture(srt.reveal_buf, screen_uv);
      /* Also add the masked color to the reveal buffer. */
      float3 col = texture(srt.color_buf, screen_uv).rgb;
      if (all(lessThan(abs(col - mask_color), float3(0.05f)))) {
        frag_out.color = float4(1.0f);
      }
    }
    else {
      /* Pre-multiply by foreground alpha (alpha mask). */
      float mask = 1.0f - clamp(dot(float3(0.333334f), texture(srt.color_buf, screen_uv).rgb),
                                0.0f,
                                1.0f);

      /* frag_out.revealage is blurred shadow. */
      float rim = clamp(dot(float3(0.333334f), frag_out.revealage.rgb), 0.0f, 1.0f);

      float4 color = float4(rim_color, 1.0f);

      blend_mode_output(blend_mode, color, rim * mask, frag_out.color, frag_out.revealage);
    }
  }
};

struct Shadow {
  [[push_constant]] float4 shadow_color;
  [[push_constant]] float2 uv_rot_x;
  [[push_constant]] float2 uv_rot_y;
  [[push_constant]] float2 uv_offset;
  [[push_constant]] float2 blur_dir;
  [[push_constant]] float2 wave_dir;
  [[push_constant]] float2 wave_offset;
  [[push_constant]] float wave_phase;
  [[push_constant]] int samp_count;
  [[push_constant]] bool is_first_pass;

  float2 compute_uvs(float2 screen_uv, float x) const
  {
    float2 uv = screen_uv;
    /* Transform UV (loc, rot, scale) */
    uv = uv.x * uv_rot_x + uv.y * uv_rot_y + uv_offset;
    uv += blur_dir * x;
    /* Wave deform. */
    float wave_time = dot(uv, wave_dir.xy);
    uv += sin(wave_time + wave_phase) * wave_offset;
    return uv;
  }

  void process(const Vfx &srt, float2 screen_uv, FragOut &frag_out) const
  {
    /* Blur revealage buffer. */
    frag_out.revealage = float4(0.0f);
    float weight_accum = 0.0f;
    for (int i = -samp_count; i <= samp_count; i++) {
      float x = float(i) / float(samp_count);
      float weight = gaussian_weight(x);
      weight_accum += weight;
      float2 uv = compute_uvs(screen_uv, x);
      float3 col = texture(srt.reveal_buf, uv).rgb;
      if (any(not(equal(float2(0.0f), floor(uv))))) {
        col = float3(1.0f);
      }
      frag_out.revealage.rgb += col * weight;
    }
    frag_out.revealage /= weight_accum;

    /* No blending in first pass, alpha over pre-multiply in second pass. */
    if (is_first_pass) {
      /* In first pass we copy the reveal buffer. This let us do alpha under in second pass. */
      frag_out.color = texture(srt.reveal_buf, screen_uv);
    }
    else {
      /* frag_out.revealage is blurred shadow. */
      float shadow_fac = 1.0f - clamp(dot(float3(0.333334f), frag_out.revealage.rgb), 0.0f, 1.0f);
      /* Pre-multiply by foreground revealage (alpha under). */
      float3 original_revealage = texture(srt.color_buf, screen_uv).rgb;
      shadow_fac *= clamp(dot(float3(0.333334f), original_revealage), 0.0f, 1.0f);
      /* Modulate by opacity */
      shadow_fac *= shadow_color.a;
      /* Apply shadow color. */
      frag_out.color.rgb = mix(float3(0.0f), shadow_color.rgb, shadow_fac);
      /* Alpha over (mask behind the shadow). */
      frag_out.color.a = shadow_fac;

      frag_out.revealage.rgb = original_revealage * (1.0f - shadow_fac);
      /* Replace the whole revealage buffer. */
      frag_out.revealage.a = 1.0f;
    }
  }
};

struct Pixelize {
  [[push_constant]] float2 target_pixel_size;
  [[push_constant]] float2 target_pixel_offset;
  [[push_constant]] float2 accum_offset;
  [[push_constant]] int samp_count;

  void process(const Vfx &srt, float2 screen_uv, FragOut &frag_out) const
  {
    float2 pixel = floor((screen_uv - target_pixel_offset) / target_pixel_size);
    float2 uv = (pixel + 0.5f) * target_pixel_size + target_pixel_offset;

    frag_out.color = float4(0.0f);
    frag_out.revealage = float4(0.0f);

    for (int i = -samp_count; i <= samp_count; i++) {
      float x = float(i) / float(samp_count + 1);
      float2 uv_ofs = uv + accum_offset * 0.5f * x;
      frag_out.color += texture(srt.color_buf, uv_ofs);
      frag_out.revealage += texture(srt.reveal_buf, uv_ofs);
    }

    frag_out.color /= float(samp_count) * 2.0f + 1.0f;
    frag_out.revealage /= float(samp_count) * 2.0f + 1.0f;
  }
};

[[fragment]]
void frag([[resource_table]] const Vfx &srt,
          [[resource_table]] [[condition(vfx_mode == COMPOSITE)]] const Composite &composite,
          [[resource_table]] [[condition(vfx_mode == COLORIZE)]] const Colorize &colorize,
          [[resource_table]] [[condition(vfx_mode == BLUR)]] const Blur &blur,
          [[resource_table]] [[condition(vfx_mode == TRANSFORM)]] const Transform &transform,
          [[resource_table]] [[condition(vfx_mode == GLOW)]] const Glow &glow,
          [[resource_table]] [[condition(vfx_mode == RIM)]] const Rim &rim,
          [[resource_table]] [[condition(vfx_mode == SHADOW)]] const Shadow &shadow,
          [[resource_table]] [[condition(vfx_mode == PIXELIZE)]] const Pixelize &pixelize,
          [[frag_coord]] const float4 frag_co,
          [[out]] FragOut &frag_out)
{
  float2 screen_uv = frag_co.xy / float2(textureSize(srt.color_buf, 0).xy);

  if (srt.vfx_mode == COMPOSITE) [[static_branch]] {
    composite.process(srt, screen_uv, frag_out);
  }
  else if (srt.vfx_mode == COLORIZE) [[static_branch]] {
    colorize.process(srt, screen_uv, frag_out);
  }
  else if (srt.vfx_mode == BLUR) [[static_branch]] {
    blur.process(srt, screen_uv, frag_out);
  }
  else if (srt.vfx_mode == TRANSFORM) [[static_branch]] {
    transform.process(srt, screen_uv, frag_out);
  }
  else if (srt.vfx_mode == GLOW) [[static_branch]] {
    glow.process(srt, screen_uv, frag_out);
  }
  else if (srt.vfx_mode == RIM) [[static_branch]] {
    rim.process(srt, screen_uv, frag_out);
  }
  else if (srt.vfx_mode == SHADOW) [[static_branch]] {
    shadow.process(srt, screen_uv, frag_out);
  }
  else if (srt.vfx_mode == PIXELIZE) [[static_branch]] {
    pixelize.process(srt, screen_uv, frag_out);
  }
}

[[vertex]]
void vert([[vertex_id]] const int vert_id, [[position]] float4 &position)
{
  fullscreen_vertex(vert_id, position);
}

}  // namespace grease_pencil::vfx

PipelineGraphic gpencil_fx_composite(grease_pencil::vfx::vert,
                                     grease_pencil::vfx::frag,
                                     grease_pencil::vfx::Vfx{.vfx_mode = 0 /* COMPOSITE */});
PipelineGraphic gpencil_fx_colorize(grease_pencil::vfx::vert,
                                    grease_pencil::vfx::frag,
                                    grease_pencil::vfx::Vfx{.vfx_mode = 1 /* COLORIZE */});
PipelineGraphic gpencil_fx_blur(grease_pencil::vfx::vert,
                                grease_pencil::vfx::frag,
                                grease_pencil::vfx::Vfx{.vfx_mode = 2 /* BLUR */});
PipelineGraphic gpencil_fx_transform(grease_pencil::vfx::vert,
                                     grease_pencil::vfx::frag,
                                     grease_pencil::vfx::Vfx{.vfx_mode = 3 /* TRANSFORM */});
PipelineGraphic gpencil_fx_glow(grease_pencil::vfx::vert,
                                grease_pencil::vfx::frag,
                                grease_pencil::vfx::Vfx{.vfx_mode = 4 /* GLOW */});
PipelineGraphic gpencil_fx_rim(grease_pencil::vfx::vert,
                               grease_pencil::vfx::frag,
                               grease_pencil::vfx::Vfx{.vfx_mode = 5 /* RIM */});
PipelineGraphic gpencil_fx_shadow(grease_pencil::vfx::vert,
                                  grease_pencil::vfx::frag,
                                  grease_pencil::vfx::Vfx{.vfx_mode = 6 /* SHADOW */});
PipelineGraphic gpencil_fx_pixelize(grease_pencil::vfx::vert,
                                    grease_pencil::vfx::frag,
                                    grease_pencil::vfx::Vfx{.vfx_mode = 7 /* PIXELIZE */});
