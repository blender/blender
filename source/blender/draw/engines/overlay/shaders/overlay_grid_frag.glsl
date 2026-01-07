/* SPDX-FileCopyrightText: 2017-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_grid_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_grid_next)

#include "draw_view_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "overlay_common_lib.glsl"

/* Returns true if both components of `v` fall within `epsilon` of 0. */
bool is_zero(float2 v, float epsilon)
{
  return all(lessThanEqual(abs(v), float2(epsilon)));
}

void main()
{
  /* Fragment color. */
  if (flag_test(grid_flag, SHOW_GRID)) {
    /* Color is a mix of [grid, grid_emphasis], dependent on the level. */
    out_color = mix(theme.colors.grid, theme.colors.grid_emphasis, vertex_out_flat.emphasis);
  }
  else if (flag_test(grid_flag, SHOW_AXES)) {
    /* Color is fixed by theme. */
    if (flag_test(grid_flag, AXIS_X) && is_zero(vertex_out.pos.yz, 1e-4f)) {
      out_color = theme.colors.grid_axis_x;
    }
    else if (flag_test(grid_flag, AXIS_Y) && is_zero(vertex_out.pos.xz, 1e-4f)) {
      out_color = theme.colors.grid_axis_y;
    }
    else if (flag_test(grid_flag, AXIS_Z) && is_zero(vertex_out.pos.xy, 1e-4f)) {
      out_color = theme.colors.grid_axis_z;
    }
  }

  /* Fragment alpha. */
  out_color.a *= vertex_out_flat.alpha;
  if (drw_view_is_perspective()) {
    /* Fade at edge of grid level. */
    float length_fade = 1.0f - min(1.0f, length(vertex_out.coord));
    out_color.a *= length_fade;

    /* Compute normalized view vector. */
    float3 V = drw_view_position() - vertex_out.pos;
    float dist = length(V);
    V /= dist;

    /* Add fade at steep angles for contents of the floor plane. */
    if (vertex_out.pos.z == 0.0f) {
      out_color.a *= 1.0f - pow3f(1.0f - abs(V.z));
    }

    /* Add fade towards camera clip plane. */
    float far_clip = -drw_view_far();
    out_color.a *= 1.0f - smoothstep(0.0f, 0.5f * far_clip, dist - 0.5f * far_clip);
  }
  else {
    /* Fade at edge of grid level in orthographic, in case of rather small units. */
    if (!flag_test(grid_flag, GRID_SIMA)) {
      float length_fade = 1.0f - min(1.0f, dot(vertex_out.coord, vertex_out.coord));
      out_color.a *= pow2f(length_fade);
    }

    /* Add fade at steep angles for contents of the floor plane. */
    if (flag_test(grid_flag, PLANE_XY)) {
      float3 V = -drw_view_forward();
      out_color.a *= 1.0f - pow3f(1.0f - abs(V.z));
    }
  }

  /* Viewport anti-aliasing output. */
  if (out_color.a != 0.0f) {
    line_output = pack_line_data(gl_FragCoord.xy, edge_start, edge_pos);
  }

  /* Alpha discard; discard by stipple pattern for low alpha, to account for overlays
   * incompatible with depth+blend, e.g. MeshEdit. */
  {
    constexpr float dash_width = 4.0f; /* Width of dash pattern; increase to make lines longer. */
    constexpr float fade_start = 0.1f; /* Cutoff for dash fade; alpha above is fully drawn. */
    constexpr float fade_rcp = 1.0f / fade_start;
    float dist = distance(edge_start, edge_pos);
    if (out_color.a < fade_start && fade_rcp * out_color.a < fract(dist / dash_width)) {
      gpu_discard_fragment();
    }
  }

  /* Grid iteration additive alpha in perspective view; lower iterations
   * are given stronger alpha to minimize pop-in of upper iterations. */
  if (drw_view_is_perspective()) {
    constexpr float additive_alpha[4] = {0.4f, 0.3f, 0.2f, 0.1f};
    out_color.a *= additive_alpha[grid_iter];
  }
}
