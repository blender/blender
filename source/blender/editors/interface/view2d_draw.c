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
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 */

#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BLI_array.h"
#include "BLI_utildefines.h"
#include "BLI_rect.h"
#include "BLI_math.h"
#include "BLI_timecode.h"
#include "BLI_string.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "WM_api.h"

#include "BLF_api.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "interface_intern.h"

/* Compute display grid resolution
 ********************************************************/

#define MIN_MAJOR_LINE_DISTANCE (U.v2d_min_gridsize * UI_DPI_FAC)

static float select_major_distance(const float *possible_distances,
                                   uint amount,
                                   float pixel_width,
                                   float view_width)
{
  BLI_assert(amount >= 1);

  if (IS_EQF(view_width, 0.0f)) {
    return possible_distances[0];
  }

  float pixels_per_view_unit = pixel_width / view_width;

  for (uint i = 0; i < amount; i++) {
    float distance = possible_distances[i];
    if (pixels_per_view_unit * distance >= MIN_MAJOR_LINE_DISTANCE) {
      return distance;
    }
  }
  return possible_distances[amount - 1];
}

static const float discrete_value_scales[] = {
    1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000};

static const float continuous_value_scales[] = {0.01, 0.02, 0.05,  0.1,   0.2,   0.5,   1,   2,
                                                5,    10,   20,    50,    100,   200,   500, 1000,
                                                2000, 5000, 10000, 20000, 50000, 100000};

static uint view2d_major_step_x__discrete(const View2D *v2d)
{
  return select_major_distance(discrete_value_scales,
                               ARRAY_SIZE(discrete_value_scales),
                               BLI_rcti_size_x(&v2d->mask),
                               BLI_rctf_size_x(&v2d->cur));
}

static float view2d_major_step_x__continuous(const View2D *v2d)
{
  return select_major_distance(continuous_value_scales,
                               ARRAY_SIZE(continuous_value_scales),
                               BLI_rcti_size_x(&v2d->mask),
                               BLI_rctf_size_x(&v2d->cur));
}

static float view2d_major_step_y__continuous(const View2D *v2d)
{
  return select_major_distance(continuous_value_scales,
                               ARRAY_SIZE(continuous_value_scales),
                               BLI_rcti_size_y(&v2d->mask),
                               BLI_rctf_size_y(&v2d->cur));
}

static float view2d_major_step_x__time(const View2D *v2d, const Scene *scene)
{
  double fps = FPS;

  float *possible_distances = NULL;
  BLI_array_staticdeclare(possible_distances, 32);

  for (uint step = 1; step < fps; step *= 2) {
    BLI_array_append(possible_distances, step);
  }
  BLI_array_append(possible_distances, fps);
  BLI_array_append(possible_distances, 2 * fps);
  BLI_array_append(possible_distances, 5 * fps);
  BLI_array_append(possible_distances, 10 * fps);
  BLI_array_append(possible_distances, 30 * fps);
  BLI_array_append(possible_distances, 60 * fps);
  BLI_array_append(possible_distances, 2 * 60 * fps);
  BLI_array_append(possible_distances, 5 * 60 * fps);
  BLI_array_append(possible_distances, 10 * 60 * fps);
  BLI_array_append(possible_distances, 30 * 60 * fps);
  BLI_array_append(possible_distances, 60 * 60 * fps);

  float distance = select_major_distance(possible_distances,
                                         BLI_array_len(possible_distances),
                                         BLI_rcti_size_x(&v2d->mask),
                                         BLI_rctf_size_x(&v2d->cur));

  BLI_array_free(possible_distances);
  return distance;
}

/* Draw parallel lines
 ************************************/

typedef struct ParallelLinesSet {
  float offset;
  float distance;
} ParallelLinesSet;

static void get_parallel_lines_draw_steps(const ParallelLinesSet *lines,
                                          float region_start,
                                          float region_end,
                                          float *r_first,
                                          uint *r_steps)
{
  if (region_start >= region_end) {
    *r_first = 0;
    *r_steps = 0;
    return;
  }

  BLI_assert(lines->distance > 0);
  BLI_assert(region_start <= region_end);

  *r_first = ceilf((region_start - lines->offset) / lines->distance) * lines->distance +
             lines->offset;

  if (region_start <= *r_first && region_end >= *r_first) {
    *r_steps = MAX2(0, floorf((region_end - *r_first) / lines->distance)) + 1;
  }
  else {
    *r_steps = 0;
  }
}

static void draw_parallel_lines(const ParallelLinesSet *lines,
                                const rctf *rect,
                                const uchar *color,
                                char direction)
{
  float first;
  uint steps;

  if (direction == 'v') {
    get_parallel_lines_draw_steps(lines, rect->xmin, rect->xmax, &first, &steps);
  }
  else {
    BLI_assert(direction == 'h');
    get_parallel_lines_draw_steps(lines, rect->ymin, rect->ymax, &first, &steps);
  }

  if (steps == 0) {
    return;
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor3ubv(color);
  immBegin(GPU_PRIM_LINES, steps * 2);

  if (direction == 'v') {
    for (uint i = 0; i < steps; i++) {
      float xpos = first + i * lines->distance;
      immVertex2f(pos, xpos, rect->ymin);
      immVertex2f(pos, xpos, rect->ymax);
    }
  }
  else {
    for (uint i = 0; i < steps; i++) {
      float ypos = first + i * lines->distance;
      immVertex2f(pos, rect->xmin, ypos);
      immVertex2f(pos, rect->xmax, ypos);
    }
  }

  immEnd();
  immUnbindProgram();
}

static void view2d_draw_lines_internal(const View2D *v2d,
                                       const ParallelLinesSet *lines,
                                       const uchar *color,
                                       char direction)
{
  GPU_matrix_push_projection();
  UI_view2d_view_ortho(v2d);
  draw_parallel_lines(lines, &v2d->cur, color, direction);
  GPU_matrix_pop_projection();
}

static void view2d_draw_lines(const View2D *v2d,
                              float major_distance,
                              bool display_minor_lines,
                              char direction)
{
  uchar major_color[3];
  uchar minor_color[3];
  UI_GetThemeColor3ubv(TH_GRID, major_color);
  UI_GetThemeColorShade3ubv(TH_GRID, 16, minor_color);

  ParallelLinesSet major_lines;
  major_lines.distance = major_distance;
  major_lines.offset = 0;
  view2d_draw_lines_internal(v2d, &major_lines, major_color, direction);

  if (display_minor_lines) {
    ParallelLinesSet minor_lines;
    minor_lines.distance = major_distance;
    minor_lines.offset = major_distance / 2.0f;
    view2d_draw_lines_internal(v2d, &minor_lines, minor_color, direction);
  }
}

/* Scale indicator text drawing
 **************************************************/

typedef void (*PositionToString)(
    void *user_data, float v2d_pos, float v2d_step, uint max_len, char *r_str);

static void draw_horizontal_scale_indicators(const ARegion *ar,
                                             const View2D *v2d,
                                             float distance,
                                             const rcti *rect,
                                             PositionToString to_string,
                                             void *to_string_data,
                                             int colorid)
{
  if (UI_view2d_scale_get_x(v2d) <= 0.0f) {
    return;
  }

  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(ar);

  float start;
  uint steps;
  {
    ParallelLinesSet lines;
    lines.distance = distance;
    lines.offset = 0;
    get_parallel_lines_draw_steps(&lines,
                                  UI_view2d_region_to_view_x(v2d, rect->xmin),
                                  UI_view2d_region_to_view_x(v2d, rect->xmax),
                                  &start,
                                  &steps);
  }

  const int font_id = BLF_default();
  UI_FontThemeColor(font_id, colorid);

  BLF_batch_draw_begin();

  float ypos = rect->ymin + 4 * UI_DPI_FAC;
  float xmin = rect->xmin;
  float xmax = rect->xmax;

  for (uint i = 0; i < steps; i++) {
    float xpos_view = start + i * distance;
    float xpos_region = UI_view2d_view_to_region_x(v2d, xpos_view);
    char text[32];
    to_string(to_string_data, xpos_view, distance, sizeof(text), text);
    float text_width = BLF_width(font_id, text, strlen(text));

    if (xpos_region - text_width / 2.0f >= xmin && xpos_region + text_width / 2.0f <= xmax) {
      BLF_draw_default_ascii(xpos_region - text_width / 2.0f, ypos, 0.0f, text, sizeof(text));
    }
  }

  BLF_batch_draw_end();

  GPU_matrix_pop_projection();
}

static void draw_vertical_scale_indicators(const ARegion *ar,
                                           const View2D *v2d,
                                           float distance,
                                           float display_offset,
                                           const rcti *rect,
                                           PositionToString to_string,
                                           void *to_string_data,
                                           int colorid)
{
  if (UI_view2d_scale_get_y(v2d) <= 0.0f) {
    return;
  }

  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(ar);

  float start;
  uint steps;
  {
    ParallelLinesSet lines;
    lines.distance = distance;
    lines.offset = 0;
    get_parallel_lines_draw_steps(&lines,
                                  UI_view2d_region_to_view_y(v2d, rect->ymin),
                                  UI_view2d_region_to_view_y(v2d, rect->ymax),
                                  &start,
                                  &steps);
  }

  const int font_id = BLF_default();
  UI_FontThemeColor(font_id, colorid);

  BLF_enable(font_id, BLF_ROTATION);
  BLF_rotation(font_id, M_PI_2);

  BLF_batch_draw_begin();

  float xpos = rect->xmax - 2.0f * UI_DPI_FAC;
  float ymin = rect->ymin;
  float ymax = rect->ymax;

  for (uint i = 0; i < steps; i++) {
    float ypos_view = start + i * distance;
    float ypos_region = UI_view2d_view_to_region_y(v2d, ypos_view + display_offset);
    char text[32];
    to_string(to_string_data, ypos_view, distance, sizeof(text), text);
    float text_width = BLF_width(font_id, text, strlen(text));

    if (ypos_region - text_width / 2.0f >= ymin && ypos_region + text_width / 2.0f <= ymax) {
      BLF_draw_default_ascii(xpos, ypos_region - text_width / 2.0f, 0.0f, text, sizeof(text));
    }
  }

  BLF_batch_draw_end();
  BLF_disable(font_id, BLF_ROTATION);

  GPU_matrix_pop_projection();
}

static void view_to_string__frame_number(
    void *UNUSED(user_data), float v2d_pos, float UNUSED(v2d_step), uint max_len, char *r_str)
{
  BLI_snprintf(r_str, max_len, "%d", (int)v2d_pos);
}

static void view_to_string__time(
    void *user_data, float v2d_pos, float UNUSED(v2d_step), uint max_len, char *r_str)
{
  const Scene *scene = (const Scene *)user_data;

  int brevity_level = 0;
  BLI_timecode_string_from_time(
      r_str, max_len, brevity_level, v2d_pos / (float)FPS, FPS, U.timecode_style);
}

static void view_to_string__value(
    void *UNUSED(user_data), float v2d_pos, float v2d_step, uint max_len, char *r_str)
{
  if (v2d_step >= 1.0f) {
    BLI_snprintf(r_str, max_len, "%d", (int)v2d_pos);
  }
  else if (v2d_step >= 0.1f) {
    BLI_snprintf(r_str, max_len, "%.1f", v2d_pos);
  }
  else if (v2d_step >= 0.01f) {
    BLI_snprintf(r_str, max_len, "%.2f", v2d_pos);
  }
  else {
    BLI_snprintf(r_str, max_len, "%.3f", v2d_pos);
  }
}

/* Grid Resolution API
 **************************************************/

float UI_view2d_grid_resolution_x__frames_or_seconds(const struct View2D *v2d,
                                                     const struct Scene *scene,
                                                     bool display_seconds)
{
  if (display_seconds) {
    return view2d_major_step_x__time(v2d, scene);
  }
  else {
    return view2d_major_step_x__continuous(v2d);
  }
}

float UI_view2d_grid_resolution_y__values(const struct View2D *v2d)
{
  return view2d_major_step_y__continuous(v2d);
}

/* Line Drawing API
 **************************************************/

void UI_view2d_draw_lines_x__discrete_values(const View2D *v2d)
{
  uint major_line_distance = view2d_major_step_x__discrete(v2d);
  view2d_draw_lines(v2d, major_line_distance, major_line_distance > 1, 'v');
}

void UI_view2d_draw_lines_x__values(const View2D *v2d)
{
  float major_line_distance = view2d_major_step_x__continuous(v2d);
  view2d_draw_lines(v2d, major_line_distance, true, 'v');
}

void UI_view2d_draw_lines_y__values(const View2D *v2d)
{
  float major_line_distance = view2d_major_step_y__continuous(v2d);
  view2d_draw_lines(v2d, major_line_distance, true, 'h');
}

void UI_view2d_draw_lines_x__discrete_time(const View2D *v2d, const Scene *scene)
{
  float major_line_distance = view2d_major_step_x__time(v2d, scene);
  view2d_draw_lines(v2d, major_line_distance, major_line_distance > 1, 'v');
}

void UI_view2d_draw_lines_x__discrete_frames_or_seconds(const View2D *v2d,
                                                        const Scene *scene,
                                                        bool display_seconds)
{
  if (display_seconds) {
    UI_view2d_draw_lines_x__discrete_time(v2d, scene);
  }
  else {
    UI_view2d_draw_lines_x__discrete_values(v2d);
  }
}

void UI_view2d_draw_lines_x__frames_or_seconds(const View2D *v2d,
                                               const Scene *scene,
                                               bool display_seconds)
{
  if (display_seconds) {
    UI_view2d_draw_lines_x__discrete_time(v2d, scene);
  }
  else {
    UI_view2d_draw_lines_x__values(v2d);
  }
}

/* Scale indicator text drawing API
 **************************************************/

static void UI_view2d_draw_scale_x__discrete_values(const ARegion *ar,
                                                    const View2D *v2d,
                                                    const rcti *rect,
                                                    int colorid)
{
  float number_step = view2d_major_step_x__discrete(v2d);
  draw_horizontal_scale_indicators(
      ar, v2d, number_step, rect, view_to_string__frame_number, NULL, colorid);
}

static void UI_view2d_draw_scale_x__discrete_time(
    const ARegion *ar, const View2D *v2d, const rcti *rect, const Scene *scene, int colorid)
{
  float step = view2d_major_step_x__time(v2d, scene);
  draw_horizontal_scale_indicators(
      ar, v2d, step, rect, view_to_string__time, (void *)scene, colorid);
}

static void UI_view2d_draw_scale_x__values(const ARegion *ar,
                                           const View2D *v2d,
                                           const rcti *rect,
                                           int colorid)
{
  float step = view2d_major_step_x__continuous(v2d);
  draw_horizontal_scale_indicators(ar, v2d, step, rect, view_to_string__value, NULL, colorid);
}

void UI_view2d_draw_scale_y__values(const ARegion *ar,
                                    const View2D *v2d,
                                    const rcti *rect,
                                    int colorid)
{
  float step = view2d_major_step_y__continuous(v2d);
  draw_vertical_scale_indicators(ar, v2d, step, 0.0f, rect, view_to_string__value, NULL, colorid);
}

void UI_view2d_draw_scale_y__block(const ARegion *ar,
                                   const View2D *v2d,
                                   const rcti *rect,
                                   int colorid)
{
  draw_vertical_scale_indicators(ar, v2d, 1.0f, 0.5f, rect, view_to_string__value, NULL, colorid);
}

void UI_view2d_draw_scale_x__discrete_frames_or_seconds(const struct ARegion *ar,
                                                        const struct View2D *v2d,
                                                        const struct rcti *rect,
                                                        const struct Scene *scene,
                                                        bool display_seconds,
                                                        int colorid)
{
  if (display_seconds) {
    UI_view2d_draw_scale_x__discrete_time(ar, v2d, rect, scene, colorid);
  }
  else {
    UI_view2d_draw_scale_x__discrete_values(ar, v2d, rect, colorid);
  }
}

void UI_view2d_draw_scale_x__frames_or_seconds(const struct ARegion *ar,
                                               const struct View2D *v2d,
                                               const struct rcti *rect,
                                               const struct Scene *scene,
                                               bool display_seconds,
                                               int colorid)
{
  if (display_seconds) {
    UI_view2d_draw_scale_x__discrete_time(ar, v2d, rect, scene, colorid);
  }
  else {
    UI_view2d_draw_scale_x__values(ar, v2d, rect, colorid);
  }
}
