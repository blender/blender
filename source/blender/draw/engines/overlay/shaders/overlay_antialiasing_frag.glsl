/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_antialiasing_info.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_antialiasing)

/**
 * We want to know how much a pixel is covered by a line.
 * We replace the square pixel with a circle of the same area and try to find the intersection
 * area. The area we search is the circular segment. https://en.wikipedia.org/wiki/Circular_segment
 * The formula for the area uses inverse trig function and is quite complex. Instead,
 * we approximate it by using the smooth-step function and a 1.05 factor to the disc radius.
 */

#define M_1_SQRTPI 0.5641895835477563f /* `1/sqrt(pi)`. */

#define DISC_RADIUS (M_1_SQRTPI * 1.05f)
#define LINE_SMOOTH_START (0.5f - DISC_RADIUS)
#define LINE_SMOOTH_END (0.5f + DISC_RADIUS)

/**
 * Returns coverage of a line onto a sample that is distance_to_line (in pixels) far from the line.
 * line_kernel_size is the inner size of the line with 100% coverage.
 */
float line_coverage(float distance_to_line, float line_kernel_size)
{
  if (doSmoothLines) {
    return smoothstep(
        LINE_SMOOTH_END, LINE_SMOOTH_START, abs(distance_to_line) - line_kernel_size);
  }
  else {
    return step(-0.5f, line_kernel_size - abs(distance_to_line));
  }
}
float4 line_coverage(float4 distance_to_line, float line_kernel_size)
{
  if (doSmoothLines) {
    return smoothstep(
        LINE_SMOOTH_END, LINE_SMOOTH_START, abs(distance_to_line) - line_kernel_size);
  }
  else {
    return step(-0.5f, line_kernel_size - abs(distance_to_line));
  }
}

float2 decode_line_dir(float2 dir)
{
  return dir * 2.0f - 1.0f;
}

float decode_line_dist(float dist)
{
  return (dist - 0.1f) * 4.0f - 2.0f;
}

float neighbor_dist(float3 line_dir_and_dist, float2 ofs)
{
  float dist = decode_line_dist(line_dir_and_dist.z);
  float2 dir = decode_line_dir(line_dir_and_dist.xy);

  bool is_line = line_dir_and_dist.z != 0.0f;
  bool dir_horiz = abs(dir.x) > abs(dir.y);
  bool ofs_horiz = (ofs.x != 0);

  if (!is_line || (ofs_horiz != dir_horiz)) {
    dist += 1e10f; /* No line. */
  }
  else {
    dist += dot(ofs, -dir);
  }
  return dist;
}

void neighbor_blend(float line_coverage,
                    float line_depth,
                    float4 line_color,
                    inout float frag_depth,
                    inout float4 col)
{
  line_color *= line_coverage;
  if (line_coverage > 0.0f && line_depth < frag_depth) {
    /* Alpha over. */
    col = col * (1.0f - line_color.a) + line_color;
    frag_depth = line_depth;
  }
  else {
    /* Alpha under. */
    col = col + line_color * (1.0f - col.a);
  }
}

void main()
{
  int2 center_texel = int2(gl_FragCoord.xy);
  float line_kernel = sizePixel * 0.5f - 0.5f;

  fragColor = texelFetch(colorTex, center_texel, 0);

  bool original_col_has_alpha = fragColor.a < 1.0f;

  float depth = texelFetch(depthTex, center_texel, 0).r;

  float dist_raw = texelFetch(lineTex, center_texel, 0).b;
  float dist = decode_line_dist(dist_raw);

  if (!doSmoothLines && dist <= 1.0f) {
    /* No expansion or AA should be applied. */
    return;
  }

  /* TODO: Optimization: use textureGather. */
  float4 neightbor_col0 = texelFetchOffset(colorTex, center_texel, 0, int2(1, 0));
  float4 neightbor_col1 = texelFetchOffset(colorTex, center_texel, 0, int2(-1, 0));
  float4 neightbor_col2 = texelFetchOffset(colorTex, center_texel, 0, int2(0, 1));
  float4 neightbor_col3 = texelFetchOffset(colorTex, center_texel, 0, int2(0, -1));

  float3 neightbor_line0 = texelFetchOffset(lineTex, center_texel, 0, int2(1, 0)).rgb;
  float3 neightbor_line1 = texelFetchOffset(lineTex, center_texel, 0, int2(-1, 0)).rgb;
  float3 neightbor_line2 = texelFetchOffset(lineTex, center_texel, 0, int2(0, 1)).rgb;
  float3 neightbor_line3 = texelFetchOffset(lineTex, center_texel, 0, int2(0, -1)).rgb;

  float4 depths;
  depths.x = texelFetchOffset(depthTex, center_texel, 0, int2(1, 0)).r;
  depths.y = texelFetchOffset(depthTex, center_texel, 0, int2(-1, 0)).r;
  depths.z = texelFetchOffset(depthTex, center_texel, 0, int2(0, 1)).r;
  depths.w = texelFetchOffset(depthTex, center_texel, 0, int2(0, -1)).r;

  float4 line_dists;
  line_dists.x = neighbor_dist(neightbor_line0, float2(1, 0));
  line_dists.y = neighbor_dist(neightbor_line1, float2(-1, 0));
  line_dists.z = neighbor_dist(neightbor_line2, float2(0, 1));
  line_dists.w = neighbor_dist(neightbor_line3, float2(0, -1));

  float4 coverage = line_coverage(line_dists, line_kernel);

  if (dist_raw > 0.0f) {
    fragColor *= line_coverage(dist, line_kernel);
  }

  /* We don't order fragments but use alpha over/alpha under based on current minimum frag depth.
   */
  neighbor_blend(coverage.x, depths.x, neightbor_col0, depth, fragColor);
  neighbor_blend(coverage.y, depths.y, neightbor_col1, depth, fragColor);
  neighbor_blend(coverage.z, depths.z, neightbor_col2, depth, fragColor);
  neighbor_blend(coverage.w, depths.w, neightbor_col3, depth, fragColor);

#if 1
  /* Fix aliasing issue with really dense meshes and 1 pixel sized lines. */
  if (!original_col_has_alpha && dist_raw > 0.0f && line_kernel < 0.45f) {
    float4 lines = float4(
        neightbor_line0.z, neightbor_line1.z, neightbor_line2.z, neightbor_line3.z);
    /* Count number of line neighbors. */
    float blend = dot(float4(0.25f), step(0.001f, lines));
    /* Only do blend if there are more than 2 neighbors. This avoids losing too much AA. */
    blend = clamp(blend * 2.0f - 1.0f, 0.0f, 1.0f);
    fragColor = mix(fragColor, fragColor / fragColor.a, blend);
  }
#endif
}
