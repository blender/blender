/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"

/* ** Combine/Separate XYZ ** */

void node_composite_combine_xyz(float x, float y, float z, out float3 vector)
{
  vector = float3(x, y, z);
}

void node_composite_separate_xyz(float3 vector, out float x, out float y, out float z)
{
  x = vector.x;
  y = vector.y;
  z = vector.z;
}

/* ** Combine/Separate RGBA ** */

void node_composite_combine_rgba(float r, float g, float b, float a, out float4 color)
{
  color = float4(r, g, b, a);
}

void node_composite_separate_rgba(float4 color, out float r, out float g, out float b, out float a)
{
  r = color.r;
  g = color.g;
  b = color.b;
  a = color.a;
}

/* ** Combine/Separate HSVA ** */

void node_composite_combine_hsva(float h, float s, float v, float a, out float4 color)
{
  hsv_to_rgb(float4(h, s, v, a), color);
}

void node_composite_separate_hsva(float4 color, out float h, out float s, out float v, out float a)
{
  float4 hsva;
  rgb_to_hsv(color, hsva);
  h = hsva.x;
  s = hsva.y;
  v = hsva.z;
  a = hsva.a;
}

/* ** Combine/Separate HSLA ** */

void node_composite_combine_hsla(float h, float s, float l, float a, out float4 color)
{
  hsl_to_rgb(float4(h, s, l, a), color);
  color.rgb = max(color.rgb, float3(0.0f));
}

void node_composite_separate_hsla(float4 color, out float h, out float s, out float l, out float a)
{
  float4 hsla;
  rgb_to_hsl(color, hsla);
  h = hsla.x;
  s = hsla.y;
  l = hsla.z;
  a = hsla.a;
}

/* ** Combine/Separate YCCA ** */

void node_composite_combine_ycca_itu_601(float y, float cb, float cr, float a, out float4 color)
{
  ycca_to_rgba_itu_601(float4(y, cb, cr, a), color);
}

void node_composite_combine_ycca_itu_709(float y, float cb, float cr, float a, out float4 color)
{
  ycca_to_rgba_itu_709(float4(y, cb, cr, a), color);
}

void node_composite_combine_ycca_jpeg(float y, float cb, float cr, float a, out float4 color)
{
  ycca_to_rgba_jpeg(float4(y, cb, cr, a), color);
}

void node_composite_separate_ycca_itu_601(
    float4 color, out float y, out float cb, out float cr, out float a)
{
  float4 ycca;
  rgba_to_ycca_itu_601(color, ycca);
  y = ycca.x;
  cb = ycca.y;
  cr = ycca.z;
  a = ycca.a;
}

void node_composite_separate_ycca_itu_709(
    float4 color, out float y, out float cb, out float cr, out float a)
{
  float4 ycca;
  rgba_to_ycca_itu_709(color, ycca);
  y = ycca.x;
  cb = ycca.y;
  cr = ycca.z;
  a = ycca.a;
}

void node_composite_separate_ycca_jpeg(
    float4 color, out float y, out float cb, out float cr, out float a)
{
  float4 ycca;
  rgba_to_ycca_jpeg(color, ycca);
  y = ycca.x;
  cb = ycca.y;
  cr = ycca.z;
  a = ycca.a;
}

/* ** Combine/Separate YUVA ** */

void node_composite_combine_yuva_itu_709(float y, float u, float v, float a, out float4 color)
{
  yuva_to_rgba_itu_709(float4(y, u, v, a), color);
}

void node_composite_separate_yuva_itu_709(
    float4 color, out float y, out float u, out float v, out float a)
{
  float4 yuva;
  rgba_to_yuva_itu_709(color, yuva);
  y = yuva.x;
  u = yuva.y;
  v = yuva.z;
  a = yuva.a;
}
