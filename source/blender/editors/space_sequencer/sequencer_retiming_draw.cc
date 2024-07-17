/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_span.hh"

#include "DNA_sequence_types.h"

#include "BKE_context.hh"
#include "BKE_fcurve.hh"

#include "BLF_api.hh"

#include "GPU_batch.hh"
#include "GPU_state.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_keyframes_draw.hh"
#include "ED_keyframes_keylist.hh"
#include "ED_screen.hh"

#include "UI_view2d.hh"

#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"

/* Own include. */
#include "sequencer_intern.hh"
#include "sequencer_quads_batch.hh"

#define KEY_SIZE (10 * U.pixelsize)
#define KEY_CENTER (UI_view2d_view_to_region_y(v2d, strip_y_rescale(seq, 0.0f)) + 4 + KEY_SIZE / 2)

bool retiming_keys_are_visible(const SpaceSeq *sseq)
{
  return (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_RETIMING) != 0;
}

static float strip_y_rescale(const Sequence *seq, const float y_value)
{
  const float y_range = SEQ_STRIP_OFSTOP - SEQ_STRIP_OFSBOTTOM;
  return (y_value * y_range) + seq->machine + SEQ_STRIP_OFSBOTTOM;
}

static float key_x_get(const Scene *scene, const Sequence *seq, const SeqRetimingKey *key)
{
  if (SEQ_retiming_is_last_key(seq, key)) {
    return SEQ_retiming_key_timeline_frame_get(scene, seq, key) + 1;
  }
  return SEQ_retiming_key_timeline_frame_get(scene, seq, key);
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

static float strip_start_screenspace_get(const Scene *scene,
                                         const View2D *v2d,
                                         const Sequence *seq)
{
  return UI_view2d_view_to_region_x(v2d, SEQ_time_left_handle_frame_get(scene, seq));
}

static float strip_end_screenspace_get(const Scene *scene, const View2D *v2d, const Sequence *seq)
{
  return UI_view2d_view_to_region_x(v2d, SEQ_time_right_handle_frame_get(scene, seq));
}

static rctf strip_box_get(const Scene *scene, const View2D *v2d, const Sequence *seq)
{
  rctf rect;
  rect.xmin = strip_start_screenspace_get(scene, v2d, seq);
  rect.xmax = strip_end_screenspace_get(scene, v2d, seq);
  rect.ymin = UI_view2d_view_to_region_y(v2d, strip_y_rescale(seq, 0));
  rect.ymax = UI_view2d_view_to_region_y(v2d, strip_y_rescale(seq, 1));
  return rect;
}

/** Size in pixels. */
#define RETIME_KEY_MOUSEOVER_THRESHOLD (16.0f * UI_SCALE_FAC)

rctf seq_retiming_keys_box_get(const Scene *scene, const View2D *v2d, const Sequence *seq)
{
  rctf rect = strip_box_get(scene, v2d, seq);
  rect.ymax = KEY_CENTER + KEY_SIZE / 2;
  rect.ymin = KEY_CENTER - KEY_SIZE / 2;
  return rect;
}

int left_fake_key_frame_get(const bContext *C, const Sequence *seq)
{
  const Scene *scene = CTX_data_scene(C);
  int sound_offset = SEQ_time_get_rounded_sound_offset(scene, seq);
  const int content_start = SEQ_time_start_frame_get(seq) + sound_offset;
  return max_ii(content_start, SEQ_time_left_handle_frame_get(scene, seq));
}

int right_fake_key_frame_get(const bContext *C, const Sequence *seq)
{
  const Scene *scene = CTX_data_scene(C);
  int sound_offset = SEQ_time_get_rounded_sound_offset(scene, seq);
  const int content_end = SEQ_time_content_end_frame_get(scene, seq) - 1 + sound_offset;
  return min_ii(content_end, SEQ_time_right_handle_frame_get(scene, seq));
}

static bool retiming_fake_key_is_clicked(const bContext *C,
                                         const Sequence *seq,
                                         const int key_timeline_frame,
                                         const int mval[2])
{
  const View2D *v2d = UI_view2d_fromcontext(C);

  rctf box = seq_retiming_keys_box_get(CTX_data_scene(C), v2d, seq);
  if (!BLI_rctf_isect_pt(&box, mval[0], mval[1])) {
    return false;
  }

  const float key_pos = UI_view2d_view_to_region_x(v2d, key_timeline_frame);
  const float distance = fabs(key_pos - mval[0]);
  return distance < RETIME_KEY_MOUSEOVER_THRESHOLD;
}

SeqRetimingKey *try_to_realize_virtual_keys(const bContext *C, Sequence *seq, const int mval[2])
{
  Scene *scene = CTX_data_scene(C);
  SeqRetimingKey *key = nullptr;

  if (retiming_fake_key_is_clicked(C, seq, left_fake_key_frame_get(C, seq), mval)) {
    SEQ_retiming_data_ensure(seq);
    int frame = SEQ_time_left_handle_frame_get(scene, seq);
    key = SEQ_retiming_add_key(scene, seq, frame);
  }

  int right_key_frame = right_fake_key_frame_get(C, seq);
  /* `key_x_get()` compensates 1 frame offset of last key, however this can not
   * be conveyed via `fake_key` alone. Therefore the same offset must be emulated. */
  if (SEQ_time_right_handle_frame_get(scene, seq) >= SEQ_time_content_end_frame_get(scene, seq)) {
    right_key_frame += 1;
  }
  if (retiming_fake_key_is_clicked(C, seq, right_key_frame, mval)) {
    SEQ_retiming_data_ensure(seq);
    const int frame = SEQ_time_right_handle_frame_get(scene, seq);
    key = SEQ_retiming_add_key(scene, seq, frame);
  }

  /* Ensure both keys are realized so we only change the speed of what is visible in the strip,
   * but return only the one that was clicked on. */
  if (key != nullptr) {
    SEQ_retiming_add_key(scene, seq, SEQ_time_right_handle_frame_get(scene, seq));
    SEQ_retiming_add_key(scene, seq, SEQ_time_left_handle_frame_get(scene, seq));
  }

  return key;
}

static SeqRetimingKey *mouse_over_key_get_from_strip(const bContext *C,
                                                     const Sequence *seq,
                                                     const int mval[2])
{
  const Scene *scene = CTX_data_scene(C);
  const View2D *v2d = UI_view2d_fromcontext(C);

  int best_distance = INT_MAX;
  SeqRetimingKey *best_key = nullptr;

  for (SeqRetimingKey &key : SEQ_retiming_keys_get(seq)) {
    int distance = round_fl_to_int(
        fabsf(UI_view2d_view_to_region_x(v2d, key_x_get(scene, seq, &key)) - mval[0]));

    int threshold = RETIME_KEY_MOUSEOVER_THRESHOLD;
    if (key_x_get(scene, seq, &key) == SEQ_time_left_handle_frame_get(scene, seq) ||
        key_x_get(scene, seq, &key) == SEQ_time_right_handle_frame_get(scene, seq))
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

SeqRetimingKey *retiming_mousover_key_get(const bContext *C, const int mval[2], Sequence **r_seq)
{
  const Scene *scene = CTX_data_scene(C);
  const View2D *v2d = UI_view2d_fromcontext(C);
  for (Sequence *seq : sequencer_visible_strips_get(C)) {
    rctf box = seq_retiming_keys_box_get(scene, v2d, seq);
    if (!BLI_rctf_isect_pt(&box, mval[0], mval[1])) {
      continue;
    }

    if (r_seq != nullptr) {
      *r_seq = seq;
    }

    SeqRetimingKey *key = mouse_over_key_get_from_strip(C, seq, mval);

    if (key == nullptr) {
      continue;
    }

    return key;
  }

  return nullptr;
}

static bool can_draw_retiming(const TimelineDrawContext *timeline_ctx,
                              const StripDrawContext &strip_ctx)
{
  if (timeline_ctx->ed == nullptr) {
    return false;
  }

  if (!retiming_keys_are_visible(timeline_ctx->sseq)) {
    return false;
  }

  if (!SEQ_retiming_is_allowed(strip_ctx.seq)) {
    return false;
  }

  if (SEQ_retiming_keys_count(strip_ctx.seq) == 0) {
    return false;
  }

  return true;
}

/* -------------------------------------------------------------------- */
/** \name Retiming Key
 * \{ */

static void retime_key_draw(const TimelineDrawContext *timeline_ctx,
                            const StripDrawContext &strip_ctx,
                            const SeqRetimingKey *key,
                            const KeyframeShaderBindings &sh_bindings)
{
  const Scene *scene = timeline_ctx->scene;
  const View2D *v2d = timeline_ctx->v2d;
  Sequence *seq = strip_ctx.seq;

  const float key_x = key_x_get(scene, seq, key);
  const rctf strip_box = strip_box_get(scene, v2d, seq);
  if (!BLI_rctf_isect_x(&strip_box, UI_view2d_view_to_region_x(v2d, key_x))) {
    return; /* Key out of the strip bounds. */
  }

  eBezTriple_KeyframeType key_type = BEZT_KEYTYPE_KEYFRAME;
  if (SEQ_retiming_key_is_freeze_frame(key)) {
    key_type = BEZT_KEYTYPE_BREAKDOWN;
  }
  if (SEQ_retiming_key_is_transition_type(key)) {
    key_type = BEZT_KEYTYPE_MOVEHOLD;
  }

  const bool is_selected = timeline_ctx->retiming_selection.contains(
      const_cast<SeqRetimingKey *>(key));
  const int size = KEY_SIZE;
  const float bottom = KEY_CENTER;

  /* Ensure, that key is always inside of strip. */
  const float right_pos_max = UI_view2d_view_to_region_x(v2d, strip_ctx.right_handle) - (size / 2);
  const float left_pos_min = UI_view2d_view_to_region_x(v2d, strip_ctx.left_handle) + (size / 2);
  float key_position = UI_view2d_view_to_region_x(v2d, key_x);
  CLAMP(key_position, left_pos_min, right_pos_max);
  const float alpha = SEQ_retiming_data_is_editable(seq) ? 1.0f : 0.3f;

  draw_keyframe_shape(key_position,
                      bottom,
                      size,
                      is_selected && SEQ_retiming_data_is_editable(seq),
                      key_type,
                      KEYFRAME_SHAPE_BOTH,
                      alpha,
                      &sh_bindings,
                      0,
                      0);
}

void sequencer_retiming_draw_continuity(const TimelineDrawContext *timeline_ctx,
                                        const StripDrawContext &strip_ctx)
{
  if (!can_draw_retiming(timeline_ctx, strip_ctx)) {
    return;
  }

  const Sequence *seq = strip_ctx.seq;
  const View2D *v2d = timeline_ctx->v2d;
  const Scene *scene = timeline_ctx->scene;
  const float left_handle_position = UI_view2d_view_to_region_x(v2d, strip_ctx.left_handle);
  const float right_handle_position = UI_view2d_view_to_region_x(v2d, strip_ctx.right_handle);

  for (const SeqRetimingKey &key : SEQ_retiming_keys_get(seq)) {
    if (key_x_get(scene, seq, &key) == strip_ctx.left_handle || key.strip_frame_index == 0) {
      continue;
    }

    float key_position = UI_view2d_view_to_region_x(v2d, key_x_get(scene, seq, &key));
    float prev_key_position = UI_view2d_view_to_region_x(v2d, key_x_get(scene, seq, &key - 1));
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
    if (SEQ_retiming_data_is_editable(seq) &&
        (timeline_ctx->retiming_selection.contains(const_cast<SeqRetimingKey *>(&key)) ||
         timeline_ctx->retiming_selection.contains(const_cast<SeqRetimingKey *>(&key - 1))))
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
    timeline_ctx->quads->add_quad(prev_key_position, bottom, key_position, top, color);
  }
}

static SeqRetimingKey retiming_key_init(const Scene *scene,
                                        const Sequence *seq,
                                        int timeline_frame)
{
  int sound_offset = SEQ_time_get_rounded_sound_offset(scene, seq);
  SeqRetimingKey fake_key;
  fake_key.strip_frame_index = (timeline_frame - SEQ_time_start_frame_get(seq) - sound_offset) *
                               SEQ_time_media_playback_rate_factor_get(scene, seq);
  fake_key.flag = 0;
  return fake_key;
}

/* If there are no keys, draw fake keys and create real key when they are selected. */
/* TODO: would be nice to draw continuity between fake keys. */
static bool fake_keys_draw(const TimelineDrawContext *timeline_ctx,
                           const StripDrawContext &strip_ctx,
                           const KeyframeShaderBindings &sh_bindings)
{
  const Sequence *seq = strip_ctx.seq;
  const Scene *scene = timeline_ctx->scene;

  if (!SEQ_retiming_is_active(seq) && !SEQ_retiming_data_is_editable(seq)) {
    return false;
  }

  const int left_key_frame = left_fake_key_frame_get(timeline_ctx->C, seq);
  if (SEQ_retiming_key_get_by_timeline_frame(scene, seq, left_key_frame) == nullptr) {
    SeqRetimingKey fake_key = retiming_key_init(scene, seq, left_key_frame);
    retime_key_draw(timeline_ctx, strip_ctx, &fake_key, sh_bindings);
  }

  int right_key_frame = right_fake_key_frame_get(timeline_ctx->C, seq);
  if (SEQ_retiming_key_get_by_timeline_frame(scene, seq, right_key_frame) == nullptr) {
    /* `key_x_get()` compensates 1 frame offset of last key, however this can not
     * be conveyed via `fake_key` alone. Therefore the same offset must be emulated. */
    if (strip_ctx.right_handle >= SEQ_time_content_end_frame_get(scene, seq)) {
      right_key_frame += 1;
    }
    SeqRetimingKey fake_key = retiming_key_init(scene, seq, right_key_frame);
    retime_key_draw(timeline_ctx, strip_ctx, &fake_key, sh_bindings);
  }
  return true;
}

void sequencer_retiming_keys_draw(const TimelineDrawContext *timeline_ctx,
                                  const StripDrawContext &strip_ctx)
{
  if (!can_draw_retiming(timeline_ctx, strip_ctx)) {
    return;
  }

  wmOrtho2_region_pixelspace(timeline_ctx->region);

  const View2D *v2d = timeline_ctx->v2d;
  const Sequence *seq = strip_ctx.seq;

  GPUVertFormat *format = immVertexFormat();
  KeyframeShaderBindings sh_bindings;
  sh_bindings.pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  sh_bindings.size_id = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  sh_bindings.color_id = GPU_vertformat_attr_add(
      format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  sh_bindings.outline_color_id = GPU_vertformat_attr_add(
      format, "outlineColor", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  sh_bindings.flags_id = GPU_vertformat_attr_add(format, "flags", GPU_COMP_U32, 1, GPU_FETCH_INT);

  GPU_program_point_size(true);
  immBindBuiltinProgram(GPU_SHADER_KEYFRAME_SHAPE);
  immUniform1f("outline_scale", 1.0f);
  immUniform2f("ViewportSize", BLI_rcti_size_x(&v2d->mask) + 1, BLI_rcti_size_y(&v2d->mask) + 1);

  constexpr int MAX_KEYS_IN_BATCH = 1024;
  int point_counter = 0;
  immBeginAtMost(GPU_PRIM_POINTS, MAX_KEYS_IN_BATCH);

  if (fake_keys_draw(timeline_ctx, strip_ctx, sh_bindings)) {
    point_counter += 2;
  }

  for (const SeqRetimingKey &key : SEQ_retiming_keys_get(seq)) {
    retime_key_draw(timeline_ctx, strip_ctx, &key, sh_bindings);
    point_counter++;

    /* Next key plus possible two fake keys for next sequence would need at
     * most 3 points, so restart the batch if we're close to that. */
    if (point_counter + 3 >= MAX_KEYS_IN_BATCH) {
      immEnd();
      immBeginAtMost(GPU_PRIM_POINTS, MAX_KEYS_IN_BATCH);
      point_counter = 0;
    }
  }

  immEnd();
  GPU_program_point_size(false);
  immUnbindProgram();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Retiming Speed Label
 * \{ */

static size_t label_str_get(const Sequence *seq,
                            const SeqRetimingKey *key,
                            char *r_label_str,
                            const size_t label_str_maxncpy)
{
  const SeqRetimingKey *next_key = key + 1;
  if (SEQ_retiming_key_is_transition_start(key)) {
    const float prev_speed = SEQ_retiming_key_speed_get(seq, key);
    const float next_speed = SEQ_retiming_key_speed_get(seq, next_key + 1);
    return BLI_snprintf_rlen(r_label_str,
                             label_str_maxncpy,
                             "%d%% - %d%%",
                             round_fl_to_int(prev_speed * 100.0f),
                             round_fl_to_int(next_speed * 100.0f));
  }
  const float speed = SEQ_retiming_key_speed_get(seq, next_key);
  return BLI_snprintf_rlen(
      r_label_str, label_str_maxncpy, "%d%%", round_fl_to_int(speed * 100.0f));
}

static bool label_rect_get(const TimelineDrawContext *timeline_ctx,
                           const StripDrawContext &strip_ctx,
                           const SeqRetimingKey *key,
                           const char *label_str,
                           const size_t label_len,
                           rctf *rect)
{
  const bContext *C = timeline_ctx->C;
  const Scene *scene = timeline_ctx->scene;
  const SeqRetimingKey *next_key = key + 1;
  const float width = pixels_to_view_width(C, BLF_width(BLF_default(), label_str, label_len));
  const float height = pixels_to_view_height(C, BLF_height(BLF_default(), label_str, label_len));
  const float xmin = max_ff(strip_ctx.left_handle, key_x_get(scene, strip_ctx.seq, key));
  const float xmax = min_ff(strip_ctx.right_handle, key_x_get(scene, strip_ctx.seq, next_key));

  rect->xmin = (xmin + xmax - width) / 2;
  rect->xmax = rect->xmin + width;
  rect->ymin = strip_y_rescale(strip_ctx.seq, 0) + pixels_to_view_height(C, 5);
  rect->ymax = rect->ymin + height;

  return width < xmax - xmin - pixels_to_view_width(C, KEY_SIZE);
}

static void retime_speed_text_draw(const TimelineDrawContext *timeline_ctx,
                                   const StripDrawContext &strip_ctx,
                                   const SeqRetimingKey *key)
{
  const Sequence *seq = strip_ctx.seq;
  const Scene *scene = timeline_ctx->scene;

  if (SEQ_retiming_is_last_key(seq, key)) {
    return;
  }

  const SeqRetimingKey *next_key = key + 1;
  if (key_x_get(scene, seq, next_key) < strip_ctx.left_handle ||
      key_x_get(scene, seq, key) > strip_ctx.right_handle)
  {
    return; /* Label out of strip bounds. */
  }

  char label_str[40];
  rctf label_rect;
  size_t label_len = label_str_get(seq, key, label_str, sizeof(label_str));

  if (!label_rect_get(timeline_ctx, strip_ctx, key, label_str, label_len, &label_rect)) {
    return; /* Not enough space to draw the label. */
  }

  uchar col[4] = {255, 255, 255, 255};
  if ((seq->flag & SELECT) == 0) {
    memset(col, 0, sizeof(col));
    col[3] = 255;
  }

  UI_view2d_text_cache_add(
      timeline_ctx->v2d, label_rect.xmin, label_rect.ymin, label_str, label_len, col);
}

void sequencer_retiming_speed_draw(const TimelineDrawContext *timeline_ctx,
                                   const StripDrawContext &strip_ctx)
{
  if (!can_draw_retiming(timeline_ctx, strip_ctx)) {
    return;
  }

  for (const SeqRetimingKey &key : SEQ_retiming_keys_get(strip_ctx.seq)) {
    retime_speed_text_draw(timeline_ctx, strip_ctx, &key);
  }

  UI_view2d_view_ortho(timeline_ctx->v2d);
}

/** \} */
