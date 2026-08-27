/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_colorspace.bsl.hh"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

namespace builtin::image {

struct Image {
  [[compilation_constant]] const bool use_linear_input;
  [[compilation_constant]] const bool use_color;
  [[compilation_constant]] const bool use_desaturation;
  [[compilation_constant]] const bool use_shuffle;

  [[push_constant]] const float4x4 ModelViewProjectionMatrix;
  [[push_constant, condition(use_linear_input)]] const float3x3 gpu_scene_linear_to_rec709;

  [[push_constant, condition(use_color)]] const float4 color;
  [[push_constant, condition(use_desaturation)]] const float factor;
  [[push_constant, condition(use_shuffle)]] const float4 shuffle;

  [[sampler(0)]] sampler2D image;
};

struct ImageRect {
  [[push_constant]] const float4 rect_icon;
  [[push_constant]] const float4 rect_geom;
};

struct VertIn {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] float2 texCoord;
};

struct VertOut {
  [[smooth]] float2 uv;
};

[[vertex]] void image_vert([[resource_table]] const Image &srt,
                           [[in]] const VertIn &v_in,
                           [[out]] VertOut &v_out,
                           [[position]] float4 &out_pos)
{
  out_pos = srt.ModelViewProjectionMatrix * float4(v_in.pos, 1.0f);
  v_out.uv = v_in.texCoord;
}

[[vertex]] void image_rect_vert([[resource_table]] const Image &srt,
                                [[resource_table]] const ImageRect &rect,
                                [[vertex_id]] const int vert_id,
                                [[in]] const VertIn &v_in,
                                [[out]] VertOut &v_out,
                                [[position]] float4 &out_pos)
{
  float3 co = float3(0);
  if (vert_id == 0) {
    co.xy = rect.rect_geom.xw;
    v_out.uv = rect.rect_icon.xw;
  }
  else if (vert_id == 1) {
    co.xy = rect.rect_geom.xy;
    v_out.uv = rect.rect_icon.xy;
  }
  else if (vert_id == 2) {
    co.xy = rect.rect_geom.zw;
    v_out.uv = rect.rect_icon.zw;
  }
  else {
    co.xy = rect.rect_geom.zy;
    v_out.uv = rect.rect_icon.zy;
  }

  out_pos = srt.ModelViewProjectionMatrix * float4(co, 1.0f);
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]] void image_frag([[resource_table]] const Image &srt,
                             [[resource_table]] const ColorSpace &colorspace,
                             [[in]] const VertOut &v_out,
                             [[out]] FragOut &frag_out)
{
  float4 col = texture(srt.image, v_out.uv);

  if (srt.use_desaturation) [[static_branch]] {
    col.rgb = mix(col.rgb, float3(average(col.rgb)), srt.factor);
  }

  if (srt.use_shuffle) [[static_branch]] {
    col *= float4(col.r * srt.shuffle.r + col.g * srt.shuffle.g + col.b * srt.shuffle.b +
                  col.a * srt.shuffle.a);
  }

  if (srt.use_color) [[static_branch]] {
    col *= srt.color;
  }

  if (srt.use_linear_input) [[static_branch]] {
    col = ColorSpace::scene_linear_to_rec709_srgb(srt.gpu_scene_linear_to_rec709, col);
  }
  col = colorspace.rec709_srgb_to_output_space(col);

  frag_out.color = col;
}

struct ImageOverlayMerge {
  [[push_constant]] const bool display_transform;
  [[push_constant]] const bool overlay;
  [[push_constant]] const bool use_hdr_display;

  [[push_constant]] const float4x4 ModelViewProjectionMatrix;

  /* Sampler slots should match OCIO's. */
  [[sampler(0)]] sampler2D image_texture;
  [[sampler(1)]] sampler2D overlays_texture;
};

[[vertex]] void image_overlay_vert([[resource_table]] const ImageOverlayMerge &srt,
                                   [[in]] const VertIn &v_in,
                                   [[out]] VertOut &v_out,
                                   [[position]] float4 &out_pos)
{
  out_pos = srt.ModelViewProjectionMatrix * float4(v_in.pos, 1.0f);
  v_out.uv = v_in.texCoord;
}

[[fragment]] void image_overlay_merge_frag([[resource_table]] const ImageOverlayMerge &srt,
                                           [[in]] const VertOut &v_out,
                                           [[out]] FragOut &frag_out)
{
  float4 image_col = texture(srt.image_texture, v_out.uv);
  float4 overlay_col = texture(srt.overlays_texture, v_out.uv);

  if (srt.overlay) {
    if (srt.use_hdr_display) {
      /* When using HDR, interpolate towards clamped color to improve display of
       * alpha-blended overlays. */
      image_col = mix(image_col, saturate(image_col), overlay_col.a);
    }
    image_col *= 1.0f - overlay_col.a;
    image_col += overlay_col;
  }

  if (srt.display_transform) {
    float3x3 identity = float3x3(float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1));
    image_col = ColorSpace::scene_linear_to_rec709_srgb(identity, image_col);
  }

  frag_out.color = image_col;
}

enum DisplayMode : int {
  S3D_DISPLAY_ANAGLYPH = 0,
  S3D_DISPLAY_INTERLACE = 1,
};

enum InterlaceMode : int {
  S3D_INTERLACE_ROW = 0,
  S3D_INTERLACE_COLUMN = 1,
  S3D_INTERLACE_CHECKERBOARD = 2,
};

struct StereoSettings {
  DisplayMode display_mode;
  InterlaceMode interlace_mode;
  bool interlace_swap;

  static StereoSettings unpack(int packed_settings)
  {
    return {.display_mode = DisplayMode(packed_settings & ((1 << 3) - 1)),
            .interlace_mode = InterlaceMode((packed_settings >> 3) & ((1 << 3) - 1)),
            .interlace_swap = bool(packed_settings >> 6)};
  }

  bool interlace(int2 texel)
  {
    switch (interlace_mode) {
      case S3D_INTERLACE_CHECKERBOARD:
        return ((texel.x + texel.y) & 1) != 0;
      case S3D_INTERLACE_ROW:
        return (texel.y & 1) != 0;
      case S3D_INTERLACE_COLUMN:
        return (texel.x & 1) != 0;
    }
    return false;
  }
};

struct ImageStereoMerge {
  [[push_constant]] /*StereoSettings*/ int stereoDisplaySettings;

  [[push_constant]] const float4x4 ModelViewProjectionMatrix;

  [[sampler(0)]] sampler2D imageTexture;
  [[sampler(1)]] sampler2D overlayTexture;
};

[[vertex]] void image_overlay_merge_stereo_vert([[resource_table]] const ImageStereoMerge &srt,
                                                [[in]] const VertIn &v_in,
                                                [[position]] float4 &out_pos)
{
  out_pos = srt.ModelViewProjectionMatrix * float4(v_in.pos, 1.0f);
}

struct FragOutStereo {
  [[frag_color(0)]] float4 color_overlay;
  [[frag_color(1)]] float4 color_image;
};

[[fragment]] void image_overlay_merge_stereo_frag([[resource_table]] const ImageStereoMerge &srt,
                                                  [[resource_table]] const ColorSpace &colorspace,
                                                  [[frag_coord]] const float4 frag_co,
                                                  [[out]] FragOutStereo &frag_out)
{
  int2 texel = int2(frag_co.xy);

  StereoSettings settings = StereoSettings::unpack(srt.stereoDisplaySettings);

  if (settings.display_mode == S3D_DISPLAY_INTERLACE &&
      (settings.interlace(texel) == settings.interlace_swap))
  {
    gpu_discard_fragment();
  }

  frag_out.color_overlay = texelFetch(srt.imageTexture, texel, 0);
  frag_out.color_image = texelFetch(srt.overlayTexture, texel, 0);
}

}  // namespace builtin::image

PipelineGraphic gpu_shader_3D_image(builtin::image::image_vert,
                                    builtin::image::image_frag,
                                    builtin::image::Image{
                                        .use_linear_input = false,
                                        .use_color = false,
                                        .use_desaturation = false,
                                        .use_shuffle = false,
                                    });
PipelineGraphic gpu_shader_3D_image_scene_linear(builtin::image::image_vert,
                                                 builtin::image::image_frag,
                                                 builtin::image::Image{
                                                     .use_linear_input = true,
                                                     .use_color = false,
                                                     .use_desaturation = false,
                                                     .use_shuffle = false,
                                                 });
PipelineGraphic gpu_shader_3D_image_color(builtin::image::image_vert,
                                          builtin::image::image_frag,
                                          builtin::image::Image{
                                              .use_linear_input = false,
                                              .use_color = true,
                                              .use_desaturation = false,
                                              .use_shuffle = false,
                                          });
PipelineGraphic gpu_shader_3D_image_color_scene_linear(builtin::image::image_vert,
                                                       builtin::image::image_frag,
                                                       builtin::image::Image{
                                                           .use_linear_input = true,
                                                           .use_color = true,
                                                           .use_desaturation = false,
                                                           .use_shuffle = false,
                                                       });
PipelineGraphic gpu_shader_2D_image_shuffle_color(builtin::image::image_vert,
                                                  builtin::image::image_frag,
                                                  builtin::image::Image{
                                                      .use_linear_input = false,
                                                      .use_color = true,
                                                      .use_desaturation = false,
                                                      .use_shuffle = true,
                                                  });
PipelineGraphic gpu_shader_2D_image_desaturate_color(builtin::image::image_vert,
                                                     builtin::image::image_frag,
                                                     builtin::image::Image{
                                                         .use_linear_input = false,
                                                         .use_color = true,
                                                         .use_desaturation = true,
                                                         .use_shuffle = false,
                                                     });
PipelineGraphic gpu_shader_2D_image_rect_color(builtin::image::image_rect_vert,
                                               builtin::image::image_frag,
                                               builtin::image::Image{
                                                   .use_linear_input = false,
                                                   .use_color = true,
                                                   .use_desaturation = false,
                                                   .use_shuffle = false,
                                               });

PipelineGraphic gpu_shader_2D_image_overlays_merge(builtin::image::image_overlay_vert,
                                                   builtin::image::image_overlay_merge_frag);
PipelineGraphic gpu_shader_2D_image_overlays_stereo_merge(
    builtin::image::image_overlay_merge_stereo_vert,
    builtin::image::image_overlay_merge_stereo_frag);
