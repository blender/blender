/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"
#include "infos/gpu_shader_sequencer_infos.hh"

COMPUTE_SHADER_CREATE_INFO(gpu_shader_sequencer_scope_raster)

/* Match eSpaceSeq_RegionType */
#define SEQ_DRAW_IMG_WAVEFORM 2
#define SEQ_DRAW_IMG_VECTORSCOPE 3
#define SEQ_DRAW_IMG_HISTOGRAM 4
#define SEQ_DRAW_IMG_RGBPARADE 5

/* Compute shader that rasterizes scope points into screen-sized
 * raster buffer, with accumulated R,G,B,A values in fixed point.
 *
 * For pixels covered by each point, just do atomic adds into
 * the buffer. Even if this has potential to be highly contented
 * on the same memory locations, it is still way faster than rasterizing
 * points using regular GPU pipeline. */

void put_pixel(int x, int y, float4 col)
{
  int index = y * view_width + x;

  /* Use 24.8 fixed point; this allows 16M points to hit the same
   * location without overflowing the values. */
  uint4 col_fx = uint4(col * 255.0f + 0.5f);
  atomicAdd(raster_buf[index].col_r, col_fx.r);
  atomicAdd(raster_buf[index].col_g, col_fx.g);
  atomicAdd(raster_buf[index].col_b, col_fx.b);
  atomicAdd(raster_buf[index].col_a, col_fx.a);
}

/* Convert from sRGB to linear using the sRGB EOTF. */
float srgb_to_linear(float c)
{
  return (c < 0.04045f) ? c / 12.92f : pow((c + 0.055f) / 1.055f, 2.4f);
}

/* Convert from scope space to linear. See libocio_scope.cc for details
 * on this mapping. */
float scope_to_linear(float c)
{
  const float SCOPE_SDR_IN_HDR_SCOPE = 0.75f;
  const float SCOPE_HDR_B = 0.0368291f;
  const float SCOPE_HDR_C = 0.8882908f;
  const float SCOPE_HDR_D = 0.8307242f;

  if (scope_is_hdr) {
    if (c > SCOPE_SDR_IN_HDR_SCOPE) {
      return exp((c - SCOPE_HDR_D) / SCOPE_HDR_B) + SCOPE_HDR_C;
    }
    c /= SCOPE_SDR_IN_HDR_SCOPE;
  }
  return srgb_to_linear(c);
}

/* Convert from linear Rec.709 to extended sRGB. */
float linear_to_extended_srgb(float c)
{
  float c_sign = sign(c);
  c = abs(c);
  c = (c < 0.0031308f) ? c * 12.92f : 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
  return c_sign * c;
}

/* Convert from scope space color (that is suitable for plotting positions)
 * to extended sRGB (the color space of the frame-buffer). */
float3 scope_to_extended_srgb(float3 color)
{
  float3 scope_linear = float3(scope_to_linear(max(color.r, 0.0f)),
                               scope_to_linear(max(color.g, 0.0f)),
                               scope_to_linear(max(color.b, 0.0f)));

  float3 rec709_linear = scope_gamut_to_rec709 * scope_linear;

  return float3(linear_to_extended_srgb(rec709_linear.r),
                linear_to_extended_srgb(rec709_linear.g),
                linear_to_extended_srgb(rec709_linear.b));
}

void main()
{
  /* Fetch pixel from the input image, corresponding to current point. */
  int2 texel = int2(gl_GlobalInvocationID.xy);
  if (any(greaterThanEqual(texel, int2(image_width, image_height)))) {
    return;
  }

  float4 image_color = texelFetch(image, texel, 0);
  if (img_premultiplied) {
    color_alpha_unpremultiply(image_color, image_color);
  }

  /* Convert scope space image color to position and frame-buffer color. */
  float3 position = clamp(image_color.rgb, 0.0f, 1.0f);
  float4 color = float4(scope_to_extended_srgb(image_color.rgb), 1.0f);

  /* Calculate point position based on scope mode; possibly adjust color too. */
  float2 pos = float2(0.0);
  if (scope_mode == SEQ_DRAW_IMG_WAVEFORM) {
    /* Waveform: pixel height based on luminance. */
    pos.x = texel.x - image_width / 2;
    pos.y = (get_luminance(position, scope_luma_coeffs) - 0.5f) * image_height;
  }
  else if (scope_mode == SEQ_DRAW_IMG_RGBPARADE) {
    /* RGB parade: similar to waveform, except three different "bands"
     * for each R/G/B intensity. */
    int channel = texel.x % 3;
    int column = texel.x / 3;

    /* Use a bit desaturated color, and blend in a bit of original pixel color. */
    float other_channels = 0.6f;
    float factor = 0.4f;
    if (channel == 0) {
      pos.x = column - image_width / 2;
      pos.y = (position.r - 0.5f) * image_height;
      color.rgb = mix(color.rgb, float3(1, other_channels, other_channels), factor);
    }
    if (channel == 1) {
      pos.x = column - image_width / 2 + image_width / 3;
      pos.y = (position.g - 0.5f) * image_height;
      color.rgb = mix(color.rgb, float3(other_channels, 1, other_channels), factor);
    }
    if (channel == 2) {
      pos.x = column - image_width / 2 + image_width * 2 / 3;
      pos.y = (position.b - 0.5f) * image_height;
      color.rgb = mix(color.rgb, float3(other_channels, other_channels, 1), factor);
    }
  }
  else if (scope_mode == SEQ_DRAW_IMG_VECTORSCOPE) {
    /* Vectorscope: pixel position is based on U,V of the color. */
    float3 yuv = scope_yuv_matrix * position;
    float vec_size = min(image_width, image_height);
    pos = yuv.yz * vec_size;
  }

  /* Determine final point color: we want to keep the hue, desaturate it a bit,
   * and use full brightness. */
  float4 hsv;
  rgb_to_hsv(color, hsv);
  if (scope_mode != SEQ_DRAW_IMG_RGBPARADE) {
    /* Saturation adjustments for parade mode are already done above. */
    hsv.y *= 0.5f;
  }
  hsv.z = 1.0f;
  hsv_to_rgb(hsv, color);

  /* Calculate final point position in integer pixels. */
  float4 clip_pos = ModelViewProjectionMatrix * float4(pos * inv_render_scale, 0.0f, 1.0f);
  int2 view_pos = int2((clip_pos.xy * 0.5f + float2(0.5f)) * float2(view_width, view_height));
  if (any(lessThan(view_pos, int2(0))) ||
      any(greaterThanEqual(view_pos, int2(view_width, view_height))))
  {
    /* Outside of view. */
    return;
  }

  /* Optimization for highly contended raster regions (e.g. vectorscope center):
   * if there's 50+ points rendered into this location already, stop adding more. */
  int index = view_pos.y * view_width + view_pos.x;
  if (raster_buf[index].col_a > 50 * 255) {
    return;
  }

  /* Adjust point transparency based on ratio of wanted point size vs
   * quantized to integer pixel count point size. */
  float raster_size = scope_point_size;
  int px_size = max(int(ceil(raster_size)), 1);
  float factor = max(raster_size / px_size * inv_render_scale, 1.0f / 255.0f);
  color.rgb *= factor;
  color.a = factor;

  if (px_size <= 1) {
    /* Single pixel. */
    put_pixel(view_pos.x, view_pos.y, color);
  }
  else {
    /* Multiple pixels. */
    px_size = min(px_size, 16);
    int x_min = view_pos.x - px_size / 2;
    int x_max = x_min + px_size;
    int y_min = view_pos.y - px_size / 2;
    int y_max = y_min + px_size;
    x_min = clamp(x_min, 0, view_width);
    x_max = clamp(x_max, 0, view_width);
    y_min = clamp(y_min, 0, view_height);
    y_max = clamp(y_max, 0, view_height);
    for (int y = y_min; y < y_max; y++) {
      for (int x = x_min; x < x_max; x++) {
        put_pixel(x, y, color);
      }
    }
  }
}
