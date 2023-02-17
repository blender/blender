
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

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

bool has_edge(uint id, vec2 uv, uint ref, inout uint ref_col, inout vec2 depth_uv)
{
  if (ref_col == 0u) {
    /* Make outline bleed on the background. */
    ref_col = id;
    depth_uv = uv;
  }
  return (id != ref);
}

/* A gather4 + check against ref. */
bvec4 gather_edges(vec2 uv, uint ref)
{
  uvec4 ids;
#ifdef GPU_ARB_texture_gather
  ids = textureGather(outlineId, uv);
#else
  vec3 ofs = vec3(0.5, 0.5, -0.5) * sizeViewportInv.xyy;
  ids.x = textureLod(outlineId, uv - ofs.xz, 0.0).r;
  ids.y = textureLod(outlineId, uv + ofs.xy, 0.0).r;
  ids.z = textureLod(outlineId, uv + ofs.xz, 0.0).r;
  ids.w = textureLod(outlineId, uv - ofs.xy, 0.0).r;
#endif

  return notEqual(ids, uvec4(ref));
}

/* Clockwise */
vec2 rotate_90(vec2 v)
{
  return vec2(v.y, -v.x);
}
vec2 rotate_180(vec2 v)
{
  return vec2(-v.x, -v.y);
}
vec2 rotate_270(vec2 v)
{
  return vec2(-v.y, v.x);
}

/* Counter-Clockwise */
bvec4 rotate_90(bvec4 v)
{
  return v.yzwx;
}
bvec4 rotate_180(bvec4 v)
{
  return v.zwxy;
}
bvec4 rotate_270(bvec4 v)
{
  return v.wxyz;
}

/* Apply offset to line endpoint based on surrounding edges infos. */
bool line_offset(bvec2 edges, vec2 ofs, inout vec2 line_point)
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

/* Changes Antialiasing pattern and makes line thicker. 0.0 is thin. */
#define PROXIMITY_OFS -0.35

/* Use surrounding edges to approximate the outline direction to create smooth lines. */
void straight_line_dir(bvec4 edges1, bvec4 edges2, out vec2 line_start, out vec2 line_end)
{
  /* Y_POS as reference. Other cases are rotated to match reference. */
  line_end = vec2(1.5, 0.5 + PROXIMITY_OFS);
  line_start = vec2(-line_end.x, line_end.y);

  vec2 line_ofs = vec2(1.0, 0.5);
  if (line_offset(edges1.xw, line_ofs, line_end)) {
    line_offset(edges1.yz, line_ofs, line_end);
  }
  line_ofs = vec2(-line_ofs.x, line_ofs.y);
  if (line_offset(edges2.yz, line_ofs, line_start)) {
    line_offset(edges2.xw, line_ofs, line_start);
  }
}

vec2 diag_offset(bvec4 edges)
{
  /* X_NEG | Y_POS as reference. Other cases are rotated to match reference.
   * So the line is coming from bottom left. */
  if (all(edges.wz)) {
    /* Horizontal line. */
    return vec2(2.5, 0.5);
  }
  else if (all(not(edges.xw))) {
    /* Vertical line. */
    return vec2(0.5, 2.5);
  }
  else if (edges.w) {
    /* Less horizontal Line. */
    return vec2(2.5, 0.5);
  }
  else {
    /* Less vertical Line. */
    return vec2(0.5, 2.5);
  }
}

/* Compute line direction vector from the bottom left corner. */
void diag_dir(bvec4 edges1, bvec4 edges2, out vec2 line_start, out vec2 line_end)
{
  /* Negate instead of rotating back the result of diag_offset. */
  edges2 = not(edges2);
  edges2 = rotate_180(edges2);
  line_end = diag_offset(edges1);
  line_end += diag_offset(edges2);

  if (line_end.x == line_end.y) {
    /* Perfect diagonal line. Push line start towards edge. */
    line_start = vec2(-1.0, 1.0) * PROXIMITY_OFS * 0.4;
  }
  else if (line_end.x > line_end.y) {
    /* Horizontal Line. Lower line start. */
    line_start = vec2(0.0, PROXIMITY_OFS);
  }
  else {
    /* Vertical Line. Push line start to the right. */
    line_start = -vec2(PROXIMITY_OFS, 0.0);
  }
  line_end += line_start;
}

void main()
{
  uint ref = textureLod(outlineId, uvcoordsvar.xy, 0.0).r;
  uint ref_col = ref;

  vec2 uvs = gl_FragCoord.xy * sizeViewportInv;
  vec3 ofs = vec3(sizeViewportInv.xy, 0.0);

  vec2 depth_uv = uvs;

  uvec4 ids;
#ifdef GPU_ARB_texture_gather
  /* Reminder: Samples order is CW starting from top left. */
  uvec2 tmp1, tmp2, tmp3, tmp4;
  if (doThickOutlines) {
    tmp1 = textureGather(outlineId, uvs + ofs.xy * vec2(1.5, -0.5)).xy;
    tmp2 = textureGather(outlineId, uvs + ofs.xy * vec2(-1.5, -0.5)).yx;
    tmp3 = textureGather(outlineId, uvs + ofs.xy * vec2(0.5, 1.5)).wx;
    tmp4 = textureGather(outlineId, uvs + ofs.xy * vec2(0.5, -1.5)).xw;
    ids.x = tmp1.x;
    ids.y = tmp2.x;
    ids.z = tmp3.x;
    ids.w = tmp4.x;
  }
  else {
    ids.xz = textureGather(outlineId, uvs + ofs.xy * 0.5).zx;
    ids.yw = textureGather(outlineId, uvs - ofs.xy * 0.5).xz;
  }
#else
  ids.x = textureLod(outlineId, uvs + ofs.xz, 0.0).r;
  ids.y = textureLod(outlineId, uvs - ofs.xz, 0.0).r;
  ids.z = textureLod(outlineId, uvs + ofs.zy, 0.0).r;
  ids.w = textureLod(outlineId, uvs - ofs.zy, 0.0).r;
#endif

  bool has_edge_pos_x = has_edge(ids.x, uvs + ofs.xz, ref, ref_col, depth_uv);
  bool has_edge_neg_x = has_edge(ids.y, uvs - ofs.xz, ref, ref_col, depth_uv);
  bool has_edge_pos_y = has_edge(ids.z, uvs + ofs.zy, ref, ref_col, depth_uv);
  bool has_edge_neg_y = has_edge(ids.w, uvs - ofs.zy, ref, ref_col, depth_uv);

  if (doThickOutlines) {
    if (!any(bvec4(has_edge_pos_x, has_edge_neg_x, has_edge_pos_y, has_edge_neg_y))) {
#ifdef GPU_ARB_texture_gather
      ids.x = tmp1.y;
      ids.y = tmp2.y;
      ids.z = tmp3.y;
      ids.w = tmp4.y;
#else
      ids.x = textureLod(outlineId, uvs + 2.0 * ofs.xz, 0.0).r;
      ids.y = textureLod(outlineId, uvs - 2.0 * ofs.xz, 0.0).r;
      ids.z = textureLod(outlineId, uvs + 2.0 * ofs.zy, 0.0).r;
      ids.w = textureLod(outlineId, uvs - 2.0 * ofs.zy, 0.0).r;
#endif

      has_edge_pos_x = has_edge(ids.x, uvs + 2.0 * ofs.xz, ref, ref_col, depth_uv);
      has_edge_neg_x = has_edge(ids.y, uvs - 2.0 * ofs.xz, ref, ref_col, depth_uv);
      has_edge_pos_y = has_edge(ids.z, uvs + 2.0 * ofs.zy, ref, ref_col, depth_uv);
      has_edge_neg_y = has_edge(ids.w, uvs - 2.0 * ofs.zy, ref, ref_col, depth_uv);
    }
  }

  if (isXrayWires) {
    /* Don't inflate the wire outlines too much. */
    has_edge_neg_x = has_edge_neg_y = false;
  }

  /* WATCH: Keep in sync with outlineId of the prepass. */
  uint color_id = ref_col >> 14u;
  if (ref_col == 0u) {
    fragColor = vec4(0.0);
  }
  else if (color_id == 1u) {
    fragColor = colorSelect;
  }
  else if (color_id == 3u) {
    fragColor = colorActive;
  }
  else {
    fragColor = colorTransform;
  }

  float ref_depth = textureLod(outlineDepth, depth_uv, 0.0).r;
  float scene_depth = textureLod(sceneDepth, depth_uv, 0.0).r;

  /* Avoid bad cases of zfighting for occlusion only. */
  const float epsilon = 3.0 / 8388608.0;
  bool occluded = (ref_depth > scene_depth + epsilon);

  /* NOTE: We never set alpha to 1.0 to avoid Antialiasing destroying the line. */
  fragColor *= (occluded ? alphaOcclu : 1.0) * (254.0 / 255.0);

  int edge_case = 0;
  edge_case += int(has_edge_pos_x) * XPOS;
  edge_case += int(has_edge_neg_x) * XNEG;
  edge_case += int(has_edge_pos_y) * YPOS;
  edge_case += int(has_edge_neg_y) * YNEG;

  if (edge_case == ALL || edge_case == NONE) {
    /* NOTE(Metal): Discards are not explicit returns in Metal. We should also return to avoid
     * erroneous derivatives which can manifest during texture sampling in
     * non-uniform-control-flow. */
    discard;
    return;
  }

  if (!doAntiAliasing) {
    lineOutput = vec4(0.0);
    return;
  }

  vec2 line_start, line_end;
  vec2 line_ofs;
  bvec4 extra_edges, extra_edges2;
  /* TODO: simplify this branching hell. */
  switch (edge_case) {
      /* Straight lines. */
    case YPOS:
      extra_edges = gather_edges(uvs + sizeViewportInv * vec2(2.5, 0.5), ref);
      extra_edges2 = gather_edges(uvs + sizeViewportInv * vec2(-2.5, 0.5), ref);
      straight_line_dir(extra_edges, extra_edges2, line_start, line_end);
      break;
    case YNEG:
      extra_edges = gather_edges(uvs + sizeViewportInv * vec2(-2.5, -0.5), ref);
      extra_edges2 = gather_edges(uvs + sizeViewportInv * vec2(2.5, -0.5), ref);
      extra_edges = rotate_180(extra_edges);
      extra_edges2 = rotate_180(extra_edges2);
      straight_line_dir(extra_edges, extra_edges2, line_start, line_end);
      line_start = rotate_180(line_start);
      line_end = rotate_180(line_end);
      break;
    case XPOS:
      extra_edges = gather_edges(uvs + sizeViewportInv * vec2(0.5, 2.5), ref);
      extra_edges2 = gather_edges(uvs + sizeViewportInv * vec2(0.5, -2.5), ref);
      extra_edges = rotate_90(extra_edges);
      extra_edges2 = rotate_90(extra_edges2);
      straight_line_dir(extra_edges, extra_edges2, line_start, line_end);
      line_start = rotate_90(line_start);
      line_end = rotate_90(line_end);
      break;
    case XNEG:
      extra_edges = gather_edges(uvs + sizeViewportInv * vec2(-0.5, 2.5), ref);
      extra_edges2 = gather_edges(uvs + sizeViewportInv * vec2(-0.5, -2.5), ref);
      extra_edges = rotate_270(extra_edges);
      extra_edges2 = rotate_270(extra_edges2);
      straight_line_dir(extra_edges, extra_edges2, line_start, line_end);
      line_start = rotate_270(line_start);
      line_end = rotate_270(line_end);
      break;

      /* Diagonal */
    case DIAG_XNEG_YPOS:
      extra_edges = gather_edges(uvs + ofs.xy * vec2(1.5), ref);
      extra_edges2 = gather_edges(uvs + ofs.xy * vec2(-1.5), ref);
      diag_dir(extra_edges, extra_edges2, line_start, line_end);
      break;
    case DIAG_XPOS_YNEG:
      extra_edges = gather_edges(uvs - ofs.xy * vec2(1.5), ref);
      extra_edges2 = gather_edges(uvs - ofs.xy * vec2(-1.5), ref);
      extra_edges = rotate_180(extra_edges);
      extra_edges2 = rotate_180(extra_edges2);
      diag_dir(extra_edges, extra_edges2, line_start, line_end);
      line_start = rotate_180(line_start);
      line_end = rotate_180(line_end);
      break;
    case DIAG_XPOS_YPOS:
      extra_edges = gather_edges(uvs + ofs.xy * vec2(1.5, -1.5), ref);
      extra_edges2 = gather_edges(uvs - ofs.xy * vec2(1.5, -1.5), ref);
      extra_edges = rotate_90(extra_edges);
      extra_edges2 = rotate_90(extra_edges2);
      diag_dir(extra_edges, extra_edges2, line_start, line_end);
      line_start = rotate_90(line_start);
      line_end = rotate_90(line_end);
      break;
    case DIAG_XNEG_YNEG:
      extra_edges = gather_edges(uvs - ofs.xy * vec2(1.5, -1.5), ref);
      extra_edges2 = gather_edges(uvs + ofs.xy * vec2(1.5, -1.5), ref);
      extra_edges = rotate_270(extra_edges);
      extra_edges2 = rotate_270(extra_edges2);
      diag_dir(extra_edges, extra_edges2, line_start, line_end);
      line_start = rotate_270(line_start);
      line_end = rotate_270(line_end);
      break;

      /* Apex */
    case APEX_XPOS:
    case APEX_XNEG:
      line_start = vec2(-0.5, 0.0);
      line_end = vec2(0.5, 0.0);
      break;
    case APEX_YPOS:
    case APEX_YNEG:
      line_start = vec2(0.0, -0.5);
      line_end = vec2(0.0, 0.5);
      break;
    default:
      /* Ensure values are assigned to, avoids undefined behavior for
       * divergent control-flow. This can occur if discard is called
       * as discard is not treated as a return in Metal 2.2. So
       * side-effects can still cause problems. */
      line_start = vec2(0.0);
      line_end = vec2(0.0);
      break;
  }

  lineOutput = pack_line_data(vec2(0.0), line_start, line_end);
}
