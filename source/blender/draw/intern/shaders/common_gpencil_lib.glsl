/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

#ifndef DRW_GPENCIL_INFO
#  error Missing additional info draw_gpencil
#endif

#ifdef GPU_FRAGMENT_SHADER
float gpencil_stroke_round_cap_mask(vec2 p1, vec2 p2, vec2 aspect, float thickness, float hardfac)
{
  /* We create our own uv space to avoid issues with triangulation and linear
   * interpolation artifacts. */
  vec2 line = p2.xy - p1.xy;
  vec2 pos = gl_FragCoord.xy - p1.xy;
  float line_len = length(line);
  float half_line_len = line_len * 0.5;
  /* Normalize */
  line = (line_len > 0.0) ? (line / line_len) : vec2(1.0, 0.0);
  /* Create a uv space that englobe the whole segment into a capsule. */
  vec2 uv_end;
  uv_end.x = max(abs(dot(line, pos) - half_line_len) - half_line_len, 0.0);
  uv_end.y = dot(vec2(-line.y, line.x), pos);
  /* Divide by stroke radius. */
  uv_end /= thickness;
  uv_end *= aspect;

  float dist = clamp(1.0 - length(uv_end) * 2.0, 0.0, 1.0);
  if (hardfac > 0.999) {
    return step(1e-8, dist);
  }
  else {
    /* Modulate the falloff profile */
    float hardness = 1.0 - hardfac;
    dist = pow(dist, mix(0.01, 10.0, hardness));
    return smoothstep(0.0, 1.0, dist);
  }
}
#endif

vec2 gpencil_decode_aspect(int packed_data)
{
  float asp = float(uint(packed_data) & 0x1FFu) * (1.0 / 255.0);
  return (asp > 1.0) ? vec2(1.0, (asp - 1.0)) : vec2(asp, 1.0);
}

float gpencil_decode_uvrot(int packed_data)
{
  uint udata = uint(packed_data);
  float uvrot = 1e-8 + float((udata & 0x1FE00u) >> 9u) * (1.0 / 255.0);
  return ((udata & 0x20000u) != 0u) ? -uvrot : uvrot;
}

float gpencil_decode_hardness(int packed_data)
{
  return float((uint(packed_data) & 0x3FC0000u) >> 18u) * (1.0 / 255.0);
}

vec2 gpencil_project_to_screenspace(vec4 v, vec4 viewport_size)
{
  return ((v.xy / v.w) * 0.5 + 0.5) * viewport_size.xy;
}

float gpencil_stroke_thickness_modulate(float thickness, vec4 ndc_pos, vec4 viewport_size)
{
  /* Modify stroke thickness by object and layer factors. */
  thickness = max(1.0, thickness * gpThicknessScale + gpThicknessOffset);

  if (gpThicknessIsScreenSpace) {
    /* Multiply offset by view Z so that offset is constant in screenspace.
     * (e.i: does not change with the distance to camera) */
    thickness *= ndc_pos.w;
  }
  else {
    /* World space point size. */
    thickness *= gpThicknessWorldScale * ProjectionMatrix[1][1] * viewport_size.y;
  }
  return thickness;
}

float gpencil_clamp_small_stroke_thickness(float thickness, vec4 ndc_pos)
{
  /* To avoid aliasing artifacts, we clamp the line thickness and
   * reduce its opacity in the fragment shader. */
  float min_thickness = ndc_pos.w * 1.3;
  thickness = max(min_thickness, thickness);

  return thickness;
}

#ifdef GPU_VERTEX_SHADER

int gpencil_stroke_point_id()
{
  return (gl_VertexID & ~GP_IS_STROKE_VERTEX_BIT) >> GP_VERTEX_ID_SHIFT;
}

bool gpencil_is_stroke_vertex()
{
  return flag_test(gl_VertexID, GP_IS_STROKE_VERTEX_BIT);
}

/**
 * Returns value of gl_Position.
 *
 * To declare in vertex shader.
 * in ivec4 ma, ma1, ma2, ma3;
 * in vec4 pos, pos1, pos2, pos3, uv1, uv2, col1, col2, fcol1;
 *
 * All of these attributes are quad loaded the same way
 * as GL_LINES_ADJACENCY would feed a geometry shader:
 * - ma reference the previous adjacency point.
 * - ma1 reference the current line first point.
 * - ma2 reference the current line second point.
 * - ma3 reference the next adjacency point.
 * Note that we are rendering quad instances and not using any index buffer
 *(except for fills).
 *
 * Material : x is material index, y is stroke_id, z is point_id,
 *            w is aspect & rotation & hardness packed.
 * Position : contains thickness in 4th component.
 * UV : xy is UV for fills, z is U of stroke, w is strength.
 *
 *
 * WARNING: Max attribute count is actually 14 because OSX OpenGL implementation
 * considers gl_VertexID and gl_InstanceID as vertex attribute. (see #74536)
 */
vec4 gpencil_vertex(vec4 viewport_size,
                    gpMaterialFlag material_flags,
                    vec2 alignment_rot,
                    /* World Position. */
                    out vec3 out_P,
                    /* World Normal. */
                    out vec3 out_N,
                    /* Vertex Color. */
                    out vec4 out_color,
                    /* Stroke Strength. */
                    out float out_strength,
                    /* UV coordinates. */
                    out vec2 out_uv,
                    /* Screen-Space segment endpoints. */
                    out vec4 out_sspos,
                    /* Stroke aspect ratio. */
                    out vec2 out_aspect,
                    /* Stroke thickness (x: clamped, y: unclamped). */
                    out vec2 out_thickness,
                    /* Stroke hardness. */
                    out float out_hardness)
{
  int stroke_point_id = (gl_VertexID & ~GP_IS_STROKE_VERTEX_BIT) >> GP_VERTEX_ID_SHIFT;

  /* Attribute Loading. */
  vec4 pos = texelFetch(gp_pos_tx, (stroke_point_id - 1) * 3 + 0);
  vec4 pos1 = texelFetch(gp_pos_tx, (stroke_point_id + 0) * 3 + 0);
  vec4 pos2 = texelFetch(gp_pos_tx, (stroke_point_id + 1) * 3 + 0);
  vec4 pos3 = texelFetch(gp_pos_tx, (stroke_point_id + 2) * 3 + 0);
  ivec4 ma = floatBitsToInt(texelFetch(gp_pos_tx, (stroke_point_id - 1) * 3 + 1));
  ivec4 ma1 = floatBitsToInt(texelFetch(gp_pos_tx, (stroke_point_id + 0) * 3 + 1));
  ivec4 ma2 = floatBitsToInt(texelFetch(gp_pos_tx, (stroke_point_id + 1) * 3 + 1));
  ivec4 ma3 = floatBitsToInt(texelFetch(gp_pos_tx, (stroke_point_id + 2) * 3 + 1));
  vec4 uv1 = texelFetch(gp_pos_tx, (stroke_point_id + 0) * 3 + 2);
  vec4 uv2 = texelFetch(gp_pos_tx, (stroke_point_id + 1) * 3 + 2);

  vec4 col1 = texelFetch(gp_col_tx, (stroke_point_id + 0) * 2 + 0);
  vec4 col2 = texelFetch(gp_col_tx, (stroke_point_id + 1) * 2 + 0);
  vec4 fcol1 = texelFetch(gp_col_tx, (stroke_point_id + 0) * 2 + 1);

#  define thickness1 pos1.w
#  define thickness2 pos2.w
#  define strength1 uv1.w
#  define strength2 uv2.w
/* Packed! need to be decoded. */
#  define hardness1 ma1.w
#  define hardness2 ma2.w
#  define uvrot1 ma1.w
#  define aspect1 ma1.w

  vec4 out_ndc;

  if (gpencil_is_stroke_vertex()) {
    bool is_dot = flag_test(material_flags, GP_STROKE_ALIGNMENT);
    bool is_squares = !flag_test(material_flags, GP_STROKE_DOTS);

    /* Special Case. Stroke with single vert are rendered as dots. Do not discard them. */
    if (!is_dot && ma.x == -1 && ma2.x == -1) {
      is_dot = true;
      is_squares = false;
    }

    /* Endpoints, we discard the vertices. */
    if (!is_dot && ma2.x == -1) {
      /* We set the vertex at the camera origin to generate 0 fragments. */
      out_ndc = vec4(0.0, 0.0, -3e36, 0.0);
      return out_ndc;
    }

    /* Avoid using a vertex attribute for quad positioning. */
    float x = float(gl_VertexID & 1) * 2.0 - 1.0; /* [-1..1] */
    float y = float(gl_VertexID & 2) - 1.0;       /* [-1..1] */

    bool use_curr = is_dot || (x == -1.0);

    vec3 wpos_adj = transform_point(ModelMatrix, (use_curr) ? pos.xyz : pos3.xyz);
    vec3 wpos1 = transform_point(ModelMatrix, pos1.xyz);
    vec3 wpos2 = transform_point(ModelMatrix, pos2.xyz);

    vec3 T;
    if (is_dot) {
      /* Shade as facing billboards. */
      T = ViewMatrixInverse[0].xyz;
    }
    else if (use_curr && ma.x != -1) {
      T = wpos1 - wpos_adj;
    }
    else {
      T = wpos2 - wpos1;
    }
    T = safe_normalize(T);

    vec3 B = cross(T, ViewMatrixInverse[2].xyz);
    out_N = normalize(cross(B, T));

    vec4 ndc_adj = point_world_to_ndc(wpos_adj);
    vec4 ndc1 = point_world_to_ndc(wpos1);
    vec4 ndc2 = point_world_to_ndc(wpos2);

    out_ndc = (use_curr) ? ndc1 : ndc2;
    out_P = (use_curr) ? wpos1 : wpos2;
    out_strength = abs((use_curr) ? strength1 : strength2);

    vec2 ss_adj = gpencil_project_to_screenspace(ndc_adj, viewport_size);
    vec2 ss1 = gpencil_project_to_screenspace(ndc1, viewport_size);
    vec2 ss2 = gpencil_project_to_screenspace(ndc2, viewport_size);
    /* Screenspace Lines tangents. */
    float line_len;
    vec2 line = safe_normalize_len(ss2 - ss1, line_len);
    vec2 line_adj = safe_normalize((use_curr) ? (ss1 - ss_adj) : (ss_adj - ss2));

    float thickness = abs((use_curr) ? thickness1 : thickness2);
    thickness = gpencil_stroke_thickness_modulate(thickness, out_ndc, viewport_size);
    float clamped_thickness = gpencil_clamp_small_stroke_thickness(thickness, out_ndc);

    out_uv = vec2(x, y) * 0.5 + 0.5;
    out_hardness = gpencil_decode_hardness(use_curr ? hardness1 : hardness2);

    if (is_dot) {
      uint alignement_mode = material_flags & GP_STROKE_ALIGNMENT;

      /* For one point strokes use object alignment. */
      if (alignement_mode == GP_STROKE_ALIGNMENT_STROKE && ma.x == -1 && ma2.x == -1) {
        alignement_mode = GP_STROKE_ALIGNMENT_OBJECT;
      }

      vec2 x_axis;
      if (alignement_mode == GP_STROKE_ALIGNMENT_STROKE) {
        x_axis = (ma2.x == -1) ? line_adj : line;
      }
      else if (alignement_mode == GP_STROKE_ALIGNMENT_FIXED) {
        /* Default for no-material drawing. */
        x_axis = vec2(1.0, 0.0);
      }
      else { /* GP_STROKE_ALIGNMENT_OBJECT */
        vec4 ndc_x = point_world_to_ndc(wpos1 + ModelMatrix[0].xyz);
        vec2 ss_x = gpencil_project_to_screenspace(ndc_x, viewport_size);
        x_axis = safe_normalize(ss_x - ss1);
      }

      /* Rotation: Encoded as Cos + Sin sign. */
      float uv_rot = gpencil_decode_uvrot(uvrot1);
      float rot_sin = sqrt(max(0.0, 1.0 - uv_rot * uv_rot)) * sign(uv_rot);
      float rot_cos = abs(uv_rot);
      /* TODO(@fclem): Optimize these 2 matrix mul into one by only having one rotation angle and
       * using a cosine approximation. */
      x_axis = mat2(rot_cos, -rot_sin, rot_sin, rot_cos) * x_axis;
      x_axis = mat2(alignment_rot.x, -alignment_rot.y, alignment_rot.y, alignment_rot.x) * x_axis;
      /* Rotate 90 degrees counter-clockwise. */
      vec2 y_axis = vec2(-x_axis.y, x_axis.x);

      out_aspect = gpencil_decode_aspect(aspect1);

      x *= out_aspect.x;
      y *= out_aspect.y;

      /* Invert for vertex shader. */
      out_aspect = 1.0 / out_aspect;

      out_ndc.xy += (x * x_axis + y * y_axis) * viewport_size.zw * clamped_thickness;

      out_sspos.xy = ss1;
      out_sspos.zw = ss1 + x_axis * 0.5;
      out_thickness.x = (is_squares) ? 1e18 : (clamped_thickness / out_ndc.w);
      out_thickness.y = (is_squares) ? 1e18 : (thickness / out_ndc.w);
    }
    else {
      bool is_stroke_start = (ma.x == -1 && x == -1);
      bool is_stroke_end = (ma3.x == -1 && x == 1);

      /* Mitter tangent vector. */
      vec2 miter_tan = safe_normalize(line_adj + line);
      float miter_dot = dot(miter_tan, line_adj);
      /* Break corners after a certain angle to avoid really thick corners. */
      const float miter_limit = 0.5; /* cos(60 degrees) */
      bool miter_break = (miter_dot < miter_limit);
      miter_tan = (miter_break || is_stroke_start || is_stroke_end) ? line :
                                                                      (miter_tan / miter_dot);
      /* Rotate 90 degrees counter-clockwise. */
      vec2 miter = vec2(-miter_tan.y, miter_tan.x);

      out_sspos.xy = ss1;
      out_sspos.zw = ss2;
      out_thickness.x = clamped_thickness / out_ndc.w;
      out_thickness.y = thickness / out_ndc.w;
      out_aspect = vec2(1.0);

      vec2 screen_ofs = miter * y;

      /* Reminder: we packed the cap flag into the sign of strength and thickness sign. */
      if ((is_stroke_start && strength1 > 0.0) || (is_stroke_end && thickness1 > 0.0) ||
          (miter_break && !is_stroke_start && !is_stroke_end))
      {
        screen_ofs += line * x;
      }

      out_ndc.xy += screen_ofs * viewport_size.zw * clamped_thickness;

      out_uv.x = (use_curr) ? uv1.z : uv2.z;
    }

    out_color = (use_curr) ? col1 : col2;
  }
  else {
    out_P = transform_point(ModelMatrix, pos1.xyz);
    out_ndc = point_world_to_ndc(out_P);
    out_uv = uv1.xy;
    out_thickness.x = 1e18;
    out_thickness.y = 1e20;
    out_hardness = 1.0;
    out_aspect = vec2(1.0);
    out_sspos = vec4(0.0);

    /* Flat normal following camera and object bounds. */
    vec3 V = cameraVec(ModelMatrix[3].xyz);
    vec3 N = normal_world_to_object(V);
    N *= OrcoTexCoFactors[1].xyz;
    N = normal_object_to_world(N);
    out_N = safe_normalize(N);

    /* Decode fill opacity. */
    out_color = vec4(fcol1.rgb, floor(fcol1.a / 10.0) / 10000.0);

    /* We still offset the fills a little to avoid overlaps */
    out_ndc.z += 0.000002;
  }

#  undef thickness1
#  undef thickness2
#  undef strength1
#  undef strength2
#  undef hardness1
#  undef hardness2
#  undef uvrot1
#  undef aspect1

  return out_ndc;
}

vec4 gpencil_vertex(vec4 viewport_size,
                    out vec3 out_P,
                    out vec3 out_N,
                    out vec4 out_color,
                    out float out_strength,
                    out vec2 out_uv,
                    out vec4 out_sspos,
                    out vec2 out_aspect,
                    out vec2 out_thickness,
                    out float out_hardness)
{
  return gpencil_vertex(viewport_size,
                        0u,
                        vec2(1.0, 0.0),
                        out_P,
                        out_N,
                        out_color,
                        out_strength,
                        out_uv,
                        out_sspos,
                        out_aspect,
                        out_thickness,
                        out_hardness);
}

#endif
