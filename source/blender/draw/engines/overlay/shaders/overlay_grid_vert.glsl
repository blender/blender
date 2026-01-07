/* SPDX-FileCopyrightText: 2017-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_grid_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_grid_next)

#include "draw_view_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

struct LineData {
  float2 P;
  uint axis;  /* [0, 1, 2] */
  uint level; /* [0, ..., OVERLAY_GRID_STEPS_DRAW - 1] */
};

/* Helper; gl_VertexID implicitly encodes a grid line. */
LineData decode_grid_data(uint vertex_id)
{
  LineData line;

  /* Every pair of consecutive verts forms a line, indicated by bit 0.
   * Every pair of consecutive lines flips x/y, indicated by bit 1. */
  uint side = vertex_id & 0x1u;
  vertex_id = vertex_id >> 1u;
  line.axis = vertex_id & 0x1u;
  vertex_id = vertex_id >> 1u;

  /* The index/level of a line are encoded by the remaining bits. We order
   * levels from "large" to "small", drawing the larger levels first. */
  line.level = OVERLAY_GRID_STEPS_DRAW - 1 - int(vertex_id / grid_buf.num_lines);
  vertex_id = vertex_id % grid_buf.num_lines;

  /* From the index, generate N+1 points equidistantly spaced on [-N/2, N/2]. */
  line.P.x = max(float(grid_buf.num_lines >> 1u), 1.0f);
  line.P.y = (float(vertex_id) - float(grid_buf.num_lines >> 1u));

  /* If this isn't the line start, flip the x-component to the end. Likewise,
   * if this isn't the x-direction, flip components to define the y-direction. */
  line.P.x = side != 0 ? line.P.x : -line.P.x;
  line.P.xy = line.axis != 0 ? line.P.yx : line.P.xy;

  return line;
}

/* Helper; gl_VertexID implicitly encodes one of three axis lines. */
LineData decode_axis_data(uint vertex_id)
{
  LineData line;

  /* Every pair of consecutive verts forms a line, indicated by bit 0.
   * They then alternate x/y/z, indicated by the remaining bits. */
  uint side = vertex_id & 0x1u;
  line.axis = vertex_id >> 1u;
  /* For an axis line, the level is fixed, and the direction is simply the vertex index. */
  line.level = OVERLAY_GRID_STEPS_DRAW - 1;
  /* Output a vertex as [-N/2, N/2], [0, 0]. */
  line.P.x = max(float(grid_buf.num_lines >> 1u), 1.0f) * select(1.0f, -1.0f, side);
  line.P.y = 0.0f;

  return line;
}

/* Returns true if components of `v` fall within `epsilon` of 0. */
bool2 is_zero(float2 v, float epsilon)
{
  return lessThanEqual(abs(v), float2(epsilon));
}

/* Test if the current line falls under an active axis line which occludes it. */
bool is_occluded_by_axis(float3 vertex_pos_global)
{
  if (flag_test(grid_flag, SHOW_GRID)) {
    return (flag_test(grid_flag, AXIS_X) && all(is_zero(vertex_pos_global.yz, 1e-4f))) ||
           (flag_test(grid_flag, AXIS_Y) && all(is_zero(vertex_pos_global.xz, 1e-4f))) ||
           (flag_test(grid_flag, AXIS_Z) && all(is_zero(vertex_pos_global.xy, 1e-4f)));
  }
  return false;
}

/* Test if the current line falls under another line on a higher level, which occludes it. */
bool is_occluded_by_higher_level(LineData line, uint level)
{
  if (flag_test(grid_flag, SHOW_GRID) && !flag_test(grid_flag, GRID_SIMA)) {
    if (line.level < OVERLAY_GRID_STEPS_DRAW - 1 && level < OVERLAY_GRID_STEPS_LEN - 1) {
      /* To determine if a higher up line occludes the current line; we solve the following:
       * given scalars s1, s2 and integer i, is there an integer j : i*s1 = j*s2. The value in
       * `line.P` holds i*s1, so we compute j = i*s1/s2 and verify if it is an integer.  */
      float j = abs(line.P[1 - line.axis]) / grid_buf.steps[level + 1][line.axis];
      if (is_equal(floor(j), j, 1e-4f)) {
        return true;
      }
    }
  }
  return false;
}

float2 screen_position(float4 p)
{
  return ((p.xy / p.w) * 0.5f + 0.5f) * uniform_buf.size_viewport;
}

void main()
{
  gl_Position = float4(NAN_FLT); /* Discard by default. */
  LineData line = flag_test(grid_flag, SHOW_GRID) ? decode_grid_data(gl_VertexID) :
                                                    decode_axis_data(gl_VertexID);

  /* Compute the actual level of a line, offset by -1 to force a sub-level in the 3D viewport. */
  int level = int(grid_buf.level) + int(line.level) - (flag_test(grid_flag, GRID_SIMA) ? 0 : 1);
  level = clamp(level, 0, OVERLAY_GRID_STEPS_LEN - 1);

  /* Compute per-level size, camera offset for lines. Offset is rounded to the nearest
   * level-dependent line position for grid, while axes simply move with the camera. */
  float step_size = grid_buf.steps[level][line.axis];
  float2 step_offs = flag_test(grid_flag, SHOW_GRID) ?
                         round(grid_buf.offset / step_size) * step_size :
                         float2(drw_view_position()[line.axis], 0.0f); /* Store value on X-axis. */

  /* Output vertex position in [-1,1], which we use to fade level boundaries. */
  vertex_out.coord = line.P / max(float(grid_buf.num_lines >> 1), 1.0f);
  /* Output an interpolant between `grid` and `grid_emphasis` for the second grid level. */
  vertex_out_flat.emphasis = saturate(float(line.level) - fract(grid_buf.level));
  /* Output alpha that smoothly transitions the lowest grid level in/out. */
  vertex_out_flat.alpha = saturate(line.level + 1.0f - fract(grid_buf.level));
  if (!drw_view_is_perspective()) {
    /* Also fade by pixel size for orthographic, as we lack proper line DFDX/DFDY. */
    vertex_out_flat.alpha *= smoothstep(
        step_size * 0.25f, step_size * pow3f(0.25f), uniform_buf.pixel_fac);
  }

  /* Apply per-level size, camera offset. */
  line.P = step_offs + step_size * line.P;

  /* Compute clipping rectangle. */
  float2 clip_min, clip_max;
  if (flag_test(grid_flag, GRID_SIMA)) {
    clip_min = float2(-1.0f);
    clip_max = grid_buf.clip_rect * 2.0f - 1.0f;
  }
  else if (flag_test(grid_flag, SHOW_GRID)) {
    clip_min = grid_buf.offset - grid_buf.clip_rect;
    clip_max = grid_buf.offset + grid_buf.clip_rect;
  }
  else { /* SHOW_AXES */
    /* Apply clipping on X-axis; this value is moved to the correct axis below. */
    uint offset_idx = drw_view_is_perspective() ? line.axis : 0;
    clip_min = float2(grid_buf.offset[offset_idx] - grid_buf.clip_rect[line.axis], 0.0f);
    clip_max = float2(grid_buf.offset[offset_idx] + grid_buf.clip_rect[line.axis], 0.0f);
  }

  /* Clip/clamp; lines entirely outside the rectangle get discarded; others get brought
   * inside the rectangle to avoid precision problems with large lines. Z-axis ignores this step.
   */
  if (line.axis != 2) {
    bool line_outside_rect = all(lessThan(line.P, clip_min)) || all(greaterThan(line.P, clip_max));
    if (line_outside_rect) {
      return; /* Discard line. */
    }
    line.P = clamp(line.P, clip_min, clip_max);
  }

  /* Output world-space position. */
  vertex_out.pos = float3(0.0f);
  if (flag_test(grid_flag, SHOW_GRID)) {
    /* Position is placed on the correct plane. */
    if (flag_test(grid_flag, PLANE_XY)) {
      vertex_out.pos.xy = line.P;
    }
    else if (flag_test(grid_flag, PLANE_XZ)) {
      vertex_out.pos.xz = line.P;
    }
    else if (flag_test(grid_flag, PLANE_YZ)) {
      vertex_out.pos.yz = line.P;
    }
    else { /* GRID_SIMA */
      /* Set z to place the grid in front of/behind image, and always behind the UV mesh.
       * See `overlay_edit_uv_edges_vert.glsl` for the full z-order. */
      float z = flag_test(grid_flag, GRID_OVER_IMAGE) ? 0.74f : 0.76f;
      vertex_out.pos = float3(line.P * 0.5f + 0.5f, z);
    }
  }
  else { /* SHOW_AXES */
    /* Test X/Y/Z axis flags per line */
    constexpr uint axis_flags[3] = {AXIS_X, AXIS_Y, AXIS_Z};
    if (!flag_test(grid_flag, axis_flags[line.axis])) {
      return; /* Discard line. */
    }
    vertex_out.pos[line.axis] = line.P.x;
  }

  /* Additional culling steps to discard occluded lines. */
  if (is_occluded_by_axis(vertex_out.pos) || is_occluded_by_higher_level(line, level)) {
    return; /* Discard line. */
  }

  gl_Position = drw_view().winmat * (drw_view().viewmat * float4(vertex_out.pos, 1.0f));

  /* Adjust z-component */
  if (drw_view_is_perspective()) {
    /* To minimize z-fighting, the grid is drawn N times with progressive alpha and z-bias,
     * making it fade through geometry. The smaller the range below, the more it pops in. */
    float z_factor = float(grid_iter * OVERLAY_GRID_STEPS_DRAW + line.level) /
                     float(OVERLAY_GRID_ITER_LEN * OVERLAY_GRID_STEPS_DRAW);
    gl_Position.z += mix(5e-4f, 1e-4f, z_factor);
  }
  else { /* orthographic */
    /* Set z to far plane in orthographic, so it is behind all things. */
    if (!flag_test(grid_flag, GRID_SIMA)) {
      gl_Position.z = 1.0f;
    }
  }

  /* Stage output for viewport anti-aliasing/alpha dithering. */
  edge_start = edge_pos = screen_position(gl_Position);
}
