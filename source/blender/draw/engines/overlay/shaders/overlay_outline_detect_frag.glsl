/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_outline_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_outline_detect)

#include "draw_view_lib.glsl"
#include "overlay_common_lib.glsl"

#define XPOS (1 << 0)
#define XNEG (1 << 1)
#define YPOS (1 << 2)
#define YNEG (1 << 3)

#define ALL (XPOS | XNEG | YPOS | YNEG)
#define NONE 0

#define DIAG_XNEG_YPOS (XNEG | YPOS)
#define DIAG_XPOS_YPOS (XPOS | YPOS)
#define DIAG_XPOS_YNEG (XPOS | YNEG)
#define DIAG_XNEG_YNEG (XNEG | YNEG)

#define APEX_XPOS (ALL & (~XPOS))
#define APEX_XNEG (ALL & (~XNEG))
#define APEX_YPOS (ALL & (~YPOS))
#define APEX_YNEG (ALL & (~YNEG))

bool has_edge(uint id, float2 uv, uint ref, inout uint ref_col, inout float2 depth_uv)
{
  if (ref_col == 0u) {
    /* Make outline bleed on the background. */
    ref_col = id;
    depth_uv = uv;
  }
  return (id != ref);
}

/* A gather4 + check against ref. */
bool4 gather_edges(float2 uv, uint ref)
{
  uint4 ids;
#ifdef GPU_ARB_texture_gather
  ids = textureGather(outline_id_tx, uv);
#else
  float3 ofs = float3(0.5f, 0.5f, -0.5f) * uniform_buf.size_viewport_inv.xyy;
  ids.x = textureLod(outline_id_tx, uv - ofs.xz, 0.0f).r;
  ids.y = textureLod(outline_id_tx, uv + ofs.xy, 0.0f).r;
  ids.z = textureLod(outline_id_tx, uv + ofs.xz, 0.0f).r;
  ids.w = textureLod(outline_id_tx, uv - ofs.xy, 0.0f).r;
#endif

  return notEqual(ids, uint4(ref));
}

/* Clockwise */
float2 rotate_90(float2 v)
{
  return float2(v.y, -v.x);
}
float2 rotate_180(float2 v)
{
  return float2(-v.x, -v.y);
}
float2 rotate_270(float2 v)
{
  return float2(-v.y, v.x);
}

/* Counter-Clockwise */
bool4 rotate_90(bool4 v)
{
  return v.yzwx;
}
bool4 rotate_180(bool4 v)
{
  return v.zwxy;
}
bool4 rotate_270(bool4 v)
{
  return v.wxyz;
}

/* Apply offset to line endpoint based on surrounding edges infos. */
bool line_offset(bool2 edges, float2 ofs, inout float2 line_point)
{
  if (all(edges.xy)) {
    line_point.y -= ofs.y;
  }
  else if (!edges.x) {
    line_point.y += ofs.y;
  }
  else /* !edges.y */ {
    line_point.x += ofs.x;
    return true;
  }
  return false;
}

/* Changes Anti-aliasing pattern and makes line thicker. 0.0 is thin. */
#define PROXIMITY_OFS -0.35f

/* Use surrounding edges to approximate the outline direction to create smooth lines. */
void straight_line_dir(bool4 edges1, bool4 edges2, out float2 line_start, out float2 line_end)
{
  /* Y_POS as reference. Other cases are rotated to match reference. */
  line_end = float2(1.5f, 0.5f + PROXIMITY_OFS);
  line_start = float2(-line_end.x, line_end.y);

  float2 line_ofs = float2(1.0f, 0.5f);
  if (line_offset(edges1.xw, line_ofs, line_end)) {
    line_offset(edges1.yz, line_ofs, line_end);
  }
  line_ofs = float2(-line_ofs.x, line_ofs.y);
  if (line_offset(edges2.yz, line_ofs, line_start)) {
    line_offset(edges2.xw, line_ofs, line_start);
  }
}

float2 diag_offset(bool4 edges)
{
  /* X_NEG | Y_POS as reference. Other cases are rotated to match reference.
   * So the line is coming from bottom left. */
  if (all(edges.wz)) {
    /* Horizontal line. */
    return float2(2.5f, 0.5f);
  }
  else if (all(not(edges.xw))) {
    /* Vertical line. */
    return float2(0.5f, 2.5f);
  }
  else if (edges.w) {
    /* Less horizontal Line. */
    return float2(2.5f, 0.5f);
  }
  else {
    /* Less vertical Line. */
    return float2(0.5f, 2.5f);
  }
}

/* Compute line direction vector from the bottom left corner. */
void diag_dir(bool4 edges1, bool4 edges2, out float2 line_start, out float2 line_end)
{
  /* Negate instead of rotating back the result of diag_offset. */
  edges2 = not(edges2);
  edges2 = rotate_180(edges2);
  line_end = diag_offset(edges1);
  line_end += diag_offset(edges2);

  if (line_end.x == line_end.y) {
    /* Perfect diagonal line. Push line start towards edge. */
    line_start = float2(-1.0f, 1.0f) * PROXIMITY_OFS * 0.4f;
  }
  else if (line_end.x > line_end.y) {
    /* Horizontal Line. Lower line start. */
    line_start = float2(0.0f, PROXIMITY_OFS);
  }
  else {
    /* Vertical Line. Push line start to the right. */
    line_start = -float2(PROXIMITY_OFS, 0.0f);
  }
  line_end += line_start;
}

void main()
{
  uint ref = textureLod(outline_id_tx, screen_uv, 0.0f).r;
  uint ref_col = ref;

  float2 uvs = gl_FragCoord.xy * uniform_buf.size_viewport_inv;
  float3 ofs = float3(uniform_buf.size_viewport_inv, 0.0f);

  float2 depth_uv = uvs;

  uint4 ids;
#ifdef GPU_ARB_texture_gather
  /* Reminder: Samples order is CW starting from top left. */
  uint2 tmp1, tmp2, tmp3, tmp4;
  if (do_thick_outlines) {
    tmp1 = textureGather(outline_id_tx, uvs + ofs.xy * float2(1.5f, -0.5f)).xy;
    tmp2 = textureGather(outline_id_tx, uvs + ofs.xy * float2(-1.5f, -0.5f)).yx;
    tmp3 = textureGather(outline_id_tx, uvs + ofs.xy * float2(0.5f, 1.5f)).wx;
    tmp4 = textureGather(outline_id_tx, uvs + ofs.xy * float2(0.5f, -1.5f)).xw;
    ids.x = tmp1.x;
    ids.y = tmp2.x;
    ids.z = tmp3.x;
    ids.w = tmp4.x;
  }
  else {
    ids.xz = textureGather(outline_id_tx, uvs + ofs.xy * 0.5f).zx;
    ids.yw = textureGather(outline_id_tx, uvs - ofs.xy * 0.5f).xz;
  }
#else
  ids.x = textureLod(outline_id_tx, uvs + ofs.xz, 0.0f).r;
  ids.y = textureLod(outline_id_tx, uvs - ofs.xz, 0.0f).r;
  ids.z = textureLod(outline_id_tx, uvs + ofs.zy, 0.0f).r;
  ids.w = textureLod(outline_id_tx, uvs - ofs.zy, 0.0f).r;
#endif

  bool has_edge_pos_x = has_edge(ids.x, uvs + ofs.xz, ref, ref_col, depth_uv);
  bool has_edge_neg_x = has_edge(ids.y, uvs - ofs.xz, ref, ref_col, depth_uv);
  bool has_edge_pos_y = has_edge(ids.z, uvs + ofs.zy, ref, ref_col, depth_uv);
  bool has_edge_neg_y = has_edge(ids.w, uvs - ofs.zy, ref, ref_col, depth_uv);

  if (do_thick_outlines) {
    if (!any(bool4(has_edge_pos_x, has_edge_neg_x, has_edge_pos_y, has_edge_neg_y))) {
#ifdef GPU_ARB_texture_gather
      ids.x = tmp1.y;
      ids.y = tmp2.y;
      ids.z = tmp3.y;
      ids.w = tmp4.y;
#else
      ids.x = textureLod(outline_id_tx, uvs + 2.0f * ofs.xz, 0.0f).r;
      ids.y = textureLod(outline_id_tx, uvs - 2.0f * ofs.xz, 0.0f).r;
      ids.z = textureLod(outline_id_tx, uvs + 2.0f * ofs.zy, 0.0f).r;
      ids.w = textureLod(outline_id_tx, uvs - 2.0f * ofs.zy, 0.0f).r;
#endif

      has_edge_pos_x = has_edge(ids.x, uvs + 2.0f * ofs.xz, ref, ref_col, depth_uv);
      has_edge_neg_x = has_edge(ids.y, uvs - 2.0f * ofs.xz, ref, ref_col, depth_uv);
      has_edge_pos_y = has_edge(ids.z, uvs + 2.0f * ofs.zy, ref, ref_col, depth_uv);
      has_edge_neg_y = has_edge(ids.w, uvs - 2.0f * ofs.zy, ref, ref_col, depth_uv);
    }
  }

  if (is_xray_wires) {
    /* Don't inflate the wire outlines too much. */
    has_edge_neg_x = has_edge_neg_y = false;
  }

  /* WATCH: Keep in sync with outline_id_tx of the pre-pass. */
  uint color_id = ref_col >> 14u;
  if (ref_col == 0u) {
    frag_color = float4(0.0f);
  }
  else if (color_id == 1u) {
    frag_color = theme.colors.object_select;
  }
  else if (color_id == 3u) {
    frag_color = theme.colors.active_object;
  }
  else {
    frag_color = theme.colors.transform;
  }

  float ref_depth = textureLod(outline_depth_tx, depth_uv, 0.0f).r;
  float scene_depth = textureLod(scene_depth_tx, depth_uv, 0.0f).r;

  /* Avoid bad cases of Z-fighting for occlusion only. */
  constexpr float epsilon = 3.0f / 8388608.0f;
  bool occluded = (ref_depth > scene_depth + epsilon);

  /* NOTE: We never set alpha to 1.0 to avoid Anti-aliasing destroying the line. */
  frag_color *= (occluded ? alpha_occlu : 1.0f) * (254.0f / 255.0f);

  int edge_case = 0;
  edge_case += int(has_edge_pos_x) * XPOS;
  edge_case += int(has_edge_neg_x) * XNEG;
  edge_case += int(has_edge_pos_y) * YPOS;
  edge_case += int(has_edge_neg_y) * YNEG;

  if (edge_case == ALL || edge_case == NONE) {
    /* NOTE(Metal): Discards are not explicit returns in Metal. We should also return to avoid
     * erroneous derivatives which can manifest during texture sampling in
     * non-uniform-control-flow. */
    gpu_discard_fragment();
    return;
  }

  if (!do_anti_aliasing) {
    line_output = float4(0.0f);
    return;
  }

  float2 line_start, line_end;
  bool4 extra_edges, extra_edges2;
  /* TODO: simplify this branching hell. */
  switch (edge_case) {
      /* Straight lines. */
    case YPOS:
      extra_edges = gather_edges(uvs + uniform_buf.size_viewport_inv * float2(2.5f, 0.5f), ref);
      extra_edges2 = gather_edges(uvs + uniform_buf.size_viewport_inv * float2(-2.5f, 0.5f), ref);
      straight_line_dir(extra_edges, extra_edges2, line_start, line_end);
      break;
    case YNEG:
      extra_edges = gather_edges(uvs + uniform_buf.size_viewport_inv * float2(-2.5f, -0.5f), ref);
      extra_edges2 = gather_edges(uvs + uniform_buf.size_viewport_inv * float2(2.5f, -0.5f), ref);
      extra_edges = rotate_180(extra_edges);
      extra_edges2 = rotate_180(extra_edges2);
      straight_line_dir(extra_edges, extra_edges2, line_start, line_end);
      line_start = rotate_180(line_start);
      line_end = rotate_180(line_end);
      break;
    case XPOS:
      extra_edges = gather_edges(uvs + uniform_buf.size_viewport_inv * float2(0.5f, 2.5f), ref);
      extra_edges2 = gather_edges(uvs + uniform_buf.size_viewport_inv * float2(0.5f, -2.5f), ref);
      extra_edges = rotate_90(extra_edges);
      extra_edges2 = rotate_90(extra_edges2);
      straight_line_dir(extra_edges, extra_edges2, line_start, line_end);
      line_start = rotate_90(line_start);
      line_end = rotate_90(line_end);
      break;
    case XNEG:
      extra_edges = gather_edges(uvs + uniform_buf.size_viewport_inv * float2(-0.5f, 2.5f), ref);
      extra_edges2 = gather_edges(uvs + uniform_buf.size_viewport_inv * float2(-0.5f, -2.5f), ref);
      extra_edges = rotate_270(extra_edges);
      extra_edges2 = rotate_270(extra_edges2);
      straight_line_dir(extra_edges, extra_edges2, line_start, line_end);
      line_start = rotate_270(line_start);
      line_end = rotate_270(line_end);
      break;

      /* Diagonal */
    case DIAG_XNEG_YPOS:
      extra_edges = gather_edges(uvs + ofs.xy * float2(1.5f), ref);
      extra_edges2 = gather_edges(uvs + ofs.xy * float2(-1.5f), ref);
      diag_dir(extra_edges, extra_edges2, line_start, line_end);
      break;
    case DIAG_XPOS_YNEG:
      extra_edges = gather_edges(uvs - ofs.xy * float2(1.5f), ref);
      extra_edges2 = gather_edges(uvs - ofs.xy * float2(-1.5f), ref);
      extra_edges = rotate_180(extra_edges);
      extra_edges2 = rotate_180(extra_edges2);
      diag_dir(extra_edges, extra_edges2, line_start, line_end);
      line_start = rotate_180(line_start);
      line_end = rotate_180(line_end);
      break;
    case DIAG_XPOS_YPOS:
      extra_edges = gather_edges(uvs + ofs.xy * float2(1.5f, -1.5f), ref);
      extra_edges2 = gather_edges(uvs - ofs.xy * float2(1.5f, -1.5f), ref);
      extra_edges = rotate_90(extra_edges);
      extra_edges2 = rotate_90(extra_edges2);
      diag_dir(extra_edges, extra_edges2, line_start, line_end);
      line_start = rotate_90(line_start);
      line_end = rotate_90(line_end);
      break;
    case DIAG_XNEG_YNEG:
      extra_edges = gather_edges(uvs - ofs.xy * float2(1.5f, -1.5f), ref);
      extra_edges2 = gather_edges(uvs + ofs.xy * float2(1.5f, -1.5f), ref);
      extra_edges = rotate_270(extra_edges);
      extra_edges2 = rotate_270(extra_edges2);
      diag_dir(extra_edges, extra_edges2, line_start, line_end);
      line_start = rotate_270(line_start);
      line_end = rotate_270(line_end);
      break;

      /* Apex */
    case APEX_XPOS:
    case APEX_XNEG:
      line_start = float2(-0.5f, 0.0f);
      line_end = float2(0.5f, 0.0f);
      break;
    case APEX_YPOS:
    case APEX_YNEG:
      line_start = float2(0.0f, -0.5f);
      line_end = float2(0.0f, 0.5f);
      break;
    default:
      /* Ensure values are assigned to, avoids undefined behavior for
       * divergent control-flow. This can occur if discard is called
       * as discard is not treated as a return in Metal 2.2. So
       * side-effects can still cause problems. */
      line_start = float2(0.0f);
      line_end = float2(0.0f);
      break;
  }

  line_output = pack_line_data(float2(0.0f), line_start, line_end);
}
