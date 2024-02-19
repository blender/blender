/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_timecode.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "WM_api.hh"

#include "BLF_api.hh"

#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "interface_intern.hh"

/* Compute display grid resolution
 ********************************************************/

#define MIN_MAJOR_LINE_DISTANCE (U.v2d_min_gridsize * UI_SCALE_FAC)

static float select_major_distance(const float *possible_distances,
                                   uint amount,
                                   float pixel_width,
                                   float view_width)
{
  BLI_assert(amount >= 1);

  if (IS_EQF(view_width, 0.0f)) {
    return possible_distances[0];
  }

  const float pixels_per_view_unit = pixel_width / view_width;

  for (uint i = 0; i < amount; i++) {
    const float distance = possible_distances[i];
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
  const double fps = FPS;

  blender::Vector<float, 32> possible_distances;

  for (int step = 1; step < fps; step *= 2) {
    possible_distances.append(step);
  }

  for (int i = 0; i <= 5; i++) {
    uint fac = pow(60, i);
    possible_distances.append(fac * fps);
    possible_distances.append(fac * 2 * fps);
    possible_distances.append(fac * 5 * fps);
    possible_distances.append(fac * 10 * fps);
    possible_distances.append(fac * 30 * fps);
    possible_distances.append(fac * 60 * fps);
  }

  float distance = select_major_distance(possible_distances.data(),
                                         possible_distances.size(),
                                         BLI_rcti_size_x(&v2d->mask),
                                         BLI_rctf_size_x(&v2d->cur));

  return distance;
}

/* Draw parallel lines
 ************************************/

struct ParallelLinesSet {
  float offset;
  float distance;
};

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
    *r_steps = std::max(0.0f, floorf((region_end - *r_first) / lines->distance)) + 1;
  }
  else {
    *r_steps = 0;
  }
}

/**
 * \param rect_mask: Region size in pixels.
 */
static void draw_parallel_lines(const ParallelLinesSet *lines,
                                const rctf *rect,
                                const rcti *rect_mask,
                                const uchar color[3],
                                char direction)
{
  float first;
  uint steps, steps_max;

  if (direction == 'v') {
    get_parallel_lines_draw_steps(lines, rect->xmin, rect->xmax, &first, &steps);
    steps_max = BLI_rcti_size_x(rect_mask);
  }
  else {
    BLI_assert(direction == 'h');
    get_parallel_lines_draw_steps(lines, rect->ymin, rect->ymax, &first, &steps);
    steps_max = BLI_rcti_size_y(rect_mask);
  }

  if (steps == 0) {
    return;
  }

  if (UNLIKELY(steps >= steps_max)) {
    /* Note that we could draw a solid color,
     * however this flickers because of numeric instability when zoomed out. */
    return;
  }

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  if (U.pixelsize > 1.0f) {
    float viewport[4];
    GPU_viewport_size_get_f(viewport);

    immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);
    immUniform2fv("viewportSize", &viewport[2]);
    /* -1.0f offset here is because the line is too fat due to the builtin anti-aliasing.
     * TODO: make a variant or a uniform to toggle it off. */
    immUniform1f("lineWidth", U.pixelsize - 1.0f);
  }
  else {
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  }
  immUniformColor3ubv(color);
  immBegin(GPU_PRIM_LINES, steps * 2);

  if (direction == 'v') {
    for (uint i = 0; i < steps; i++) {
      const float xpos = first + i * lines->distance;
      immVertex2f(pos, xpos, rect->ymin);
      immVertex2f(pos, xpos, rect->ymax);
    }
  }
  else {
    for (uint i = 0; i < steps; i++) {
      const float ypos = first + i * lines->distance;
      immVertex2f(pos, rect->xmin, ypos);
      immVertex2f(pos, rect->xmax, ypos);
    }
  }

  immEnd();
  immUnbindProgram();
}

static void view2d_draw_lines_internal(const View2D *v2d,
                                       const ParallelLinesSet *lines,
                                       const uchar color[3],
                                       char direction)
{
  GPU_matrix_push_projection();
  UI_view2d_view_ortho(v2d);
  draw_parallel_lines(lines, &v2d->cur, &v2d->mask, color, direction);
  GPU_matrix_pop_projection();
}

static void view2d_draw_lines(const View2D *v2d,
                              float major_distance,
                              bool display_minor_lines,
                              char direction)
{
  {
    uchar major_color[3];
    UI_GetThemeColor3ubv(TH_GRID, major_color);
    ParallelLinesSet major_lines;
    major_lines.distance = major_distance;
    major_lines.offset = 0;
    view2d_draw_lines_internal(v2d, &major_lines, major_color, direction);
  }

  if (display_minor_lines) {
    uchar minor_color[3];
    UI_GetThemeColorShade3ubv(TH_GRID, 16, minor_color);
    ParallelLinesSet minor_lines;
    minor_lines.distance = major_distance;
    minor_lines.offset = major_distance / 2.0f;
    view2d_draw_lines_internal(v2d, &minor_lines, minor_color, direction);
  }
}

/* Scale indicator text drawing
 **************************************************/

using PositionToString =
    void (*)(void *user_data, float v2d_pos, float v2d_step, char *r_str, uint str_maxncpy);

static void draw_horizontal_scale_indicators(const ARegion *region,
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
    const uint steps_max = BLI_rcti_size_x(&v2d->mask);
    if (UNLIKELY(steps >= steps_max)) {
      return;
    }
  }

  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(region);

  const int font_id = BLF_default();
  UI_FontThemeColor(font_id, colorid);

  BLF_batch_draw_begin();

  const float ypos = rect->ymin + 4 * UI_SCALE_FAC;
  const float xmin = rect->xmin;
  const float xmax = rect->xmax;

  char text[32];

  /* Calculate max_label_count and draw_frequency based on largest visible label. */
  int draw_frequency;
  {
    to_string(to_string_data, start, 0, text, sizeof(text));
    const float left_text_width = BLF_width(font_id, text, strlen(text));
    to_string(to_string_data, start + steps * distance, 0, text, sizeof(text));
    const float right_text_width = BLF_width(font_id, text, strlen(text));
    const float max_text_width = max_ff(left_text_width, right_text_width);
    const float max_label_count = BLI_rcti_size_x(&v2d->mask) / (max_text_width + 10.0f);
    draw_frequency = ceil(float(steps) / max_label_count);
  }

  if (draw_frequency != 0) {
    const int start_index = abs(int(start / distance)) % draw_frequency;
    for (uint i = start_index; i < steps; i += draw_frequency) {
      const float xpos_view = start + i * distance;
      const float xpos_region = UI_view2d_view_to_region_x(v2d, xpos_view);
      to_string(to_string_data, xpos_view, distance, text, sizeof(text));
      const float text_width = BLF_width(font_id, text, strlen(text));

      if (xpos_region - text_width / 2.0f >= xmin && xpos_region + text_width / 2.0f <= xmax) {
        BLF_draw_default(xpos_region - text_width / 2.0f, ypos, 0.0f, text, sizeof(text));
      }
    }
  }

  BLF_batch_draw_end();
  GPU_matrix_pop_projection();
}

static void draw_vertical_scale_indicators(const ARegion *region,
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
    const uint steps_max = BLI_rcti_size_y(&v2d->mask);
    if (UNLIKELY(steps >= steps_max)) {
      return;
    }
  }

  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(region);

  const int font_id = BLF_default();
  UI_FontThemeColor(font_id, colorid);

  BLF_batch_draw_begin();

  BLF_enable(font_id, BLF_SHADOW);
  const float shadow_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  BLF_shadow(font_id, 5, shadow_color);
  BLF_shadow_offset(font_id, 1, -1);

  const float x_offset = 8.0f;
  const float xpos = (rect->xmin + x_offset) * UI_SCALE_FAC;
  const float ymin = rect->ymin;
  const float ymax = rect->ymax;
  const float y_offset = (BLF_height(font_id, "0", 1) / 2.0f) - U.pixelsize;

  for (uint i = 0; i < steps; i++) {
    const float ypos_view = start + i * distance;
    const float ypos_region = UI_view2d_view_to_region_y(v2d, ypos_view + display_offset);
    char text[32];
    to_string(to_string_data, ypos_view, distance, text, sizeof(text));

    if (ypos_region - y_offset >= ymin && ypos_region + y_offset <= ymax) {
      BLF_draw_default(xpos, ypos_region - y_offset, 0.0f, text, sizeof(text));
    }
  }

  BLF_disable(font_id, BLF_SHADOW);

  BLF_batch_draw_end();

  GPU_matrix_pop_projection();
}

static void view_to_string__frame_number(
    void * /*user_data*/, float v2d_pos, float /*v2d_step*/, char *r_str, uint str_maxncpy)
{
  BLI_snprintf(r_str, str_maxncpy, "%d", int(v2d_pos));
}

static void view_to_string__time(
    void *user_data, float v2d_pos, float v2d_step, char *r_str, uint str_maxncpy)
{
  const Scene *scene = (const Scene *)user_data;

  int brevity_level = 0;
  if (U.timecode_style == USER_TIMECODE_MINIMAL && v2d_step >= FPS) {
    brevity_level = 1;
  }

  BLI_timecode_string_from_time(
      r_str, str_maxncpy, brevity_level, v2d_pos / float(FPS), FPS, U.timecode_style);
}

static void view_to_string__value(
    void * /*user_data*/, float v2d_pos, float v2d_step, char *r_str, uint str_maxncpy)
{
  if (v2d_step >= 1.0f) {
    BLI_snprintf(r_str, str_maxncpy, "%d", int(v2d_pos));
  }
  else if (v2d_step >= 0.1f) {
    BLI_snprintf(r_str, str_maxncpy, "%.1f", v2d_pos);
  }
  else if (v2d_step >= 0.01f) {
    BLI_snprintf(r_str, str_maxncpy, "%.2f", v2d_pos);
  }
  else {
    BLI_snprintf(r_str, str_maxncpy, "%.3f", v2d_pos);
  }
}

/* Grid Resolution API
 **************************************************/

float UI_view2d_grid_resolution_x__frames_or_seconds(const View2D *v2d,
                                                     const Scene *scene,
                                                     bool display_seconds)
{
  if (display_seconds) {
    return view2d_major_step_x__time(v2d, scene);
  }
  return view2d_major_step_x__continuous(v2d);
}

float UI_view2d_grid_resolution_y__values(const View2D *v2d)
{
  return view2d_major_step_y__continuous(v2d);
}

/* Line Drawing API
 **************************************************/

void UI_view2d_draw_lines_x__discrete_values(const View2D *v2d, bool display_minor_lines)
{
  const uint major_line_distance = view2d_major_step_x__discrete(v2d);
  view2d_draw_lines(
      v2d, major_line_distance, display_minor_lines && (major_line_distance > 1), 'v');
}

void UI_view2d_draw_lines_x__values(const View2D *v2d)
{
  const float major_line_distance = view2d_major_step_x__continuous(v2d);
  view2d_draw_lines(v2d, major_line_distance, true, 'v');
}

void UI_view2d_draw_lines_y__values(const View2D *v2d)
{
  const float major_line_distance = view2d_major_step_y__continuous(v2d);
  view2d_draw_lines(v2d, major_line_distance, true, 'h');
}

void UI_view2d_draw_lines_x__discrete_time(const View2D *v2d,
                                           const Scene *scene,
                                           bool display_minor_lines)
{
  const float major_line_distance = view2d_major_step_x__time(v2d, scene);
  view2d_draw_lines(
      v2d, major_line_distance, display_minor_lines && (major_line_distance > 1), 'v');
}

void UI_view2d_draw_lines_x__discrete_frames_or_seconds(const View2D *v2d,
                                                        const Scene *scene,
                                                        bool display_seconds,
                                                        bool display_minor_lines)
{
  if (display_seconds) {
    UI_view2d_draw_lines_x__discrete_time(v2d, scene, display_minor_lines);
  }
  else {
    UI_view2d_draw_lines_x__discrete_values(v2d, display_minor_lines);
  }
}

void UI_view2d_draw_lines_x__frames_or_seconds(const View2D *v2d,
                                               const Scene *scene,
                                               bool display_seconds)
{
  if (display_seconds) {
    UI_view2d_draw_lines_x__discrete_time(v2d, scene, true);
  }
  else {
    UI_view2d_draw_lines_x__values(v2d);
  }
}

/* Scale indicator text drawing API
 **************************************************/

static void UI_view2d_draw_scale_x__discrete_values(const ARegion *region,
                                                    const View2D *v2d,
                                                    const rcti *rect,
                                                    int colorid)
{
  const float number_step = view2d_major_step_x__discrete(v2d);
  draw_horizontal_scale_indicators(
      region, v2d, number_step, rect, view_to_string__frame_number, nullptr, colorid);
}

static void UI_view2d_draw_scale_x__discrete_time(
    const ARegion *region, const View2D *v2d, const rcti *rect, const Scene *scene, int colorid)
{
  const float step = view2d_major_step_x__time(v2d, scene);
  draw_horizontal_scale_indicators(
      region, v2d, step, rect, view_to_string__time, (void *)scene, colorid);
}

static void UI_view2d_draw_scale_x__values(const ARegion *region,
                                           const View2D *v2d,
                                           const rcti *rect,
                                           int colorid)
{
  const float step = view2d_major_step_x__continuous(v2d);
  draw_horizontal_scale_indicators(
      region, v2d, step, rect, view_to_string__value, nullptr, colorid);
}

void UI_view2d_draw_scale_y__values(const ARegion *region,
                                    const View2D *v2d,
                                    const rcti *rect,
                                    int colorid)
{
  const float step = view2d_major_step_y__continuous(v2d);
  draw_vertical_scale_indicators(
      region, v2d, step, 0.0f, rect, view_to_string__value, nullptr, colorid);
}

void UI_view2d_draw_scale_y__block(const ARegion *region,
                                   const View2D *v2d,
                                   const rcti *rect,
                                   int colorid)
{
  draw_vertical_scale_indicators(
      region, v2d, 1.0f, 0.5f, rect, view_to_string__value, nullptr, colorid);
}

void UI_view2d_draw_scale_x__discrete_frames_or_seconds(const ARegion *region,
                                                        const View2D *v2d,
                                                        const rcti *rect,
                                                        const Scene *scene,
                                                        bool display_seconds,
                                                        int colorid)
{
  if (display_seconds) {
    UI_view2d_draw_scale_x__discrete_time(region, v2d, rect, scene, colorid);
  }
  else {
    UI_view2d_draw_scale_x__discrete_values(region, v2d, rect, colorid);
  }
}

void UI_view2d_draw_scale_x__frames_or_seconds(const ARegion *region,
                                               const View2D *v2d,
                                               const rcti *rect,
                                               const Scene *scene,
                                               bool display_seconds,
                                               int colorid)
{
  if (display_seconds) {
    UI_view2d_draw_scale_x__discrete_time(region, v2d, rect, scene, colorid);
  }
  else {
    UI_view2d_draw_scale_x__values(region, v2d, rect, colorid);
  }
}
