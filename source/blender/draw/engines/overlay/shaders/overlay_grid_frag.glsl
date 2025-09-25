/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_grid_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_grid_next)

/**
 * Infinite grid:
 * Draw anti-aliased grid and axes of different sizes with smooth blending between levels of
 * detail. We draw multiple triangles to avoid float precision issues due to perspective
 * interpolation.
 */

#include "draw_view_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

float get_grid(float2 co, float2 fwidthCos, float2 grid_scale)
{
  float2 half_size = grid_scale / 2.0f;
  /* Triangular wave pattern, amplitude is [0, half_size]. */
  float2 grid_domain = abs(mod(co + half_size, grid_scale) - half_size);
  /* Modulate by the absolute rate of change of the coordinates
   * (make line have the same width under perspective). */
  grid_domain /= fwidthCos;
  /* Collapse waves. */
  float line_dist = min(grid_domain.x, grid_domain.y);
  return 1.0 - LINE_STEP(line_dist - grid_buf.line_size);
}

float3 get_axes(float3 co, float3 fwidthCos, float line_size)
{
  float3 axes_domain = abs(co);
  /* Modulate by the absolute rate of change of the coordinates
   * (make line have the same width under perspective). */
  axes_domain /= fwidthCos;
  return 1.0 - LINE_STEP(axes_domain - (line_size + grid_buf.line_size));
}

#define linearstep(p0, p1, v) (clamp(((v) - (p0)) / abs((p1) - (p0)), 0.0f, 1.0f))

void main()
{
  float3 P = local_pos * grid_buf.size.xyz;
  float3 dFdxPos = gpu_dfdx(P);
  float3 dFdyPos = gpu_dfdy(P);
  float3 fwidthPos = abs(dFdxPos) + abs(dFdyPos);
  P += drw_view_position() * plane_axes;

  float dist, fade;
  bool is_persp = drw_view().winmat[3][3] == 0.0f;
  if (is_persp) {
    float3 V = drw_view_position() - P;
    dist = length(V);
    V /= dist;

    float angle;
    if (flag_test(grid_flag, PLANE_XZ)) {
      angle = V.y;
    }
    else if (flag_test(grid_flag, PLANE_YZ)) {
      angle = V.x;
    }
    else {
      angle = V.z;
    }

    angle = 1.0f - abs(angle);
    angle *= angle;
    fade = 1.0f - angle * angle;
    fade *= 1.0f - smoothstep(0.0f, grid_buf.distance, dist - grid_buf.distance);
  }
  else {
    dist = gl_FragCoord.z * 2.0f - 1.0f;
    /* Avoid fading in +Z direction in camera view (see #70193). */
    dist = flag_test(grid_flag, GRID_CAMERA) ? clamp(dist, 0.0f, 1.0f) : abs(dist);
    fade = 1.0f - smoothstep(0.0f, 0.5f, dist - 0.5f);
    dist = 1.0f; /* Avoid branch after. */

    if (flag_test(grid_flag, PLANE_XY)) {
      float angle = 1.0f - abs(drw_view().viewinv[2].z);
      dist = 1.0f + angle * 2.0f;
      angle *= angle;
      fade *= 1.0f - angle * angle;
    }
  }

  if (flag_test(grid_flag, SHOW_GRID)) {
    /* Using `max(dot(dFdxPos, drw_view().viewinv[0]), dot(dFdyPos, drw_view().viewinv[1]))`
     * would be more accurate, but not really necessary. */
    float grid_res = dot(dFdxPos, drw_view().viewinv[0].xyz);

    /* The grid begins to appear when it comprises 4 pixels. */
    grid_res *= 4;

    /* For UV/Image editor use grid_buf.zoom_factor. */
    if (flag_test(grid_flag, PLANE_IMAGE) &&
        /* Grid begins to appear when the length of one grid unit is at least
         * (256/grid_size) pixels Value of grid_size defined in `overlay_grid.c`. */
        !flag_test(grid_flag, CUSTOM_GRID))
    {
      grid_res = grid_buf.zoom_factor;
    }

/** Keep in sync with `SI_GRID_STEPS_LEN` in `DNA_space_types.h`. */
#define STEPS_LEN 8
    int step_id_x = STEPS_LEN - 1;
    int step_id_y = STEPS_LEN - 1;

    /* Loop backwards a compile-time-constant number of steps. */
    for (int i = STEPS_LEN - 2; i >= 0; --i) {
      step_id_x = (grid_res < grid_buf.steps[i].x) ? i : step_id_x; /* Branchless. */
      step_id_y = (grid_res < grid_buf.steps[i].y) ? i : step_id_y;
    }

    /* From biggest to smallest. */
    float scale0x = step_id_x > 0 ? grid_buf.steps[step_id_x - 1].x : 0.0f;
    float scaleAx = grid_buf.steps[step_id_x].x;
    float scaleBx = grid_buf.steps[min(step_id_x + 1, STEPS_LEN - 1)].x;
    float scaleCx = grid_buf.steps[min(step_id_x + 2, STEPS_LEN - 1)].x;

    float scale0y = step_id_y > 0 ? grid_buf.steps[step_id_y - 1].y : 0.0f;
    float scaleAy = grid_buf.steps[step_id_y].y;
    float scaleBy = grid_buf.steps[min(step_id_y + 1, STEPS_LEN - 1)].y;
    float scaleCy = grid_buf.steps[min(step_id_y + 2, STEPS_LEN - 1)].y;

    /* Subtract from 1.0 to fix blending when `scale0x == scaleAx`. */
    float blend = 1.0f - linearstep(scale0x + scale0y, scaleAx + scaleAy, grid_res + grid_res);
    blend = blend * blend * blend;

    float2 grid_pos, grid_fwidth;
    if (flag_test(grid_flag, PLANE_XZ)) {
      grid_pos = P.xz;
      grid_fwidth = fwidthPos.xz;
    }
    else if (flag_test(grid_flag, PLANE_YZ)) {
      grid_pos = P.yz;
      grid_fwidth = fwidthPos.yz;
    }
    else {
      grid_pos = P.xy;
      grid_fwidth = fwidthPos.xy;
    }

    float gridA = get_grid(grid_pos, grid_fwidth, float2(scaleAx, scaleAy));
    float gridB = get_grid(grid_pos, grid_fwidth, float2(scaleBx, scaleBy));
    float gridC = get_grid(grid_pos, grid_fwidth, float2(scaleCx, scaleCy));

    out_color = theme.colors.grid;
    out_color.a *= gridA * blend;
    out_color = mix(out_color, mix(theme.colors.grid, theme.colors.grid_emphasis, blend), gridB);
    out_color = mix(out_color, theme.colors.grid_emphasis, gridC);
  }
  else {
    out_color = float4(theme.colors.grid.rgb, 0.0f);
  }

  if (flag_test(grid_flag, (SHOW_AXIS_X | SHOW_AXIS_Y | SHOW_AXIS_Z))) {
    /* Setup axes 'domains' */
    float3 axes_dist, axes_fwidth;

    if (flag_test(grid_flag, SHOW_AXIS_X)) {
      axes_dist.x = dot(P.yz, plane_axes.yz);
      axes_fwidth.x = dot(fwidthPos.yz, plane_axes.yz);
    }
    if (flag_test(grid_flag, SHOW_AXIS_Y)) {
      axes_dist.y = dot(P.xz, plane_axes.xz);
      axes_fwidth.y = dot(fwidthPos.xz, plane_axes.xz);
    }
    if (flag_test(grid_flag, SHOW_AXIS_Z)) {
      axes_dist.z = dot(P.xy, plane_axes.xy);
      axes_fwidth.z = dot(fwidthPos.xy, plane_axes.xy);
    }

    /* Computing all axes at once using float3 */
    float3 axes = get_axes(axes_dist, axes_fwidth, 0.1f);

    if (flag_test(grid_flag, SHOW_AXIS_X)) {
      out_color.a = max(out_color.a, axes.x);
      out_color.rgb = (axes.x < 1e-8f) ? out_color.rgb : theme.colors.grid_axis_x.rgb;
    }
    if (flag_test(grid_flag, SHOW_AXIS_Y)) {
      out_color.a = max(out_color.a, axes.y);
      out_color.rgb = (axes.y < 1e-8f) ? out_color.rgb : theme.colors.grid_axis_y.rgb;
    }
    if (flag_test(grid_flag, SHOW_AXIS_Z)) {
      out_color.a = max(out_color.a, axes.z);
      out_color.rgb = (axes.z < 1e-8f) ? out_color.rgb : theme.colors.grid_axis_z.rgb;
    }
  }

  float2 uv = gl_FragCoord.xy / float2(textureSize(depth_tx, 0));
  float scene_depth = texture(depth_tx, uv, 0).r;

  float scene_depth_infront = texture(depth_infront_tx, uv, 0).r;
  if (scene_depth_infront != 1.0f) {
    /* Treat in front objects as if they were on the near plane to occlude the grid. */
    scene_depth = 0.0f;
  }

  if (flag_test(grid_flag, GRID_BACK)) {
    fade *= (scene_depth == 1.0f) ? 1.0f : 0.0f;
  }
  else {
    /* Add a small bias so the grid will always be below of a mesh with the same depth. */
    float grid_depth = gl_FragCoord.z + 4.8e-7f;
    /* Manual, non hard, depth test:
     * Progressively fade the grid below occluders
     * (avoids popping visuals due to depth buffer precision) */
    /* Harder settings tend to flicker more,
     * but have less "see through" appearance. */
    float bias = max(gpu_fwidth(gl_FragCoord.z), 2.4e-7f);
    fade *= linearstep(grid_depth, grid_depth + bias, scene_depth);
  }

  out_color.a *= fade;
}
