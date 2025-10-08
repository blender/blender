/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU immediate mode drawing utilities
 */

#include <cstring>

#include "DNA_userdef_types.h"

#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "GPU_immediate.hh"

#include "UI_resources.hh"

static const float cube_coords[8][3] = {
    {-1, -1, -1},
    {-1, -1, +1},
    {-1, +1, -1},
    {-1, +1, +1},
    {+1, -1, -1},
    {+1, -1, +1},
    {+1, +1, -1},
    {+1, +1, +1},
};
static const int cube_quad_index[6][4] = {
    {0, 1, 3, 2},
    {0, 2, 6, 4},
    {0, 4, 5, 1},
    {1, 5, 7, 3},
    {2, 3, 7, 6},
    {4, 6, 7, 5},
};
static const int cube_line_index[12][2] = {
    {0, 1},
    {0, 2},
    {0, 4},
    {1, 3},
    {1, 5},
    {2, 3},
    {2, 6},
    {3, 7},
    {4, 5},
    {4, 6},
    {5, 7},
    {6, 7},
};

void immRectf(uint pos, float x1, float y1, float x2, float y2)
{
  immBegin(GPU_PRIM_TRI_FAN, 4);
  immVertex2f(pos, x1, y1);
  immVertex2f(pos, x2, y1);
  immVertex2f(pos, x2, y2);
  immVertex2f(pos, x1, y2);
  immEnd();
}

void immRecti(uint pos, int x1, int y1, int x2, int y2)
{
  immBegin(GPU_PRIM_TRI_FAN, 4);
  immVertex2i(pos, x1, y1);
  immVertex2i(pos, x2, y1);
  immVertex2i(pos, x2, y2);
  immVertex2i(pos, x1, y2);
  immEnd();
}

void immRectf_fast(uint pos, float x1, float y1, float x2, float y2)
{
  immVertex2f(pos, x1, y1);
  immVertex2f(pos, x2, y1);
  immVertex2f(pos, x2, y2);

  immVertex2f(pos, x1, y1);
  immVertex2f(pos, x2, y2);
  immVertex2f(pos, x1, y2);
}

void immRectf_fast_with_color(
    uint pos, uint col, float x1, float y1, float x2, float y2, const float color[4])
{
  immAttr4fv(col, color);
  immVertex2f(pos, x1, y1);
  immAttr4fv(col, color);
  immVertex2f(pos, x2, y1);
  immAttr4fv(col, color);
  immVertex2f(pos, x2, y2);

  immAttr4fv(col, color);
  immVertex2f(pos, x1, y1);
  immAttr4fv(col, color);
  immVertex2f(pos, x2, y2);
  immAttr4fv(col, color);
  immVertex2f(pos, x1, y2);
}

void immRecti_fast_with_color(
    uint pos, uint col, int x1, int y1, int x2, int y2, const float color[4])
{
  immAttr4fv(col, color);
  immVertex2i(pos, x1, y1);
  immAttr4fv(col, color);
  immVertex2i(pos, x2, y1);
  immAttr4fv(col, color);
  immVertex2i(pos, x2, y2);

  immAttr4fv(col, color);
  immVertex2i(pos, x1, y1);
  immAttr4fv(col, color);
  immVertex2i(pos, x2, y2);
  immAttr4fv(col, color);
  immVertex2i(pos, x1, y2);
}

void immRectf_with_texco(const uint pos, const uint tex_coord, const rctf &p, const rctf &uv)
{
  immBegin(GPU_PRIM_TRI_FAN, 4);
  immAttr2f(tex_coord, uv.xmin, uv.ymin);
  immVertex2f(pos, p.xmin, p.ymin);

  immAttr2f(tex_coord, uv.xmin, uv.ymax);
  immVertex2f(pos, p.xmin, p.ymax);

  immAttr2f(tex_coord, uv.xmax, uv.ymax);
  immVertex2f(pos, p.xmax, p.ymax);

  immAttr2f(tex_coord, uv.xmax, uv.ymin);
  immVertex2f(pos, p.xmax, p.ymin);
  immEnd();
}

#if 0 /* more complete version in case we want that */
void immRecti_complete(int x1, int y1, int x2, int y2, const float color[4])
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = add_attr(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4fv(color);
  immRectf(pos, x1, y1, x2, y2);
  immUnbindProgram();
}
#endif

void imm_cpack(uint x)
{
  immUniformColor3ub(((x) & 0xFF), (((x) >> 8) & 0xFF), (((x) >> 16) & 0xFF));
}

static void imm_draw_circle(GPUPrimType prim_type,
                            const uint shdr_pos,
                            float x,
                            float y,
                            float radius_x,
                            float radius_y,
                            int nsegments)
{
  if (prim_type == GPU_PRIM_LINE_LOOP) {
    /* NOTE(Metal/AMD): For small primitives, line list more efficient than line strip.. */
    immBegin(GPU_PRIM_LINES, nsegments * 2);

    immVertex2f(shdr_pos, x + (radius_x * cosf(0.0f)), y + (radius_y * sinf(0.0f)));
    for (int i = 1; i < nsegments; i++) {
      const float angle = float(2 * M_PI) * (float(i) / float(nsegments));
      immVertex2f(shdr_pos, x + (radius_x * cosf(angle)), y + (radius_y * sinf(angle)));
      immVertex2f(shdr_pos, x + (radius_x * cosf(angle)), y + (radius_y * sinf(angle)));
    }
    immVertex2f(shdr_pos, x + (radius_x * cosf(0.0f)), y + (radius_y * sinf(0.0f)));
    immEnd();
  }
  else {
    immBegin(prim_type, nsegments);
    for (int i = 0; i < nsegments; i++) {
      const float angle = float(2 * M_PI) * (float(i) / float(nsegments));
      immVertex2f(shdr_pos, x + (radius_x * cosf(angle)), y + (radius_y * sinf(angle)));
    }
    immEnd();
  }
}

void imm_draw_circle_wire_2d(uint shdr_pos, float x, float y, float radius, int nsegments)
{
  imm_draw_circle(GPU_PRIM_LINE_LOOP, shdr_pos, x, y, radius, radius, nsegments);
}

void imm_draw_circle_fill_2d(uint shdr_pos, float x, float y, float radius, int nsegments)
{
  imm_draw_circle(GPU_PRIM_TRI_FAN, shdr_pos, x, y, radius, radius, nsegments);
}

void imm_draw_circle_wire_aspect_2d(
    uint shdr_pos, float x, float y, float radius_x, float radius_y, int nsegments)
{
  imm_draw_circle(GPU_PRIM_LINE_LOOP, shdr_pos, x, y, radius_x, radius_y, nsegments);
}

void imm_draw_circle_fill_aspect_2d(
    uint shdr_pos, float x, float y, float radius_x, float radius_y, int nsegments)
{
  imm_draw_circle(GPU_PRIM_TRI_FAN, shdr_pos, x, y, radius_x, radius_y, nsegments);
}

static void imm_draw_circle_partial(GPUPrimType prim_type,
                                    uint pos,
                                    float x,
                                    float y,
                                    float radius,
                                    int nsegments,
                                    float start,
                                    float sweep)
{
  /* shift & reverse angle, increase 'nsegments' to match gluPartialDisk */
  const float angle_start = -DEG2RADF(start) + float(M_PI_2);
  const float angle_end = -(DEG2RADF(sweep) - angle_start);
  nsegments += 1;
  immBegin(prim_type, nsegments);
  for (int i = 0; i < nsegments; i++) {
    const float angle = interpf(angle_start, angle_end, (float(i) / float(nsegments - 1)));
    const float angle_sin = sinf(angle);
    const float angle_cos = cosf(angle);
    immVertex2f(pos, x + radius * angle_cos, y + radius * angle_sin);
  }
  immEnd();
}

static void imm_draw_circle_partial_3d(GPUPrimType prim_type,
                                       uint pos,
                                       float x,
                                       float y,
                                       float z,
                                       float rad,
                                       int nsegments,
                                       float start,
                                       float sweep)
{
  /* shift & reverse angle, increase 'nsegments' to match gluPartialDisk */
  const float angle_start = -DEG2RADF(start) + float(M_PI / 2);
  const float angle_end = -(DEG2RADF(sweep) - angle_start);
  nsegments += 1;
  immBegin(prim_type, nsegments);
  for (int i = 0; i < nsegments; i++) {
    const float angle = interpf(angle_start, angle_end, (float(i) / float(nsegments - 1)));
    const float angle_sin = sinf(angle);
    const float angle_cos = cosf(angle);
    immVertex3f(pos, x + rad * angle_cos, y + rad * angle_sin, z);
  }
  immEnd();
}

void imm_draw_circle_partial_wire_2d(
    uint pos, float x, float y, float radius, int nsegments, float start, float sweep)
{
  imm_draw_circle_partial(GPU_PRIM_LINE_STRIP, pos, x, y, radius, nsegments, start, sweep);
}

void imm_draw_circle_partial_wire_3d(
    uint pos, float x, float y, float z, float radius, int nsegments, float start, float sweep)
{
  imm_draw_circle_partial_3d(GPU_PRIM_LINE_STRIP, pos, x, y, z, radius, nsegments, start, sweep);
}

static void imm_draw_disk_partial(GPUPrimType prim_type,
                                  uint pos,
                                  float x,
                                  float y,
                                  float rad_inner,
                                  float rad_outer,
                                  int nsegments,
                                  float start,
                                  float sweep)
{
  /* to avoid artifacts */
  const float max_angle = 3 * 360;
  CLAMP(sweep, -max_angle, max_angle);

  /* shift & reverse angle, increase 'nsegments' to match gluPartialDisk */
  const float angle_start = -DEG2RADF(start) + float(M_PI_2);
  const float angle_end = -(DEG2RADF(sweep) - angle_start);
  nsegments += 1;
  immBegin(prim_type, nsegments * 2);
  for (int i = 0; i < nsegments; i++) {
    const float angle = interpf(angle_start, angle_end, (float(i) / float(nsegments - 1)));
    const float angle_sin = sinf(angle);
    const float angle_cos = cosf(angle);
    immVertex2f(pos, x + rad_inner * angle_cos, y + rad_inner * angle_sin);
    immVertex2f(pos, x + rad_outer * angle_cos, y + rad_outer * angle_sin);
  }
  immEnd();
}

static void imm_draw_disk_partial_3d(GPUPrimType prim_type,
                                     uint pos,
                                     float x,
                                     float y,
                                     float z,
                                     float rad_inner,
                                     float rad_outer,
                                     int nsegments,
                                     float start,
                                     float sweep)
{
  /* to avoid artifacts */
  const float max_angle = 3 * 360;
  CLAMP(sweep, -max_angle, max_angle);

  /* shift & reverse angle, increase 'nsegments' to match gluPartialDisk */
  const float angle_start = -DEG2RADF(start) + float(M_PI_2);
  const float angle_end = -(DEG2RADF(sweep) - angle_start);
  nsegments += 1;
  immBegin(prim_type, nsegments * 2);
  for (int i = 0; i < nsegments; i++) {
    const float angle = interpf(angle_start, angle_end, (float(i) / float(nsegments - 1)));
    const float angle_sin = sinf(angle);
    const float angle_cos = cosf(angle);
    immVertex3f(pos, x + rad_inner * angle_cos, y + rad_inner * angle_sin, z);
    immVertex3f(pos, x + rad_outer * angle_cos, y + rad_outer * angle_sin, z);
  }
  immEnd();
}

void imm_draw_disk_partial_fill_2d(uint pos,
                                   float x,
                                   float y,
                                   float rad_inner,
                                   float rad_outer,
                                   int nsegments,
                                   float start,
                                   float sweep)
{
  imm_draw_disk_partial(
      GPU_PRIM_TRI_STRIP, pos, x, y, rad_inner, rad_outer, nsegments, start, sweep);
}
void imm_draw_disk_partial_fill_3d(uint pos,
                                   float x,
                                   float y,
                                   float z,
                                   float rad_inner,
                                   float rad_outer,
                                   int nsegments,
                                   float start,
                                   float sweep)
{
  imm_draw_disk_partial_3d(
      GPU_PRIM_TRI_STRIP, pos, x, y, z, rad_inner, rad_outer, nsegments, start, sweep);
}

static void imm_draw_circle_3D(GPUPrimType prim_type,
                               uint pos,
                               float x,
                               float y,
                               float radius_x,
                               float radius_y,
                               int nsegments)
{
  if (prim_type == GPU_PRIM_LINE_LOOP) {
    /* NOTE(Metal/AMD): For small primitives, line list more efficient than line strip. */
    immBegin(GPU_PRIM_LINES, nsegments * 2);

    const float angle = float(2 * M_PI) / float(nsegments);
    float xprev = cosf(-angle) * radius_x;
    float yprev = sinf(-angle) * radius_y;
    const float alpha = 2.0f * cosf(angle);

    float xr = radius_x;
    float yr = 0;

    for (int i = 0; i < nsegments; i++) {
      immVertex3f(pos, x + xr, y + yr, 0.0f);
      if (i) {
        immVertex3f(pos, x + xr, y + yr, 0.0f);
      }
      /* `cos[(n + 1)a] = 2cos(a)cos(na) - cos[(n - 1)a]`. */
      const float xnext = alpha * xr - xprev;
      /* `sin[(n + 1)a] = 2cos(a)sin(na) - sin[(n - 1)a]`. */
      const float ynext = alpha * yr - yprev;
      xprev = xr;
      yprev = yr;
      xr = xnext;
      yr = ynext;
    }
    immVertex3f(pos, x + radius_x, y, 0.0f);
    immEnd();
  }
  else {
    immBegin(prim_type, nsegments);
    for (int i = 0; i < nsegments; i++) {
      float angle = float(2 * M_PI) * (float(i) / float(nsegments));
      immVertex3f(pos, x + radius_x * cosf(angle), y + radius_y * sinf(angle), 0.0f);
    }
    immEnd();
  }
}

void imm_draw_circle_wire_3d(uint pos, float x, float y, float radius, int nsegments)
{
  imm_draw_circle_3D(GPU_PRIM_LINE_LOOP, pos, x, y, radius, radius, nsegments);
}

void imm_draw_circle_wire_aspect_3d(
    uint pos, float x, float y, float radius_x, float radius_y, int nsegments)
{
  imm_draw_circle_3D(GPU_PRIM_LINE_LOOP, pos, x, y, radius_x, radius_y, nsegments);
}

void imm_draw_circle_dashed_3d(uint pos, float x, float y, float radius, int nsegments)
{
  imm_draw_circle_3D(GPU_PRIM_LINES, pos, x, y, radius, radius, nsegments / 2);
}

void imm_draw_circle_fill_3d(uint pos, float x, float y, float radius, int nsegments)
{
  imm_draw_circle_3D(GPU_PRIM_TRI_FAN, pos, x, y, radius, radius, nsegments);
}

void imm_draw_circle_fill_aspect_3d(
    uint pos, float x, float y, float radius_x, float radius_y, int nsegments)
{
  imm_draw_circle_3D(GPU_PRIM_TRI_FAN, pos, x, y, radius_x, radius_y, nsegments);
}

void imm_draw_box_wire_2d(uint pos, float x1, float y1, float x2, float y2)
{
  /* NOTE(Metal/AMD): For small primitives, line list more efficient than line-strip. */
  immBegin(GPU_PRIM_LINES, 8);
  immVertex2f(pos, x1, y1);
  immVertex2f(pos, x1, y2);

  immVertex2f(pos, x1, y2);
  immVertex2f(pos, x2, y2);

  immVertex2f(pos, x2, y2);
  immVertex2f(pos, x2, y1);

  immVertex2f(pos, x2, y1);
  immVertex2f(pos, x1, y1);
  immEnd();
}

void imm_draw_box_wire_3d(uint pos, float x1, float y1, float x2, float y2)
{
  /* use this version when GPUVertFormat has a vec3 position */
  /* NOTE(Metal/AMD): For small primitives, line list more efficient than line-strip. */
  immBegin(GPU_PRIM_LINES, 8);
  immVertex3f(pos, x1, y1, 0.0f);
  immVertex3f(pos, x1, y2, 0.0f);

  immVertex3f(pos, x1, y2, 0.0f);
  immVertex3f(pos, x2, y2, 0.0f);

  immVertex3f(pos, x2, y2, 0.0f);
  immVertex3f(pos, x2, y1, 0.0f);

  immVertex3f(pos, x2, y1, 0.0f);
  immVertex3f(pos, x1, y1, 0.0f);
  immEnd();
}

void imm_draw_box_checker_2d_ex(float x1,
                                float y1,
                                float x2,
                                float y2,
                                const float color_primary[4],
                                const float color_secondary[4],
                                int checker_size)
{
  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_2D_CHECKER);

  immUniform4fv("color1", color_primary);
  immUniform4fv("color2", color_secondary);
  immUniform1i("size", checker_size);

  immRectf(pos, x1, y1, x2, y2);

  immUnbindProgram();
}
void imm_draw_box_checker_2d(float x1, float y1, float x2, float y2, bool clear_alpha)
{
  float checker_primary[4];
  float checker_secondary[4];
  UI_GetThemeColor4fv(TH_TRANSPARENT_CHECKER_PRIMARY, checker_primary);
  UI_GetThemeColor4fv(TH_TRANSPARENT_CHECKER_SECONDARY, checker_secondary);
  checker_primary[3] = clear_alpha ? 0.0 : checker_primary[3];
  checker_secondary[3] = clear_alpha ? 0.0 : checker_secondary[3];
  int checker_size = UI_GetThemeValue(TH_TRANSPARENT_CHECKER_SIZE) * U.pixelsize;
  imm_draw_box_checker_2d_ex(x1, y1, x2, y2, checker_primary, checker_secondary, checker_size);
}

void imm_draw_cube_fill_3d(uint pos, const float center[3], const float aspect[3])
{
  float coords[ARRAY_SIZE(cube_coords)][3];

  for (int i = 0; i < ARRAY_SIZE(cube_coords); i++) {
    madd_v3_v3v3v3(coords[i], center, cube_coords[i], aspect);
  }

  immBegin(GPU_PRIM_TRIS, ARRAY_SIZE(cube_quad_index) * 3 * 2);
  for (int i = 0; i < ARRAY_SIZE(cube_quad_index); i++) {
    immVertex3fv(pos, coords[cube_quad_index[i][0]]);
    immVertex3fv(pos, coords[cube_quad_index[i][1]]);
    immVertex3fv(pos, coords[cube_quad_index[i][2]]);

    immVertex3fv(pos, coords[cube_quad_index[i][0]]);
    immVertex3fv(pos, coords[cube_quad_index[i][2]]);
    immVertex3fv(pos, coords[cube_quad_index[i][3]]);
  }
  immEnd();
}

void imm_draw_cube_wire_3d(uint pos, const float center[3], const float aspect[3])
{
  float coords[ARRAY_SIZE(cube_coords)][3];

  for (int i = 0; i < ARRAY_SIZE(cube_coords); i++) {
    madd_v3_v3v3v3(coords[i], center, cube_coords[i], aspect);
  }

  immBegin(GPU_PRIM_LINES, ARRAY_SIZE(cube_line_index) * 2);
  for (int i = 0; i < ARRAY_SIZE(cube_line_index); i++) {
    immVertex3fv(pos, coords[cube_line_index[i][0]]);
    immVertex3fv(pos, coords[cube_line_index[i][1]]);
  }
  immEnd();
}

void imm_draw_cube_corners_3d(uint pos,
                              const float center[3],
                              const float aspect[3],
                              const float factor)
{
  float coords[ARRAY_SIZE(cube_coords)][3];

  for (int i = 0; i < ARRAY_SIZE(cube_coords); i++) {
    madd_v3_v3v3v3(coords[i], center, cube_coords[i], aspect);
  }

  immBegin(GPU_PRIM_LINES, ARRAY_SIZE(cube_line_index) * 4);
  for (int i = 0; i < ARRAY_SIZE(cube_line_index); i++) {
    float vec[3], co[3];
    sub_v3_v3v3(vec, coords[cube_line_index[i][1]], coords[cube_line_index[i][0]]);
    mul_v3_fl(vec, factor);

    copy_v3_v3(co, coords[cube_line_index[i][0]]);
    immVertex3fv(pos, co);
    add_v3_v3(co, vec);
    immVertex3fv(pos, co);

    copy_v3_v3(co, coords[cube_line_index[i][1]]);
    immVertex3fv(pos, co);
    sub_v3_v3(co, vec);
    immVertex3fv(pos, co);
  }
  immEnd();
}

void imm_draw_cylinder_fill_normal_3d(
    uint pos, uint nor, float base, float top, float height, int slices, int stacks)
{
  immBegin(GPU_PRIM_TRIS, 6 * slices * stacks);
  for (int i = 0; i < slices; i++) {
    const float angle1 = float(2 * M_PI) * (float(i) / float(slices));
    const float angle2 = float(2 * M_PI) * (float(i + 1) / float(slices));
    const float cos1 = cosf(angle1);
    const float sin1 = sinf(angle1);
    const float cos2 = cosf(angle2);
    const float sin2 = sinf(angle2);

    for (int j = 0; j < stacks; j++) {
      float fac1 = float(j) / float(stacks);
      float fac2 = float(j + 1) / float(stacks);
      float r1 = base * (1.0f - fac1) + top * fac1;
      float r2 = base * (1.0f - fac2) + top * fac2;
      float h1 = height * (float(j) / float(stacks));
      float h2 = height * (float(j + 1) / float(stacks));

      const float v1[3] = {r1 * cos2, r1 * sin2, h1};
      const float v2[3] = {r2 * cos2, r2 * sin2, h2};
      const float v3[3] = {r2 * cos1, r2 * sin1, h2};
      const float v4[3] = {r1 * cos1, r1 * sin1, h1};
      float n1[3], n2[3];

      /* calc normals */
      sub_v3_v3v3(n1, v2, v1);
      normalize_v3(n1);
      n1[0] = cos1;
      n1[1] = sin1;
      n1[2] = 1 - n1[2];

      sub_v3_v3v3(n2, v3, v4);
      normalize_v3(n2);
      n2[0] = cos2;
      n2[1] = sin2;
      n2[2] = 1 - n2[2];

      /* first tri */
      immAttr3fv(nor, n2);
      immVertex3fv(pos, v1);
      immVertex3fv(pos, v2);
      immAttr3fv(nor, n1);
      immVertex3fv(pos, v3);

      /* second tri */
      immVertex3fv(pos, v3);
      immVertex3fv(pos, v4);
      immAttr3fv(nor, n2);
      immVertex3fv(pos, v1);
    }
  }
  immEnd();
}

void imm_draw_cylinder_wire_3d(
    uint pos, float base, float top, float height, int slices, int stacks)
{
  immBegin(GPU_PRIM_LINES, 6 * slices * stacks);
  for (int i = 0; i < slices; i++) {
    const float angle1 = float(2 * M_PI) * (float(i) / float(slices));
    const float angle2 = float(2 * M_PI) * (float(i + 1) / float(slices));
    const float cos1 = cosf(angle1);
    const float sin1 = sinf(angle1);
    const float cos2 = cosf(angle2);
    const float sin2 = sinf(angle2);

    for (int j = 0; j < stacks; j++) {
      float fac1 = float(j) / float(stacks);
      float fac2 = float(j + 1) / float(stacks);
      float r1 = base * (1.0f - fac1) + top * fac1;
      float r2 = base * (1.0f - fac2) + top * fac2;
      float h1 = height * (float(j) / float(stacks));
      float h2 = height * (float(j + 1) / float(stacks));

      const float v1[3] = {r1 * cos2, r1 * sin2, h1};
      const float v2[3] = {r2 * cos2, r2 * sin2, h2};
      const float v3[3] = {r2 * cos1, r2 * sin1, h2};
      const float v4[3] = {r1 * cos1, r1 * sin1, h1};

      immVertex3fv(pos, v1);
      immVertex3fv(pos, v2);

      immVertex3fv(pos, v2);
      immVertex3fv(pos, v3);

      immVertex3fv(pos, v1);
      immVertex3fv(pos, v4);
    }
  }
  immEnd();
}

void imm_draw_cylinder_fill_3d(
    uint pos, float base, float top, float height, int slices, int stacks)
{
  immBegin(GPU_PRIM_TRIS, 6 * slices * stacks);
  for (int i = 0; i < slices; i++) {
    const float angle1 = float(2 * M_PI) * (float(i) / float(slices));
    const float angle2 = float(2 * M_PI) * (float(i + 1) / float(slices));
    const float cos1 = cosf(angle1);
    const float sin1 = sinf(angle1);
    const float cos2 = cosf(angle2);
    const float sin2 = sinf(angle2);

    for (int j = 0; j < stacks; j++) {
      float fac1 = float(j) / float(stacks);
      float fac2 = float(j + 1) / float(stacks);
      float r1 = base * (1.0f - fac1) + top * fac1;
      float r2 = base * (1.0f - fac2) + top * fac2;
      float h1 = height * (float(j) / float(stacks));
      float h2 = height * (float(j + 1) / float(stacks));

      const float v1[3] = {r1 * cos2, r1 * sin2, h1};
      const float v2[3] = {r2 * cos2, r2 * sin2, h2};
      const float v3[3] = {r2 * cos1, r2 * sin1, h2};
      const float v4[3] = {r1 * cos1, r1 * sin1, h1};

      /* first tri */
      immVertex3fv(pos, v1);
      immVertex3fv(pos, v2);
      immVertex3fv(pos, v3);

      /* second tri */
      immVertex3fv(pos, v3);
      immVertex3fv(pos, v4);
      immVertex3fv(pos, v1);
    }
  }
  immEnd();
}

/* Circle Drawing - Tables for Optimized Drawing Speed */
constexpr static int CIRCLE_RESOL = 32;

static void circball_array_fill(const float verts[CIRCLE_RESOL][3],
                                const float cent[3],
                                const float radius,
                                const float tmat[4][4])
{
  /* 32 values of sin function (still same result!) */
  const float sinval[CIRCLE_RESOL] = {
      0.00000000,  0.20129852,  0.39435585,  0.57126821,  0.72479278,  0.84864425,  0.93775213,
      0.98846832,  0.99871650,  0.96807711,  0.89780453,  0.79077573,  0.65137248,  0.48530196,
      0.29936312,  0.10116832,  -0.10116832, -0.29936312, -0.48530196, -0.65137248, -0.79077573,
      -0.89780453, -0.96807711, -0.99871650, -0.98846832, -0.93775213, -0.84864425, -0.72479278,
      -0.57126821, -0.39435585, -0.20129852, 0.00000000,
  };

  /* 32 values of cos function (still same result!) */
  const float cosval[CIRCLE_RESOL] = {
      1.00000000,  0.97952994,  0.91895781,  0.82076344,  0.68896691,  0.52896401,  0.34730525,
      0.15142777,  -0.05064916, -0.25065253, -0.44039415, -0.61210598, -0.75875812, -0.87434661,
      -0.95413925, -0.99486932, -0.99486932, -0.95413925, -0.87434661, -0.75875812, -0.61210598,
      -0.44039415, -0.25065253, -0.05064916, 0.15142777,  0.34730525,  0.52896401,  0.68896691,
      0.82076344,  0.91895781,  0.97952994,  1.00000000,
  };

  float vx[3], vy[3];
  float *viter = (float *)verts;

  mul_v3_v3fl(vx, tmat[0], radius);
  mul_v3_v3fl(vy, tmat[1], radius);

  for (uint a = 0; a < CIRCLE_RESOL; a++, viter += 3) {
    viter[0] = cent[0] + sinval[a] * vx[0] + cosval[a] * vy[0];
    viter[1] = cent[1] + sinval[a] * vx[1] + cosval[a] * vy[1];
    viter[2] = cent[2] + sinval[a] * vx[2] + cosval[a] * vy[2];
  }
}

void imm_drawcircball(const float cent[3], float radius, const float tmat[4][4], uint pos)
{
  float verts[CIRCLE_RESOL][3];

  circball_array_fill(verts, cent, radius, tmat);

  immBegin(GPU_PRIM_LINE_LOOP, CIRCLE_RESOL);
  for (int i = 0; i < CIRCLE_RESOL; i++) {
    immVertex3fv(pos, verts[i]);
  }
  immEnd();
}
