/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma USE_SSBO_VERTEX_FETCH(TriangleList, 6)

/* Local vars to store results per input vertex. */
#if !defined(UNIFORM)
vec4 finalColor_g[2];
#endif

#ifdef CLIP
float clip_g[2];
#endif

#define SMOOTH_WIDTH 1.0

/* Clips point to near clip plane before perspective divide. */
vec4 clip_line_point_homogeneous_space(vec4 p, vec4 q)
{
  if (p.z < -p.w) {
    /* Just solves p + (q - p) * A; for A when p.z / p.w = -1.0. */
    float denom = q.z - p.z + q.w - p.w;
    if (denom == 0.0) {
      /* No solution. */
      return p;
    }
    float A = (-p.z - p.w) / denom;
    p = p + (q - p) * A;
  }
  return p;
}

void do_vertex(int index, vec4 pos, vec2 ofs, float flip)
{
#if defined(UNIFORM)
  interp.final_color = color;

#elif defined(FLAT)
  /* WATCH: Assuming last provoking vertex. */
  interp.final_color = finalColor_g[index];

#elif defined(SMOOTH)
  interp.final_color = finalColor_g[index];
#endif

#ifdef CLIP
  interp.clip = clip_g[index];
#endif

  interp_noperspective.smoothline = flip * (lineWidth + SMOOTH_WIDTH * float(lineSmooth)) * 0.5;
  gl_Position = pos;
  gl_Position.xy += flip * ofs * pos.w;
}

void main()
{
  /** Determine output quad primitive structure. */
  /* Index of the quad primitive. Each quad corresponds to one line in the input primitive. */
  int quad_id = gl_VertexID / 6;

  /* Determine vertex within the quad (A, B, C)(A, C, D). */
  int quad_vertex_id = gl_VertexID % 6;

  uint src_index_a;
  uint src_index_b;
  if (vertex_fetch_get_input_prim_type() == GPU_PRIM_LINE_STRIP) {
    src_index_a = quad_id;
    src_index_b = quad_id + 1;
  }
  else if (vertex_fetch_get_input_prim_type() == GPU_PRIM_LINES) {
    src_index_a = quad_id * 2;
    src_index_b = quad_id * 2 + 1;
  }
  else if (vertex_fetch_get_input_prim_type() == GPU_PRIM_LINE_LOOP) {
    src_index_a = quad_id;
    src_index_b = quad_id + 1;
    if (quad_id == vertex_fetch_get_input_vert_count() - 1) {
      src_index_b = 0;
    }
  }
  else {
    src_index_a = 0;
    src_index_b = 0;
  }

  /* Fetch input attributes for line prims -- either provided as vec2 or vec3 -- So we need to
   * query the type. */
  vec3 in_pos0, in_pos1;
  in_pos0 = vec3(0.0);
  in_pos1 = vec3(0.0);
  if (vertex_fetch_get_attr_type(pos) == GPU_SHADER_ATTR_TYPE_VEC4) {
    in_pos0 = vertex_fetch_attribute(src_index_a, pos, vec4).xyz;
    in_pos1 = vertex_fetch_attribute(src_index_b, pos, vec4).xyz;
  }
  else if (vertex_fetch_get_attr_type(pos) == GPU_SHADER_ATTR_TYPE_VEC3) {
    in_pos0 = vertex_fetch_attribute(src_index_a, pos, vec3);
    in_pos1 = vertex_fetch_attribute(src_index_b, pos, vec3);
  }
  else if (vertex_fetch_get_attr_type(pos) == GPU_SHADER_ATTR_TYPE_VEC2) {
    in_pos0 = vec3(vertex_fetch_attribute(src_index_a, pos, vec2), 0.0);
    in_pos1 = vec3(vertex_fetch_attribute(src_index_b, pos, vec2), 0.0);
  }
#if !defined(UNIFORM)
  vec4 in_color0 = vec4(0.0);
  vec4 in_color1 = vec4(0.0);

  if (vertex_fetch_get_attr_type(color) == GPU_SHADER_ATTR_TYPE_VEC4) {
    in_color0 = vertex_fetch_attribute(src_index_a, color, vec4);
    in_color1 = vertex_fetch_attribute(src_index_b, color, vec4);
  }
  else if (vertex_fetch_get_attr_type(color) == GPU_SHADER_ATTR_TYPE_VEC3) {
    in_color0 = vec4(vertex_fetch_attribute(src_index_a, color, vec3), 1.0);
    in_color1 = vec4(vertex_fetch_attribute(src_index_b, color, vec3), 1.0);
  }
  else if (vertex_fetch_get_attr_type(color) == GPU_SHADER_ATTR_TYPE_UCHAR4_NORM) {
    in_color0 = vec4(vertex_fetch_attribute(src_index_a, color, uchar4)) / vec4(255.0);
    in_color1 = vec4(vertex_fetch_attribute(src_index_b, color, uchar4)) / vec4(255.0);
  }
  else if (vertex_fetch_get_attr_type(color) == GPU_SHADER_ATTR_TYPE_UCHAR3_NORM) {
    in_color0 = vec4(vec3(vertex_fetch_attribute(src_index_a, color, uchar3)) / vec3(255.0), 1.0);
    in_color1 = vec4(vec3(vertex_fetch_attribute(src_index_b, color, uchar3)) / vec3(255.0), 1.0);
  }
#endif

  /* Calculate Vertex shader for both points in Line. */
  vec4 out_pos0 = ModelViewProjectionMatrix * vec4(in_pos0, 1.0);
  vec4 out_pos1 = ModelViewProjectionMatrix * vec4(in_pos1, 1.0);
#if !defined(UNIFORM)
  finalColor_g[0] = in_color0;
  finalColor_g[1] = in_color1;
#endif
#ifdef CLIP
  clip_g[0] = dot(ModelMatrix * vec4(in_pos0, 1.0), ClipPlane);
  clip_g[1] = dot(ModelMatrix * vec4(in_pos1, 1.0), ClipPlane);
#endif

  /** Geometry Shader Alternative. */
  vec4 p0 = clip_line_point_homogeneous_space(out_pos0, out_pos1);
  vec4 p1 = clip_line_point_homogeneous_space(out_pos1, out_pos0);
  vec2 e = normalize(((p1.xy / p1.w) - (p0.xy / p0.w)) * viewportSize.xy);

#if 0 /* Hard turn when line direction changes quadrant. */
  e = abs(e);
  vec2 ofs = (e.x > e.y) ? vec2(0.0, 1.0 / e.x) : vec2(1.0 / e.y, 0.0);
#else /* Use perpendicular direction. */
  vec2 ofs = vec2(-e.y, e.x);
#endif

  ofs /= viewportSize.xy;
  ofs *= lineWidth + SMOOTH_WIDTH * float(lineSmooth);

  if (quad_vertex_id == 0) {
    do_vertex(0, p0, ofs, 1.0);
  }
  else if (quad_vertex_id == 1 || quad_vertex_id == 3) {
    do_vertex(0, p0, ofs, -1.0);
  }
  else if (quad_vertex_id == 2 || quad_vertex_id == 5) {
    do_vertex(1, p1, ofs, 1.0);
  }
  else if (quad_vertex_id == 4) {
    do_vertex(1, p1, ofs, -1.0);
  }
}
