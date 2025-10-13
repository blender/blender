/* SPDX-FileCopyrightText: 2013-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Blender OpenColorIO implementation */

/* -------------------------------------------------------------------- */
/** \name Hardcoded color space conversion for fallback implementation
 *
 * NOTE: It is tempting to include gpu_shader_common_color_utils.glsl, but it should not be done
 * here as that header is intended to be used from the node shaders, and the source processor does
 * much more than simply including the file (it also follows some implicit dependencies that is
 * undesired here, and might break since we do not use node shaders here.
 * \{ */

float srgb_to_linear_rgb(float color)
{
  if (color < 0.04045f) {
    return (color < 0.0f) ? 0.0f : color * (1.0f / 12.92f);
  }
  return pow((color + 0.055f) * (1.0f / 1.055f), 2.4f);
}

float3 srgb_to_linear_rgb(float3 color)
{
  return float3(
      srgb_to_linear_rgb(color.r), srgb_to_linear_rgb(color.g), srgb_to_linear_rgb(color.b));
}

float linear_rgb_to_srgb(float color)
{
  if (color < 0.0031308f) {
    return (color < 0.0f) ? 0.0f : color * 12.92f;
  }

  return 1.055f * pow(color, 1.0f / 2.4f) - 0.055f;
}

float3 linear_rgb_to_srgb(float3 color)
{
  return float3(
      linear_rgb_to_srgb(color.r), linear_rgb_to_srgb(color.g), linear_rgb_to_srgb(color.b));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve Mapping Implementation
 * \{ */

#ifdef USE_CURVE_MAPPING

float read_curve_mapping(int table, int index)
{
  return texelFetch(curve_mapping_texture, index, 0)[table];
}

float curvemap_calc_extend(int table, float x, float2 first, float2 last)
{
  if (x <= first[0]) {
    if (curve_mapping.use_extend_extrapolate == 0) {
      /* horizontal extrapolation */
      return first[1];
    }
    else {
      float fac = (curve_mapping.ext_in_x[table] != 0.0) ?
                      ((x - first[0]) / curve_mapping.ext_in_x[table]) :
                      10000.0;
      return first[1] + curve_mapping.ext_in_y[table] * fac;
    }
  }
  else if (x >= last[0]) {
    if (curve_mapping.use_extend_extrapolate == 0) {
      /* horizontal extrapolation */
      return last[1];
    }
    else {
      float fac = (curve_mapping.ext_out_x[table] != 0.0) ?
                      ((x - last[0]) / curve_mapping.ext_out_x[table]) :
                      -10000.0;
      return last[1] + curve_mapping.ext_out_y[table] * fac;
    }
  }
  return 0.0;
}

float curvemap_evaluateF(int table, float value)
{
  float mintable_ = curve_mapping.mintable[table];
  float range = curve_mapping.range[table];
  float mintable = 0.0;
  int CM_TABLE = curve_mapping.lut_size - 1;

  float fi;
  int i;

  /* index in table */
  fi = (value - mintable) * range;
  i = int(fi);

  /* fi is table float index and should check against table range i.e. [0.0 CM_TABLE] */
  if (fi < 0.0 || fi > float(CM_TABLE)) {
    return curvemap_calc_extend(table,
                                value,
                                float2(curve_mapping.first_x[table], curve_mapping.first_y[table]),
                                float2(curve_mapping.last_x[table], curve_mapping.last_y[table]));
  }
  else {
    if (i < 0) {
      return read_curve_mapping(table, 0);
    }
    if (i >= CM_TABLE) {
      return read_curve_mapping(table, CM_TABLE);
    }
    fi = fi - float(i);
    float cm1 = read_curve_mapping(table, i);
    float cm2 = read_curve_mapping(table, i + 1);
    return mix(cm1, cm2, fi);
  }
}

float4 curvemapping_evaluate_premulRGBF(float4 col)
{
  col.rgb = (col.rgb - curve_mapping.black.rgb) * curve_mapping.bwmul.rgb;

  float4 result;
  result.r = curvemap_evaluateF(0, col.r);
  result.g = curvemap_evaluateF(1, col.g);
  result.b = curvemap_evaluateF(2, col.b);
  result.a = col.a;
  return result;
}

#endif /* USE_CURVE_MAPPING */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dithering
 * \{ */

/* 2D hash (iqint3) recommended from "Hash Functions for GPU Rendering" JCGT Vol. 9, No. 3, 2020
 * https://jcgt.org/published/0009/03/02/ */
float hash_iqint3_f(uint2 x)
{
  uint2 q = 1103515245u * ((x >> 1u) ^ (x.yx));
  uint n = 1103515245u * ((q.x) ^ (q.y >> 3u));
  return float(n) * (1.0 / float(0xffffffffu));
}

/* Returns triangle noise in [-1..+1) range, given integer pixel coordinates.
 * Triangle distribution which gives a more final uniform noise,
 * see "Banding in Games: A Noisy Rant" by Mikkel Gjoel (slide 27)
 * https://loopit.dk/banding_in_games.pdf */
float dither_random_value(uint2 co)
{
  float v = hash_iqint3_f(co);
  /* Convert uniform distribution into triangle-shaped distribution. Based on
   * "remap_pdf_tri_unity" from https://www.shadertoy.com/view/WldSRf */
  v = v * 2.0 - 1.0;
  v = sign(v) * (1.0 - sqrt(1.0 - abs(v)));
  return v;
}

uint2 get_pixel_coord(sampler2D tex, float2 uv)
{
  float2 size = float2(textureSize(tex, 0));
  return uint2(uv * size);
}

float4 apply_dither(float4 col, uint2 uv)
{
  col.rgb += dither_random_value(uv) * 0.0033 * parameters.dither;
  return col;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Processing
 * \{ */

/* Prototypes: Implementation is generated and defined after. */
#ifndef GPU_METAL /* Forward declaration invalid in MSL. */
float4 OCIO_to_scene_linear(float4 pixel);
float4 OCIO_to_display(float4 pixel);
#endif

float4 OCIO_ProcessColor(float4 col, float4 col_overlay)
{
#ifdef USE_CURVE_MAPPING
  col = curvemapping_evaluate_premulRGBF(col);
#endif

  if (parameters.use_predivide) {
    if (col.a > 0.0 && col.a < 1.0) {
      col.rgb *= 1.0 / col.a;
    }
  }

  /* NOTE: This is true we only do de-pre-multiply here and NO pre-multiply and the reason is
   * simple -- opengl is always configured for straight alpha at this moment. */

  /* Convert to scene linear (usually a no-op). */
  col = OCIO_to_scene_linear(col);

  /* Skip the rest of the transformation when the shader is only used to transform the input
   * texture to the scene linear space.
   * This will simplify the shader code, potentially making it faster. More importantly doing so
   * avoids math that might lead to nan values: such as applying exposure on negative values. */
#ifndef USE_TO_SCENE_LINEAR_ONLY
  /* Apply exposure and white balance in scene linear. */
  col = parameters.scene_linear_matrix * col;

  /* Convert to display space. */
  col = OCIO_to_display(col);

  /* Blend with overlay in UI color-space.
   *
   * UI color-space here refers to the display linear color space, i.e: The linear color space
   * w.r.t. display chromaticity and radiometry. We separate the color-management process into two
   * steps to be able to merge UI using alpha blending in the correct color space. */
  if (parameters.do_overlay_merge) {
    /* This sign/abs is used to preserve negative values for extended sRGB. */
    col.rgb = sign(col.rgb) * pow(abs(col.rgb), float3(parameters.exponent * 2.2));

    if (parameters.use_hdr_display) {
      /* When using extended color-space, interpolate towards clamped color to improve display of
       * alpha-blended overlays. */
      col = mix(col, clamp(col, 0.0, 1.0), col_overlay.a);
    }
    col *= 1.0 - col_overlay.a;
    col += col_overlay; /* Assumed unassociated alpha. */
    col.rgb = sign(col.rgb) * pow(abs(col.rgb), float3(1.0 / 2.2));
  }
  else {
    col.rgb = sign(col.rgb) * pow(abs(col.rgb), float3(parameters.exponent));
  }

  if (parameters.dither > 0.0) {
    uint2 texel = get_pixel_coord(image_texture, texCoord_interp.xy);
    col = apply_dither(col, texel);
  }
#endif

#ifdef OUTPUT_PREMULTIPLIED
  if (col.a > 0.0 && col.a < 1.0) {
    col.rgb *= col.a;
  }
#endif

  return col;
}

/** \} */

void main()
{
  float4 col = texture(image_texture, texCoord_interp.xy);
  float4 col_overlay = texture(overlay_texture, texCoord_interp.xy);

  fragColor = OCIO_ProcessColor(col, col_overlay);
}
