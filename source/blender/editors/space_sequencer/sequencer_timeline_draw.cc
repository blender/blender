/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include <cmath>

#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_task.hh"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_defaults.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_screen.hh"
#include "BKE_sound.hh"

#include "ED_anim_api.hh"
#include "ED_markers.hh"
#include "ED_mask.hh"
#include "ED_sequencer.hh"
#include "ED_space_api.hh"
#include "ED_time_scrub_ui.hh"

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "RNA_prototypes.hh"

#include "SEQ_channels.hh"
#include "SEQ_connect.hh"
#include "SEQ_effects.hh"
#include "SEQ_prefetch.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_retiming.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_thumbnail_cache.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"
#include "SEQ_utils.hh"

#include "UI_interface_icons.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "BLF_api.hh"

/* Own include. */
#include "sequencer_intern.hh"
#include "sequencer_quads_batch.hh"
#include "sequencer_strips_batch.hh"

namespace blender::ed::vse {

constexpr int MUTE_ALPHA = 120;

constexpr float ICON_SIZE = 12.0f;

Vector<Strip *> sequencer_visible_strips_get(const bContext *C)
{
  return sequencer_visible_strips_get(CTX_data_sequencer_scene(C), UI_view2d_fromcontext(C));
}

Vector<Strip *> sequencer_visible_strips_get(const Scene *scene, const View2D *v2d)
{
  const Editing *ed = seq::editing_get(scene);
  Vector<Strip *> strips;

  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (min_ii(seq::time_left_handle_frame_get(scene, strip), seq::time_start_frame_get(strip)) >
        v2d->cur.xmax)
    {
      continue;
    }
    if (max_ii(seq::time_right_handle_frame_get(scene, strip),
               seq::time_content_end_frame_get(scene, strip)) < v2d->cur.xmin)
    {
      continue;
    }
    if (strip->channel + 1.0f < v2d->cur.ymin) {
      continue;
    }
    if (strip->channel > v2d->cur.ymax) {
      continue;
    }
    strips.append(strip);
  }
  return strips;
}

static TimelineDrawContext timeline_draw_context_get(const bContext *C, SeqQuadsBatch *quads_batch)
{
  TimelineDrawContext ctx;

  ctx.C = C;
  ctx.region = CTX_wm_region(C);
  ctx.scene = CTX_data_sequencer_scene(C);
  ctx.sseq = CTX_wm_space_seq(C);
  ctx.v2d = UI_view2d_fromcontext(C);

  ctx.ed = ctx.scene ? seq::editing_get(ctx.scene) : nullptr;
  ctx.channels = ctx.ed ? seq::channels_displayed_get(ctx.ed) : nullptr;

  ctx.viewport = WM_draw_region_get_viewport(ctx.region);
  ctx.framebuffer_overlay = GPU_viewport_framebuffer_overlay_get(ctx.viewport);

  ctx.pixely = BLI_rctf_size_y(&ctx.v2d->cur) / (BLI_rcti_size_y(&ctx.v2d->mask) + 1);
  ctx.pixelx = BLI_rctf_size_x(&ctx.v2d->cur) / (BLI_rcti_size_x(&ctx.v2d->mask) + 1);

  ctx.retiming_selection = seq::retiming_selection_get(ctx.ed);

  ctx.quads = quads_batch;

  return ctx;
}

static bool seq_draw_waveforms_poll(const SpaceSeq *sseq, const Strip *strip)
{
  const bool strip_is_valid = strip->type == STRIP_TYPE_SOUND_RAM && strip->sound != nullptr;
  const bool overlays_enabled = (sseq->flag & SEQ_SHOW_OVERLAY) != 0;
  const bool overlay_option = ((sseq->timeline_overlay.flag & SEQ_TIMELINE_ALL_WAVEFORMS) != 0 ||
                               (strip->flag & SEQ_AUDIO_DRAW_WAVEFORM));

  if ((sseq->timeline_overlay.flag & SEQ_TIMELINE_NO_WAVEFORMS) != 0) {
    return false;
  }

  if (strip_is_valid && overlays_enabled && overlay_option) {
    return true;
  }

  return false;
}

static bool strip_hides_text_overlay_first(const TimelineDrawContext &ctx,
                                           const StripDrawContext &strip_ctx)
{
  return seq_draw_waveforms_poll(ctx.sseq, strip_ctx.strip) ||
         strip_ctx.strip->type == STRIP_TYPE_COLOR;
}

static void strip_draw_context_set_text_overlay_visibility(const TimelineDrawContext &ctx,
                                                           StripDrawContext &strip_ctx)
{
  float threshold = 8 * UI_SCALE_FAC;
  if (strip_hides_text_overlay_first(ctx, strip_ctx)) {
    threshold = 20 * UI_SCALE_FAC;
  }

  const bool overlays_enabled = (ctx.sseq->timeline_overlay.flag &
                                 (SEQ_TIMELINE_SHOW_STRIP_NAME | SEQ_TIMELINE_SHOW_STRIP_SOURCE |
                                  SEQ_TIMELINE_SHOW_STRIP_DURATION)) != 0;

  strip_ctx.can_draw_text_overlay = (strip_ctx.top - strip_ctx.bottom) / ctx.pixely >= threshold;
  strip_ctx.can_draw_text_overlay &= overlays_enabled;
}

static void strip_draw_context_set_strip_content_visibility(const TimelineDrawContext &ctx,
                                                            StripDrawContext &strip_ctx)
{
  float threshold = 20 * UI_SCALE_FAC;
  if (strip_hides_text_overlay_first(ctx, strip_ctx)) {
    threshold = 8 * UI_SCALE_FAC;
  }

  strip_ctx.can_draw_strip_content = ((strip_ctx.top - strip_ctx.bottom) / ctx.pixely) > threshold;
}

static void strip_draw_context_set_retiming_overlay_visibility(const TimelineDrawContext &ctx,
                                                               StripDrawContext &strip_ctx)
{
  float2 threshold{15 * UI_SCALE_FAC, 25 * UI_SCALE_FAC};
  strip_ctx.can_draw_retiming_overlay = (strip_ctx.top - strip_ctx.bottom) / ctx.pixely >=
                                        threshold.y;
  strip_ctx.can_draw_retiming_overlay &= strip_ctx.strip_length / ctx.pixelx >= threshold.x;
  strip_ctx.can_draw_retiming_overlay &= retiming_keys_can_be_displayed(ctx.sseq);
}

static float strip_header_size_get(const TimelineDrawContext &ctx)
{
  return min_ff(0.40f, 20 * UI_SCALE_FAC * ctx.pixely);
}

static StripDrawContext strip_draw_context_get(const TimelineDrawContext &ctx, Strip *strip)
{
  using namespace seq;
  StripDrawContext strip_ctx;
  Scene *scene = ctx.scene;

  strip_ctx.strip = strip;
  strip_ctx.bottom = strip->channel + STRIP_OFSBOTTOM;
  strip_ctx.top = strip->channel + STRIP_OFSTOP;
  strip_ctx.left_handle = time_left_handle_frame_get(scene, strip);
  strip_ctx.right_handle = time_right_handle_frame_get(scene, strip);
  strip_ctx.content_start = time_start_frame_get(strip);
  strip_ctx.content_end = time_content_end_frame_get(scene, strip);

  if (strip->type == STRIP_TYPE_SOUND_RAM && strip->sound != nullptr) {
    /* Visualize sub-frame sound offsets. */
    const double sound_offset = (strip->sound->offset_time + strip->sound_offset) *
                                scene->frames_per_second();
    strip_ctx.content_start += sound_offset;
    strip_ctx.content_end += sound_offset;
  }

  /* Limit body to strip bounds. */
  strip_ctx.content_start = min_ff(strip_ctx.content_start, strip_ctx.right_handle);
  strip_ctx.content_end = max_ff(strip_ctx.content_end, strip_ctx.left_handle);

  strip_ctx.strip_length = strip_ctx.right_handle - strip_ctx.left_handle;

  strip_draw_context_set_text_overlay_visibility(ctx, strip_ctx);
  strip_draw_context_set_strip_content_visibility(ctx, strip_ctx);
  strip_draw_context_set_retiming_overlay_visibility(ctx, strip_ctx);
  strip_ctx.strip_is_too_small = (!strip_ctx.can_draw_text_overlay &&
                                  !strip_ctx.can_draw_strip_content);
  strip_ctx.is_active_strip = strip == select_active_get(scene);
  strip_ctx.is_single_image = transform_single_image_check(strip);
  strip_ctx.handle_width = strip_handle_draw_size_get(ctx.scene, strip, ctx.pixelx);
  strip_ctx.show_strip_color_tag = (ctx.sseq->timeline_overlay.flag &
                                    SEQ_TIMELINE_SHOW_STRIP_COLOR_TAG);

  /* Determine if strip (or contents of meta strip) has missing data/media. */
  strip_ctx.missing_data_block = !strip_has_valid_data(strip);
  strip_ctx.missing_media = media_presence_is_missing(scene, strip);
  strip_ctx.is_connected = is_strip_connected(strip);
  if (strip->type == STRIP_TYPE_META) {
    const ListBase *seqbase = &strip->seqbase;
    LISTBASE_FOREACH (const Strip *, sub, seqbase) {
      if (!strip_has_valid_data(sub)) {
        strip_ctx.missing_data_block = true;
      }
      if (media_presence_is_missing(scene, sub)) {
        strip_ctx.missing_media = true;
      }
    }
  }

  if (strip_ctx.can_draw_text_overlay) {
    strip_ctx.strip_content_top = strip_ctx.top - strip_header_size_get(ctx);
  }
  else {
    strip_ctx.strip_content_top = strip_ctx.top;
  }

  strip_ctx.is_muted = render_is_muted(ctx.channels, strip);
  strip_ctx.curve = nullptr;
  return strip_ctx;
}

static void strip_draw_context_curve_get(const TimelineDrawContext &ctx,
                                         StripDrawContext &strip_ctx)
{
  strip_ctx.curve = nullptr;
  const bool showing_curve_overlay = strip_ctx.can_draw_strip_content &&
                                     (ctx.sseq->flag & SEQ_SHOW_OVERLAY) != 0 &&
                                     (ctx.sseq->timeline_overlay.flag &
                                      SEQ_TIMELINE_SHOW_FCURVES) != 0;
  const bool showing_waveform = (strip_ctx.strip->type == STRIP_TYPE_SOUND_RAM) &&
                                !strip_ctx.strip_is_too_small &&
                                seq_draw_waveforms_poll(ctx.sseq, strip_ctx.strip);
  if (showing_curve_overlay || showing_waveform) {
    const char *prop_name = strip_ctx.strip->type == STRIP_TYPE_SOUND_RAM ? "volume" :
                                                                            "blend_alpha";
    strip_ctx.curve = id_data_find_fcurve(
        &ctx.scene->id, strip_ctx.strip, &RNA_Strip, prop_name, 0, nullptr);
    if (strip_ctx.curve && BKE_fcurve_is_empty(strip_ctx.curve)) {
      strip_ctx.curve = nullptr;
    }
  }
}

static void color3ubv_from_seq(const Scene *curscene,
                               const Strip *strip,
                               const bool show_strip_color_tag,
                               const bool is_muted,
                               uchar r_col[3])
{
  if (show_strip_color_tag && uint(strip->color_tag) < STRIP_COLOR_TOT &&
      strip->color_tag != STRIP_COLOR_NONE)
  {
    bTheme *btheme = UI_GetTheme();
    const ThemeStripColor *strip_color = &btheme->strip_color[strip->color_tag];
    copy_v3_v3_uchar(r_col, strip_color->color);
    return;
  }

  uchar blendcol[3];

  /* Sometimes the active theme is not the sequencer theme, e.g. when an operator invokes the file
   * browser. This makes sure we get the right color values for the theme. */
  bThemeState theme_state;
  UI_Theme_Store(&theme_state);
  UI_SetTheme(SPACE_SEQ, RGN_TYPE_WINDOW);

  switch (strip->type) {
    case STRIP_TYPE_IMAGE:
      UI_GetThemeColor3ubv(TH_SEQ_IMAGE, r_col);
      break;

    case STRIP_TYPE_META:
      UI_GetThemeColor3ubv(TH_SEQ_META, r_col);
      break;

    case STRIP_TYPE_MOVIE:
      UI_GetThemeColor3ubv(TH_SEQ_MOVIE, r_col);
      break;

    case STRIP_TYPE_MOVIECLIP:
      UI_GetThemeColor3ubv(TH_SEQ_MOVIECLIP, r_col);
      break;

    case STRIP_TYPE_MASK:
      UI_GetThemeColor3ubv(TH_SEQ_MASK, r_col);
      break;

    case STRIP_TYPE_SCENE:
      UI_GetThemeColor3ubv(TH_SEQ_SCENE, r_col);

      if (strip->scene == curscene) {
        UI_GetColorPtrShade3ubv(r_col, 20, r_col);
      }
      break;

    /* Transitions use input colors, fallback for when the input is a transition itself. */
    case STRIP_TYPE_CROSS:
    case STRIP_TYPE_GAMCROSS:
    case STRIP_TYPE_WIPE:
      UI_GetThemeColor3ubv(TH_SEQ_TRANSITION, r_col);

      /* Slightly offset hue to distinguish different transition types. */
      if (strip->type == STRIP_TYPE_GAMCROSS) {
        rgb_byte_set_hue_float_offset(r_col, 0.03);
      }
      else if (strip->type == STRIP_TYPE_WIPE) {
        rgb_byte_set_hue_float_offset(r_col, 0.06);
      }
      break;

    /* Effects. */
    case STRIP_TYPE_SPEED:
    case STRIP_TYPE_ADD:
    case STRIP_TYPE_SUB:
    case STRIP_TYPE_MUL:
    case STRIP_TYPE_ALPHAOVER:
    case STRIP_TYPE_ALPHAUNDER:
    case STRIP_TYPE_GLOW:
    case STRIP_TYPE_MULTICAM:
    case STRIP_TYPE_ADJUSTMENT:
    case STRIP_TYPE_GAUSSIAN_BLUR:
    case STRIP_TYPE_COLORMIX:
      UI_GetThemeColor3ubv(TH_SEQ_EFFECT, r_col);

      /* Slightly offset hue to distinguish different effects. */
      if (strip->type == STRIP_TYPE_ADD) {
        rgb_byte_set_hue_float_offset(r_col, 0.09);
      }
      else if (strip->type == STRIP_TYPE_SUB) {
        rgb_byte_set_hue_float_offset(r_col, 0.03);
      }
      else if (strip->type == STRIP_TYPE_MUL) {
        rgb_byte_set_hue_float_offset(r_col, 0.06);
      }
      else if (strip->type == STRIP_TYPE_ALPHAOVER) {
        rgb_byte_set_hue_float_offset(r_col, 0.16);
      }
      else if (strip->type == STRIP_TYPE_ALPHAUNDER) {
        rgb_byte_set_hue_float_offset(r_col, 0.19);
      }
      else if (strip->type == STRIP_TYPE_COLORMIX) {
        rgb_byte_set_hue_float_offset(r_col, 0.25);
      }
      else if (strip->type == STRIP_TYPE_GAUSSIAN_BLUR) {
        rgb_byte_set_hue_float_offset(r_col, 0.31);
      }
      else if (strip->type == STRIP_TYPE_GLOW) {
        rgb_byte_set_hue_float_offset(r_col, 0.34);
      }
      else if (strip->type == STRIP_TYPE_ADJUSTMENT) {
        rgb_byte_set_hue_float_offset(r_col, 0.89);
      }
      else if (strip->type == STRIP_TYPE_SPEED) {
        rgb_byte_set_hue_float_offset(r_col, 0.72);
      }
      else if (strip->type == STRIP_TYPE_MULTICAM) {
        rgb_byte_set_hue_float_offset(r_col, 0.85);
      }
      break;

    case STRIP_TYPE_COLOR:
      UI_GetThemeColor3ubv(TH_SEQ_COLOR, r_col);
      break;

    case STRIP_TYPE_SOUND_RAM:
      UI_GetThemeColor3ubv(TH_SEQ_AUDIO, r_col);
      blendcol[0] = blendcol[1] = blendcol[2] = 128;
      if (is_muted) {
        UI_GetColorPtrBlendShade3ubv(r_col, blendcol, 0.5, 20, r_col);
      }
      break;

    case STRIP_TYPE_TEXT:
      UI_GetThemeColor3ubv(TH_SEQ_TEXT, r_col);
      break;

    default:
      r_col[0] = 10;
      r_col[1] = 255;
      r_col[2] = 40;
      break;
  }

  UI_Theme_Restore(&theme_state);
}

static void waveform_job_start_if_needed(const bContext *C, const Strip *strip)
{
  bSound *sound = strip->sound;

  BLI_spin_lock(static_cast<SpinLock *>(sound->spinlock));
  if (!sound->waveform) {
    /* Load the waveform data if it hasn't been loaded and cached already. */
    if (!(sound->tags & SOUND_TAGS_WAVEFORM_LOADING)) {
      /* Prevent sounds from reloading. */
      sound->tags |= SOUND_TAGS_WAVEFORM_LOADING;
      BLI_spin_unlock(static_cast<SpinLock *>(sound->spinlock));
      sequencer_preview_add_sound(C, strip);
    }
    else {
      BLI_spin_unlock(static_cast<SpinLock *>(sound->spinlock));
    }
  }
  BLI_spin_unlock(static_cast<SpinLock *>(sound->spinlock));
}

static float align_frame_with_pixel(float frame_coord, float frames_per_pixel)
{
  return round_fl_to_int(frame_coord / frames_per_pixel) * frames_per_pixel;
}

static void draw_seq_waveform_overlay(const TimelineDrawContext &ctx,
                                      const StripDrawContext &strip_ctx)
{
  if (!seq_draw_waveforms_poll(ctx.sseq, strip_ctx.strip) || strip_ctx.strip_is_too_small) {
    return;
  }

  const View2D *v2d = ctx.v2d;
  Scene *scene = ctx.scene;
  Strip *strip = strip_ctx.strip;

  const bool half_style = (ctx.sseq->timeline_overlay.flag & SEQ_TIMELINE_WAVEFORMS_HALF) != 0;

  const float frames_per_pixel = BLI_rctf_size_x(&v2d->cur) / ctx.region->winx;
  const float samples_per_frame = SOUND_WAVE_SAMPLES_PER_SECOND / scene->frames_per_second();
  const float samples_per_pixel = samples_per_frame * frames_per_pixel;
  const float bottom = strip_ctx.bottom + ctx.pixely * 2.0f;
  const float top = strip_ctx.strip_content_top;
  /* The y coordinate of signal level zero. */
  const float y_zero = half_style ? bottom : (bottom + top) / 2.0f;
  /* The y range of unit signal level. */
  const float y_scale = half_style ? top - bottom : (top - bottom) / 2.0f;

  /* Align strip start with nearest pixel to prevent waveform flickering. */
  const float strip_start_aligned = align_frame_with_pixel(
      strip_ctx.left_handle + ctx.pixelx * 3.0f, frames_per_pixel);
  /* Offset x1 and x2 values, to match view min/max, if strip is out of bounds. */
  const float draw_start_frame = max_ff(v2d->cur.xmin, strip_start_aligned);
  const float draw_end_frame = min_ff(v2d->cur.xmax, strip_ctx.right_handle - ctx.pixelx * 3.0f);
  /* Offset must be also aligned, otherwise waveform flickers when moving left handle. */
  float sample_start_frame = draw_start_frame - (strip->sound->offset_time + strip->sound_offset) *
                                                    scene->frames_per_second();

  const int pixels_to_draw = round_fl_to_int((draw_end_frame - draw_start_frame) /
                                             frames_per_pixel);

  if (pixels_to_draw < 2) {
    return; /* Not much to draw, exit before running job. */
  }

  waveform_job_start_if_needed(ctx.C, strip);

  SoundWaveform *waveform = static_cast<SoundWaveform *>(strip->sound->waveform);
  if (waveform == nullptr || waveform->length == 0) {
    return; /* Waveform was not built. */
  }

  /* Draw zero line (when actual samples close to zero are drawn, they might not cover a pixel). */
  uchar color[4] = {255, 255, 255, 127};
  uchar color_clip[4] = {255, 0, 0, 127};
  uchar color_rms[4] = {255, 255, 255, 204};
  ctx.quads->add_line(draw_start_frame, y_zero, draw_end_frame, y_zero, color);

  float prev_y_mid = y_zero;
  for (int i = 0; i < pixels_to_draw; i++) {
    float timeline_frame = sample_start_frame + i * frames_per_pixel;
    float frame_index = seq::give_frame_index(scene, strip, timeline_frame) + strip->anim_startofs;
    float sample = frame_index * samples_per_frame;
    int sample_index = round_fl_to_int(sample);

    if (sample_index < 0) {
      continue;
    }

    if (sample_index >= waveform->length) {
      break;
    }

    float value_min = waveform->data[sample_index * 3];
    float value_max = waveform->data[sample_index * 3 + 1];
    float rms = waveform->data[sample_index * 3 + 2];

    if (samples_per_pixel > 1.0f) {
      /* We need to sum up the values we skip over until the next step. */
      float next_pos = sample + samples_per_pixel;
      int end_idx = round_fl_to_int(next_pos);

      for (int j = sample_index + 1; (j < waveform->length) && (j < end_idx); j++) {
        value_min = min_ff(value_min, waveform->data[j * 3]);
        value_max = max_ff(value_max, waveform->data[j * 3 + 1]);
        rms = max_ff(rms, waveform->data[j * 3 + 2]);
      }
    }

    float volume = strip->volume;
    if (strip_ctx.curve != nullptr) {
      float evaltime = draw_start_frame + (i * frames_per_pixel);
      volume = evaluate_fcurve(strip_ctx.curve, evaltime);
      CLAMP_MIN(volume, 0.0f);
    }

    value_min *= volume;
    value_max *= volume;
    rms *= volume;

    bool is_clipping = false;
    float clamped_min = clamp_f(value_min, -1.0f, 1.0f);
    float clamped_max = clamp_f(value_max, -1.0f, 1.0f);
    if (clamped_min != value_min || clamped_max != value_max) {
      is_clipping = true;
    }
    value_min = clamped_min;
    value_max = clamped_max;

    /* We are drawing only half to the waveform, mirroring the lower part upwards.
     * If both min and max are on the same side of zero line, we want to draw a bar
     * between them. If min and max cross zero, we want to fill bar from zero to max
     * of those. */
    if (half_style) {
      bool pos_min = value_min > 0.0f;
      bool pos_max = value_max > 0.0f;
      float abs_min = std::abs(value_min);
      float abs_max = std::abs(value_max);
      if (pos_min == pos_max) {
        value_min = std::min(abs_min, abs_max);
        value_max = std::max(abs_min, abs_max);
      }
      else {
        value_min = 0;
        value_max = std::max(abs_min, abs_max);
      }
    }

    float x1 = draw_start_frame + i * frames_per_pixel;
    float x2 = draw_start_frame + (i + 1) * frames_per_pixel;
    float y_min = y_zero + value_min * y_scale;
    float y_max = y_zero + value_max * y_scale;
    float y_mid = (y_max + y_min) * 0.5f;

    /* If a bar would be below 2px, make it a line. */
    if (y_max - y_min < ctx.pixely * 2) {
      /* If previous segment was also a line of different enough
       * height, join them. */
      if (std::abs(y_mid - prev_y_mid) > ctx.pixely) {
        float x0 = draw_start_frame + (i - 1) * frames_per_pixel;
        ctx.quads->add_line(x0, prev_y_mid, x1, y_mid, is_clipping ? color_clip : color);
      }
      ctx.quads->add_line(x1, y_mid, x2, y_mid, is_clipping ? color_clip : color);
    }
    else {
      float rms_min = y_zero + max_ff(-rms, value_min) * y_scale;
      float rms_max = y_zero + min_ff(rms, value_max) * y_scale;
      /* RMS */
      ctx.quads->add_quad(x1, rms_min, x2, rms_max, is_clipping ? color_clip : color_rms);
      /* Sample */
      ctx.quads->add_quad(x1, y_min, x2, y_max, is_clipping ? color_clip : color);
    }

    prev_y_mid = y_mid;
  }
}

static void drawmeta_contents(const TimelineDrawContext &ctx,
                              const StripDrawContext &strip_ctx,
                              float corner_radius)
{
  using namespace seq;
  Strip *strip_meta = strip_ctx.strip;
  if (!strip_ctx.can_draw_strip_content || (ctx.sseq->flag & SEQ_SHOW_OVERLAY) == 0) {
    return;
  }
  if ((strip_meta->type != STRIP_TYPE_META) &&
      ((strip_meta->type != STRIP_TYPE_SCENE) || (strip_meta->flag & SEQ_SCENE_STRIPS) == 0))
  {
    return;
  }

  Scene *scene = ctx.scene;

  uchar col[4];

  int chan_min = MAX_CHANNELS;
  int chan_max = 0;
  int chan_range = 0;
  /* Some vertical margin to account for rounded corners, so that contents do
   * not draw outside them. Can be removed when meta contents are drawn with
   * full rounded corners masking shader. */
  const float bottom = strip_ctx.bottom + corner_radius * 0.8f * ctx.pixely;
  const float top = strip_ctx.strip_content_top - corner_radius * 0.8f * ctx.pixely;
  const float draw_range = top - bottom;
  if (draw_range < ctx.pixely) {
    return;
  }

  ListBase *meta_channels;
  int offset;

  ListBase *meta_seqbase = get_seqbase_from_strip(strip_meta, &meta_channels, &offset);

  if (!meta_seqbase || BLI_listbase_is_empty(meta_seqbase)) {
    return;
  }

  if (strip_meta->type == STRIP_TYPE_SCENE) {
    offset = strip_meta->start - offset;
  }
  else {
    offset = 0;
  }

  LISTBASE_FOREACH (Strip *, strip, meta_seqbase) {
    chan_min = min_ii(chan_min, strip->channel);
    chan_max = max_ii(chan_max, strip->channel);
  }

  chan_range = (chan_max - chan_min) + 1;
  float draw_height = draw_range / chan_range;

  col[3] = 196; /* Alpha, used for all meta children. */

  const float meta_x1 = strip_ctx.left_handle;
  const float meta_x2 = strip_ctx.right_handle;

  /* Draw only immediate children (1 level depth). */
  LISTBASE_FOREACH (Strip *, strip, meta_seqbase) {
    float x1_chan = time_left_handle_frame_get(scene, strip) + offset;
    float x2_chan = time_right_handle_frame_get(scene, strip) + offset;
    if (x1_chan <= meta_x2 && x2_chan >= meta_x1) {
      float y_chan = (strip->channel - chan_min) / float(chan_range) * draw_range;

      if (strip->type == STRIP_TYPE_COLOR) {
        SolidColorVars *colvars = (SolidColorVars *)strip->effectdata;
        rgb_float_to_uchar(col, colvars->col);
      }
      else {
        color3ubv_from_seq(scene, strip, strip_ctx.show_strip_color_tag, strip_ctx.is_muted, col);
      }

      if (strip_ctx.is_muted || render_is_muted(meta_channels, strip)) {
        col[3] = 64;
      }
      else {
        col[3] = 196;
      }

      const bool missing_data = !strip_has_valid_data(strip);
      const bool missing_media = media_presence_is_missing(scene, strip);
      if (missing_data || missing_media) {
        col[0] = 112;
        col[1] = 0;
        col[2] = 0;
      }

      /* Clamp within parent sequence strip bounds. */
      x1_chan = max_ff(x1_chan, meta_x1);
      x2_chan = min_ff(x2_chan, meta_x2);

      float y1_chan = bottom + y_chan + (draw_height * STRIP_OFSBOTTOM);
      float y2_chan = bottom + y_chan + (draw_height * STRIP_OFSTOP);

      ctx.quads->add_quad(x1_chan, y1_chan, x2_chan, y2_chan, col);
    }
  }
}

static void draw_handle_transform_text(const TimelineDrawContext &ctx,
                                       const StripDrawContext &strip_ctx,
                                       eStripHandle handle)
{
  /* Draw numbers for start and end of the strip next to its handles. */
  if (strip_ctx.strip_is_too_small || (strip_ctx.strip->flag & SELECT) == 0) {
    return;
  }

  if (handle_is_selected(strip_ctx.strip, handle) == 0 && (G.moving & G_TRANSFORM_SEQ) == 0) {
    return;
  }

  char numstr[64];
  BLF_set_default();

  /* Calculate if strip is wide enough for showing the labels. */
  size_t numstr_len = SNPRINTF_UTF8_RLEN(
      numstr, "%d%d", int(strip_ctx.left_handle), int(strip_ctx.right_handle));
  const float tot_width = BLF_width(BLF_default(), numstr, numstr_len);

  if (strip_ctx.strip_length / ctx.pixelx < 20 + tot_width) {
    return;
  }

  constexpr uchar col[4] = {255, 255, 255, 255};
  const float text_margin = 1.2f * strip_ctx.handle_width;
  const float text_y = strip_ctx.bottom + 0.09f;
  float text_x = strip_ctx.left_handle;

  if (handle == STRIP_HANDLE_LEFT) {
    numstr_len = SNPRINTF_UTF8_RLEN(numstr, "%d", int(strip_ctx.left_handle));
    text_x += text_margin;
  }
  else {
    numstr_len = SNPRINTF_UTF8_RLEN(numstr, "%d", int(strip_ctx.right_handle - 1));
    text_x = strip_ctx.right_handle -
             (text_margin + ctx.pixelx * BLF_width(BLF_default(), numstr, numstr_len));
  }
  UI_view2d_text_cache_add(ctx.v2d, text_x, text_y, numstr, numstr_len, col);
}

float strip_handle_draw_size_get(const Scene *scene, const Strip *strip, const float pixelx)
{
  const float handle_size = pixelx * (5.0f * U.pixelsize);

  /* Ensure that the handle is not wider than a quarter of the strip. */
  return min_ff(handle_size,
                (float(seq::time_right_handle_frame_get(scene, strip) -
                       seq::time_left_handle_frame_get(scene, strip)) /
                 4.0f));
}

static const char *draw_seq_text_get_name(const Strip *strip)
{
  const char *name = strip->name + 2;
  if (name[0] == '\0') {
    name = seq::strip_give_name(strip);
  }
  return name;
}

static void draw_seq_text_get_source(const Strip *strip, char *r_source, size_t source_maxncpy)
{
  *r_source = '\0';

  /* Set source for the most common types. */
  switch (strip->type) {
    case STRIP_TYPE_IMAGE:
    case STRIP_TYPE_MOVIE: {
      BLI_path_join(
          r_source, source_maxncpy, strip->data->dirpath, strip->data->stripdata->filename);
      break;
    }
    case STRIP_TYPE_SOUND_RAM: {
      if (strip->sound != nullptr) {
        BLI_strncpy_utf8(r_source, strip->sound->filepath, source_maxncpy);
      }
      break;
    }
    case STRIP_TYPE_MULTICAM: {
      BLI_snprintf_utf8(r_source, source_maxncpy, "Channel: %d", strip->multicam_source);
      break;
    }
    case STRIP_TYPE_TEXT: {
      const TextVars *textdata = static_cast<TextVars *>(strip->effectdata);
      BLI_strncpy_utf8(r_source, textdata->text_ptr, source_maxncpy);
      break;
    }
    case STRIP_TYPE_SCENE: {
      if (strip->scene != nullptr) {
        if (strip->scene_camera != nullptr) {
          BLI_snprintf_utf8(r_source,
                            source_maxncpy,
                            "%s (%s)",
                            strip->scene->id.name + 2,
                            strip->scene_camera->id.name + 2);
        }
        else {
          BLI_strncpy_utf8(r_source, strip->scene->id.name + 2, source_maxncpy);
        }
      }
      break;
    }
    case STRIP_TYPE_MOVIECLIP: {
      if (strip->clip != nullptr) {
        BLI_strncpy_utf8(r_source, strip->clip->id.name + 2, source_maxncpy);
      }
      break;
    }
    case STRIP_TYPE_MASK: {
      if (strip->mask != nullptr) {
        BLI_strncpy_utf8(r_source, strip->mask->id.name + 2, source_maxncpy);
      }
      break;
    }
  }
}

static size_t draw_seq_text_get_overlay_string(const TimelineDrawContext &ctx,
                                               const StripDrawContext &strip_ctx,
                                               char *r_overlay_string,
                                               size_t overlay_string_len)
{
  const Strip *strip = strip_ctx.strip;

  const char *text_sep = " | ";
  const char *text_array[5];
  int i = 0;

  if (ctx.sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_NAME) {
    text_array[i++] = draw_seq_text_get_name(strip);
  }

  char source[FILE_MAX];
  if (ctx.sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_SOURCE) {
    draw_seq_text_get_source(strip, source, sizeof(source));
    if (source[0] != '\0') {
      if (i != 0) {
        text_array[i++] = text_sep;
      }
      text_array[i++] = source;
    }
  }

  char strip_duration_text[16];
  if (ctx.sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_DURATION) {
    SNPRINTF_UTF8(strip_duration_text, "%d", int(strip_ctx.strip_length));
    if (i != 0) {
      text_array[i++] = text_sep;
    }
    text_array[i++] = strip_duration_text;
  }

  BLI_assert(i <= ARRAY_SIZE(text_array));

  return BLI_string_join_array(r_overlay_string, overlay_string_len, text_array, i);
}

static void get_strip_text_color(const StripDrawContext &strip_ctx, uchar r_col[4])
{
  const Strip *strip = strip_ctx.strip;
  const bool active_or_selected = (strip->flag & SELECT) || strip_ctx.is_active_strip;

  /* Text: white when selected/active, black otherwise. */
  r_col[0] = r_col[1] = r_col[2] = r_col[3] = 255;

  /* If not active or selected, draw text black. */
  if (!active_or_selected) {
    r_col[0] = r_col[1] = r_col[2] = 0;

    /* On muted and missing media/data-block strips: gray color, reduce opacity. */
    if (strip_ctx.is_muted || strip_ctx.missing_data_block || strip_ctx.missing_media) {
      r_col[0] = r_col[1] = r_col[2] = 192;
      r_col[3] *= 0.66f;
    }
  }
}

static void draw_icon_centered(const TimelineDrawContext &ctx,
                               const rctf &rect,
                               int icon_id,
                               const uchar color[4])
{
  UI_view2d_view_ortho(ctx.v2d);
  wmOrtho2_region_pixelspace(ctx.region);

  const float icon_size = ICON_SIZE * UI_SCALE_FAC;
  if (BLI_rctf_size_x(&rect) * 1.1f < icon_size * ctx.pixelx ||
      BLI_rctf_size_y(&rect) * 1.1f < icon_size * ctx.pixely)
  {
    UI_view2d_view_restore(ctx.C);
    return;
  }

  const float left = ((rect.xmin - ctx.v2d->cur.xmin) / ctx.pixelx);
  const float right = ((rect.xmax - ctx.v2d->cur.xmin) / ctx.pixelx);
  const float bottom = ((rect.ymin - ctx.v2d->cur.ymin) / ctx.pixely);
  const float top = ((rect.ymax - ctx.v2d->cur.ymin) / ctx.pixely);
  const float x_offset = (right - left - icon_size) * 0.5f;
  const float y_offset = (top - bottom - icon_size) * 0.5f;

  const float inv_scale_fac = (ICON_DEFAULT_HEIGHT / ICON_SIZE) * UI_INV_SCALE_FAC;

  UI_icon_draw_ex(left + x_offset,
                  bottom + y_offset,
                  icon_id,
                  inv_scale_fac,
                  1.0f,
                  0.0f,
                  color,
                  false,
                  UI_NO_ICON_OVERLAY_TEXT);

  /* Restore view matrix. */
  UI_view2d_view_restore(ctx.C);
}

static void draw_strip_icons(const TimelineDrawContext &ctx,
                             const Vector<StripDrawContext> &strips)
{
  const float icon_size_x = ICON_SIZE * ctx.pixelx * UI_SCALE_FAC;

  for (const StripDrawContext &strip : strips) {
    const bool missing_data = strip.missing_data_block;
    const bool missing_media = strip.missing_media;
    const bool is_connected = strip.is_connected;
    if (!missing_data && !missing_media && !is_connected) {
      continue;
    }

    /* Draw icon in the title bar area. */
    if ((ctx.sseq->flag & SEQ_SHOW_OVERLAY) != 0 && !strip.strip_is_too_small) {
      uchar col[4];
      get_strip_text_color(strip, col);

      float icon_indent = 2.0f * strip.handle_width - 4 * ctx.pixelx * UI_SCALE_FAC;
      rctf rect;
      rect.ymin = strip.top - strip_header_size_get(ctx);
      rect.ymax = strip.top;
      rect.xmin = max_ff(strip.left_handle, ctx.v2d->cur.xmin) + icon_indent;
      if (missing_data) {
        rect.xmax = min_ff(strip.right_handle - strip.handle_width, rect.xmin + icon_size_x);
        draw_icon_centered(ctx, rect, ICON_LIBRARY_DATA_BROKEN, col);
        rect.xmin = rect.xmax;
      }
      if (missing_media) {
        rect.xmax = min_ff(strip.right_handle - strip.handle_width, rect.xmin + icon_size_x);
        draw_icon_centered(ctx, rect, ICON_ERROR, col);
        rect.xmin = rect.xmax;
      }
      if (is_connected) {
        rect.xmax = min_ff(strip.right_handle - strip.handle_width, rect.xmin + icon_size_x);
        draw_icon_centered(ctx, rect, ICON_LINKED, col);
      }
    }

    /* Draw icon in center of content. */
    if (strip.can_draw_strip_content && strip.strip->type != STRIP_TYPE_META) {
      rctf rect;
      rect.xmin = strip.left_handle + strip.handle_width;
      rect.xmax = strip.right_handle - strip.handle_width;
      rect.ymin = strip.bottom;
      rect.ymax = strip.strip_content_top;
      uchar col[4] = {112, 0, 0, 255};
      if (missing_data) {
        draw_icon_centered(ctx, rect, ICON_LIBRARY_DATA_BROKEN, col);
      }
      if (missing_media) {
        draw_icon_centered(ctx, rect, ICON_ERROR, col);
      }
    }
  }
}

/* Draw info text on a sequence strip. */
static void draw_seq_text_overlay(const TimelineDrawContext &ctx,
                                  const StripDrawContext &strip_ctx)
{
  if ((ctx.sseq->flag & SEQ_SHOW_OVERLAY) == 0) {
    return;
  }
  /* Draw text only if there is enough horizontal or vertical space. */
  if ((strip_ctx.strip_length <= 32 * ctx.pixelx * UI_SCALE_FAC) || strip_ctx.strip_is_too_small ||
      !strip_ctx.can_draw_text_overlay)
  {
    return;
  }

  char overlay_string[FILE_MAX];
  size_t overlay_string_len = draw_seq_text_get_overlay_string(
      ctx, strip_ctx, overlay_string, sizeof(overlay_string));

  if (overlay_string_len == 0) {
    return;
  }

  uchar col[4];
  get_strip_text_color(strip_ctx, col);

  float text_margin = 2.0f * strip_ctx.handle_width;
  rctf rect;
  rect.xmin = strip_ctx.left_handle + text_margin;
  rect.xmax = strip_ctx.right_handle - text_margin;
  rect.ymax = strip_ctx.top;
  /* Depending on the vertical space, draw text on top or in the center of strip. */
  rect.ymin = !strip_ctx.can_draw_strip_content ? strip_ctx.bottom : strip_ctx.strip_content_top;
  rect.xmin = max_ff(rect.xmin, ctx.v2d->cur.xmin + text_margin);
  int num_icons = 0;
  if (strip_ctx.missing_data_block) {
    num_icons++;
  }
  if (strip_ctx.missing_media) {
    num_icons++;
  }
  if (strip_ctx.is_connected) {
    num_icons++;
  }
  rect.xmin += num_icons * ICON_SIZE * ctx.pixelx * UI_SCALE_FAC;
  rect.xmin = min_ff(rect.xmin, ctx.v2d->cur.xmax);

  CLAMP(rect.xmax, ctx.v2d->cur.xmin + text_margin, ctx.v2d->cur.xmax);
  if (rect.xmin >= rect.xmax) { /* No space for label left. */
    return;
  }

  UI_view2d_text_cache_add_rectf(ctx.v2d, &rect, overlay_string, overlay_string_len, col);
}

static void draw_strip_offsets(const TimelineDrawContext &ctx, const StripDrawContext &strip_ctx)
{
  const Strip *strip = strip_ctx.strip;
  if ((ctx.sseq->flag & SEQ_SHOW_OVERLAY) == 0) {
    return;
  }
  if (strip_ctx.is_single_image || ctx.pixely <= 0) {
    return;
  }
  if ((ctx.sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_OFFSETS) == 0 &&
      (strip_ctx.strip != special_preview_get()) &&
      (strip_ctx.strip->runtime.flag & STRIP_SHOW_OFFSETS) == 0)
  {
    return;
  }

  const Scene *scene = ctx.scene;

  uchar col[4], blend_col[4];
  color3ubv_from_seq(scene, strip, strip_ctx.show_strip_color_tag, strip_ctx.is_muted, col);
  if (strip->flag & SELECT) {
    UI_GetColorPtrShade3ubv(col, 50, col);
  }
  col[3] = strip_ctx.is_muted ? MUTE_ALPHA : 200;
  UI_GetColorPtrShade3ubv(col, 10, blend_col);
  blend_col[3] = 255;

  if (strip_ctx.left_handle > strip_ctx.content_start) {
    ctx.quads->add_quad(strip_ctx.left_handle,
                        strip_ctx.bottom - ctx.pixely,
                        strip_ctx.content_start,
                        strip_ctx.bottom - STRIP_OFSBOTTOM,
                        col);
    ctx.quads->add_wire_quad(strip_ctx.left_handle,
                             strip_ctx.bottom - ctx.pixely,
                             strip_ctx.content_start,
                             strip_ctx.bottom - STRIP_OFSBOTTOM,
                             blend_col);
  }
  if (strip_ctx.right_handle < strip_ctx.content_end) {
    ctx.quads->add_quad(strip_ctx.right_handle,
                        strip_ctx.top + ctx.pixely,
                        strip_ctx.content_end,
                        strip_ctx.top + STRIP_OFSBOTTOM,
                        col);
    ctx.quads->add_wire_quad(strip_ctx.right_handle,
                             strip_ctx.top + ctx.pixely,
                             strip_ctx.content_end,
                             strip_ctx.top + STRIP_OFSBOTTOM,
                             blend_col);
  }
}

/**
 * Draw f-curves as darkened regions of the strip:
 * - Volume for sound strips.
 * - Opacity for the other types.
 */
static void draw_seq_fcurve_overlay(const TimelineDrawContext &ctx,
                                    const StripDrawContext &strip_ctx)
{
  if (!strip_ctx.can_draw_strip_content || (ctx.sseq->flag & SEQ_SHOW_OVERLAY) == 0 ||
      (ctx.sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_FCURVES) == 0)
  {
    return;
  }
  if (strip_ctx.curve == nullptr) {
    return;
  }

  const int eval_step = max_ii(1, floor(ctx.pixelx));
  uchar color[4] = {0, 0, 0, 38};

  /* Clamp curve evaluation to the editor's borders. */
  int eval_start = max_ff(strip_ctx.left_handle, ctx.v2d->cur.xmin);
  int eval_end = min_ff(strip_ctx.right_handle, ctx.v2d->cur.xmax + 1);
  if (eval_start >= eval_end) {
    return;
  }

  const float y_height = strip_ctx.top - strip_ctx.bottom;
  float prev_x = eval_start;
  float prev_val = evaluate_fcurve(strip_ctx.curve, eval_start);
  CLAMP(prev_val, 0.0f, 1.0f);
  bool skip = false;

  for (int timeline_frame = eval_start + eval_step; timeline_frame <= eval_end;
       timeline_frame += eval_step)
  {
    float curve_val = evaluate_fcurve(strip_ctx.curve, timeline_frame);
    CLAMP(curve_val, 0.0f, 1.0f);

    /* Avoid adding adjacent verts that have the same value. */
    if (curve_val == prev_val && timeline_frame < eval_end - eval_step) {
      skip = true;
      continue;
    }

    /* If some frames were skipped above, we need to close the shape. */
    if (skip) {
      ctx.quads->add_quad(prev_x,
                          (prev_val * y_height) + strip_ctx.bottom,
                          prev_x,
                          strip_ctx.top,
                          timeline_frame - eval_step,
                          (prev_val * y_height) + strip_ctx.bottom,
                          timeline_frame - eval_step,
                          strip_ctx.top,
                          color);
      skip = false;
      prev_x = timeline_frame - eval_step;
    }

    ctx.quads->add_quad(prev_x,
                        (prev_val * y_height) + strip_ctx.bottom,
                        prev_x,
                        strip_ctx.top,
                        timeline_frame,
                        (curve_val * y_height) + strip_ctx.bottom,
                        timeline_frame,
                        strip_ctx.top,
                        color);
    prev_x = timeline_frame;
    prev_val = curve_val;
  }
}

/* When active strip is a Multi-cam strip, highlight its source channel. */
static void draw_multicam_highlight(const TimelineDrawContext &ctx,
                                    const StripDrawContext &strip_ctx)
{
  Strip *act_strip = seq::select_active_get(ctx.scene);

  if (strip_ctx.strip != act_strip || act_strip == nullptr) {
    return;
  }
  if ((act_strip->flag & SELECT) == 0 || act_strip->type != STRIP_TYPE_MULTICAM) {
    return;
  }

  int channel = act_strip->multicam_source;

  if (channel == 0) {
    return;
  }

  View2D *v2d = ctx.v2d;
  uchar color[4] = {255, 255, 255, 48};
  ctx.quads->add_quad(v2d->cur.xmin, channel, v2d->cur.xmax, channel + 1, color);
}

/* Force redraw, when prefetching and using cache view. */
static void seq_prefetch_wm_notify(const bContext *C, Scene *scene)
{
  if (seq::prefetch_need_redraw(C, scene)) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, nullptr);
  }
}

static void draw_seq_timeline_channels(const TimelineDrawContext &ctx)
{
  View2D *v2d = ctx.v2d;
  UI_view2d_view_ortho(v2d);

  GPU_blend(GPU_BLEND_ALPHA);
  uchar4 color;
  UI_GetThemeColor4ubv(TH_ROW_ALTERNATE, color);

  /* Alternating horizontal stripes. */
  int i = max_ii(1, int(v2d->cur.ymin) - 1);
  while (i < v2d->cur.ymax) {
    if (i & 1) {
      ctx.quads->add_quad(v2d->cur.xmin, i, v2d->cur.xmax, i + 1, color);
    }
    i++;
  }

  ctx.quads->draw();
  GPU_blend(GPU_BLEND_NONE);
}

/* Get visible strips into two sets: regular strips, and strips
 * that are dragged over other strips right now (e.g. dragging
 * selection in the timeline). This is to make the dragged strips
 * always render "on top" of others. */
static void visible_strips_ordered_get(const TimelineDrawContext &ctx,
                                       Vector<StripDrawContext> &r_bottom_layer,
                                       Vector<StripDrawContext> &r_top_layer)
{
  r_bottom_layer.clear();
  r_top_layer.clear();

  Vector<Strip *> strips = sequencer_visible_strips_get(ctx.C);
  r_bottom_layer.reserve(strips.size());

  for (Strip *strip : strips) {
    StripDrawContext strip_ctx = strip_draw_context_get(ctx, strip);
    if ((strip->runtime.flag & STRIP_OVERLAP) == 0) {
      r_bottom_layer.append(strip_ctx);
    }
    else {
      r_top_layer.append(strip_ctx);
    }
  }

  /* Finding which curves (if any) drive a strip is expensive, do these lookups
   * in parallel. */
  threading::parallel_for(IndexRange(r_bottom_layer.size()), 64, [&](IndexRange range) {
    for (int64_t index : range) {
      strip_draw_context_curve_get(ctx, r_bottom_layer[index]);
    }
  });
  threading::parallel_for(IndexRange(r_top_layer.size()), 64, [&](IndexRange range) {
    for (int64_t index : range) {
      strip_draw_context_curve_get(ctx, r_top_layer[index]);
    }
  });
}

static void draw_strips_background(const TimelineDrawContext &ctx,
                                   StripsDrawBatch &strips_batch,
                                   const Vector<StripDrawContext> &strips)
{
  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(ctx.region);

  GPU_blend(GPU_BLEND_ALPHA_PREMULT);

  const bool show_overlay = (ctx.sseq->flag & SEQ_SHOW_OVERLAY) != 0;
  const Scene *scene = ctx.scene;
  for (const StripDrawContext &strip : strips) {
    SeqStripDrawData &data = strips_batch.add_strip(strip.content_start,
                                                    strip.content_end,
                                                    strip.top,
                                                    strip.bottom,
                                                    strip.strip_content_top,
                                                    strip.left_handle,
                                                    strip.right_handle,
                                                    strip.handle_width,
                                                    strip.is_single_image);

    /* Background color. */
    uchar col[4];
    data.flags |= GPU_SEQ_FLAG_BACKGROUND;
    color3ubv_from_seq(scene, strip.strip, strip.show_strip_color_tag, strip.is_muted, col);
    col[3] = strip.is_muted ? MUTE_ALPHA : 255;
    /* Muted strips: turn almost gray. */
    if (strip.is_muted) {
      uchar muted_color[3] = {128, 128, 128};
      UI_GetColorPtrBlendShade3ubv(col, muted_color, 0.5f, 0, col);
    }
    data.col_background = color_pack(col);

    /* Color band state. */
    if (show_overlay && (strip.strip->type == STRIP_TYPE_COLOR)) {
      data.flags |= GPU_SEQ_FLAG_COLOR_BAND;
      SolidColorVars *colvars = (SolidColorVars *)strip.strip->effectdata;
      rgb_float_to_uchar(col, colvars->col);
      data.col_color_band = color_pack(col);
    }

    /* Transition state. */
    if (show_overlay && strip.can_draw_strip_content &&
        seq::effect_is_transition(StripType(strip.strip->type)))
    {
      data.flags |= GPU_SEQ_FLAG_TRANSITION;

      const Strip *input1 = strip.strip->input1;
      const Strip *input2 = strip.strip->input2;

      /* Left side. */
      if (input1->type == STRIP_TYPE_COLOR) {
        rgb_float_to_uchar(col, ((const SolidColorVars *)input1->effectdata)->col);
      }
      else {
        color3ubv_from_seq(scene, input1, strip.show_strip_color_tag, strip.is_muted, col);
      }
      data.col_transition_in = color_pack(col);

      /* Right side. */
      if (input2->type == STRIP_TYPE_COLOR) {
        rgb_float_to_uchar(col, ((const SolidColorVars *)input2->effectdata)->col);
      }
      else {
        color3ubv_from_seq(scene, input2, strip.show_strip_color_tag, strip.is_muted, col);
        /* If the transition inputs are of the same type, draw the right side slightly darker. */
        if (input1->type == input2->type) {
          UI_GetColorPtrShade3ubv(col, -15, col);
        }
      }
      data.col_transition_out = color_pack(col);
    }
  }
  strips_batch.flush_batch();
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_matrix_pop_projection();
}

static void strip_data_missing_media_flags_set(const StripDrawContext &strip,
                                               SeqStripDrawData &data)
{
  if (strip.missing_data_block || strip.missing_media) {
    /* Do not tint title area for muted strips; we want to see gray for them. */
    if (!strip.is_muted) {
      data.flags |= GPU_SEQ_FLAG_MISSING_TITLE;
    }
    /* Do not tint content area for meta strips; we want to display children. */
    if (strip.strip->type != STRIP_TYPE_META) {
      data.flags |= GPU_SEQ_FLAG_MISSING_CONTENT;
    }
  }
}

static void strip_data_lock_flags_set(const StripDrawContext &strip,
                                      const TimelineDrawContext &ctx,
                                      SeqStripDrawData &data)
{
  if (seq::transform_is_locked(ctx.channels, strip.strip)) {
    data.flags |= GPU_SEQ_FLAG_LOCKED;
  }
}

static void strip_data_outline_params_set(const StripDrawContext &strip,
                                          const TimelineDrawContext &ctx,
                                          SeqStripDrawData &data)
{
  const bool active = strip.is_active_strip;
  const bool selected = strip.strip->flag & SELECT;
  uchar4 col{0, 0, 0, 255};

  if (selected) {
    UI_GetThemeColor3ubv(TH_SEQ_SELECTED, col);
    data.flags |= GPU_SEQ_FLAG_SELECTED;
  }
  if (active) {
    if (selected) {
      UI_GetThemeColor3ubv(TH_SEQ_ACTIVE, col);
    }
    else {
      UI_GetThemeColorShade3ubv(TH_SEQ_ACTIVE, -40, col);
    }
    data.flags |= GPU_SEQ_FLAG_ACTIVE;
  }
  if (!selected && !active) {
    /* Color for unselected strips is a bit darker than the background. */
    UI_GetThemeColorShade3ubv(TH_BACK, -40, col);
  }

  const bool translating = (G.moving & G_TRANSFORM_SEQ);

  const eSeqOverlapMode overlap_mode = seq::tool_settings_overlap_mode_get(ctx.scene);
  const bool use_overwrite = overlap_mode == SEQ_OVERLAP_OVERWRITE;
  const bool overlaps = (strip.strip->runtime.flag & STRIP_OVERLAP) && translating;

  const bool clamped_l = (strip.strip->runtime.flag & STRIP_CLAMPED_LH);
  const bool clamped_r = (strip.strip->runtime.flag & STRIP_CLAMPED_RH);

  /* Strip outline is:
   *  - Red when overlapping with other strips or handles are clamped.
   *  - Slightly lighter while translating strips. */
  if ((translating && overlaps && !use_overwrite) || clamped_l || clamped_r) {
    col[0] = 255;
    col[1] = col[2] = 33;
    data.flags |= GPU_SEQ_FLAG_OVERLAP;
  }
  else if (translating && selected) {
    UI_GetColorPtrShade3ubv(col, 70, col);
  }

  data.col_outline = color_pack(col);
}

static void strip_data_highlight_flags_set(const StripDrawContext &strip,
                                           const TimelineDrawContext &ctx,
                                           SeqStripDrawData &data)
{
  const Strip *act_strip = seq::select_active_get(ctx.scene);
  const Strip *special_preview = special_preview_get();
  /* Highlight if strip is an input of an active strip, or if the strip is solo preview. */
  if (act_strip != nullptr && (act_strip->flag & SELECT) != 0) {
    if (act_strip->input1 == strip.strip || act_strip->input2 == strip.strip) {
      data.flags |= GPU_SEQ_FLAG_HIGHLIGHT;
    }
  }
  if (special_preview == strip.strip) {
    data.flags |= GPU_SEQ_FLAG_HIGHLIGHT;
  }
}

static void strip_data_handle_flags_set(const StripDrawContext &strip,
                                        const TimelineDrawContext &ctx,
                                        SeqStripDrawData &data)
{
  const Scene *scene = ctx.scene;
  const bool selected = strip.strip->flag & SELECT;
  /* Handles on left/right side. */
  if (!seq::transform_is_locked(ctx.channels, strip.strip) &&
      can_select_handle(scene, strip.strip, ctx.v2d))
  {
    const bool selected_l = selected && handle_is_selected(strip.strip, STRIP_HANDLE_LEFT);
    const bool selected_r = selected && handle_is_selected(strip.strip, STRIP_HANDLE_RIGHT);
    if (selected_l) {
      data.flags |= GPU_SEQ_FLAG_SELECTED_LH;
    }
    if (selected_r) {
      data.flags |= GPU_SEQ_FLAG_SELECTED_RH;
    }
  }
}

static void draw_strips_foreground(const TimelineDrawContext &ctx,
                                   StripsDrawBatch &strips_batch,
                                   const Vector<StripDrawContext> &strips)
{
  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(ctx.region);
  GPU_blend(GPU_BLEND_ALPHA_PREMULT);

  for (const StripDrawContext &strip : strips) {
    SeqStripDrawData &data = strips_batch.add_strip(strip.content_start,
                                                    strip.content_end,
                                                    strip.top,
                                                    strip.bottom,
                                                    strip.strip_content_top,
                                                    strip.left_handle,
                                                    strip.right_handle,
                                                    strip.handle_width,
                                                    strip.is_single_image);
    data.flags |= GPU_SEQ_FLAG_BORDER;
    strip_data_missing_media_flags_set(strip, data);
    strip_data_lock_flags_set(strip, ctx, data);
    strip_data_handle_flags_set(strip, ctx, data);
    strip_data_outline_params_set(strip, ctx, data);
    strip_data_highlight_flags_set(strip, ctx, data);
  }

  strips_batch.flush_batch();
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_matrix_pop_projection();
}

static void draw_retiming_continuity_ranges(const TimelineDrawContext &ctx,
                                            const Vector<StripDrawContext> &strips)
{
  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(ctx.region);

  for (const StripDrawContext &strip_ctx : strips) {
    sequencer_retiming_draw_continuity(ctx, strip_ctx);
  }
  ctx.quads->draw();

  GPU_matrix_pop_projection();
}

static void draw_seq_strips(const TimelineDrawContext &ctx,
                            StripsDrawBatch &strips_batch,
                            const Vector<StripDrawContext> &strips)
{
  if (strips.is_empty()) {
    return;
  }

  UI_view2d_view_ortho(ctx.v2d);

  /* Draw parts of strips below thumbnails. */
  draw_strips_background(ctx, strips_batch, strips);

  GPU_blend(GPU_BLEND_ALPHA);
  const float round_radius = calc_strip_round_radius(ctx.pixely);
  for (const StripDrawContext &strip_ctx : strips) {
    draw_strip_offsets(ctx, strip_ctx);
    drawmeta_contents(ctx, strip_ctx, round_radius);
  }
  ctx.quads->draw();

  /* Draw thumbnails. */
  draw_strip_thumbnails(ctx, strips_batch, strips);

  /* Draw parts of strips above thumbnails. */
  GPU_blend(GPU_BLEND_ALPHA);
  for (const StripDrawContext &strip_ctx : strips) {
    draw_seq_fcurve_overlay(ctx, strip_ctx);
    draw_seq_waveform_overlay(ctx, strip_ctx);
    draw_multicam_highlight(ctx, strip_ctx);
    draw_handle_transform_text(ctx, strip_ctx, STRIP_HANDLE_LEFT);
    draw_handle_transform_text(ctx, strip_ctx, STRIP_HANDLE_RIGHT);
    draw_seq_text_overlay(ctx, strip_ctx);
    sequencer_retiming_speed_draw(ctx, strip_ctx);
  }
  ctx.quads->draw();

  /* Draw retiming continuity ranges. */
  draw_retiming_continuity_ranges(ctx, strips);
  sequencer_retiming_keys_draw(ctx, strips);

  draw_strips_foreground(ctx, strips_batch, strips);

  /* Draw icons. */
  draw_strip_icons(ctx, strips);

  /* Draw text labels. */
  UI_view2d_text_cache_draw(ctx.region);
  GPU_blend(GPU_BLEND_NONE);
}

static void draw_seq_strips(const TimelineDrawContext &ctx, StripsDrawBatch &strips_batch)
{
  if (ctx.ed == nullptr) {
    return;
  }

  /* Discard thumbnail requests that are far enough from viewing area:
   * by +- 30 frames and +-2 channels outside of current view. */
  rctf rect = ctx.v2d->cur;
  rect.xmin -= 30;
  rect.xmax += 30;
  rect.ymin -= 2;
  rect.ymax += 2;
  seq::thumbnail_cache_discard_requests_outside(ctx.scene, rect);
  seq::thumbnail_cache_maintain_capacity(ctx.scene);

  Vector<StripDrawContext> bottom_layer, top_layer;
  visible_strips_ordered_get(ctx, bottom_layer, top_layer);
  draw_seq_strips(ctx, strips_batch, bottom_layer);
  draw_seq_strips(ctx, strips_batch, top_layer);
}

static void draw_timeline_sfra_efra(const TimelineDrawContext &ctx)
{
  const Scene *scene = ctx.scene;
  if (!scene) {
    return;
  }
  const View2D *v2d = ctx.v2d;
  const Editing *ed = seq::editing_get(scene);
  const int frame_sta = scene->r.sfra;
  const int frame_end = scene->r.efra + 1;

  GPU_blend(GPU_BLEND_ALPHA);

  /* Draw overlay outside of frame range. */
  uchar4 color;
  UI_GetThemeColorShadeAlpha4ubv(TH_BACK, -10, -100, color);

  if (frame_sta < frame_end) {
    ctx.quads->add_quad(v2d->cur.xmin, v2d->cur.ymin, float(frame_sta), v2d->cur.ymax, color);
    ctx.quads->add_quad(float(frame_end), v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax, color);
  }
  else {
    ctx.quads->add_quad(v2d->cur.xmin, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax, color);
  }

  /* Draw frame range boundary. */
  UI_GetThemeColorShade4ubv(TH_BACK, -60, color);

  ctx.quads->add_line(frame_end, v2d->cur.ymin, frame_end, v2d->cur.ymax, color);
  ctx.quads->add_line(frame_end, v2d->cur.ymin, frame_end, v2d->cur.ymax, color);

  ctx.quads->draw();

  /* While in meta strip, draw a checkerboard overlay outside of frame range. */
  if (ed && !BLI_listbase_is_empty(&ed->metastack)) {
    const MetaStack *ms = static_cast<const MetaStack *>(ed->metastack.last);

    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);
    immBindBuiltinProgram(GPU_SHADER_2D_CHECKER);

    immUniform4f("color1", 0.0f, 0.0f, 0.0f, 0.22f);
    immUniform4f("color2", 1.0f, 1.0f, 1.0f, 0.0f);
    immUniform1i("size", 8);

    immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, ms->disp_range[0], v2d->cur.ymax);
    immRectf(pos, ms->disp_range[1], v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);

    immUnbindProgram();

    UI_GetThemeColorShade4ubv(TH_BACK, -40, color);
    ctx.quads->add_line(ms->disp_range[0], v2d->cur.ymin, ms->disp_range[0], v2d->cur.ymax, color);
    ctx.quads->add_line(ms->disp_range[1], v2d->cur.ymin, ms->disp_range[1], v2d->cur.ymax, color);
    ctx.quads->draw();
  }

  GPU_blend(GPU_BLEND_NONE);
}

struct CacheDrawData {
  const View2D *v2d;
  float stripe_ofs_y;
  float stripe_ht;
  SeqQuadsBatch *quads;
};

/* Draw final cache entries on top of the timeline. */
static void draw_cache_final_iter_fn(void *userdata, int timeline_frame)
{
  CacheDrawData *drawdata = static_cast<CacheDrawData *>(userdata);

  /* Same as movie clip cache color, see ED_region_cache_draw_cached_segments. */
  const uchar4 col{108, 108, 210, 255};

  const View2D *v2d = drawdata->v2d;
  float stripe_top = v2d->cur.ymax - (UI_TIME_SCRUB_MARGIN_Y / UI_view2d_scale_get_y(v2d));
  float stripe_bot = stripe_top - (UI_TIME_CACHE_MARGIN_Y / UI_view2d_scale_get_y(v2d));
  drawdata->quads->add_quad(timeline_frame, stripe_bot, timeline_frame + 1, stripe_top, col);
}

/* Draw source cache entries at bottom of the strips. */
static void draw_cache_source_iter_fn(void *userdata, const Strip *strip, int timeline_frame)
{
  CacheDrawData *drawdata = static_cast<CacheDrawData *>(userdata);

  const uchar4 col{255, 25, 5, 100};
  float stripe_bot = strip->channel + STRIP_OFSBOTTOM + drawdata->stripe_ofs_y;
  float stripe_top = stripe_bot + drawdata->stripe_ht;
  drawdata->quads->add_quad(timeline_frame, stripe_bot, timeline_frame + 1, stripe_top, col);
}

static void draw_cache_stripe(const Scene *scene,
                              const Strip *strip,
                              SeqQuadsBatch &quads,
                              const float stripe_bot,
                              const float stripe_ht,
                              const uchar color[4])
{
  quads.add_quad(seq::time_left_handle_frame_get(scene, strip),
                 stripe_bot,
                 seq::time_right_handle_frame_get(scene, strip),
                 stripe_bot + stripe_ht,
                 color);
}

static void draw_cache_background(const bContext *C, const CacheDrawData *draw_data)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  const View2D *v2d = UI_view2d_fromcontext(C);
  const SpaceSeq *sseq = CTX_wm_space_seq(C);

  /* NOTE: Final bg color is the same as the movie clip cache color.
   * See ED_region_cache_draw_background.
   */
  const uchar4 bg_final{78, 78, 145, 255};
  const uchar4 bg_raw{255, 25, 5, 25};

  float stripe_bot;
  bool dev_ui = (U.flag & USER_DEVELOPER_UI);

  if (sseq->cache_overlay.flag & SEQ_CACHE_SHOW_FINAL_OUT) {
    /* Draw the final cache on top of the timeline */
    float stripe_top = v2d->cur.ymax - (UI_TIME_SCRUB_MARGIN_Y / UI_view2d_scale_get_y(v2d));
    stripe_bot = stripe_top - (UI_TIME_CACHE_MARGIN_Y / UI_view2d_scale_get_y(v2d));

    draw_data->quads->add_quad(scene->r.sfra, stripe_bot, scene->r.efra, stripe_top, bg_final);
  }

  if (!dev_ui) {
    /* Don't show these cache types below unless developer extras is on. */
    return;
  }

  Vector<Strip *> strips = sequencer_visible_strips_get(C);
  strips.remove_if([&](const Strip *strip) { return strip->type == STRIP_TYPE_SOUND_RAM; });

  for (const Strip *strip : strips) {
    stripe_bot = strip->channel + STRIP_OFSBOTTOM + draw_data->stripe_ofs_y;
    if (sseq->cache_overlay.flag & SEQ_CACHE_SHOW_RAW) {
      draw_cache_stripe(scene, strip, *draw_data->quads, stripe_bot, draw_data->stripe_ht, bg_raw);
    }
  }
}

static void draw_cache_view(const bContext *C)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  const View2D *v2d = UI_view2d_fromcontext(C);
  const SpaceSeq *sseq = CTX_wm_space_seq(C);

  if ((sseq->flag & SEQ_SHOW_OVERLAY) == 0 || (sseq->cache_overlay.flag & SEQ_CACHE_SHOW) == 0) {
    return;
  }

  float stripe_ofs_y = UI_view2d_region_to_view_y(v2d, 1.0f) - v2d->cur.ymin;
  float stripe_ht = UI_view2d_region_to_view_y(v2d, 4.0f * UI_SCALE_FAC * U.pixelsize) -
                    v2d->cur.ymin;

  CLAMP_MAX(stripe_ht, 0.2f);
  CLAMP_MIN(stripe_ofs_y, stripe_ht / 2);

  SeqQuadsBatch quads;
  CacheDrawData userdata;
  userdata.v2d = v2d;
  userdata.stripe_ofs_y = stripe_ofs_y;
  userdata.stripe_ht = stripe_ht;
  userdata.quads = &quads;

  GPU_blend(GPU_BLEND_ALPHA);

  draw_cache_background(C, &userdata);
  if (sseq->cache_overlay.flag & SEQ_CACHE_SHOW_FINAL_OUT) {
    seq::final_image_cache_iterate(scene, &userdata, draw_cache_final_iter_fn);
  }
  if ((U.flag & USER_DEVELOPER_UI) && (sseq->cache_overlay.flag & SEQ_CACHE_SHOW_RAW)) {
    seq::source_image_cache_iterate(scene, &userdata, draw_cache_source_iter_fn);
  }

  quads.draw();
  GPU_blend(GPU_BLEND_NONE);
}

static void draw_overlay_frame_indicator(const Scene *scene, const View2D *v2d)
{
  int overlay_frame = (scene->ed->overlay_frame_flag & SEQ_EDIT_OVERLAY_FRAME_ABS) ?
                          scene->ed->overlay_frame_abs :
                          scene->r.cfra + scene->ed->overlay_frame_ofs;

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);
  /* Shader may have color set from past usage - reset it. */
  immUniform1i("colors_len", 0);
  immUniform1f("dash_width", 20.0f * U.pixelsize);
  immUniform1f("udash_factor", 0.5f);
  immUniformThemeColor(TH_CFRAME);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(pos, overlay_frame, v2d->cur.ymin);
  immVertex2f(pos, overlay_frame, v2d->cur.ymax);
  immEnd();

  immUnbindProgram();
}

static void draw_timeline_grid(const TimelineDrawContext &ctx)
{
  if ((ctx.sseq->flag & SEQ_SHOW_OVERLAY) == 0 ||
      (ctx.sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_GRID) == 0)
  {
    return;
  }

  const Scene *scene = ctx.scene;
  if (scene == nullptr) {
    /* If we don't have a scene available, pick what we defined as default for framerate to show
     * *something*. */
    scene = DNA_struct_default_get(Scene);
  }
  UI_view2d_draw_lines_x__discrete_frames_or_seconds(
      ctx.v2d, scene, (ctx.sseq->flag & SEQ_DRAWFRAMES) == 0, false);
}

static void draw_timeline_markers(const TimelineDrawContext &ctx)
{
  if (!ED_markers_region_visible(CTX_wm_area(ctx.C), ctx.region)) {
    return;
  }
  if (ctx.scene == nullptr) {
    return;
  }

  UI_view2d_view_orthoSpecial(ctx.region, ctx.v2d, true);
  ED_markers_draw(ctx.C, DRAW_MARKERS_MARGIN);
}

static void draw_timeline_gizmos(const TimelineDrawContext &ctx)
{
  if ((ctx.sseq->gizmo_flag & SEQ_GIZMO_HIDE) != 0) {
    return;
  }

  WM_gizmomap_draw(ctx.region->runtime->gizmo_map, ctx.C, WM_GIZMOMAP_DRAWSTEP_2D);
}

static void draw_timeline_pre_view_callbacks(const TimelineDrawContext &ctx)
{
  GPU_framebuffer_bind_no_srgb(ctx.framebuffer_overlay);
  GPU_depth_test(GPU_DEPTH_NONE);
  GPU_framebuffer_bind(ctx.framebuffer_overlay);
  ED_region_draw_cb_draw(ctx.C, ctx.region, REGION_DRAW_PRE_VIEW);
  GPU_framebuffer_bind_no_srgb(ctx.framebuffer_overlay);
}

static void draw_timeline_post_view_callbacks(const TimelineDrawContext &ctx)
{
  GPU_framebuffer_bind(ctx.framebuffer_overlay);
  ED_region_draw_cb_draw(ctx.C, ctx.region, REGION_DRAW_POST_VIEW);
  GPU_framebuffer_bind_no_srgb(ctx.framebuffer_overlay);
}

void draw_timeline_seq(const bContext *C, const ARegion *region)
{
  SeqQuadsBatch quads_batch;
  TimelineDrawContext ctx = timeline_draw_context_get(C, &quads_batch);
  StripsDrawBatch strips_batch(ctx.v2d);

  draw_timeline_pre_view_callbacks(ctx);
  UI_ThemeClearColor(TH_BACK);
  draw_seq_timeline_channels(ctx);
  draw_timeline_grid(ctx);
  draw_timeline_sfra_efra(ctx);
  draw_seq_strips(ctx, strips_batch);
  draw_timeline_markers(ctx);
  UI_view2d_view_ortho(ctx.v2d);
  if (ctx.scene) {
    ANIM_draw_previewrange(ctx.scene, ctx.v2d, 1);
  }
  UI_view2d_view_restore(C);
  draw_timeline_gizmos(ctx);
  draw_timeline_post_view_callbacks(ctx);
  if (ctx.scene) {
    const int fps = round_db_to_int(ctx.scene->frames_per_second());
    ED_time_scrub_draw(region, ctx.scene, !(ctx.sseq->flag & SEQ_DRAWFRAMES), true, fps);
  }

  if (ctx.scene) {
    seq_prefetch_wm_notify(C, ctx.scene);
  }
}

void draw_timeline_seq_display(const bContext *C, ARegion *region)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return;
  }
  const SpaceSeq *sseq = CTX_wm_space_seq(C);
  View2D *v2d = &region->v2d;

  if (scene->ed != nullptr) {
    UI_view2d_view_ortho(v2d);
    draw_cache_view(C);
    if (scene->ed->overlay_frame_flag & SEQ_EDIT_OVERLAY_FRAME_SHOW) {
      draw_overlay_frame_indicator(scene, v2d);
    }
    UI_view2d_view_restore(C);
  }

  ED_time_scrub_draw_current_frame(
      region, scene, !(sseq->flag & SEQ_DRAWFRAMES), region->winy >= UI_ANIM_MINY);

  if (region->winy > UI_ANIM_MINY) {
    if (const Editing *ed = seq::editing_get(scene)) {
      const ListBase *seqbase = seq::active_seqbase_get(ed);
      seq::timeline_boundbox(scene, seqbase, &v2d->tot);
      const rcti scroller_mask = ED_time_scrub_clamp_scroller_mask(v2d->mask);
      region->v2d.scroll |= V2D_SCROLL_BOTTOM;
      UI_view2d_scrollers_draw(v2d, &scroller_mask);
    }
  }
  else {
    region->v2d.scroll &= ~V2D_SCROLL_BOTTOM;
  }
}

}  // namespace blender::ed::vse
