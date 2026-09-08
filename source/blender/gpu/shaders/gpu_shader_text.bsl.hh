/* SPDX-FileCopyrightText: 2016-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_shader_shared.hh"
#include "gpu_shader_colorspace.bsl.hh"

namespace builtin::text {

bool is_inside_box(int2 box_size, int2 v)
{
  return all(greaterThanEqual(v, int2(0))) && all(lessThan(v, box_size));
}

struct Resources {
  [[push_constant]] float4x4 ModelViewProjectionMatrix;
  [[push_constant]] int glyph_tex_width_mask;
  [[push_constant]] int glyph_tex_width_shift;

  [[sampler(0), frequency(PASS)]] sampler2D glyph;

  [[storage(0, read)]] GlyphQuad (&glyphs)[];

  /* Font texture is conceptually laid out like a big 1D buffer: each glyph
   * rectangle is flattened in row-major order into a "pixel strip". Inside
   * the texture, glyphs strips are put one after another. The texture pixel
   * rows can conceptually be treated as a really wide 1D texture.
   *
   * Because of all this, texture filtering has to be implemented manually,
   * as well as checks for whether filtering samples fall outside of the
   * glyph rectangle. */

  float texel_fetch(int index) const
  {
    int2 texel = int2(index & this->glyph_tex_width_mask, index >> this->glyph_tex_width_shift);
    return texelFetch(glyph, texel, 0).r;
  }

  float sample_glyph_bilinear(int2 glyph_size, int glyph_ofs, float2 bilin_f, float2 uv) const
  {
    int2 texel = int2(floor(uv)) - 1;
    int index = glyph_ofs + texel.y * glyph_size.x + texel.x;

    /* Fetch 2x2 texels for filtering. */
    int offset_x = 1;
    int offset_y = glyph_size.x;
    float tl = texel_fetch(index);
    float tr = texel_fetch(index + offset_x);
    float bl = texel_fetch(index + offset_y);
    float br = texel_fetch(index + offset_x + offset_y);

    /* Texels outside of glyph box: zero. */
    if (!is_inside_box(glyph_size, texel)) {
      tl = 0.0f;
    }
    if (!is_inside_box(glyph_size, texel + int2(1, 0))) {
      tr = 0.0f;
    }
    if (!is_inside_box(glyph_size, texel + int2(0, 1))) {
      bl = 0.0f;
    }
    if (!is_inside_box(glyph_size, texel + int2(1, 1))) {
      br = 0.0f;
    }

    /* Bilinear filter. */
    float tA = mix(tl, tr, bilin_f.x);
    float tB = mix(bl, br, bilin_f.x);
    return mix(tA, tB, bilin_f.y);
  }

  float4 sample_glyph_rgba(int2 glyph_size, int glyph_ofs, float2 uv) const
  {
    int2 texel = int2(round(uv)) - 1;

    float4 col = float4(0.0f);
    if (is_inside_box(glyph_size, texel)) {
      int index = glyph_ofs + (texel.y * glyph_size.x + texel.x) * 4;
      col.r = texel_fetch(index);
      col.g = texel_fetch(index + 1);
      col.b = texel_fetch(index + 2);
      col.a = texel_fetch(index + 3);
    }
    return col;
  }
};

struct VertOut {
  [[flat]] float4 color_flat;
  [[no_perspective]] float2 uv_interp;
  [[flat]] int glyph_offset;
  [[flat]] uint glyph_flags;
  [[flat]] int2 glyph_dim;
};

[[vertex]] void main_vert([[resource_table]] const Resources &srt,
                          [[instance_index]] const int inst_index,
                          [[vertex_id]] const int vert_id,
                          [[out]] VertOut &v_out,
                          [[position]] float4 &out_pos)
{
  int glyph_index = inst_index;

  v_out.color_flat = srt.glyphs[glyph_index].glyph_color;
  v_out.glyph_offset = srt.glyphs[glyph_index].offset;
  v_out.glyph_dim = srt.glyphs[glyph_index].glyph_size;
  v_out.glyph_flags = srt.glyphs[glyph_index].flags;

  /* Depending on shadow outline / blur level, we might need to expand the quad. */
  uint shadow_type = v_out.glyph_flags & 0xFu;
  int interp_size = shadow_type > 4 ? 2 : (shadow_type > 0 ? 1 : 0);

  /* Quad expansion using instanced rendering. */
  float x = float(vert_id % 2);
  float y = float(vert_id / 2);
  float2 quad = float2(x, y);

  float4 pos = float4(srt.glyphs[glyph_index].position);
  float2 interp_offset = float(interp_size) / abs(pos.zw - pos.xy);
  v_out.uv_interp = mix(-interp_offset, 1.0f + interp_offset, quad) * float2(v_out.glyph_dim) +
                    float2(0.5f);

  float2 final_pos = mix(float2(int2(pos.xy) + int2(-interp_size, interp_size)),
                         float2(int2(pos.zw) + int2(interp_size, -interp_size)),
                         quad);

  out_pos = srt.ModelViewProjectionMatrix * float4(final_pos, 0.0f, 1.0f);
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]] void main_frag([[resource_table]] const Resources &srt,
                            [[resource_table]] const ColorSpace &colorspace,
                            [[in]] const VertOut &v_out,
                            [[out]] FragOut &frag_out)
{
  float2 uv_base = v_out.uv_interp;
  uint num_channels = (v_out.glyph_flags >> 4) & 0xFu;
  uint shadow_type = v_out.glyph_flags & 0xFu;

  /* Colored glyphs: do not do filtering or blurring. */
  if (num_channels == 4) {
    frag_out.color = srt.sample_glyph_rgba(v_out.glyph_dim, v_out.glyph_offset, uv_base).rgba;
    frag_out.color.a *= v_out.color_flat.a;
    return;
  }

  float2 bilin_f = fract(uv_base);

  frag_out.color.rgb = v_out.color_flat.rgb;

  if (shadow_type == 0) {
    /* No blurring: just a bilinear sample. */
    frag_out.color.a = srt.sample_glyph_bilinear(
        v_out.glyph_dim, v_out.glyph_offset, bilin_f, uv_base);
  }
  else {

    /* Blurring or dilation: will fetch (N+1)x(N+1) are of glyph texels,
     * shifting the filter kernel weights by bilinear fraction. */
    frag_out.color.a = 0.0f;

    int2 texel = int2(floor(uv_base)) - 1;
    int frag_offset = v_out.glyph_offset + texel.y * v_out.glyph_dim.x + texel.x;

    if (shadow_type == 6) {
      /* 3x3 outline by dilation */

      float maxval = 0.0f;
      for (int iy = 0; iy < 4; ++iy) {
        int ofsy = iy - 1;
        for (int ix = 0; ix < 4; ++ix) {
          int ofsx = ix - 1;
          float v = srt.texel_fetch(frag_offset + ofsy * v_out.glyph_dim.x + ofsx);
          if (!is_inside_box(v_out.glyph_dim, texel + int2(ofsx, ofsy))) {
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
      frag_out.color.a = maxval;
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
          float v = srt.texel_fetch(frag_offset + ofsy * v_out.glyph_dim.x + ofsx);
          if (!is_inside_box(v_out.glyph_dim, texel + int2(ofsx, ofsy))) {
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
      frag_out.color.a = sum * (1.0f / 16.0f);
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
          float v = srt.texel_fetch(frag_offset + ofsy * v_out.glyph_dim.x + ofsx);
          if (!is_inside_box(v_out.glyph_dim, texel + int2(ofsx, ofsy))) {
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
      frag_out.color.a = sum * (1.0f / 80.0f);
    }
  }

  frag_out.color.a *= v_out.color_flat.a;
  frag_out.color = colorspace.rec709_srgb_to_output_space(frag_out.color);
}

}  // namespace builtin::text

PipelineGraphic gpu_shader_text(builtin::text::main_vert, builtin::text::main_frag);
