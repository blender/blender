/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup edscr
 */

#include "ED_screen.h"

#include "GPU_batch_presets.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "screen_intern.h"

/**
 * Draw horizontal shape visualizing future joining
 * (left as well right direction of future joining).
 */
static void draw_horizontal_join_shape(ScrArea *sa, char dir, unsigned int pos)
{
  const float width = screen_geom_area_width(sa) - 1;
  const float height = screen_geom_area_height(sa) - 1;
  vec2f points[10];
  short i;
  float w, h;

  if (height < width) {
    h = height / 8;
    w = height / 4;
  }
  else {
    h = width / 8;
    w = width / 4;
  }

  points[0].x = sa->v1->vec.x;
  points[0].y = sa->v1->vec.y + height / 2;

  points[1].x = sa->v1->vec.x;
  points[1].y = sa->v1->vec.y;

  points[2].x = sa->v4->vec.x - w;
  points[2].y = sa->v4->vec.y;

  points[3].x = sa->v4->vec.x - w;
  points[3].y = sa->v4->vec.y + height / 2 - 2 * h;

  points[4].x = sa->v4->vec.x - 2 * w;
  points[4].y = sa->v4->vec.y + height / 2;

  points[5].x = sa->v4->vec.x - w;
  points[5].y = sa->v4->vec.y + height / 2 + 2 * h;

  points[6].x = sa->v3->vec.x - w;
  points[6].y = sa->v3->vec.y;

  points[7].x = sa->v2->vec.x;
  points[7].y = sa->v2->vec.y;

  points[8].x = sa->v4->vec.x;
  points[8].y = sa->v4->vec.y + height / 2 - h;

  points[9].x = sa->v4->vec.x;
  points[9].y = sa->v4->vec.y + height / 2 + h;

  if (dir == 'l') {
    /* when direction is left, then we flip direction of arrow */
    float cx = sa->v1->vec.x + width;
    for (i = 0; i < 10; i++) {
      points[i].x -= cx;
      points[i].x = -points[i].x;
      points[i].x += sa->v1->vec.x;
    }
  }

  immBegin(GPU_PRIM_TRI_FAN, 5);

  for (i = 0; i < 5; i++) {
    immVertex2f(pos, points[i].x, points[i].y);
  }

  immEnd();

  immBegin(GPU_PRIM_TRI_FAN, 5);

  for (i = 4; i < 8; i++) {
    immVertex2f(pos, points[i].x, points[i].y);
  }

  immVertex2f(pos, points[0].x, points[0].y);
  immEnd();

  immRectf(pos, points[2].x, points[2].y, points[8].x, points[8].y);
  immRectf(pos, points[6].x, points[6].y, points[9].x, points[9].y);
}

/**
 * Draw vertical shape visualizing future joining (up/down direction).
 */
static void draw_vertical_join_shape(ScrArea *sa, char dir, unsigned int pos)
{
  const float width = screen_geom_area_width(sa) - 1;
  const float height = screen_geom_area_height(sa) - 1;
  vec2f points[10];
  short i;
  float w, h;

  if (height < width) {
    h = height / 4;
    w = height / 8;
  }
  else {
    h = width / 4;
    w = width / 8;
  }

  points[0].x = sa->v1->vec.x + width / 2;
  points[0].y = sa->v3->vec.y;

  points[1].x = sa->v2->vec.x;
  points[1].y = sa->v2->vec.y;

  points[2].x = sa->v1->vec.x;
  points[2].y = sa->v1->vec.y + h;

  points[3].x = sa->v1->vec.x + width / 2 - 2 * w;
  points[3].y = sa->v1->vec.y + h;

  points[4].x = sa->v1->vec.x + width / 2;
  points[4].y = sa->v1->vec.y + 2 * h;

  points[5].x = sa->v1->vec.x + width / 2 + 2 * w;
  points[5].y = sa->v1->vec.y + h;

  points[6].x = sa->v4->vec.x;
  points[6].y = sa->v4->vec.y + h;

  points[7].x = sa->v3->vec.x;
  points[7].y = sa->v3->vec.y;

  points[8].x = sa->v1->vec.x + width / 2 - w;
  points[8].y = sa->v1->vec.y;

  points[9].x = sa->v1->vec.x + width / 2 + w;
  points[9].y = sa->v1->vec.y;

  if (dir == 'u') {
    /* when direction is up, then we flip direction of arrow */
    float cy = sa->v1->vec.y + height;
    for (i = 0; i < 10; i++) {
      points[i].y -= cy;
      points[i].y = -points[i].y;
      points[i].y += sa->v1->vec.y;
    }
  }

  immBegin(GPU_PRIM_TRI_FAN, 5);

  for (i = 0; i < 5; i++) {
    immVertex2f(pos, points[i].x, points[i].y);
  }

  immEnd();

  immBegin(GPU_PRIM_TRI_FAN, 5);

  for (i = 4; i < 8; i++) {
    immVertex2f(pos, points[i].x, points[i].y);
  }

  immVertex2f(pos, points[0].x, points[0].y);
  immEnd();

  immRectf(pos, points[2].x, points[2].y, points[8].x, points[8].y);
  immRectf(pos, points[6].x, points[6].y, points[9].x, points[9].y);
}

/**
 * Draw join shape due to direction of joining.
 */
static void draw_join_shape(ScrArea *sa, char dir, unsigned int pos)
{
  if (dir == 'u' || dir == 'd') {
    draw_vertical_join_shape(sa, dir, pos);
  }
  else {
    draw_horizontal_join_shape(sa, dir, pos);
  }
}

#define CORNER_RESOLUTION 3

static void do_vert_pair(GPUVertBuf *vbo, uint pos, uint *vidx, int corner, int i)
{
  float inter[2], exter[2];
  inter[0] = cosf(corner * M_PI_2 + (i * M_PI_2 / (CORNER_RESOLUTION - 1.0f)));
  inter[1] = sinf(corner * M_PI_2 + (i * M_PI_2 / (CORNER_RESOLUTION - 1.0f)));

  /* Snap point to edge */
  float div = 1.0f / max_ff(fabsf(inter[0]), fabsf(inter[1]));
  mul_v2_v2fl(exter, inter, div);
  exter[0] = roundf(exter[0]);
  exter[1] = roundf(exter[1]);

  if (i == 0 || i == (CORNER_RESOLUTION - 1)) {
    copy_v2_v2(inter, exter);
  }

  /* Line width is 20% of the entire corner size. */
  const float line_width = 0.2f; /* Keep in sync with shader */
  mul_v2_fl(inter, 1.0f - line_width);
  mul_v2_fl(exter, 1.0f + line_width);

  switch (corner) {
    case 0:
      add_v2_v2(inter, (float[2]){-1.0f, -1.0f});
      add_v2_v2(exter, (float[2]){-1.0f, -1.0f});
      break;
    case 1:
      add_v2_v2(inter, (float[2]){1.0f, -1.0f});
      add_v2_v2(exter, (float[2]){1.0f, -1.0f});
      break;
    case 2:
      add_v2_v2(inter, (float[2]){1.0f, 1.0f});
      add_v2_v2(exter, (float[2]){1.0f, 1.0f});
      break;
    case 3:
      add_v2_v2(inter, (float[2]){-1.0f, 1.0f});
      add_v2_v2(exter, (float[2]){-1.0f, 1.0f});
      break;
  }

  GPU_vertbuf_attr_set(vbo, pos, (*vidx)++, inter);
  GPU_vertbuf_attr_set(vbo, pos, (*vidx)++, exter);
}

static GPUBatch *batch_screen_edges_get(int *corner_len)
{
  static GPUBatch *screen_edges_batch = NULL;

  if (screen_edges_batch == NULL) {
    GPUVertFormat format = {0};
    uint pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, CORNER_RESOLUTION * 2 * 4 + 2);

    uint vidx = 0;
    for (int corner = 0; corner < 4; ++corner) {
      for (int c = 0; c < CORNER_RESOLUTION; ++c) {
        do_vert_pair(vbo, pos, &vidx, corner, c);
      }
    }
    /* close the loop */
    do_vert_pair(vbo, pos, &vidx, 0, 0);

    screen_edges_batch = GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
    gpu_batch_presets_register(screen_edges_batch);
  }

  if (corner_len) {
    *corner_len = CORNER_RESOLUTION * 2;
  }
  return screen_edges_batch;
}

#undef CORNER_RESOLUTION

/**
 * Draw screen area darker with arrow (visualization of future joining).
 */
static void scrarea_draw_shape_dark(ScrArea *sa, char dir, unsigned int pos)
{
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  immUniformColor4ub(0, 0, 0, 50);

  draw_join_shape(sa, dir, pos);
}

/**
 * Draw screen area lighter with arrow shape ("eraser" of previous dark shape).
 */
static void scrarea_draw_shape_light(ScrArea *sa, char UNUSED(dir), unsigned int pos)
{
  GPU_blend_set_func(GPU_DST_COLOR, GPU_SRC_ALPHA);
  /* value 181 was hardly computed: 181~105 */
  immUniformColor4ub(255, 255, 255, 50);
  /* draw_join_shape(sa, dir); */

  immRectf(pos, sa->v1->vec.x, sa->v1->vec.y, sa->v3->vec.x, sa->v3->vec.y);
}

static void drawscredge_area_draw(
    int sizex, int sizey, short x1, short y1, short x2, short y2, float edge_thickness)
{
  rctf rect;
  BLI_rctf_init(&rect, (float)x1, (float)x2, (float)y1, (float)y2);

  /* right border area */
  if (x2 >= sizex - 1) {
    rect.xmax += edge_thickness * 0.5f;
  }

  /* left border area */
  if (x1 <= 0) { /* otherwise it draws the emboss of window over */
    rect.xmin -= edge_thickness * 0.5f;
  }

  /* top border area */
  if (y2 >= sizey - 1) {
    rect.ymax += edge_thickness * 0.5f;
  }

  /* bottom border area */
  if (y1 <= 0) {
    rect.ymin -= edge_thickness * 0.5f;
  }

  GPUBatch *batch = batch_screen_edges_get(NULL);
  GPU_batch_uniform_4fv(batch, "rect", (float *)&rect);
  GPU_batch_draw(batch);
}

/**
 * \brief Screen edges drawing.
 */
static void drawscredge_area(ScrArea *sa, int sizex, int sizey, float edge_thickness)
{
  short x1 = sa->v1->vec.x;
  short y1 = sa->v1->vec.y;
  short x2 = sa->v3->vec.x;
  short y2 = sa->v3->vec.y;

  drawscredge_area_draw(sizex, sizey, x1, y1, x2, y2, edge_thickness);
}

/**
 * Only for edge lines between areas.
 */
void ED_screen_draw_edges(wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);
  screen->do_draw = false;

  if (screen->state == SCREENFULL) {
    return;
  }

  if (screen->temp && BLI_listbase_is_single(&screen->areabase)) {
    return;
  }

  const int winsize_x = WM_window_pixels_x(win);
  const int winsize_y = WM_window_pixels_y(win);
  float col[4], corner_scale, edge_thickness;
  int verts_per_corner = 0;

  ScrArea *sa;

  rcti scissor_rect;
  BLI_rcti_init_minmax(&scissor_rect);
  for (sa = screen->areabase.first; sa; sa = sa->next) {
    BLI_rcti_do_minmax_v(&scissor_rect, (int[2]){sa->v1->vec.x, sa->v1->vec.y});
    BLI_rcti_do_minmax_v(&scissor_rect, (int[2]){sa->v3->vec.x, sa->v3->vec.y});
  }

  if (GPU_type_matches(GPU_DEVICE_INTEL_UHD, GPU_OS_UNIX, GPU_DRIVER_ANY)) {
    /* For some reason, on linux + Intel UHD Graphics 620 the driver
     * hangs if we don't flush before this. (See T57455) */
    GPU_flush();
  }

  GPU_scissor(scissor_rect.xmin,
              scissor_rect.ymin,
              BLI_rcti_size_x(&scissor_rect) + 1,
              BLI_rcti_size_y(&scissor_rect) + 1);

  /* It seems that all areas gets smaller when pixelsize is > 1.
   * So in order to avoid missing pixels we just disable de scissors. */
  if (U.pixelsize <= 1.0f) {
    glEnable(GL_SCISSOR_TEST);
  }

  UI_GetThemeColor4fv(TH_EDITOR_OUTLINE, col);
  col[3] = 1.0f;
  corner_scale = U.pixelsize * 8.0f;
  edge_thickness = corner_scale * 0.21f;

  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  GPUBatch *batch = batch_screen_edges_get(&verts_per_corner);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_AREA_EDGES);
  GPU_batch_uniform_1i(batch, "cornerLen", verts_per_corner);
  GPU_batch_uniform_1f(batch, "scale", corner_scale);
  GPU_batch_uniform_4fv(batch, "color", col);

  for (sa = screen->areabase.first; sa; sa = sa->next) {
    drawscredge_area(sa, winsize_x, winsize_y, edge_thickness);
  }

  GPU_blend(false);

  if (U.pixelsize <= 1.0f) {
    glDisable(GL_SCISSOR_TEST);
  }
}

/**
 * The blended join arrows.
 *
 * \param sa1: Area from which the resultant originates.
 * \param sa2: Target area that will be replaced.
 */
void ED_screen_draw_join_shape(ScrArea *sa1, ScrArea *sa2)
{
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  GPU_line_width(1);

  /* blended join arrow */
  int dir = area_getorientation(sa1, sa2);
  int dira = -1;
  if (dir != -1) {
    switch (dir) {
      case 0: /* W */
        dir = 'r';
        dira = 'l';
        break;
      case 1: /* N */
        dir = 'd';
        dira = 'u';
        break;
      case 2: /* E */
        dir = 'l';
        dira = 'r';
        break;
      case 3: /* S */
        dir = 'u';
        dira = 'd';
        break;
    }

    GPU_blend(true);

    scrarea_draw_shape_dark(sa2, dir, pos);
    scrarea_draw_shape_light(sa1, dira, pos);

    GPU_blend(false);
  }

  immUnbindProgram();
}

void ED_screen_draw_split_preview(ScrArea *sa, const int dir, const float fac)
{
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* splitpoint */
  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  immUniformColor4ub(255, 255, 255, 100);

  immBegin(GPU_PRIM_LINES, 2);

  if (dir == 'h') {
    const float y = (1 - fac) * sa->totrct.ymin + fac * sa->totrct.ymax;

    immVertex2f(pos, sa->totrct.xmin, y);
    immVertex2f(pos, sa->totrct.xmax, y);

    immEnd();

    immUniformColor4ub(0, 0, 0, 100);

    immBegin(GPU_PRIM_LINES, 2);

    immVertex2f(pos, sa->totrct.xmin, y + 1);
    immVertex2f(pos, sa->totrct.xmax, y + 1);

    immEnd();
  }
  else {
    BLI_assert(dir == 'v');
    const float x = (1 - fac) * sa->totrct.xmin + fac * sa->totrct.xmax;

    immVertex2f(pos, x, sa->totrct.ymin);
    immVertex2f(pos, x, sa->totrct.ymax);

    immEnd();

    immUniformColor4ub(0, 0, 0, 100);

    immBegin(GPU_PRIM_LINES, 2);

    immVertex2f(pos, x + 1, sa->totrct.ymin);
    immVertex2f(pos, x + 1, sa->totrct.ymax);

    immEnd();
  }

  GPU_blend(false);

  immUnbindProgram();
}

/* -------------------------------------------------------------------- */
/* Screen Thumbnail Preview */

/**
 * Calculates a scale factor to squash the preview for \a screen into a rectangle
 * of given size and aspect.
 */
static void screen_preview_scale_get(
    const bScreen *screen, float size_x, float size_y, const float asp[2], float r_scale[2])
{
  float max_x = 0, max_y = 0;

  for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
    max_x = MAX2(max_x, sa->totrct.xmax);
    max_y = MAX2(max_y, sa->totrct.ymax);
  }
  r_scale[0] = (size_x * asp[0]) / max_x;
  r_scale[1] = (size_y * asp[1]) / max_y;
}

static void screen_preview_draw_areas(const bScreen *screen,
                                      const float scale[2],
                                      const float col[4],
                                      const float ofs_between_areas)
{
  const float ofs_h = ofs_between_areas * 0.5f;
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor4fv(col);

  for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
    rctf rect = {
        .xmin = sa->totrct.xmin * scale[0] + ofs_h,
        .xmax = sa->totrct.xmax * scale[0] - ofs_h,
        .ymin = sa->totrct.ymin * scale[1] + ofs_h,
        .ymax = sa->totrct.ymax * scale[1] - ofs_h,
    };

    immBegin(GPU_PRIM_TRI_FAN, 4);
    immVertex2f(pos, rect.xmin, rect.ymin);
    immVertex2f(pos, rect.xmax, rect.ymin);
    immVertex2f(pos, rect.xmax, rect.ymax);
    immVertex2f(pos, rect.xmin, rect.ymax);
    immEnd();
  }

  immUnbindProgram();
}

static void screen_preview_draw(const bScreen *screen, int size_x, int size_y)
{
  const float asp[2] = {1.0f, 0.8f}; /* square previews look a bit ugly */
  /* could use theme color (tui.wcol_menu_item.text),
   * but then we'd need to regenerate all previews when changing. */
  const float col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  float scale[2];

  wmOrtho2(0.0f, size_x, 0.0f, size_y);
  /* center */
  GPU_matrix_push();
  GPU_matrix_identity_set();
  GPU_matrix_translate_2f(size_x * (1.0f - asp[0]) * 0.5f, size_y * (1.0f - asp[1]) * 0.5f);

  screen_preview_scale_get(screen, size_x, size_y, asp, scale);
  screen_preview_draw_areas(screen, scale, col, 1.5f);

  GPU_matrix_pop();
}

/**
 * Render the preview for a screen layout in \a screen.
 */
void ED_screen_preview_render(const bScreen *screen, int size_x, int size_y, unsigned int *r_rect)
{
  char err_out[256] = "unknown";
  GPUOffScreen *offscreen = GPU_offscreen_create(size_x, size_y, 0, true, false, err_out);

  GPU_offscreen_bind(offscreen, true);
  GPU_clear_color(0.0, 0.0, 0.0, 0.0);
  GPU_clear(GPU_COLOR_BIT | GPU_DEPTH_BIT);

  screen_preview_draw(screen, size_x, size_y);

  GPU_offscreen_read_pixels(offscreen, GL_UNSIGNED_BYTE, r_rect);
  GPU_offscreen_unbind(offscreen, true);

  GPU_offscreen_free(offscreen);
}
