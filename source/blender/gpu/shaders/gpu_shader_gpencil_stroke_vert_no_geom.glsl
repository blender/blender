
#pragma USE_SSBO_VERTEX_FETCH(TriangleList, 27)

#define GP_XRAY_FRONT 0
#define GP_XRAY_3DSPACE 1
#define GP_XRAY_BACK 2

#define GPENCIL_FLATCAP 1

#define DISCARD_VERTEX \
  gl_Position = vec4(0.0); \
  return;

/* project 3d point to 2d on screen space */
vec2 toScreenSpace(vec4 in_vertex)
{
  return vec2(in_vertex.xy / in_vertex.w) * gpencil_stroke_data.viewport;
}

/* get zdepth value */
float getZdepth(vec4 point)
{
  if (gpencil_stroke_data.xraymode == GP_XRAY_FRONT) {
    return 0.0;
  }
  if (gpencil_stroke_data.xraymode == GP_XRAY_3DSPACE) {
    return (point.z / point.w);
  }
  if (gpencil_stroke_data.xraymode == GP_XRAY_BACK) {
    return 1.0;
  }

  /* in front by default */
  return 0.0;
}

/* check equality but with a small tolerance */
bool is_equal(vec4 p1, vec4 p2)
{
  float limit = 0.0001;
  float x = abs(p1.x - p2.x);
  float y = abs(p1.y - p2.y);
  float z = abs(p1.z - p2.z);

  if ((x < limit) && (y < limit) && (z < limit)) {
    return true;
  }

  return false;
}

/* Vertex emission. */

#define EMIT_VERTEX(vertex_selector, _v0, _v1, _v2) \
  { \
    switch (vertex_selector) { \
      case 0: { \
        _v0 \
      } break; \
      case 1: { \
        _v1 \
      } break; \
      case 2: { \
        _v2 \
      } break; \
    } \
  } \
  return;

#define EMIT_VERTEX_COND(vertex_selector, condition, _v0, _v1, _v2) \
  { \
    if (condition) { \
      switch (vertex_selector) { \
        case 0: { \
          _v0 \
        } break; \
        case 1: { \
          _v1 \
        } break; \
        case 2: { \
          _v2 \
        } break; \
      } \
    } \
    else { \
      DISCARD_VERTEX; \
    } \
  } \
  return;

/** All output vertex combinations. */
/* Excessively long mitre gap. */
#define V0_a \
  geometry_out.mTexCoord = vec2(0, 0); \
  geometry_out.mColor = finalColor[1]; \
  gl_Position = vec4( \
      (sp1 + finalThickness[2] * n0) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0);

#define V1_a \
  geometry_out.mTexCoord = vec2(0, 0); \
  geometry_out.mColor = finalColor[1]; \
  gl_Position = vec4( \
      (sp1 + finalThickness[2] * n1) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0);

#define V2_a \
  geometry_out.mTexCoord = vec2(0, 0.5); \
  geometry_out.mColor = finalColor[1]; \
  gl_Position = vec4(sp1 / gpencil_stroke_data.viewport, getZdepth(P1), 1.0);

#define V0_b \
  geometry_out.mTexCoord = vec2(0, 1); \
  geometry_out.mColor = finalColor[1]; \
  gl_Position = vec4( \
      (sp1 - finalThickness[2] * n1) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0);
#define V1_b \
  geometry_out.mTexCoord = vec2(0, 1); \
  geometry_out.mColor = finalColor[1]; \
  gl_Position = vec4( \
      (sp1 - finalThickness[2] * n0) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0);

#define V2_b \
  geometry_out.mTexCoord = vec2(0, 0.5); \
  geometry_out.mColor = finalColor[1]; \
  gl_Position = vec4(sp1 / gpencil_stroke_data.viewport, getZdepth(P1), 1.0);

/* -- start end cap. -- */
#define V3 \
  geometry_out.mTexCoord = vec2(1, 0.5); \
  geometry_out.mColor = vec4(finalColor[1].rgb, finalColor[1].a * -1.0); \
  vec2 svn1 = normalize(sp1 - sp2) * length_a * 4.0 * extend; \
  gl_Position = vec4((sp1 + svn1) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0);

/* V4. */
#define V4 \
  geometry_out.mTexCoord = vec2(0, 0); \
  geometry_out.mColor = vec4(finalColor[1].rgb, finalColor[1].a * -1.0); \
  gl_Position = vec4( \
      (sp1 - (length_a * 2.0) * miter_a) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0);

/* V5. */
#define V5 \
  geometry_out.mTexCoord = vec2(0, 1); \
  geometry_out.mColor = vec4(finalColor[1].rgb, finalColor[1].a * -1.0); \
  gl_Position = vec4( \
      (sp1 + (length_a * 2.0) * miter_a) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0);

/* -- Main triangle strip --*/
#define V6 \
  geometry_out.mTexCoord = vec2(0, 0); \
  geometry_out.mColor = finalColor[1]; \
  gl_Position = vec4( \
      (sp1 + length_a * miter_a) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0);

/* V7. */
#define V7 \
  geometry_out.mTexCoord = vec2(0, 1); \
  geometry_out.mColor = finalColor[1]; \
  gl_Position = vec4( \
      (sp1 - length_a * miter_a) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0);

/* V8. */
#define V8 \
  geometry_out.mTexCoord = vec2(0, 0); \
  geometry_out.mColor = finalColor[2]; \
  gl_Position = vec4( \
      (sp2 + length_b * miter_b) / gpencil_stroke_data.viewport, getZdepth(P2), 1.0);

/* V9. */
#define V9 \
  geometry_out.mTexCoord = vec2(0, 1); \
  geometry_out.mColor = finalColor[2]; \
  gl_Position = vec4( \
      (sp2 - length_b * miter_b) / gpencil_stroke_data.viewport, getZdepth(P2), 1.0);

/* End end-cap. */
/* V10. */
#define V10 \
  geometry_out.mTexCoord = vec2(0, 1); \
  geometry_out.mColor = vec4(finalColor[2].rgb, finalColor[2].a * -1.0); \
  gl_Position = vec4( \
      (sp2 + (length_b * 2.0) * miter_b) / gpencil_stroke_data.viewport, getZdepth(P2), 1.0);

/* V11. */
#define V11 \
  geometry_out.mTexCoord = vec2(0, 0); \
  geometry_out.mColor = vec4(finalColor[2].rgb, finalColor[2].a * -1.0); \
  gl_Position = vec4( \
      (sp2 - (length_b * 2.0) * miter_b) / gpencil_stroke_data.viewport, getZdepth(P2), 1.0);

/* V12. */
#define V12 \
  geometry_out.mTexCoord = vec2(1, 0.5); \
  geometry_out.mColor = vec4(finalColor[2].rgb, finalColor[2].a * -1.0); \
  vec2 svn2 = normalize(sp2 - sp1) * length_b * 4.0 * extend; \
  gl_Position = vec4((sp2 + svn2) / gpencil_stroke_data.viewport, getZdepth(P2), 1.0);

void main(void)
{
  /* Determine output geometry IDs. */
  uint input_prim_id = gl_VertexID / 27;
  uint output_vertex_id = gl_VertexID % 27;
  uint output_prim_triangle_id = output_vertex_id / 3;
  uint vertex_in_triangle = output_vertex_id % 3;

  /** Run Vertex shader for all input vertices (Lines adjacency). */
  vec4 finalPos[4];
  vec4 finalColor[4];
  float finalThickness[4];

  float defaultpixsize = gpencil_stroke_data.pixsize * (1000.0 / gpencil_stroke_data.pixfactor);

  for (int i = 0; i < 4; i++) {
    finalPos[i] = ModelViewProjectionMatrix *
                  vec4(vertex_fetch_attribute(input_prim_id + i, pos, vec3).xyz, 1.0);
    finalColor[i] = vertex_fetch_attribute(input_prim_id + i, color, vec4);
    float in_thickness = vertex_fetch_attribute(input_prim_id + i, thickness, float);

    if (gpencil_stroke_data.keep_size) {
      finalThickness[i] = in_thickness;
    }
    else {
      float size = (ProjectionMatrix[3][3] == 0.0) ?
                       (in_thickness / (gl_Position.z * defaultpixsize)) :
                       (in_thickness / defaultpixsize);
      finalThickness[i] = max(size * gpencil_stroke_data.objscale, 1.0);
    }
  }

  /** Perform Geometry shader alternative. */
  float MiterLimit = 0.75;

  /* receive 4 points */
  vec4 P0 = finalPos[0];
  vec4 P1 = finalPos[1];
  vec4 P2 = finalPos[2];
  vec4 P3 = finalPos[3];

  /* get the four vertices passed to the shader */
  vec2 sp0 = toScreenSpace(P0); /* start of previous segment */
  vec2 sp1 = toScreenSpace(P1); /* end of previous segment, start of current segment */
  vec2 sp2 = toScreenSpace(P2); /* end of current segment, start of next segment */
  vec2 sp3 = toScreenSpace(P3); /* end of next segment */

  /* culling outside viewport */
  vec2 area = gpencil_stroke_data.viewport * 4.0;
  if (sp1.x < -area.x || sp1.x > area.x) {
    DISCARD_VERTEX;
  }
  if (sp1.y < -area.y || sp1.y > area.y) {
    DISCARD_VERTEX;
  }
  if (sp2.x < -area.x || sp2.x > area.x) {
    DISCARD_VERTEX;
  }
  if (sp2.y < -area.y || sp2.y > area.y) {
    DISCARD_VERTEX;
  }

  /* determine the direction of each of the 3 segments (previous,
   * current, next) */
  vec2 v0 = normalize(sp1 - sp0);
  vec2 v1 = normalize(sp2 - sp1);
  vec2 v2 = normalize(sp3 - sp2);

  /* determine the normal of each of the 3 segments (previous,
   * current, next) */
  vec2 n0 = vec2(-v0.y, v0.x);
  vec2 n1 = vec2(-v1.y, v1.x);
  vec2 n2 = vec2(-v2.y, v2.x);

  /* determine miter lines by averaging the normals of the 2
   * segments */
  vec2 miter_a = normalize(n0 + n1); /* miter at start of current segment */
  vec2 miter_b = normalize(n1 + n2); /* miter at end of current segment */

  /* determine the length of the miter by projecting it onto normal
   * and then inverse it */
  float an1 = dot(miter_a, n1);
  float bn1 = dot(miter_b, n2);
  if (an1 == 0) {
    an1 = 1;
  }
  if (bn1 == 0) {
    bn1 = 1;
  }
  float length_a = finalThickness[1] / an1;
  float length_b = finalThickness[2] / bn1;
  if (length_a <= 0.0) {
    length_a = 0.01;
  }
  if (length_b <= 0.0) {
    length_b = 0.01;
  }

  /** Geometry output. */
  /* First triangle (T0). prevent excessively long miters at sharp
   * corners */
  if (output_prim_triangle_id == 0) {
    if (dot(v0, v1) < -MiterLimit) {
      if (dot(v0, n1) > 0) {
        EMIT_VERTEX(vertex_in_triangle, V0_a, V1_a, V2_a)
      }
      else {
        EMIT_VERTEX(vertex_in_triangle, V0_b, V1_b, V2_b)
      }
    }
    else {
      DISCARD_VERTEX
    }
  }

  if (dot(v1, v2) < -MiterLimit) {
    miter_b = n1;
    length_b = finalThickness[2];
  }

  float extend = gpencil_stroke_data.fill_stroke ? 2 : 1;
  bool start_endcap = ((gpencil_stroke_data.caps_start != GPENCIL_FLATCAP) && is_equal(P0, P2));
  bool end_endcap = (gpencil_stroke_data.caps_end != GPENCIL_FLATCAP) && is_equal(P1, P3);

  switch (output_prim_triangle_id) {
    /* -- Start end cap. -*/
    case 1:
      EMIT_VERTEX_COND(vertex_in_triangle, start_endcap, V3, V4, V5)
    case 2:
      EMIT_VERTEX_COND(vertex_in_triangle, start_endcap, V4, V5, V6)
    case 3:
      EMIT_VERTEX_COND(vertex_in_triangle, start_endcap, V5, V6, V7)
    /* -- Standard triangle strip. -- */
    case 4:
      EMIT_VERTEX(vertex_in_triangle, V6, V7, V8)
    case 5:
      EMIT_VERTEX(vertex_in_triangle, V7, V8, V9)
    /* -- End end cap. -- */
    case 6:
      EMIT_VERTEX_COND(vertex_in_triangle, end_endcap, V8, V9, V10)
    case 7:
      EMIT_VERTEX_COND(vertex_in_triangle, end_endcap, V9, V10, V11)
    case 8:
      EMIT_VERTEX_COND(vertex_in_triangle, end_endcap, V10, V11, V12)
    default:
      DISCARD_VERTEX
  }
}
