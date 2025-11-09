/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "BLI_string_utf8.h"

#include "DNA_sequence_types.h"

#include "BKE_context.hh"
#include "BKE_fcurve.hh"

#include "BLF_api.hh"

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"
#include "GPU_vertex_format.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_keyframes_draw.hh"
#include "ED_keyframes_keylist.hh"
#include "ED_screen.hh"

#include "UI_view2d.hh"

#include "SEQ_retiming.hh"
#include "SEQ_time.hh"

/* Own include. */
#include "sequencer_intern.hh"
#include "sequencer_quads_batch.hh"

namespace blender::ed::vse {

#define KEY_SIZE (10 * U.pixelsize)
#define KEY_CENTER \
  (UI_view2d_view_to_region_y(v2d, strip_y_rescale(strip, 0.0f)) + 4 + KEY_SIZE / 2)

bool retiming_keys_can_be_displayed(const SpaceSeq *sseq)
{
  return (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_RETIMING) &&
         (sseq->flag & SEQ_SHOW_OVERLAY);
}

static float strip_y_rescale(const Strip *strip, const float y_value)
{
  const float y_range = STRIP_OFSTOP - STRIP_OFSBOTTOM;
  return (y_value * y_range) + strip->channel + STRIP_OFSBOTTOM;
}

static float key_x_get(const Scene *scene, const Strip *strip, const SeqRetimingKey *key)
{
  if (seq::retiming_is_last_key(strip, key)) {
    return seq::retiming_key_timeline_frame_get(scene, strip, key) + 1;
  }
  return seq::retiming_key_timeline_frame_get(scene, strip, key);
}

static float pixels_to_view_width(const bContext *C, const float width)
{
  const View2D *v2d = UI_view2d_fromcontext(C);
  float scale_x = UI_view2d_view_to_region_x(v2d, 1) - UI_view2d_view_to_region_x(v2d, 0.0f);
  return width / scale_x;
}

static float pixels_to_view_height(const bContext *C, const float height)
{
  const View2D *v2d = UI_view2d_fromcontext(C);
  float scale_y = UI_view2d_view_to_region_y(v2d, 1) - UI_view2d_view_to_region_y(v2d, 0.0f);
  return height / scale_y;
}

static float strip_start_screenspace_get(const Scene *scene, const View2D *v2d, const Strip *strip)
{
  return UI_view2d_view_to_region_x(v2d, seq::time_left_handle_frame_get(scene, strip));
}

static float strip_end_screenspace_get(const Scene *scene, const View2D *v2d, const Strip *strip)
{
  return UI_view2d_view_to_region_x(v2d, seq::time_right_handle_frame_get(scene, strip));
}

static rctf strip_box_get(const Scene *scene, const View2D *v2d, const Strip *strip)
{
  rctf rect;
  rect.xmin = strip_start_screenspace_get(scene, v2d, strip);
  rect.xmax = strip_end_screenspace_get(scene, v2d, strip);
  rect.ymin = UI_view2d_view_to_region_y(v2d, strip_y_rescale(strip, 0));
  rect.ymax = UI_view2d_view_to_region_y(v2d, strip_y_rescale(strip, 1));
  return rect;
}

/** Size in pixels. */
#define RETIME_KEY_MOUSEOVER_THRESHOLD (16.0f * UI_SCALE_FAC)

rctf strip_retiming_keys_box_get(const Scene *scene, const View2D *v2d, const Strip *strip)
{
  rctf rect = strip_box_get(scene, v2d, strip);
  rect.ymax = KEY_CENTER + KEY_SIZE / 2;
  rect.ymin = KEY_CENTER - KEY_SIZE / 2;
  return rect;
}

int left_fake_key_frame_get(const bContext *C, const Strip *strip)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  const int sound_offset = seq::time_get_rounded_sound_offset(strip, scene_fps);
  const int content_start = seq::time_start_frame_get(strip) + sound_offset;
  return max_ii(content_start, seq::time_left_handle_frame_get(scene, strip));
}

int right_fake_key_frame_get(const bContext *C, const Strip *strip)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  const int sound_offset = seq::time_get_rounded_sound_offset(strip, scene_fps);
  const int content_end = seq::time_content_end_frame_get(scene, strip) - 1 + sound_offset;
  return min_ii(content_end, seq::time_right_handle_frame_get(scene, strip));
}

static bool retiming_fake_key_frame_clicked(const bContext *C,
                                            const Strip *strip,
                                            const int mval[2],
                                            int &r_frame)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  const View2D *v2d = UI_view2d_fromcontext(C);

  rctf box = strip_retiming_keys_box_get(scene, v2d, strip);
  if (!BLI_rctf_isect_pt(&box, mval[0], mval[1])) {
    return false;
  }

  const int left_frame = left_fake_key_frame_get(C, strip);
  const float left_distance = fabs(UI_view2d_view_to_region_x(v2d, left_frame) - mval[0]);

  const int right_frame = right_fake_key_frame_get(C, strip);
  int right_x = right_frame;
  /* `key_x_get()` compensates 1 frame offset of last key, however this can not
   * be conveyed via `fake_key` alone. Therefore the same offset must be emulated. */
  if (seq::time_right_handle_frame_get(scene, strip) >=
      seq::time_content_end_frame_get(scene, strip))
  {
    right_x += 1;
  }
  const float right_distance = fabs(UI_view2d_view_to_region_x(v2d, right_x) - mval[0]);

  r_frame = (left_distance < right_distance) ? left_frame : right_frame;

  /* Fake key threshold is doubled to make them easier to select. */
  return min_ff(left_distance, right_distance) < RETIME_KEY_MOUSEOVER_THRESHOLD * 2;
}

void realize_fake_keys(const Scene *scene, Strip *strip)
{
  seq::retiming_data_ensure(strip);
  seq::retiming_add_key(scene, strip, seq::time_left_handle_frame_get(scene, strip));
  seq::retiming_add_key(scene, strip, seq::time_right_handle_frame_get(scene, strip));
}

SeqRetimingKey *try_to_realize_fake_keys(const bContext *C, Strip *strip, const int mval[2])
{
  Scene *scene = CTX_data_sequencer_scene(C);
  SeqRetimingKey *key = nullptr;

  int key_frame;
  if (retiming_fake_key_frame_clicked(C, strip, mval, key_frame)) {
    realize_fake_keys(scene, strip);
    key = seq::retiming_key_get_by_timeline_frame(scene, strip, key_frame);
  }
  return key;
}

static SeqRetimingKey *mouse_over_key_get_from_strip(const bContext *C,
                                                     const Strip *strip,
                                                     const int mval[2])
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  const View2D *v2d = UI_view2d_fromcontext(C);

  int best_distance = INT_MAX;
  SeqRetimingKey *best_key = nullptr;

  for (SeqRetimingKey &key : seq::retiming_keys_get(strip)) {
    int distance = round_fl_to_int(
        fabsf(UI_view2d_view_to_region_x(v2d, key_x_get(scene, strip, &key)) - mval[0]));

    int threshold = RETIME_KEY_MOUSEOVER_THRESHOLD;
    if (key_x_get(scene, strip, &key) == seq::time_left_handle_frame_get(scene, strip) ||
        key_x_get(scene, strip, &key) == seq::time_right_handle_frame_get(scene, strip))
    {
      threshold *= 2; /* Make first and last key easier to select. */
    }

    if (distance < threshold && distance < best_distance) {
      best_distance = distance;
      best_key = &key;
    }
  }

  return best_key;
}

SeqRetimingKey *retiming_mouseover_key_get(const bContext *C, const int mval[2], Strip **r_strip)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  const View2D *v2d = UI_view2d_fromcontext(C);
  for (Strip *strip : sequencer_visible_strips_get(C)) {
    if (!seq::retiming_data_is_editable(strip)) {
      continue;
    }

    rctf box = strip_retiming_keys_box_get(scene, v2d, strip);
    if (!BLI_rctf_isect_pt(&box, mval[0], mval[1])) {
      continue;
    }

    if (r_strip != nullptr) {
      *r_strip = strip;
    }

    SeqRetimingKey *key = mouse_over_key_get_from_strip(C, strip, mval);

    if (key == nullptr) {
      continue;
    }

    return key;
  }

  return nullptr;
}

static bool can_draw_retiming(const TimelineDrawContext &ctx, const StripDrawContext &strip_ctx)
{
  if (ctx.ed == nullptr) {
    return false;
  }

  if (!retiming_keys_can_be_displayed(ctx.sseq)) {
    return false;
  }

  if (!seq::retiming_is_allowed(strip_ctx.strip)) {
    return false;
  }

  if (!strip_ctx.can_draw_retiming_overlay) {
    return false;
  }

  return true;
}

/* -------------------------------------------------------------------- */
/** \name Retiming Key
 * \{ */

static void retime_key_draw(const TimelineDrawContext &ctx,
                            const StripDrawContext &strip_ctx,
                            const SeqRetimingKey *key,
                            const KeyframeShaderBindings &sh_bindings)
{
  const Scene *scene = ctx.scene;
  const View2D *v2d = ctx.v2d;
  Strip *strip = strip_ctx.strip;

  const float key_x = key_x_get(scene, strip, key);
  const rctf strip_box = strip_box_get(scene, v2d, strip);
  if (!BLI_rctf_isect_x(&strip_box, UI_view2d_view_to_region_x(v2d, key_x))) {
    return; /* Key out of the strip bounds. */
  }

  eBezTriple_KeyframeType key_type = BEZT_KEYTYPE_KEYFRAME;
  if (seq::retiming_key_is_freeze_frame(key)) {
    key_type = BEZT_KEYTYPE_BREAKDOWN;
  }
  if (seq::retiming_key_is_transition_type(key)) {
    key_type = BEZT_KEYTYPE_MOVEHOLD;
  }

  const bool is_selected = ctx.retiming_selection.contains(const_cast<SeqRetimingKey *>(key));
  const int size = KEY_SIZE;
  const float bottom = KEY_CENTER;

  /* Ensure, that key is always inside of strip. */
  const float right_pos_max = UI_view2d_view_to_region_x(v2d, strip_ctx.right_handle) - (size / 2);
  const float left_pos_min = UI_view2d_view_to_region_x(v2d, strip_ctx.left_handle) + (size / 2);
  float key_position = UI_view2d_view_to_region_x(v2d, key_x);
  CLAMP(key_position, left_pos_min, right_pos_max);
  const float alpha = seq::retiming_data_is_editable(strip) ? 1.0f : 0.3f;

  draw_keyframe_shape(key_position,
                      bottom,
                      size,
                      is_selected && seq::retiming_data_is_editable(strip),
                      key_type,
                      KEYFRAME_SHAPE_BOTH,
                      alpha,
                      &sh_bindings,
                      0,
                      0);
}

void sequencer_retiming_draw_continuity(const TimelineDrawContext &ctx,
                                        const StripDrawContext &strip_ctx)
{
  if (!can_draw_retiming(ctx, strip_ctx) || seq::retiming_keys_count(strip_ctx.strip) == 0) {
    return;
  }

  const Strip *strip = strip_ctx.strip;
  const View2D *v2d = ctx.v2d;
  const Scene *scene = ctx.scene;
  const float left_handle_position = UI_view2d_view_to_region_x(v2d, strip_ctx.left_handle);
  const float right_handle_position = UI_view2d_view_to_region_x(v2d, strip_ctx.right_handle);

  for (const SeqRetimingKey &key : seq::retiming_keys_get(strip)) {
    if (key_x_get(scene, strip, &key) == strip_ctx.left_handle || key.strip_frame_index == 0) {
      continue;
    }

    float key_position = UI_view2d_view_to_region_x(v2d, key_x_get(scene, strip, &key));
    float prev_key_position = UI_view2d_view_to_region_x(v2d, key_x_get(scene, strip, &key - 1));
    if (prev_key_position > right_handle_position || key_position < left_handle_position) {
      /* Don't draw highlights for out of bounds retiming keys. */
      continue;
    }
    prev_key_position = max_ff(prev_key_position, left_handle_position);
    key_position = min_ff(key_position, right_handle_position);

    const int size = KEY_SIZE;
    const float y_center = KEY_CENTER;

    const float width_fac = 0.5f;
    const float bottom = y_center - size * width_fac;
    const float top = y_center + size * width_fac;

    uchar color[4];
    if (seq::retiming_data_is_editable(strip) &&
        (ctx.retiming_selection.contains(const_cast<SeqRetimingKey *>(&key)) ||
         ctx.retiming_selection.contains(const_cast<SeqRetimingKey *>(&key - 1))))
    {
      color[0] = 166;
      color[1] = 127;
      color[2] = 51;
      color[3] = 255;
    }
    else {
      color[0] = 0;
      color[1] = 0;
      color[2] = 0;
      color[3] = 25;
    }
    ctx.quads->add_quad(prev_key_position, bottom, key_position, top, color);
  }
}

static SeqRetimingKey fake_retiming_key_init(const Scene *scene, const Strip *strip, int key_x)
{
  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  const int sound_offset = seq::time_get_rounded_sound_offset(strip, scene_fps);
  SeqRetimingKey fake_key = {0};
  fake_key.strip_frame_index = (key_x - seq::time_start_frame_get(strip) - sound_offset) *
                               seq::time_media_playback_rate_factor_get(strip, scene_fps);
  fake_key.flag = 0;
  return fake_key;
}

/* If there are no keys, draw fake keys and create real key when they are selected. */
/* TODO: would be nice to draw continuity between fake keys. */
static bool fake_keys_draw(const TimelineDrawContext &ctx,
                           const StripDrawContext &strip_ctx,
                           const KeyframeShaderBindings &sh_bindings)
{
  const Strip *strip = strip_ctx.strip;
  const Scene *scene = ctx.scene;

  if (!seq::retiming_is_active(strip) && !seq::retiming_data_is_editable(strip)) {
    return false;
  }

  const int left_key_frame = left_fake_key_frame_get(ctx.C, strip);
  if (seq::retiming_key_get_by_timeline_frame(scene, strip, left_key_frame) == nullptr) {
    SeqRetimingKey fake_key = fake_retiming_key_init(scene, strip, left_key_frame);
    retime_key_draw(ctx, strip_ctx, &fake_key, sh_bindings);
  }

  int right_key_frame = right_fake_key_frame_get(ctx.C, strip);
  if (seq::retiming_key_get_by_timeline_frame(scene, strip, right_key_frame) == nullptr) {
    /* `key_x_get()` compensates 1 frame offset of last key, however this can not
     * be conveyed via `fake_key` alone. Therefore the same offset must be emulated. */
    if (strip_ctx.right_handle >= seq::time_content_end_frame_get(scene, strip)) {
      right_key_frame += 1;
    }
    SeqRetimingKey fake_key = fake_retiming_key_init(scene, strip, right_key_frame);
    retime_key_draw(ctx, strip_ctx, &fake_key, sh_bindings);
  }
  return true;
}

void sequencer_retiming_keys_draw(const TimelineDrawContext &ctx, Span<StripDrawContext> strips)
{
  if (strips.is_empty()) {
    return;
  }
  if (ctx.ed == nullptr || !retiming_keys_can_be_displayed(ctx.sseq)) {
    return;
  }

  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(ctx.region);

  const View2D *v2d = ctx.v2d;

  GPUVertFormat *format = immVertexFormat();
  KeyframeShaderBindings sh_bindings;
  sh_bindings.pos_id = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  sh_bindings.size_id = GPU_vertformat_attr_add(format, "size", gpu::VertAttrType::SFLOAT_32);
  sh_bindings.color_id = GPU_vertformat_attr_add(
      format, "color", gpu::VertAttrType::UNORM_8_8_8_8);
  sh_bindings.outline_color_id = GPU_vertformat_attr_add(
      format, "outlineColor", gpu::VertAttrType::UNORM_8_8_8_8);
  sh_bindings.flags_id = GPU_vertformat_attr_add(format, "flags", gpu::VertAttrType::UINT_32);

  GPU_program_point_size(true);
  immBindBuiltinProgram(GPU_SHADER_KEYFRAME_SHAPE);
  immUniform1f("outline_scale", 1.0f);
  immUniform2f("ViewportSize", BLI_rcti_size_x(&v2d->mask) + 1, BLI_rcti_size_y(&v2d->mask) + 1);

  constexpr int MAX_KEYS_IN_BATCH = 1024;
  int point_counter = 0;
  immBeginAtMost(GPU_PRIM_POINTS, MAX_KEYS_IN_BATCH);

  for (const StripDrawContext &strip_ctx : strips) {
    if (!can_draw_retiming(ctx, strip_ctx)) {
      continue;
    }
    if (fake_keys_draw(ctx, strip_ctx, sh_bindings)) {
      point_counter += 2;
    }

    for (const SeqRetimingKey &key : seq::retiming_keys_get(strip_ctx.strip)) {
      retime_key_draw(ctx, strip_ctx, &key, sh_bindings);
      point_counter++;

      /* Next key plus possible two fake keys for next sequence would need at
       * most 3 points, so restart the batch if we're close to that. */
      if (point_counter + 3 >= MAX_KEYS_IN_BATCH) {
        immEnd();
        immBeginAtMost(GPU_PRIM_POINTS, MAX_KEYS_IN_BATCH);
        point_counter = 0;
      }
    }
  }

  immEnd();
  GPU_program_point_size(false);
  immUnbindProgram();

  GPU_matrix_pop_projection();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Retiming Speed Label
 * \{ */

static size_t label_str_get(const Strip *strip,
                            const SeqRetimingKey *key,
                            char *r_label_str,
                            const size_t label_str_maxncpy)
{
  const SeqRetimingKey *next_key = key + 1;
  if (seq::retiming_key_is_transition_start(key)) {
    const float prev_speed = seq::retiming_key_speed_get(strip, key);
    const float next_speed = seq::retiming_key_speed_get(strip, next_key + 1);
    return BLI_snprintf_utf8_rlen(r_label_str,
                                  label_str_maxncpy,
                                  "%d%% - %d%%",
                                  round_fl_to_int(prev_speed * 100.0f),
                                  round_fl_to_int(next_speed * 100.0f));
  }
  const float speed = seq::retiming_key_speed_get(strip, next_key);
  return BLI_snprintf_utf8_rlen(
      r_label_str, label_str_maxncpy, "%d%%", round_fl_to_int(speed * 100.0f));
}

static bool label_rect_get(const TimelineDrawContext &ctx,
                           const StripDrawContext &strip_ctx,
                           const SeqRetimingKey *key,
                           const char *label_str,
                           const size_t label_len,
                           rctf *rect)
{
  const bContext *C = ctx.C;
  const Scene *scene = ctx.scene;
  const SeqRetimingKey *next_key = key + 1;
  const float width = pixels_to_view_width(C, BLF_width(BLF_default(), label_str, label_len));
  const float height = pixels_to_view_height(C, BLF_height(BLF_default(), label_str, label_len));
  const float xmin = max_ff(strip_ctx.left_handle, key_x_get(scene, strip_ctx.strip, key));
  const float xmax = min_ff(strip_ctx.right_handle, key_x_get(scene, strip_ctx.strip, next_key));

  rect->xmin = (xmin + xmax - width) / 2;
  rect->xmax = rect->xmin + width;
  rect->ymin = strip_y_rescale(strip_ctx.strip, 0) + pixels_to_view_height(C, 5);
  rect->ymax = rect->ymin + height;

  return width < xmax - xmin - pixels_to_view_width(C, KEY_SIZE);
}

static void retime_speed_text_draw(const TimelineDrawContext &ctx,
                                   const StripDrawContext &strip_ctx,
                                   const SeqRetimingKey *key)
{
  const Strip *strip = strip_ctx.strip;
  const Scene *scene = ctx.scene;

  if (seq::retiming_is_last_key(strip, key)) {
    return;
  }

  const SeqRetimingKey *next_key = key + 1;
  if (key_x_get(scene, strip, next_key) < strip_ctx.left_handle ||
      key_x_get(scene, strip, key) > strip_ctx.right_handle)
  {
    return; /* Label out of strip bounds. */
  }

  char label_str[40];
  rctf label_rect;
  size_t label_len = label_str_get(strip, key, label_str, sizeof(label_str));

  if (!label_rect_get(ctx, strip_ctx, key, label_str, label_len, &label_rect)) {
    return; /* Not enough space to draw the label. */
  }

  uchar col[4] = {255, 255, 255, 255};
  if ((strip->flag & SELECT) == 0) {
    memset(col, 0, sizeof(col));
    col[3] = 255;
  }

  UI_view2d_text_cache_add(ctx.v2d, label_rect.xmin, label_rect.ymin, label_str, label_len, col);
}

void sequencer_retiming_speed_draw(const TimelineDrawContext &ctx,
                                   const StripDrawContext &strip_ctx)
{
  if (!can_draw_retiming(ctx, strip_ctx)) {
    return;
  }

  for (const SeqRetimingKey &key : seq::retiming_keys_get(strip_ctx.strip)) {
    retime_speed_text_draw(ctx, strip_ctx, &key);
  }

  UI_view2d_view_ortho(ctx.v2d);
}

/** \} */

}  // namespace blender::ed::vse
