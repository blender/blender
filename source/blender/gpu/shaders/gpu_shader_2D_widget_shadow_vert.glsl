/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_widget_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_2D_widget_shadow)

#define BIT_RANGE(x) uint((1 << x) - 1)

/* 2 bits for corner */
/* Attention! Not the same order as in UI_interface.hh!
 * Ordered by drawing order. */
#define BOTTOM_LEFT 0u
#define BOTTOM_RIGHT 1u
#define TOP_RIGHT 2u
#define TOP_LEFT 3u
#define CNR_FLAG_RANGE BIT_RANGE(2)

/* 4bits for corner id */
#define CORNER_VEC_OFS 2u
#define CORNER_VEC_RANGE BIT_RANGE(4)

#define INNER_FLAG uint(1 << 10) /* is inner vert */

/* Radii and rad per corner. */
#define recti parameters[0]
#define rect parameters[1]
#define radsi parameters[2].x
#define rads parameters[2].y
#define roundCorners parameters[3]

void main()
{
  /* NOTE(Metal): Declaring constant array in function scope to avoid increasing local shader
   * memory pressure. */
  constexpr float2 cornervec[36] = float2_array(float2(0.0f, 1.0f),
                                                float2(0.02f, 0.805f),
                                                float2(0.067f, 0.617f),
                                                float2(0.169f, 0.45f),
                                                float2(0.293f, 0.293f),
                                                float2(0.45f, 0.169f),
                                                float2(0.617f, 0.076f),
                                                float2(0.805f, 0.02f),
                                                float2(1.0f, 0.0f),
                                                float2(-1.0f, 0.0f),
                                                float2(-0.805f, 0.02f),
                                                float2(-0.617f, 0.067f),
                                                float2(-0.45f, 0.169f),
                                                float2(-0.293f, 0.293f),
                                                float2(-0.169f, 0.45f),
                                                float2(-0.076f, 0.617f),
                                                float2(-0.02f, 0.805f),
                                                float2(0.0f, 1.0f),
                                                float2(0.0f, -1.0f),
                                                float2(-0.02f, -0.805f),
                                                float2(-0.067f, -0.617f),
                                                float2(-0.169f, -0.45f),
                                                float2(-0.293f, -0.293f),
                                                float2(-0.45f, -0.169f),
                                                float2(-0.617f, -0.076f),
                                                float2(-0.805f, -0.02f),
                                                float2(-1.0f, 0.0f),
                                                float2(1.0f, 0.0f),
                                                float2(0.805f, -0.02f),
                                                float2(0.617f, -0.067f),
                                                float2(0.45f, -0.169f),
                                                float2(0.293f, -0.293f),
                                                float2(0.169f, -0.45f),
                                                float2(0.076f, -0.617f),
                                                float2(0.02f, -0.805f),
                                                float2(0.0f, -1.0f));

  constexpr float2 center_offset[4] = float2_array(
      float2(1.0f, 1.0f), float2(-1.0f, 1.0f), float2(-1.0f, -1.0f), float2(1.0f, -1.0f));

  uint cflag = vflag & CNR_FLAG_RANGE;
  uint vofs = (vflag >> CORNER_VEC_OFS) & CORNER_VEC_RANGE;

  bool is_inner = (vflag & INNER_FLAG) != 0u;

  float shadow_width = rads - radsi;
  float shadow_width_top = rect.w - recti.w;

  float rad_inner = radsi * roundCorners[cflag];
  float rad_outer = rad_inner + shadow_width;
  float radius = (is_inner) ? rad_inner : rad_outer;

  float shadow_offset = (is_inner && (cflag > BOTTOM_RIGHT)) ? (shadow_width - shadow_width_top) :
                                                               0.0f;

  float2 c = center_offset[cflag];
  float2 center_outer = rad_outer * c;
  float2 center = radius * c;

  /* First expand all vertices to the outer shadow border. */
  float2 v = rad_outer * cornervec[cflag * 9u + vofs];

  /* Now shrink the inner vertices onto the inner rectangle.
   * At the top corners we keep the vertical offset to distribute a few of the vertices along the
   * straight part of the rectangle. This allows us to get a better falloff at the top. */
  if (is_inner && (cflag > BOTTOM_RIGHT) && (v.y < (shadow_offset - rad_outer))) {
    v.y += shadow_width_top;
    v.x = 0.0f;
  }
  else {
    v = radius * normalize(v - (center_outer + float2(0.0f, shadow_offset))) + center;
  }

  /* Position to corner */
  float4 rct = (is_inner) ? recti : rect;
  if (cflag == BOTTOM_LEFT) {
    v += rct.xz;
  }
  else if (cflag == BOTTOM_RIGHT) {
    v += rct.yz;
  }
  else if (cflag == TOP_RIGHT) {
    v += rct.yw;
  }
  else /* (cflag == TOP_LEFT) */ {
    v += rct.xw;
  }

  float inner_shadow_strength = min((rect.w - v.y) / rad_outer + 0.1f, 1.0f);
  shadowFalloff = (is_inner) ? inner_shadow_strength : 0.0f;
  innerMask = (is_inner) ? 0.0f : 1.0f;

  gl_Position = ModelViewProjectionMatrix * float4(v, 0.0f, 1.0f);
}
