/* SPDX-FileCopyrightText: 2016-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_colorspace_lib.glsl)

//#define GPU_NEAREST
#define sample_glyph_offset(texel, ofs) \
  texture_1D_custom_bilinear_filter(texCoord_interp + ofs * texel)

float texel_fetch(int index)
{
  int size_x = textureSize(glyph, 0).r;
  if (index >= size_x) {
    return texelFetch(glyph, ivec2(index % size_x, index / size_x), 0).r;
  }
  return texelFetch(glyph, ivec2(index, 0), 0).r;
}

bool is_inside_box(ivec2 v)
{
  return all(greaterThanEqual(v, ivec2(0))) && all(lessThan(v, glyph_dim));
}

float texture_1D_custom_bilinear_filter(vec2 uv)
{
  vec2 texel_2d = uv * vec2(glyph_dim) + vec2(0.5);
  ivec2 texel_2d_near = ivec2(texel_2d) - 1;
  int frag_offset = glyph_offset + texel_2d_near.y * glyph_dim.x + texel_2d_near.x;

  float tl = 0.0;

  if (is_inside_box(texel_2d_near)) {
    tl = texel_fetch(frag_offset);
  }

#ifdef GPU_NEAREST
  return tl;
#else  // GPU_LINEAR
  int offset_x = 1;
  int offset_y = glyph_dim.x;

  float tr = 0.0;
  float bl = 0.0;
  float br = 0.0;

  if (is_inside_box(texel_2d_near + ivec2(1, 0))) {
    tr = texel_fetch(frag_offset + offset_x);
  }
  if (is_inside_box(texel_2d_near + ivec2(0, 1))) {
    bl = texel_fetch(frag_offset + offset_y);
  }
  if (is_inside_box(texel_2d_near + ivec2(1, 1))) {
    br = texel_fetch(frag_offset + offset_x + offset_y);
  }

  vec2 f = fract(texel_2d);
  float tA = mix(tl, tr, f.x);
  float tB = mix(bl, br, f.x);

  return mix(tA, tB, f.y);
#endif
}

void main()
{
  // input color replaces texture color
  fragColor.rgb = color_flat.rgb;

  // modulate input alpha & texture alpha
  if (interp_size == 0) {
    fragColor.a = texture_1D_custom_bilinear_filter(texCoord_interp);
  }
  else {
    vec2 texel = 1.0 / vec2(glyph_dim);
    fragColor.a = 0.0;

    if (interp_size == 1) {
      /* NOTE(Metal): Declaring constant array in function scope to avoid increasing local shader
       * memory pressure. */
      const vec2 offsets4[4] = vec2[4](
          vec2(-0.5, 0.5), vec2(0.5, 0.5), vec2(-0.5, -0.5), vec2(-0.5, -0.5));

      /* 3x3 blur */
      /* Manual unroll for perf. (stupid glsl compiler) */
      fragColor.a += sample_glyph_offset(texel, offsets4[0]);
      fragColor.a += sample_glyph_offset(texel, offsets4[1]);
      fragColor.a += sample_glyph_offset(texel, offsets4[2]);
      fragColor.a += sample_glyph_offset(texel, offsets4[3]);
      fragColor.a *= (1.0 / 4.0);
    }
    else {
      /* NOTE(Metal): Declaring constant array in function scope to avoid increasing local shader
       * memory pressure. */
      const vec2 offsets16[16] = vec2[16](vec2(-1.5, 1.5),
                                          vec2(-0.5, 1.5),
                                          vec2(0.5, 1.5),
                                          vec2(1.5, 1.5),
                                          vec2(-1.5, 0.5),
                                          vec2(-0.5, 0.5),
                                          vec2(0.5, 0.5),
                                          vec2(1.5, 0.5),
                                          vec2(-1.5, -0.5),
                                          vec2(-0.5, -0.5),
                                          vec2(0.5, -0.5),
                                          vec2(1.5, -0.5),
                                          vec2(-1.5, -1.5),
                                          vec2(-0.5, -1.5),
                                          vec2(0.5, -1.5),
                                          vec2(1.5, -1.5));

      /* 5x5 blur */
      /* Manual unroll for perf. (stupid glsl compiler) */
      fragColor.a += sample_glyph_offset(texel, offsets16[0]);
      fragColor.a += sample_glyph_offset(texel, offsets16[1]);
      fragColor.a += sample_glyph_offset(texel, offsets16[2]);
      fragColor.a += sample_glyph_offset(texel, offsets16[3]);

      fragColor.a += sample_glyph_offset(texel, offsets16[4]);
      fragColor.a += sample_glyph_offset(texel, offsets16[5]) * 2.0;
      fragColor.a += sample_glyph_offset(texel, offsets16[6]) * 2.0;
      fragColor.a += sample_glyph_offset(texel, offsets16[7]);

      fragColor.a += sample_glyph_offset(texel, offsets16[8]);
      fragColor.a += sample_glyph_offset(texel, offsets16[9]) * 2.0;
      fragColor.a += sample_glyph_offset(texel, offsets16[10]) * 2.0;
      fragColor.a += sample_glyph_offset(texel, offsets16[11]);

      fragColor.a += sample_glyph_offset(texel, offsets16[12]);
      fragColor.a += sample_glyph_offset(texel, offsets16[13]);
      fragColor.a += sample_glyph_offset(texel, offsets16[14]);
      fragColor.a += sample_glyph_offset(texel, offsets16[15]);
      fragColor.a *= (1.0 / 20.0);
    }
  }

  fragColor.a *= color_flat.a;
  fragColor = blender_srgb_to_framebuffer_space(fragColor);
}
