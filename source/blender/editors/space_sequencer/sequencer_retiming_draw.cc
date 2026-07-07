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

#include "sequencer_intern.hh"
#include "sequencer_quads_batch.hh"

namespace blender::ed::vse {

/* -------------------------------------------------------------------- */
/** \name Draw Retiming Generic Functions
 * \{ */

bool retiming_overlay_enabled(const SpaceSeq *sseq)
{
  return (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_RETIMING) &&
         (sseq->flag & SEQ_SHOW_OVERLAY);
}

static bool can_draw_retiming(const TimelineDrawContext &ctx, const StripDrawContext &strip_ctx)
{
  if (ctx.ed == nullptr) {
    return false;
  }

  if (!retiming_overlay_enabled(ctx.sseq)) {
    return false;
  }

  if (!strip_ctx.can_draw_retiming_overlay) {
    return false;
  }

  if (!seq::retiming_is_allowed(strip_ctx.strip)) {
    return false;
  }

  if (!seq::retiming_show_keys(strip_ctx.strip)) {
    return false;
  }

  return true;
}

static inline float retiming_key_size()
{
  /* Pixel size of whole retiming key, from left side to right side or top to bottom. */
  return 10.0f * U.pixelsize;
}

static inline float retiming_key_center(const View2D *v2d, const Strip *strip)
{
  return (ui::view2d_view_to_region_y(v2d, strip->channel + STRIP_OFSBOTTOM) + 4 +
          retiming_key_size() / 2);
}

rcti strip_retiming_keys_box_get(const Scene *scene, const View2D *v2d, const Strip *strip)
{
  rctf strip_bounds = strip_bounds_get(scene, strip);
  rcti key_bounds;
  ui::view2d_view_to_region_rcti(v2d, &strip_bounds, &key_bounds);

  key_bounds.ymax = retiming_key_center(v2d, strip) + retiming_key_size() / 2;
  key_bounds.ymin = retiming_key_center(v2d, strip) - retiming_key_size() / 2;
  return key_bounds;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Retiming Keys
 * \{ */

static void retiming_key_draw(const TimelineDrawContext &ctx,
                              const StripDrawContext &strip_ctx,
                              const SeqRetimingKey *key,
                              const KeyframeShaderBindings &sh_bindings)
{
  const Scene *scene = ctx.scene;
  const View2D *v2d = ctx.v2d;
  Strip *strip = strip_ctx.strip;

  const float key_frame = seq::retiming_key_frame_get(scene, strip, key);
  const rctf strip_bounds = strip_bounds_get(scene, strip);
  if (!BLI_rctf_isect_x(&strip_bounds, key_frame)) {
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
  const int size = retiming_key_size();
  const float bottom = retiming_key_center(v2d, strip);

  /* For keys on the edge of the strip, ensure that their entire extent is drawn with a shift. */
  const float right_max = ui::view2d_view_to_region_x(v2d, strip_ctx.right_handle) - (size / 2);
  const float left_min = ui::view2d_view_to_region_x(v2d, strip_ctx.left_handle) + (size / 2);
  float key_position = ui::view2d_view_to_region_x(v2d, key_frame);
  CLAMP(key_position, left_min, right_max);

  draw_keyframe_shape(key_position,
                      bottom,
                      size,
                      is_selected,
                      key_type,
                      KEYFRAME_SHAPE_BOTH,
                      1.0,
                      &sh_bindings,
                      0,
                      0);
}

/* If there are no keys, draw fake keys and create real key when they are selected. */
/* TODO: would be nice to draw segments between fake keys. */
static bool fake_keys_draw(const TimelineDrawContext &ctx,
                           const StripDrawContext &strip_ctx,
                           const KeyframeShaderBindings &sh_bindings)
{
  const Strip *strip = strip_ctx.strip;
  const Scene *scene = ctx.scene;

  const int left_key_frame = seq::left_fake_key_frame_get(scene, strip);
  if (seq::retiming_key_get_by_frame(scene, strip, left_key_frame) == nullptr) {
    SeqRetimingKey fake_key = seq::fake_retiming_key_init(scene, strip, left_key_frame);
    retiming_key_draw(ctx, strip_ctx, &fake_key, sh_bindings);
  }

  int right_key_frame = seq::right_fake_key_frame_get(scene, strip);
  if (seq::retiming_key_get_by_frame(scene, strip, right_key_frame) == nullptr) {
    SeqRetimingKey fake_key = seq::fake_retiming_key_init(scene, strip, right_key_frame);
    retiming_key_draw(ctx, strip_ctx, &fake_key, sh_bindings);
  }
  return true;
}

void sequencer_retiming_keys_draw(const TimelineDrawContext &ctx, Span<StripDrawContext> strips)
{
  if (strips.is_empty()) {
    return;
  }
  if (ctx.ed == nullptr || !retiming_overlay_enabled(ctx.sseq)) {
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
      retiming_key_draw(ctx, strip_ctx, &key, sh_bindings);
      point_counter++;

      /* Next key plus possible two fake keys for next strip would need at
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
/** \name Draw Retiming Segments / Speed Labels
 * \{ */

void sequencer_retiming_draw_segments(const TimelineDrawContext &ctx,
                                      const StripDrawContext &strip_ctx)
{
  if (!can_draw_retiming(ctx, strip_ctx) || seq::retiming_keys_count(strip_ctx.strip) == 0) {
    return;
  }

  const Strip *strip = strip_ctx.strip;
  const View2D *v2d = ctx.v2d;
  const Scene *scene = ctx.scene;
  const float left_handle_pos = ui::view2d_view_to_region_x(v2d, strip_ctx.left_handle);
  const float right_handle_pos = ui::view2d_view_to_region_x(v2d, strip_ctx.right_handle);

  for (const SeqRetimingKey &key : seq::retiming_keys_get(strip)) {
    const int key_frame = seq::retiming_key_frame_get(scene, strip, &key);
    if (key_frame == strip_ctx.left_handle || key.strip_frame_index == 0) {
      continue;
    }

    float key_pos = ui::view2d_view_to_region_x(v2d, key_frame);
    float prev_key_pos = ui::view2d_view_to_region_x(
        v2d, seq::retiming_key_frame_get(scene, strip, &key - 1));
    if (prev_key_pos > right_handle_pos || key_pos < left_handle_pos) {
      /* Don't draw highlights for out of bounds retiming keys. */
      continue;
    }
    prev_key_pos = max_ff(prev_key_pos, left_handle_pos);
    key_pos = min_ff(key_pos, right_handle_pos);

    const int size = retiming_key_size();
    const float y_center = retiming_key_center(v2d, strip);

    const float width_fac = 0.5f;
    const float bottom = y_center - size * width_fac;
    const float top = y_center + size * width_fac;

    uchar color[4];
    if ((ctx.retiming_selection.contains(const_cast<SeqRetimingKey *>(&key)) ||
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
    ctx.quads->add_quad(prev_key_pos, bottom, key_pos, top, color);
  }
}

static size_t speed_label_str_get(const Strip *strip,
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

static std::optional<float2> speed_label_pos_get(const TimelineDrawContext &ctx,
                                                 const StripDrawContext &strip_ctx,
                                                 const SeqRetimingKey *key,
                                                 const char *label_str,
                                                 const size_t label_len)
{
  const Scene *scene = ctx.scene;

  const float key_x = max_ff(strip_ctx.left_handle,
                             seq::retiming_key_frame_get(scene, strip_ctx.strip, key));
  const float next_x = min_ff(strip_ctx.right_handle,
                              seq::retiming_key_frame_get(scene, strip_ctx.strip, key + 1));

  const float label_width = ctx.pixelx * BLF_width(BLF_default(), label_str, label_len);

  /* Available space for text is segment width minus two "half" keys (one key width in total). */
  const float available_width = (next_x - key_x) - (ctx.pixelx * retiming_key_size());
  if (available_width < label_width) {
    return std::nullopt;
  }

  /* Label rests 5px above bottom of strip. */
  const float bottom_pad = (ctx.pixely * 5.0f);

  const float x = 0.5f * (key_x + next_x - label_width); /* Left edge of centered label. */
  const float y = (strip_ctx.strip->channel + STRIP_OFSBOTTOM) + bottom_pad;
  return float2{x, y};
}

static void speed_label_draw(const TimelineDrawContext &ctx,
                             const StripDrawContext &strip_ctx,
                             const SeqRetimingKey *key)
{
  const Strip *strip = strip_ctx.strip;
  const Scene *scene = ctx.scene;

  if (seq::retiming_is_last_key(strip, key)) {
    return;
  }

  const SeqRetimingKey *next_key = key + 1;
  if (seq::retiming_key_frame_get(scene, strip, next_key) < strip_ctx.left_handle ||
      seq::retiming_key_frame_get(scene, strip, key) > strip_ctx.right_handle)
  {
    return; /* Label out of strip bounds. */
  }

  char label_str[40];
  size_t label_len = speed_label_str_get(strip, key, label_str, sizeof(label_str));

  const std::optional<float2> pos = speed_label_pos_get(ctx, strip_ctx, key, label_str, label_len);
  if (!pos) {
    return; /* Not enough space to draw the label. */
  }

  uchar col[4] = {255, 255, 255, 255};
  if ((strip->flag & SEQ_SELECT) == 0) {
    memset(col, 0, sizeof(col));
    col[3] = 255;
  }

  ui::view2d_text_cache_add(ctx.v2d, pos->x, pos->y, label_str, label_len, col);
}

void sequencer_retiming_speed_labels_draw(const TimelineDrawContext &ctx,
                                          const StripDrawContext &strip_ctx)
{
  if (!can_draw_retiming(ctx, strip_ctx)) {
    return;
  }

  for (const SeqRetimingKey &key : seq::retiming_keys_get(strip_ctx.strip)) {
    speed_label_draw(ctx, strip_ctx, &key);
  }

  ui::view2d_view_ortho(ctx.v2d);
}

/** \} */

}  // namespace blender::ed::vse
