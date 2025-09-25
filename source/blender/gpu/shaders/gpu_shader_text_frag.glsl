/* SPDX-FileCopyrightText: 2016-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_text_infos.hh"

#include "gpu_shader_colorspace_lib.glsl"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_text)

/* Font texture is conceptually laid out like a big 1D buffer: each glyph
 * rectangle is flattened in row-major order into a "pixel strip". Inside
 * the texture, glyphs strips are put one after another. The texture pixel
 * rows can conceptually be treated as a really wide 1D texture.
 *
 * Because of all this, texture filtering has to be implemented manually,
 * as well as checks for whether filtering samples fall outside of the
 * glyph rectangle. */

float texel_fetch(int index)
{
  int2 texel = int2(index & glyph_tex_width_mask, index >> glyph_tex_width_shift);
  return texelFetch(glyph, texel, 0).r;
}

bool is_inside_box(int2 v)
{
  return all(greaterThanEqual(v, int2(0))) && all(lessThan(v, glyph_dim));
}

float sample_glyph_bilinear(float2 bilin_f, float2 uv)
{
  int2 texel = int2(floor(uv)) - 1;
  int index = glyph_offset + texel.y * glyph_dim.x + texel.x;

  /* Fetch 2x2 texels for filtering. */
  int offset_x = 1;
  int offset_y = glyph_dim.x;
  float tl = texel_fetch(index);
  float tr = texel_fetch(index + offset_x);
  float bl = texel_fetch(index + offset_y);
  float br = texel_fetch(index + offset_x + offset_y);

  /* Texels outside of glyph box: zero. */
  if (!is_inside_box(texel)) {
    tl = 0.0f;
  }
  if (!is_inside_box(texel + int2(1, 0))) {
    tr = 0.0f;
  }
  if (!is_inside_box(texel + int2(0, 1))) {
    bl = 0.0f;
  }
  if (!is_inside_box(texel + int2(1, 1))) {
    br = 0.0f;
  }

  /* Bilinear filter. */
  float tA = mix(tl, tr, bilin_f.x);
  float tB = mix(bl, br, bilin_f.x);
  return mix(tA, tB, bilin_f.y);
}

float4 sample_glyph_rgba(float2 uv)
{
  int2 texel = int2(round(uv)) - 1;

  float4 col = float4(0.0f);
  if (is_inside_box(texel)) {
    int index = glyph_offset + (texel.y * glyph_dim.x + texel.x) * 4;
    col.r = texel_fetch(index);
    col.g = texel_fetch(index + 1);
    col.b = texel_fetch(index + 2);
    col.a = texel_fetch(index + 3);
  }
  return col;
}

void main()
{
  float2 uv_base = texCoord_interp;
  uint num_channels = (glyph_flags >> 4) & 0xFu;
  uint shadow_type = glyph_flags & 0xFu;

  /* Colored glyphs: do not do filtering or blurring. */
  if (num_channels == 4) {
    fragColor.rgba = sample_glyph_rgba(uv_base).rgba;
    fragColor.a *= color_flat.a;
    return;
  }

  float2 bilin_f = fract(uv_base);

  fragColor.rgb = color_flat.rgb;

  if (shadow_type == 0) {
    /* No blurring: just a bilinear sample. */
    fragColor.a = sample_glyph_bilinear(bilin_f, uv_base);
  }
  else {

    /* Blurring or dilation: will fetch (N+1)x(N+1) are of glyph texels,
     * shifting the filter kernel weights by bilinear fraction. */
    fragColor.a = 0.0f;

    int2 texel = int2(floor(uv_base)) - 1;
    int frag_offset = glyph_offset + texel.y * glyph_dim.x + texel.x;

    if (shadow_type == 6) {
      /* 3x3 outline by dilation */

      float maxval = 0.0f;
      for (int iy = 0; iy < 4; ++iy) {
        int ofsy = iy - 1;
        for (int ix = 0; ix < 4; ++ix) {
          int ofsx = ix - 1;
          float v = texel_fetch(frag_offset + ofsy * glyph_dim.x + ofsx);
          if (!is_inside_box(texel + int2(ofsx, ofsy))) {
            v = 0.0f;
          }

          /* Bilinearly compute weight for this sample. */
          float w00 = ix < 3 && iy < 3 ? 1.0f : 0.0f;
          float w10 = ix > 0 && iy < 3 ? 1.0f : 0.0f;
          float w01 = ix < 3 && iy > 0 ? 1.0f : 0.0f;
          float w11 = ix > 0 && iy > 0 ? 1.0f : 0.0f;
          float w = mix(mix(w00, w10, bilin_f.x), mix(w01, w11, bilin_f.x), bilin_f.y);

          maxval = max(maxval, v * w);
        }
      }
      fragColor.a = maxval;
    }
    else if (shadow_type <= 4) {
      /* 3x3 blur */

      /* clang-format off */
      constexpr float weights3x3[16] = float_array(
        1.0f, 2.0f, 1.0f, 0.0f,
        2.0f, 4.0f, 2.0f, 0.0f,
        1.0f, 2.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f
      );
      /* clang-format on */

      float sum = 0.0f;
      int idx = 0;
      for (int iy = 0; iy < 4; ++iy) {
        int ofsy = iy - 1;
        for (int ix = 0; ix < 4; ++ix) {
          int ofsx = ix - 1;
          float v = texel_fetch(frag_offset + ofsy * glyph_dim.x + ofsx);
          if (!is_inside_box(texel + int2(ofsx, ofsy))) {
            v = 0.0f;
          }

          /* Bilinearly compute filter weight for this sample. */
          float w00 = weights3x3[idx];
          float w10 = ix > 0 ? weights3x3[idx - 1] : 0.0f;
          float w01 = iy > 0 ? weights3x3[idx - 4] : 0.0f;
          float w11 = ix > 0 && iy > 0 ? weights3x3[idx - 5] : 0.0f;
          float w = mix(mix(w00, w10, bilin_f.x), mix(w01, w11, bilin_f.x), bilin_f.y);

          sum += v * w;
          ++idx;
        }
      }
      fragColor.a = sum * (1.0f / 16.0f);
    }
    else {
      /* 5x5 blur */

      /* clang-format off */
      constexpr float weights5x5[36] = float_array(
        1.0f, 2.0f, 2.0f, 2.0f, 1.0f, 0.0f,
        2.0f, 5.0f, 6.0f, 5.0f, 2.0f, 0.0f,
        2.0f, 6.0f, 8.0f, 6.0f, 2.0f, 0.0f,
        2.0f, 5.0f, 6.0f, 5.0f, 2.0f, 0.0f,
        1.0f, 2.0f, 2.0f, 2.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
      );
      /* clang-format on */

      float sum = 0.0f;
      int idx = 0;
      for (int iy = 0; iy < 6; ++iy) {
        int ofsy = iy - 2;
        for (int ix = 0; ix < 6; ++ix) {
          int ofsx = ix - 2;
          float v = texel_fetch(frag_offset + ofsy * glyph_dim.x + ofsx);
          if (!is_inside_box(texel + int2(ofsx, ofsy))) {
            v = 0.0f;
          }

          /* Bilinearly compute filter weight for this sample. */
          float w00 = weights5x5[idx];
          float w10 = ix > 0 ? weights5x5[idx - 1] : 0.0f;
          float w01 = iy > 0 ? weights5x5[idx - 6] : 0.0f;
          float w11 = ix > 0 && iy > 0 ? weights5x5[idx - 7] : 0.0f;
          float w = mix(mix(w00, w10, bilin_f.x), mix(w01, w11, bilin_f.x), bilin_f.y);

          sum += v * w;
          ++idx;
        }
      }
      fragColor.a = sum * (1.0f / 80.0f);
    }
  }

  fragColor.a *= color_flat.a;
  fragColor = blender_srgb_to_framebuffer_space(fragColor);
}
