/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BLI_math_base.h"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"
#include "BLI_timecode.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "WM_api.hh"

#include "BLF_api.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

namespace blender::ui {

/* Compute display grid resolution
 ********************************************************/

#define MIN_MAJOR_LINE_DISTANCE (U.v2d_min_gridsize * UI_SCALE_FAC)

/* This number defines the smallest scale unit that will be displayed. For example 100 will give
 * 1/100 -> 0.01 as the smallest step. This is only relevant for editors that do display subframe
 * information, for example the Graph Editor. */
constexpr int subframe_range = 100;

/* This essentially performs a special prime factor decomposition where it can only use 2, 3 and 5
 * as prime factors. Divisions that result in 2 are preferred. */
static int get_divisor(const int distance)
{
  const int divisors[3] = {2, 3, 5};
  constexpr uint8_t num_divisors = ARRAY_SIZE(divisors);
  bool divides_no_remainder[num_divisors];

  for (int i = 0; i < num_divisors; i++) {
    const int divisor = divisors[i];
    const int result = distance / divisor;
    /* If the division was without loss due to integer cast and the result is 2, return
     * that. Animating on 2s is a very useful thing for animators so the lines should be shown with
     * that distance. */
    const bool has_no_remainder = result * divisor == distance;
    if (has_no_remainder && result == 2) {
      return divisor;
    }
    divides_no_remainder[i] = has_no_remainder;
  }

  /* If no division results in a 2, take the first to divide cleanly. */
  for (int i = 0; i < num_divisors; i++) {
    if (divides_no_remainder[i]) {
      return divisors[i];
    }
  }

  /* In case none of the above if is true, the divisor will be the full distance meaning the next
   * step down from that number is 1. */
  return distance;
}

/**
 * Calculates the distance in frames between major lines.
 * The lowest value it can return is 1.
 *
 * \param base: Defines how the step is calculated.
 * The returned step is either a full fraction or a multiple of that number.
 */
static int calculate_grid_step(const int base, const float pixel_width, const float view_width)
{
  if (IS_EQF(view_width, 0.0f) || base == 0) {
    return 1;
  }
  const float pixels_per_view_unit = pixel_width / view_width;
  int distance = base;
  if (pixels_per_view_unit * distance > MIN_MAJOR_LINE_DISTANCE) {
    /* Shrink the distance. */
    while (distance > 1) {
      const int divisor = get_divisor(distance);
      const int result = (distance / divisor);
      if (pixels_per_view_unit * result < MIN_MAJOR_LINE_DISTANCE) {
        /* If the distance would fall below the threshold, stop dividing. */
        break;
      }
      distance = result;
    }
  }
  else {
    /* Grow the distance, doubling every time. Break just before hitting an integer overflow. This
     * creates a drawing issue after hitting the limit where the numbers will overlap but that is
     * better than an endless loop. See #150543. */
    while (pixels_per_view_unit * distance < MIN_MAJOR_LINE_DISTANCE && distance < (1 << 30)) {
      distance *= 2;
    }
  }
  BLI_assert(distance != 0);
  return distance;
}

/* Mostly the same as `calculate_grid_step, except in can divide into the 0-1 range. */
static float calculate_grid_step_fractions(const int base,
                                           const float pixel_width,
                                           const float view_width)
{
  float distance = calculate_grid_step(base, pixel_width, view_width);
  if (distance > 1) {
    return distance;
  }

  /* Using `calculate_grid_step` to break down subframe_range simulating a larger view. */
  distance = calculate_grid_step(subframe_range, pixel_width, view_width * subframe_range);
  return distance / subframe_range;
}

/* Draw parallel lines
 ************************************/

/**
 * Calculate the amount of lines to draw and the starting position in view space (frame or value).
 *
 * \param line_distance value distance between lines.
 * \param view_bounds the value bounds visible in the region. x has to be lower than y.
 */
static void get_parallel_lines_draw_steps(const float line_distance,
                                          const float2 view_bounds,
                                          float *r_first,
                                          uint *r_steps)
{
  if (view_bounds.x >= view_bounds.y) {
    *r_first = 0;
    *r_steps = 0;
    return;
  }

  BLI_assert(line_distance > 0);

  *r_first = ceilf(view_bounds.x / line_distance) * line_distance;

  if (view_bounds.x <= *r_first && view_bounds.y >= *r_first) {
    *r_steps = std::max(0.0f, floorf((view_bounds.y - *r_first) / line_distance)) + 1;
  }
  else {
    *r_steps = 0;
  }
}

/**
 * \param rect_mask Region size in pixels.
 * \param line_distance Distance in view space (frame or value) between lines.
 */
static void draw_parallel_lines(const float line_distance,
                                const rctf *rect,
                                const rcti *rect_mask,
                                const uchar color[3],
                                const char direction)
{
  float first;
  uint steps, steps_max;

  if (direction == 'v') {
    get_parallel_lines_draw_steps(line_distance, {rect->xmin, rect->xmax}, &first, &steps);
    steps_max = BLI_rcti_size_x(rect_mask);
  }
  else {
    BLI_assert(direction == 'h');
    get_parallel_lines_draw_steps(line_distance, {rect->ymin, rect->ymax}, &first, &steps);
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
  const uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);

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
      const float xpos = first + i * line_distance;
      immVertex2f(pos, xpos, rect->ymin);
      immVertex2f(pos, xpos, rect->ymax);
    }
  }
  else {
    for (uint i = 0; i < steps; i++) {
      const float ypos = first + i * line_distance;
      immVertex2f(pos, rect->xmin, ypos);
      immVertex2f(pos, rect->xmax, ypos);
    }
  }

  immEnd();
  immUnbindProgram();
}

static void view2d_draw_lines_internal(const View2D *v2d,
                                       const float line_distance,
                                       const uchar color[3],
                                       char direction)
{
  GPU_matrix_push_projection();
  view2d_view_ortho(v2d);
  draw_parallel_lines(line_distance, &v2d->cur, &v2d->mask, color, direction);
  GPU_matrix_pop_projection();
}

static void view2d_draw_lines(const View2D *v2d,
                              const float major_distance,
                              const bool display_minor_lines,
                              const char direction)
{
  if (display_minor_lines) {
    uchar minor_color[3];
    theme::get_color_shade_3ubv(TH_GRID, 16, minor_color);
    int distance_int;
    if (major_distance > 1) {
      distance_int = round_fl_to_int(major_distance);
    }
    else {
      /* By multiplying by the subframe range, the smallest distance in which minor lines are drawn
       * is the same as the smallest distance between major lines. We can just do this
       * multiplication because from the result, the next divisor is found and applied to the
       * major distance. The returned divisor may be 1. */
      distance_int = round_fl_to_int(major_distance * subframe_range);
    }
    const int divisor = get_divisor(distance_int);
    const float line_distance = major_distance / divisor;
    const int pixel_width = BLI_rcti_size_x(&v2d->mask) + 1;
    const float view_width = BLI_rctf_size_x(&v2d->cur);

    if ((pixel_width / view_width) * (major_distance / divisor) > MIN_MAJOR_LINE_DISTANCE / 5) {
      view2d_draw_lines_internal(v2d, line_distance, minor_color, direction);
    }
  }

  {
    uchar major_color[3];
    theme::get_color_3ubv(TH_GRID, major_color);
    view2d_draw_lines_internal(v2d, major_distance, major_color, direction);
  }
}

/* Scale indicator text drawing
 **************************************************/

using PositionToString =
    void (*)(void *user_data, float value, float step, char *r_str, uint str_maxncpy);

/**
 * \param distance is the distance between lines in the data unit of the v2d (frame or value).
 */
static void draw_horizontal_scale_indicators(const ARegion *region,
                                             const View2D *v2d,
                                             const float distance,
                                             const rcti *rect,
                                             PositionToString to_string,
                                             void *to_string_data,
                                             const int colorid)
{
  if (view2d_scale_get_x(v2d) <= 0.0f) {
    return;
  }

  float start;
  uint steps;
  {
    get_parallel_lines_draw_steps(
        distance,
        {view2d_region_to_view_x(v2d, rect->xmin), view2d_region_to_view_x(v2d, rect->xmax)},
        &start,
        &steps);
    const uint steps_max = BLI_rcti_size_x(&v2d->mask) + 1;
    if (UNLIKELY(steps >= steps_max)) {
      return;
    }
  }

  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(region);

  const int font_id = BLF_set_default();
  theme::font_theme_color_set(font_id, colorid);

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
    const float max_label_count = (BLI_rcti_size_x(&v2d->mask) + 1) / (max_text_width + 6.0f);
    draw_frequency = ceil(float(steps) / max_label_count);
  }

  if (draw_frequency != 0) {
    const int start_index = abs(int(start / distance)) % draw_frequency;
    for (uint i = start_index; i < steps; i += draw_frequency) {
      const float xpos_view = start + i * distance;
      const float xpos_region = view2d_view_to_region_x(v2d, xpos_view);
      to_string(to_string_data, xpos_view, distance, text, sizeof(text));
      const float text_width = BLF_width(font_id, text, strlen(text));

      if (xpos_region - text_width / 2.0f >= xmin && xpos_region + text_width / 2.0f <= xmax) {
        BLF_draw_default(
            xpos_region - std::trunc(text_width / 2.0f), ypos, 0.0f, text, sizeof(text));
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
  if (view2d_scale_get_y(v2d) <= 0.0f) {
    return;
  }

  float start;
  uint steps;
  {
    get_parallel_lines_draw_steps(
        distance,
        {view2d_region_to_view_y(v2d, rect->ymin), view2d_region_to_view_y(v2d, rect->ymax)},
        &start,
        &steps);
    const uint steps_max = BLI_rcti_size_y(&v2d->mask) + 1;
    if (UNLIKELY(steps >= steps_max)) {
      return;
    }
  }

  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(region);

  const int font_id = BLF_set_default();
  theme::font_theme_color_set(font_id, colorid);

  BLF_batch_draw_begin();

  BLF_enable(font_id, BLF_SHADOW);
  float shadow_color[4];
  theme::get_color_4fv(TH_BACK, shadow_color);
  BLF_shadow_offset(font_id, 0, 0);
  BLF_shadow(font_id, FontShadowType::Outline, shadow_color);

  const float x_offset = 8.0f;
  const float xpos = (rect->xmin + x_offset) * UI_SCALE_FAC;
  const float ymin = rect->ymin;
  const float ymax = rect->ymax;
  const float y_offset = (BLF_height(font_id, "0", 1) / 2.0f) - U.pixelsize;

  for (uint i = 0; i < steps; i++) {
    const float ypos_view = start + i * distance;
    const float ypos_region = view2d_view_to_region_y(v2d, ypos_view + display_offset);
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

/**
 * Generates a timecode string with the time represented by `frame`.
 * The timecode formatting depends on the user preferences.
 *
 * \param user_data has to be a `Scene *` so we can get the frames per second.
 */
static void frame_to_time_string(
    void *user_data, const float frame, const float step, char *r_str, const uint str_maxncpy)
{
  const Scene *scene = static_cast<const Scene *>(user_data);

  int brevity_level = -1;
  if (U.timecode_style == USER_TIMECODE_MINIMAL && step >= scene->frames_per_second()) {
    brevity_level = 1;
  }

  BLI_timecode_string_from_time(r_str,
                                str_maxncpy,
                                brevity_level,
                                frame / float(scene->frames_per_second()),
                                scene->frames_per_second(),
                                U.timecode_style);
}

/**
 * Generates a string based on the given `frame`.
 * Depending on `step` the returned string has 0 - 3 decimal positions.
 */
static void frame_to_string(
    void * /*user_data*/, const float frame, const float step, char *r_str, const uint str_maxncpy)
{
  if (step >= 1.0f) {
    BLI_snprintf_utf8(r_str, str_maxncpy, "%d", int(frame));
  }
  else if (step >= 0.5f) {
    BLI_snprintf_utf8(r_str, str_maxncpy, "%.1f", frame);
  }
  else if (step >= 0.01f) {
    BLI_snprintf_utf8(r_str, str_maxncpy, "%.2f", frame);
  }
  else {
    BLI_snprintf_utf8(r_str, str_maxncpy, "%.3f", frame);
  }
}

/* Grid Resolution API
 **************************************************/

float view2d_grid_resolution_x__frames_or_seconds(const View2D *v2d, const Scene *scene)
{
  const int fps = round_db_to_int(scene->frames_per_second());
  return calculate_grid_step_fractions(
      fps, BLI_rcti_size_x(&v2d->mask) + 1, BLI_rctf_size_x(&v2d->cur));
}

float view2d_grid_resolution_y__values(const View2D *v2d, const int base)
{
  return calculate_grid_step_fractions(
      base, BLI_rcti_size_y(&v2d->mask) + 1, BLI_rctf_size_y(&v2d->cur));
}

/* Line Drawing API
 **************************************************/

void view2d_draw_lines_x(const View2D *v2d,
                         const bool display_seconds,
                         const bool show_fractions,
                         bool draw_minor_lines,
                         const int base)
{
  float major_line_distance;
  /* Fractions are only drawn when not showing a timecode. See `view2d_draw_scale_x`. */
  if (show_fractions && !display_seconds) {
    major_line_distance = calculate_grid_step_fractions(
        base, BLI_rcti_size_x(&v2d->mask) + 1, BLI_rctf_size_x(&v2d->cur));
  }
  else {
    major_line_distance = calculate_grid_step(
        base, BLI_rcti_size_x(&v2d->mask) + 1, BLI_rctf_size_x(&v2d->cur));
    draw_minor_lines = draw_minor_lines && (major_line_distance > 1);
  }
  /* The extra check for minor line drawing here is so minor lines are *not* drawn
   * below a distance of 1. */
  view2d_draw_lines(v2d, major_line_distance, draw_minor_lines, 'v');
}

void view2d_draw_lines_x_frames(const View2D *v2d,
                                const Scene *scene,
                                const bool display_seconds,
                                const bool show_fractions,
                                const bool draw_minor_lines)
{
  const int fps = round_db_to_int(scene->frames_per_second());
  view2d_draw_lines_x(v2d, display_seconds, show_fractions, draw_minor_lines, fps);
}

void view2d_draw_lines_y(const View2D *v2d, const bool show_fractions, const int base)
{
  float major_line_distance;
  if (show_fractions) {
    major_line_distance = calculate_grid_step_fractions(
        base, BLI_rcti_size_y(&v2d->mask) + 1, BLI_rctf_size_y(&v2d->cur));
  }
  else {
    major_line_distance = calculate_grid_step(
        base, BLI_rcti_size_y(&v2d->mask) + 1, BLI_rctf_size_y(&v2d->cur));
  }
  view2d_draw_lines(v2d, major_line_distance, true, 'h');
}

/* Scale indicator text drawing API
 **************************************************/

void view2d_draw_scale_y(
    const ARegion *region, const View2D *v2d, const rcti *rect, const int colorid, const int base)
{
  const float step = calculate_grid_step_fractions(
      base, BLI_rcti_size_y(&v2d->mask) + 1, BLI_rctf_size_y(&v2d->cur));
  draw_vertical_scale_indicators(region, v2d, step, 0.0f, rect, frame_to_string, nullptr, colorid);
}

void view2d_draw_scale_x(const ARegion *region,
                         const View2D *v2d,
                         const rcti *rect,
                         const Scene *scene,
                         const bool display_seconds,
                         const bool show_fractions,
                         const int colorid,
                         const int base)
{
  float step;
  /* The timecode string does not change on fractions of a frame so it makes no sense to display
   * that. */
  if (show_fractions && !display_seconds) {
    step = calculate_grid_step_fractions(
        base, BLI_rcti_size_x(&v2d->mask) + 1, BLI_rctf_size_x(&v2d->cur));
  }
  else {
    step = calculate_grid_step(base, BLI_rcti_size_x(&v2d->mask) + 1, BLI_rctf_size_x(&v2d->cur));
  }
  if (display_seconds) {
    draw_horizontal_scale_indicators(
        region, v2d, step, rect, frame_to_time_string, (void *)scene, colorid);
  }
  else {
    draw_horizontal_scale_indicators(region, v2d, step, rect, frame_to_string, nullptr, colorid);
  }
}

}  // namespace blender::ui
