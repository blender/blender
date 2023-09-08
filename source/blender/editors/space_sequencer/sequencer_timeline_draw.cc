/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include <cmath>
#include <cstring>

#include "BLI_blenlib.h"
#include "BLI_string_utils.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_sound.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_vertex_buffer.h"
#include "GPU_viewport.h"

#include "ED_anim_api.hh"
#include "ED_markers.hh"
#include "ED_mask.hh"
#include "ED_screen.hh"
#include "ED_sequencer.hh"
#include "ED_space_api.hh"
#include "ED_time_scrub_ui.hh"
#include "ED_util.hh"

#include "RNA_prototypes.h"

#include "SEQ_channels.h"
#include "SEQ_effects.h"
#include "SEQ_iterator.h"
#include "SEQ_prefetch.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_select.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"
#include "SEQ_utils.h"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "BLF_api.h"

#include "MEM_guardedalloc.h"

/* Own include. */
#include "sequencer_intern.hh"

#define SEQ_LEFTHANDLE 1
#define SEQ_RIGHTHANDLE 2
#define SEQ_HANDLE_SIZE 8.0f
#define SEQ_SCROLLER_TEXT_OFFSET 8
#define MUTE_ALPHA 120

static Sequence *special_seq_update = nullptr;

void color3ubv_from_seq(const Scene *curscene,
                        const Sequence *seq,
                        const bool show_strip_color_tag,
                        uchar r_col[3])
{
  Editing *ed = SEQ_editing_get(curscene);
  ListBase *channels = SEQ_channels_displayed_get(ed);

  if (show_strip_color_tag && uint(seq->color_tag) < SEQUENCE_COLOR_TOT &&
      seq->color_tag != SEQUENCE_COLOR_NONE)
  {
    bTheme *btheme = UI_GetTheme();
    const ThemeStripColor *strip_color = &btheme->strip_color[seq->color_tag];
    copy_v3_v3_uchar(r_col, strip_color->color);
    return;
  }

  uchar blendcol[3];

  /* Sometimes the active theme is not the sequencer theme, e.g. when an operator invokes the file
   * browser. This makes sure we get the right color values for the theme. */
  bThemeState theme_state;
  UI_Theme_Store(&theme_state);
  UI_SetTheme(SPACE_SEQ, RGN_TYPE_WINDOW);

  switch (seq->type) {
    case SEQ_TYPE_IMAGE:
      UI_GetThemeColor3ubv(TH_SEQ_IMAGE, r_col);
      break;

    case SEQ_TYPE_META:
      UI_GetThemeColor3ubv(TH_SEQ_META, r_col);
      break;

    case SEQ_TYPE_MOVIE:
      UI_GetThemeColor3ubv(TH_SEQ_MOVIE, r_col);
      break;

    case SEQ_TYPE_MOVIECLIP:
      UI_GetThemeColor3ubv(TH_SEQ_MOVIECLIP, r_col);
      break;

    case SEQ_TYPE_MASK:
      UI_GetThemeColor3ubv(TH_SEQ_MASK, r_col);
      break;

    case SEQ_TYPE_SCENE:
      UI_GetThemeColor3ubv(TH_SEQ_SCENE, r_col);

      if (seq->scene == curscene) {
        UI_GetColorPtrShade3ubv(r_col, r_col, 20);
      }
      break;

    /* Transitions use input colors, fallback for when the input is a transition itself. */
    case SEQ_TYPE_CROSS:
    case SEQ_TYPE_GAMCROSS:
    case SEQ_TYPE_WIPE:
      UI_GetThemeColor3ubv(TH_SEQ_TRANSITION, r_col);

      /* Slightly offset hue to distinguish different transition types. */
      if (seq->type == SEQ_TYPE_GAMCROSS) {
        rgb_byte_set_hue_float_offset(r_col, 0.03);
      }
      else if (seq->type == SEQ_TYPE_WIPE) {
        rgb_byte_set_hue_float_offset(r_col, 0.06);
      }
      break;

    /* Effects. */
    case SEQ_TYPE_TRANSFORM:
    case SEQ_TYPE_SPEED:
    case SEQ_TYPE_ADD:
    case SEQ_TYPE_SUB:
    case SEQ_TYPE_MUL:
    case SEQ_TYPE_ALPHAOVER:
    case SEQ_TYPE_ALPHAUNDER:
    case SEQ_TYPE_OVERDROP:
    case SEQ_TYPE_GLOW:
    case SEQ_TYPE_MULTICAM:
    case SEQ_TYPE_ADJUSTMENT:
    case SEQ_TYPE_GAUSSIAN_BLUR:
    case SEQ_TYPE_COLORMIX:
      UI_GetThemeColor3ubv(TH_SEQ_EFFECT, r_col);

      /* Slightly offset hue to distinguish different effects. */
      if (seq->type == SEQ_TYPE_ADD) {
        rgb_byte_set_hue_float_offset(r_col, 0.03);
      }
      else if (seq->type == SEQ_TYPE_SUB) {
        rgb_byte_set_hue_float_offset(r_col, 0.06);
      }
      else if (seq->type == SEQ_TYPE_MUL) {
        rgb_byte_set_hue_float_offset(r_col, 0.13);
      }
      else if (seq->type == SEQ_TYPE_ALPHAOVER) {
        rgb_byte_set_hue_float_offset(r_col, 0.16);
      }
      else if (seq->type == SEQ_TYPE_ALPHAUNDER) {
        rgb_byte_set_hue_float_offset(r_col, 0.23);
      }
      else if (seq->type == SEQ_TYPE_OVERDROP) {
        rgb_byte_set_hue_float_offset(r_col, 0.26);
      }
      else if (seq->type == SEQ_TYPE_COLORMIX) {
        rgb_byte_set_hue_float_offset(r_col, 0.33);
      }
      else if (seq->type == SEQ_TYPE_GAUSSIAN_BLUR) {
        rgb_byte_set_hue_float_offset(r_col, 0.43);
      }
      else if (seq->type == SEQ_TYPE_GLOW) {
        rgb_byte_set_hue_float_offset(r_col, 0.46);
      }
      else if (seq->type == SEQ_TYPE_ADJUSTMENT) {
        rgb_byte_set_hue_float_offset(r_col, 0.55);
      }
      else if (seq->type == SEQ_TYPE_SPEED) {
        rgb_byte_set_hue_float_offset(r_col, 0.65);
      }
      else if (seq->type == SEQ_TYPE_TRANSFORM) {
        rgb_byte_set_hue_float_offset(r_col, 0.75);
      }
      else if (seq->type == SEQ_TYPE_MULTICAM) {
        rgb_byte_set_hue_float_offset(r_col, 0.85);
      }
      break;

    case SEQ_TYPE_COLOR:
      UI_GetThemeColor3ubv(TH_SEQ_COLOR, r_col);
      break;

    case SEQ_TYPE_SOUND_RAM:
      UI_GetThemeColor3ubv(TH_SEQ_AUDIO, r_col);
      blendcol[0] = blendcol[1] = blendcol[2] = 128;
      if (SEQ_render_is_muted(channels, seq)) {
        UI_GetColorPtrBlendShade3ubv(r_col, blendcol, r_col, 0.5, 20);
      }
      break;

    case SEQ_TYPE_TEXT:
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

struct WaveVizData {
  float pos[2];
  float rms_pos;
  bool clip;
  bool draw_line;    /* Draw triangle otherwise. */
  bool final_sample; /* There are no more samples. */
};

static bool seq_draw_waveforms_poll(const bContext * /*C*/, SpaceSeq *sseq, Sequence *seq)
{
  const bool strip_is_valid = seq->type == SEQ_TYPE_SOUND_RAM && seq->sound != nullptr;
  const bool overlays_enabled = (sseq->flag & SEQ_SHOW_OVERLAY) != 0;
  const bool ovelay_option = ((sseq->timeline_overlay.flag & SEQ_TIMELINE_ALL_WAVEFORMS) != 0 ||
                              (seq->flag & SEQ_AUDIO_DRAW_WAVEFORM));

  if ((sseq->timeline_overlay.flag & SEQ_TIMELINE_NO_WAVEFORMS) != 0) {
    return false;
  }

  if (strip_is_valid && overlays_enabled && ovelay_option) {
    return true;
  }

  return false;
}

static void waveform_job_start_if_needed(const bContext *C, Sequence *seq)
{
  bSound *sound = seq->sound;

  BLI_spin_lock(static_cast<SpinLock *>(sound->spinlock));
  if (!sound->waveform) {
    /* Load the waveform data if it hasn't been loaded and cached already. */
    if (!(sound->tags & SOUND_TAGS_WAVEFORM_LOADING)) {
      /* Prevent sounds from reloading. */
      sound->tags |= SOUND_TAGS_WAVEFORM_LOADING;
      BLI_spin_unlock(static_cast<SpinLock *>(sound->spinlock));
      sequencer_preview_add_sound(C, seq);
    }
    else {
      BLI_spin_unlock(static_cast<SpinLock *>(sound->spinlock));
    }
  }
  BLI_spin_unlock(static_cast<SpinLock *>(sound->spinlock));
}

static size_t get_vertex_count(WaveVizData *waveform_data)
{
  bool draw_line = waveform_data->draw_line;
  size_t length = 0;

  while (waveform_data->draw_line == draw_line && !waveform_data->final_sample) {
    waveform_data++;
    length++;
  }

  return length;
}

static size_t draw_waveform_segment(WaveVizData *waveform_data, bool use_rms)
{
  size_t vertices_done = 0;
  size_t vertex_count = get_vertex_count(waveform_data);

  /* Not enough data to draw. */
  if (vertex_count <= 2) {
    return vertex_count;
  }

  GPU_blend(GPU_BLEND_ALPHA);
  GPUVertFormat *format = immVertexFormat();
  GPUPrimType prim_type = waveform_data->draw_line ? GPU_PRIM_LINE_STRIP : GPU_PRIM_TRI_STRIP;
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint col = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);
  immBegin(prim_type, vertex_count);

  while (vertices_done < vertex_count && !waveform_data->final_sample) {
    /* Color. */
    if (waveform_data->clip) {
      immAttr4f(col, 1.0f, 0.0f, 0.0f, 0.5f);
    }
    else if (use_rms) {
      immAttr4f(col, 1.0f, 1.0f, 1.0f, 0.8f);
    }
    else {
      immAttr4f(col, 1.0f, 1.0f, 1.0f, 0.5f);
    }

    /* Vertices. */
    if (use_rms) {
      immVertex2f(pos, waveform_data->pos[0], waveform_data->rms_pos);
    }
    else {
      immVertex2f(pos, waveform_data->pos[0], waveform_data->pos[1]);
    }

    vertices_done++;
    waveform_data++;
  }

  immEnd();
  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);

  return vertices_done;
}

static void draw_waveform(WaveVizData *waveform_data, size_t wave_data_len)
{
  size_t items_done = 0;
  while (items_done < wave_data_len) {
    if (!waveform_data[items_done].draw_line) { /* Draw RMS. */
      draw_waveform_segment(&waveform_data[items_done], true);
    }
    items_done += draw_waveform_segment(&waveform_data[items_done], false);
  }
}

static float align_frame_with_pixel(float frame_coord, float frames_per_pixel)
{
  return round_fl_to_int(frame_coord / frames_per_pixel) * frames_per_pixel;
}

static void write_waveform_data(WaveVizData *waveform_data,
                                const vec2f pos,
                                const float rms,
                                const bool is_clipping,
                                const bool draw_line)
{
  waveform_data->pos[0] = pos.x;
  waveform_data->pos[1] = pos.y;
  waveform_data->clip = is_clipping;
  waveform_data->rms_pos = rms;
  waveform_data->draw_line = draw_line;
}

static size_t waveform_append_sample(WaveVizData *waveform_data,
                                     vec2f pos,
                                     const float value_min,
                                     const float value_max,
                                     const float y_mid,
                                     const float y_scale,
                                     const float rms,
                                     const bool is_clipping,
                                     const bool is_line_strip)
{
  size_t data_written = 0;
  pos.y = y_mid + value_min * y_scale;
  float rms_value = y_mid + max_ff(-rms, value_min) * y_scale;
  write_waveform_data(&waveform_data[0], pos, rms_value, is_clipping, is_line_strip);
  data_written++;

  /* Use `value_max` as second vertex for triangle drawing. */
  if (!is_line_strip) {
    pos.y = y_mid + value_max * y_scale;
    rms_value = y_mid + min_ff(rms, value_max) * y_scale;
    write_waveform_data(&waveform_data[1], pos, rms_value, is_clipping, is_line_strip);
    data_written++;
  }
  return data_written;
}

/**
 * \param x1, x2, y1, y2: The starting and end X value to draw the wave, same for y1 and y2.
 * \param frames_per_pixel: The amount of pixels a whole frame takes up (x-axis direction).
 */
static void draw_seq_waveform_overlay(
    const bContext *C, ARegion *region, Sequence *seq, float x1, float y1, float x2, float y2)
{
  const View2D *v2d = &region->v2d;
  Scene *scene = CTX_data_scene(C);

  const float frames_per_pixel = BLI_rctf_size_x(&region->v2d.cur) / region->winx;
  const float samples_per_frame = SOUND_WAVE_SAMPLES_PER_SECOND / FPS;

  /* Align strip start with nearest pixel to prevent waveform flickering. */
  const float x1_aligned = align_frame_with_pixel(x1, frames_per_pixel);
  /* Offset x1 and x2 values, to match view min/max, if strip is out of bounds. */
  const float frame_start = max_ff(v2d->cur.xmin, x1_aligned);
  const float frame_end = min_ff(v2d->cur.xmax, x2);
  const int pixels_to_draw = round_fl_to_int((frame_end - frame_start) / frames_per_pixel);

  if (pixels_to_draw < 2) {
    return; /* Not much to draw, exit before running job. */
  }

  waveform_job_start_if_needed(C, seq);

  SoundWaveform *waveform = static_cast<SoundWaveform *>(seq->sound->waveform);
  if (waveform == nullptr || waveform->length == 0) {
    return; /* Waveform was not built. */
  }

  /* F-Curve lookup is quite expensive, so do this after precondition. */
  FCurve *fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, "volume", 0, nullptr);
  WaveVizData *waveform_data = MEM_cnew_array<WaveVizData>(pixels_to_draw * 3, __func__);
  size_t wave_data_len = 0;

  /* Offset must be also aligned, otherwise waveform flickers when moving left handle. */
  float start_frame = SEQ_time_left_handle_frame_get(scene, seq);

  /* Add off-screen part of strip to offset. */
  start_frame += (frame_start - x1_aligned);
  start_frame += seq->sound->offset_time / FPS;

  for (int i = 0; i < pixels_to_draw; i++) {
    float timeline_frame = start_frame + i * frames_per_pixel;
    /* TODO: Use linear interpolation between frames to avoid bad drawing quality. */
    float frame_index = SEQ_give_frame_index(scene, seq, timeline_frame);
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

    if (sample_index + 1 < waveform->length) {
      /* Use simple linear interpolation. */
      float f = sample - sample_index;
      value_min = (1.0f - f) * value_min + f * waveform->data[sample_index * 3 + 3];
      value_max = (1.0f - f) * value_max + f * waveform->data[sample_index * 3 + 4];
      rms = (1.0f - f) * rms + f * waveform->data[sample_index * 3 + 5];

      float samples_per_pixel = samples_per_frame * frames_per_pixel;
      if (samples_per_pixel > 1.0f) {
        /* We need to sum up the values we skip over until the next step. */
        float next_pos = sample + samples_per_pixel;
        int end_idx = next_pos;

        for (int j = sample_index + 1; (j < waveform->length) && (j < end_idx); j++) {
          value_min = min_ff(value_min, waveform->data[j * 3]);
          value_max = max_ff(value_max, waveform->data[j * 3 + 1]);
          rms = max_ff(rms, waveform->data[j * 3 + 2]);
        }
      }
    }

    float volume = seq->volume;
    if (fcu && !BKE_fcurve_is_empty(fcu)) {
      float evaltime = frame_start + (i * frames_per_pixel);
      volume = evaluate_fcurve(fcu, evaltime);
      CLAMP_MIN(volume, 0.0f);
    }

    value_min *= volume;
    value_max *= volume;
    rms *= volume;

    bool is_clipping = false;

    if (value_max > 1 || value_min < -1) {
      is_clipping = true;

      CLAMP_MAX(value_max, 1.0f);
      CLAMP_MIN(value_min, -1.0f);
    }

    bool is_line_strip = (value_max - value_min < 0.05f);
    /* The y coordinate for the middle of the strip. */
    float y_mid = (y1 + y2) / 2.0f;
    /* The length from the middle of the strip to the top/bottom. */
    float y_scale = (y2 - y1) / 2.0f;

    vec2f pos = {frame_start + i * frames_per_pixel, y_mid + value_min * y_scale};
    WaveVizData *new_data = &waveform_data[wave_data_len];
    wave_data_len += waveform_append_sample(
        new_data, pos, value_min, value_max, y_mid, y_scale, rms, is_clipping, is_line_strip);
  }

  /* Terminate array, so `get_segment_length()` can know when to stop. */
  waveform_data[wave_data_len].final_sample = true;
  draw_waveform(waveform_data, wave_data_len);
  MEM_freeN(waveform_data);
}

static void drawmeta_contents(Scene *scene,
                              Sequence *seqm,
                              float x1,
                              float y1,
                              float x2,
                              float y2,
                              const bool show_strip_color_tag)
{
  uchar col[4];

  int chan_min = MAXSEQ;
  int chan_max = 0;
  int chan_range = 0;
  float draw_range = y2 - y1;
  float draw_height;

  Editing *ed = SEQ_editing_get(scene);
  ListBase *channels = SEQ_channels_displayed_get(ed);
  ListBase *meta_seqbase;
  ListBase *meta_channels;
  int offset;

  meta_seqbase = SEQ_get_seqbase_from_sequence(seqm, &meta_channels, &offset);

  if (!meta_seqbase || BLI_listbase_is_empty(meta_seqbase)) {
    return;
  }

  if (seqm->type == SEQ_TYPE_SCENE) {
    offset = seqm->start - offset;
  }
  else {
    offset = 0;
  }

  GPU_blend(GPU_BLEND_ALPHA);

  LISTBASE_FOREACH (Sequence *, seq, meta_seqbase) {
    chan_min = min_ii(chan_min, seq->machine);
    chan_max = max_ii(chan_max, seq->machine);
  }

  chan_range = (chan_max - chan_min) + 1;
  draw_height = draw_range / chan_range;

  col[3] = 196; /* Alpha, used for all meta children. */

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* Draw only immediate children (1 level depth). */
  LISTBASE_FOREACH (Sequence *, seq, meta_seqbase) {
    const int startdisp = SEQ_time_left_handle_frame_get(scene, seq) + offset;
    const int enddisp = SEQ_time_right_handle_frame_get(scene, seq) + offset;

    if ((startdisp > x2 || enddisp < x1) == 0) {
      float y_chan = (seq->machine - chan_min) / float(chan_range) * draw_range;
      float x1_chan = startdisp;
      float x2_chan = enddisp;
      float y1_chan, y2_chan;

      if (seq->type == SEQ_TYPE_COLOR) {
        SolidColorVars *colvars = (SolidColorVars *)seq->effectdata;
        rgb_float_to_uchar(col, colvars->col);
      }
      else {
        color3ubv_from_seq(scene, seq, show_strip_color_tag, col);
      }

      if (SEQ_render_is_muted(channels, seqm) || SEQ_render_is_muted(meta_channels, seq)) {
        col[3] = 64;
      }
      else {
        col[3] = 196;
      }

      immUniformColor4ubv(col);

      /* Clamp within parent sequence strip bounds. */
      if (x1_chan < x1) {
        x1_chan = x1;
      }
      if (x2_chan > x2) {
        x2_chan = x2;
      }

      y1_chan = y1 + y_chan + (draw_height * SEQ_STRIP_OFSBOTTOM);
      y2_chan = y1 + y_chan + (draw_height * SEQ_STRIP_OFSTOP);

      immRectf(pos, x1_chan, y1_chan, x2_chan, y2_chan);
    }
  }

  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
}

float sequence_handle_size_get_clamped(const Scene *scene, Sequence *seq, const float pixelx)
{
  const float maxhandle = (pixelx * SEQ_HANDLE_SIZE) * U.pixelsize;

  /* Ensure that handle is not wider, than quarter of strip. */
  return min_ff(maxhandle,
                (float(SEQ_time_right_handle_frame_get(scene, seq) -
                       SEQ_time_left_handle_frame_get(scene, seq)) /
                 4.0f));
}

/* Draw a handle, on left or right side of strip. */
static void draw_seq_handle(const Scene *scene,
                            View2D *v2d,
                            Sequence *seq,
                            const float handsize_clamped,
                            const short direction,
                            uint pos,
                            bool seq_active,
                            float pixelx,
                            const bool draw_strip_preview)
{
  float rx1 = 0, rx2 = 0;
  float x1, x2, y1, y2;
  uint whichsel = 0;
  uchar col[4];

  x1 = SEQ_time_left_handle_frame_get(scene, seq);
  x2 = SEQ_time_right_handle_frame_get(scene, seq);

  y1 = seq->machine + SEQ_STRIP_OFSBOTTOM;
  y2 = seq->machine + SEQ_STRIP_OFSTOP;

  /* Set up co-ordinates and dimensions for either left or right handle. */
  if (direction == SEQ_LEFTHANDLE) {
    rx1 = x1;
    rx2 = x1 + handsize_clamped;
    whichsel = SEQ_LEFTSEL;
  }
  else if (direction == SEQ_RIGHTHANDLE) {
    rx1 = x2 - handsize_clamped;
    rx2 = x2;
    whichsel = SEQ_RIGHTSEL;
  }

  if (!(seq->type & SEQ_TYPE_EFFECT) || SEQ_effect_get_num_inputs(seq->type) == 0) {
    GPU_blend(GPU_BLEND_ALPHA);

    GPU_blend(GPU_BLEND_ALPHA);

    if (seq->flag & whichsel) {
      if (seq_active) {
        UI_GetThemeColor3ubv(TH_SEQ_ACTIVE, col);
      }
      else {
        UI_GetThemeColor3ubv(TH_SEQ_SELECTED, col);
        /* Make handles slightly brighter than the outlines. */
        UI_GetColorPtrShade3ubv(col, col, 50);
      }
      col[3] = 255;
      immUniformColor4ubv(col);
    }
    else {
      immUniformColor4ub(0, 0, 0, 50);
    }

    immRectf(pos, rx1, y1, rx2, y2);
    GPU_blend(GPU_BLEND_NONE);
  }

  /* Draw numbers for start and end of the strip next to its handles. */
  if (draw_strip_preview &&
      (((seq->flag & SELECT) && (G.moving & G_TRANSFORM_SEQ)) || (seq->flag & whichsel)))
  {

    char numstr[64];
    size_t numstr_len;
    const int fontid = BLF_default();
    BLF_set_default();

    /* Calculate if strip is wide enough for showing the labels. */
    numstr_len = SNPRINTF_RLEN(numstr,
                               "%d%d",
                               SEQ_time_left_handle_frame_get(scene, seq),
                               SEQ_time_right_handle_frame_get(scene, seq));
    float tot_width = BLF_width(fontid, numstr, numstr_len);

    if ((x2 - x1) / pixelx > 20 + tot_width) {
      col[0] = col[1] = col[2] = col[3] = 255;
      float text_margin = 1.2f * handsize_clamped;

      if (direction == SEQ_LEFTHANDLE) {
        numstr_len = SNPRINTF_RLEN(numstr, "%d", SEQ_time_left_handle_frame_get(scene, seq));
        x1 += text_margin;
        y1 += 0.09f;
      }
      else {
        numstr_len = SNPRINTF_RLEN(numstr, "%d", SEQ_time_right_handle_frame_get(scene, seq) - 1);
        x1 = x2 - (text_margin + pixelx * BLF_width(fontid, numstr, numstr_len));
        y1 += 0.09f;
      }
      UI_view2d_text_cache_add(v2d, x1, y1, numstr, numstr_len, col);
    }
  }
}

static void draw_seq_outline(Scene *scene,
                             Sequence *seq,
                             uint pos,
                             float x1,
                             float x2,
                             float y1,
                             float y2,
                             float pixelx,
                             float pixely,
                             bool seq_active)
{
  uchar col[3];

  /* Get the color for the outline. */
  if (seq_active && (seq->flag & SELECT)) {
    UI_GetThemeColor3ubv(TH_SEQ_ACTIVE, col);
  }
  else if (seq->flag & SELECT) {
    UI_GetThemeColor3ubv(TH_SEQ_SELECTED, col);
  }
  else {
    /* Color for unselected strips is a bit darker than the background. */
    UI_GetThemeColorShade3ubv(TH_BACK, -40, col);
  }

  /* Outline while translating strips:
   *  - Slightly lighter.
   *  - Red when overlapping with other strips.
   */
  const eSeqOverlapMode overlap_mode = SEQ_tool_settings_overlap_mode_get(scene);
  if ((G.moving & G_TRANSFORM_SEQ) && (seq->flag & SELECT) &&
      overlap_mode != SEQ_OVERLAP_OVERWRITE) {
    if (seq->flag & SEQ_OVERLAP) {
      col[0] = 255;
      col[1] = col[2] = 33;
    }
    else {
      UI_GetColorPtrShade3ubv(col, col, 70);
    }
  }
  immUniformColor3ubv(col);

  /* 2px wide outline for selected strips. */
  /* XXX: some platforms don't support OpenGL lines wider than 1px (see #57570),
   * draw outline as four boxes instead. */
  if (seq->flag & SELECT) {
    /* Left */
    immRectf(pos, x1 - pixelx, y1, x1 + pixelx, y2);
    /* Bottom */
    immRectf(pos, x1 - pixelx, y1, x2 + pixelx, y1 + 2 * pixely);
    /* Right */
    immRectf(pos, x2 - pixelx, y1, x2 + pixelx, y2);
    /* Top */
    immRectf(pos, x1 - pixelx, y2 - 2 * pixely, x2 + pixelx, y2);
  }
  else {
    /* 1px wide outline for unselected strips. */
    imm_draw_box_wire_2d(pos, x1, y1, x2, y2);
  }
}

static const char *draw_seq_text_get_name(Sequence *seq)
{
  const char *name = seq->name + 2;
  if (name[0] == '\0') {
    name = SEQ_sequence_give_name(seq);
  }
  return name;
}

static void draw_seq_text_get_source(Sequence *seq, char *r_source, size_t source_maxncpy)
{
  *r_source = '\0';

  /* Set source for the most common types. */
  switch (seq->type) {
    case SEQ_TYPE_IMAGE:
    case SEQ_TYPE_MOVIE: {
      BLI_path_join(
          r_source, source_maxncpy, seq->strip->dirpath, seq->strip->stripdata->filename);
      break;
    }
    case SEQ_TYPE_SOUND_RAM: {
      if (seq->sound != nullptr) {
        BLI_strncpy(r_source, seq->sound->filepath, source_maxncpy);
      }
      break;
    }
    case SEQ_TYPE_MULTICAM: {
      BLI_snprintf(r_source, source_maxncpy, "Channel: %d", seq->multicam_source);
      break;
    }
    case SEQ_TYPE_TEXT: {
      const TextVars *textdata = static_cast<TextVars *>(seq->effectdata);
      BLI_strncpy(r_source, textdata->text, source_maxncpy);
      break;
    }
    case SEQ_TYPE_SCENE: {
      if (seq->scene != nullptr) {
        if (seq->scene_camera != nullptr) {
          BLI_snprintf(r_source,
                       source_maxncpy,
                       "%s (%s)",
                       seq->scene->id.name + 2,
                       seq->scene_camera->id.name + 2);
        }
        else {
          BLI_strncpy(r_source, seq->scene->id.name + 2, source_maxncpy);
        }
      }
      break;
    }
    case SEQ_TYPE_MOVIECLIP: {
      if (seq->clip != nullptr) {
        BLI_strncpy(r_source, seq->clip->id.name + 2, source_maxncpy);
      }
      break;
    }
    case SEQ_TYPE_MASK: {
      if (seq->mask != nullptr) {
        BLI_strncpy(r_source, seq->mask->id.name + 2, source_maxncpy);
      }
      break;
    }
  }
}

static size_t draw_seq_text_get_overlay_string(const Scene *scene,
                                               SpaceSeq *sseq,
                                               Sequence *seq,
                                               char *r_overlay_string,
                                               size_t overlay_string_len)
{
  const char *text_sep = " | ";
  const char *text_array[5];
  int i = 0;

  if (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_NAME) {
    text_array[i++] = draw_seq_text_get_name(seq);
  }

  char source[FILE_MAX];
  if (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_SOURCE) {
    draw_seq_text_get_source(seq, source, sizeof(source));
    if (source[0] != '\0') {
      if (i != 0) {
        text_array[i++] = text_sep;
      }
      text_array[i++] = source;
    }
  }

  char strip_duration_text[16];
  if (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_DURATION) {
    const int strip_duration = SEQ_time_right_handle_frame_get(scene, seq) -
                               SEQ_time_left_handle_frame_get(scene, seq);
    SNPRINTF(strip_duration_text, "%d", strip_duration);
    if (i != 0) {
      text_array[i++] = text_sep;
    }
    text_array[i++] = strip_duration_text;
  }

  BLI_assert(i <= ARRAY_SIZE(text_array));

  return BLI_string_join_array(r_overlay_string, overlay_string_len, text_array, i);
}

/* Draw info text on a sequence strip. */
static void draw_seq_text_overlay(Scene *scene,
                                  View2D *v2d,
                                  Sequence *seq,
                                  SpaceSeq *sseq,
                                  float x1,
                                  float x2,
                                  float y1,
                                  float y2,
                                  bool seq_active)
{
  Editing *ed = SEQ_editing_get(scene);
  ListBase *channels = SEQ_channels_displayed_get(ed);
  char overlay_string[FILE_MAX];
  size_t overlay_string_len = draw_seq_text_get_overlay_string(
      scene, sseq, seq, overlay_string, sizeof(overlay_string));

  if (overlay_string_len == 0) {
    return;
  }

  /* White text for the active strip. */
  uchar col[4];
  col[0] = col[1] = col[2] = seq_active ? 255 : 10;
  col[3] = 255;

  /* Make the text duller when the strip is muted. */
  if (SEQ_render_is_muted(channels, seq)) {
    if (seq_active) {
      UI_GetColorPtrShade3ubv(col, col, -70);
    }
    else {
      UI_GetColorPtrShade3ubv(col, col, 15);
    }
  }

  rctf rect;
  rect.xmin = x1;
  rect.ymin = y1;
  rect.xmax = x2;
  rect.ymax = y2;

  UI_view2d_text_cache_add_rectf(v2d, &rect, overlay_string, overlay_string_len, col);
}

static void draw_sequence_extensions_overlay(
    Scene *scene, Sequence *seq, uint pos, float pixely, const bool show_strip_color_tag)
{
  Editing *ed = SEQ_editing_get(scene);
  ListBase *channels = SEQ_channels_displayed_get(ed);
  float x1, x2, y1, y2;
  uchar col[4], blend_col[3];

  x1 = SEQ_time_left_handle_frame_get(scene, seq);
  x2 = SEQ_time_right_handle_frame_get(scene, seq);

  y1 = seq->machine + SEQ_STRIP_OFSBOTTOM;
  y2 = seq->machine + SEQ_STRIP_OFSTOP;

  GPU_blend(GPU_BLEND_ALPHA);

  color3ubv_from_seq(scene, seq, show_strip_color_tag, col);
  if (seq->flag & SELECT) {
    UI_GetColorPtrShade3ubv(col, col, 50);
  }
  col[3] = SEQ_render_is_muted(channels, seq) ? MUTE_ALPHA : 200;
  UI_GetColorPtrShade3ubv(col, blend_col, 10);

  const float strip_content_start = SEQ_time_start_frame_get(seq);
  const float strip_content_end = SEQ_time_content_end_frame_get(scene, seq);
  float right_handle_frame = SEQ_time_right_handle_frame_get(scene, seq);
  float left_handle_frame = SEQ_time_left_handle_frame_get(scene, seq);

  if (left_handle_frame > strip_content_start) {
    immUniformColor4ubv(col);
    immRectf(pos, strip_content_start, y1 - pixely, x1, y1 - SEQ_STRIP_OFSBOTTOM);

    /* Outline. */
    immUniformColor3ubv(blend_col);
    imm_draw_box_wire_2d(pos, x1, y1 - pixely, strip_content_start, y1 - SEQ_STRIP_OFSBOTTOM);
  }
  if (right_handle_frame < strip_content_end) {
    immUniformColor4ubv(col);
    immRectf(pos, x2, y2 + pixely, strip_content_end, y2 + SEQ_STRIP_OFSBOTTOM);

    /* Outline. */
    immUniformColor3ubv(blend_col);
    imm_draw_box_wire_2d(pos, x2, y2 + pixely, strip_content_end, y2 + SEQ_STRIP_OFSBOTTOM);
  }
  GPU_blend(GPU_BLEND_NONE);
}

static uchar mute_overlap_alpha_factor_get(const ListBase *channels, const Sequence *seq)
{
  /* Draw muted strips semi-transparent. */
  if (SEQ_render_is_muted(channels, seq)) {
    return MUTE_ALPHA;
  }
  /* Draw background semi-transparent when overlapping strips. */
  else if (seq->flag & SEQ_OVERLAP) {
    return OVERLAP_ALPHA;
  }
  else {
    return 255;
  }
}

static void draw_color_strip_band(
    const Scene *scene, ListBase *channels, Sequence *seq, uint pos, float text_margin_y, float y1)
{
  uchar col[4];
  SolidColorVars *colvars = (SolidColorVars *)seq->effectdata;

  GPU_blend(GPU_BLEND_ALPHA);
  rgb_float_to_uchar(col, colvars->col);

  col[3] = mute_overlap_alpha_factor_get(channels, seq);

  immUniformColor4ubv(col);

  immRectf(pos,
           SEQ_time_left_handle_frame_get(scene, seq),
           y1,
           SEQ_time_right_handle_frame_get(scene, seq),
           text_margin_y);

  /* 1px line to better separate the color band. */
  UI_GetColorPtrShade3ubv(col, col, -20);
  immUniformColor4ubv(col);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(pos, SEQ_time_left_handle_frame_get(scene, seq), text_margin_y);
  immVertex2f(pos, SEQ_time_right_handle_frame_get(scene, seq), text_margin_y);
  immEnd();

  GPU_blend(GPU_BLEND_NONE);
}

static void draw_seq_background(Scene *scene,
                                Sequence *seq,
                                uint pos,
                                float x1,
                                float x2,
                                float y1,
                                float y2,
                                bool is_single_image,
                                bool show_strip_color_tag)
{
  Editing *ed = SEQ_editing_get(scene);
  ListBase *channels = SEQ_channels_displayed_get(ed);
  uchar col[4];
  GPU_blend(GPU_BLEND_ALPHA);

  color3ubv_from_seq(scene, seq, show_strip_color_tag, col);

  col[3] = mute_overlap_alpha_factor_get(channels, seq);

  immUniformColor4ubv(col);

  /* Draw the main strip body. */
  if (is_single_image) {
    immRectf(pos,
             SEQ_time_left_handle_frame_get(scene, seq),
             y1,
             SEQ_time_right_handle_frame_get(scene, seq),
             y2);
  }
  else {
    immRectf(pos, x1, y1, x2, y2);
  }

  /* Draw background for hold still regions. */
  if (!is_single_image) {
    UI_GetColorPtrShade3ubv(col, col, -35);
    immUniformColor4ubv(col);

    if (SEQ_time_has_left_still_frames(scene, seq)) {
      float left_handle_frame = SEQ_time_left_handle_frame_get(scene, seq);
      const float content_start = SEQ_time_start_frame_get(seq);
      immRectf(pos, left_handle_frame, y1, content_start, y2);
    }
    if (SEQ_time_has_right_still_frames(scene, seq)) {
      float right_handle_frame = SEQ_time_right_handle_frame_get(scene, seq);
      const float content_end = SEQ_time_content_end_frame_get(scene, seq);
      immRectf(pos, content_end, y1, right_handle_frame, y2);
    }
  }

  GPU_blend(GPU_BLEND_NONE);
}

typedef enum {
  STRIP_TRANSITION_IN,
  STRIP_TRANSITION_OUT,
} TransitionType;

static void draw_seq_transition_strip_half(const Scene *scene,
                                           const Sequence *transition_seq,
                                           const float x1,
                                           const float x2,
                                           const float y1,
                                           const float y2,
                                           const int timeline_overlay_flags,
                                           const TransitionType transition_type)
{
  Editing *ed = SEQ_editing_get(scene);
  const ListBase *channels = SEQ_channels_displayed_get(ed);

  const Sequence *seq1 = transition_seq->seq1;
  const Sequence *seq2 = transition_seq->seq2;
  const Sequence *target_seq = (transition_type == STRIP_TRANSITION_IN) ? seq1 : seq2;

  const bool show_strip_color_tag = (timeline_overlay_flags & SEQ_TIMELINE_SHOW_STRIP_COLOR_TAG);

  float col[4];
  if (target_seq->type == SEQ_TYPE_COLOR) {
    SolidColorVars *colvars = (SolidColorVars *)target_seq->effectdata;
    memcpy(col, colvars->col, sizeof(colvars->col));
  }
  else {
    uchar ucol[3];
    color3ubv_from_seq(scene, target_seq, show_strip_color_tag, ucol);
    /* If the transition inputs are of the same type, draw the right side slightly darker. */
    if ((seq1->type == seq2->type) && (transition_type == STRIP_TRANSITION_OUT)) {
      UI_GetColorPtrShade3ubv(ucol, ucol, -15);
    }
    rgb_uchar_to_float(col, ucol);
  }

  col[3] = mute_overlap_alpha_factor_get(channels, transition_seq) / 255.0f;

  GPU_blend(GPU_BLEND_ALPHA);

  const uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor4fv(col);

  float vert_pos[3][2];

  if (transition_type == STRIP_TRANSITION_IN) {
    copy_v2_fl2(vert_pos[0], x1, y1);
    copy_v2_fl2(vert_pos[1], x1, y2);
    copy_v2_fl2(vert_pos[2], x2, y1);
  }
  else {
    copy_v2_fl2(vert_pos[0], x1, y2);
    copy_v2_fl2(vert_pos[1], x2, y2);
    copy_v2_fl2(vert_pos[2], x2, y1);
  }

  immBegin(GPU_PRIM_TRIS, 3);
  immVertex2fv(pos, vert_pos[0]);
  immVertex2fv(pos, vert_pos[1]);
  immVertex2fv(pos, vert_pos[2]);
  immEnd();

  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
}

static void draw_seq_transition_strip(const Scene *scene,
                                      const Sequence *seq,
                                      const float x1,
                                      const float x2,
                                      const float y1,
                                      const float y2,
                                      const int timeline_overlay_flags)
{
  draw_seq_transition_strip_half(
      scene, seq, x1, x2, y1, y2, timeline_overlay_flags, STRIP_TRANSITION_IN);
  draw_seq_transition_strip_half(
      scene, seq, x1, x2, y1, y2, timeline_overlay_flags, STRIP_TRANSITION_OUT);
}

static void draw_seq_locked(float x1, float y1, float x2, float y2)
{
  GPU_blend(GPU_BLEND_ALPHA);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_DIAG_STRIPES);

  immUniform4f("color1", 1.0f, 1.0f, 1.0f, 0.0f);
  immUniform4f("color2", 0.0f, 0.0f, 0.0f, 0.25f);
  immUniform1i("size1", 8);
  immUniform1i("size2", 4);

  immRectf(pos, x1, y1, x2, y2);

  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
}

static void draw_seq_invalid(float x1, float x2, float y2, float text_margin_y)
{
  GPU_blend(GPU_BLEND_ALPHA);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4f(1.0f, 0.0f, 0.0f, 0.9f);
  immRectf(pos, x1, y2, x2, text_margin_y);

  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
}

/* For text on the strip. */
static void calculate_seq_text_offsets(
    const Scene *scene, View2D *v2d, Sequence *seq, float *x1, float *x2, float pixelx)
{
  const float handsize_clamped = sequence_handle_size_get_clamped(scene, seq, pixelx);
  float text_margin = 2.0f * handsize_clamped;

  *x1 += text_margin;
  *x2 -= text_margin;

  CLAMP(*x1, v2d->cur.xmin + text_margin, v2d->cur.xmax);
  CLAMP(*x2, v2d->cur.xmin + text_margin, v2d->cur.xmax);
}

static void fcurve_batch_add_verts(GPUVertBuf *vbo,
                                   float y1,
                                   float y2,
                                   float y_height,
                                   int timeline_frame,
                                   float curve_val,
                                   uint *vert_count)
{
  float vert_pos[2][2];

  copy_v2_fl2(vert_pos[0], timeline_frame, (curve_val * y_height) + y1);
  copy_v2_fl2(vert_pos[1], timeline_frame, y2);

  GPU_vertbuf_vert_set(vbo, *vert_count, vert_pos[0]);
  GPU_vertbuf_vert_set(vbo, *vert_count + 1, vert_pos[1]);
  *vert_count += 2;
}

/**
 * Draw f-curves as darkened regions of the strip:
 * - Volume for sound strips.
 * - Opacity for the other types.
 */
static void draw_seq_fcurve_overlay(
    Scene *scene, View2D *v2d, Sequence *seq, float x1, float y1, float x2, float y2, float pixelx)
{
  FCurve *fcu;

  if (seq->type == SEQ_TYPE_SOUND_RAM) {
    fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, "volume", 0, nullptr);
  }
  else {
    fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, "blend_alpha", 0, nullptr);
  }

  if (fcu && !BKE_fcurve_is_empty(fcu)) {

    /* Clamp curve evaluation to the editor's borders. */
    int eval_start = max_ff(x1, v2d->cur.xmin);
    int eval_end = min_ff(x2, v2d->cur.xmax + 1);

    int eval_step = max_ii(1, floor(pixelx));

    if (eval_start >= eval_end) {
      return;
    }

    GPUVertFormat format = {0};
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);

    uint max_verts = 2 * ((eval_end - eval_start) / eval_step + 1);
    GPU_vertbuf_data_alloc(vbo, max_verts);
    uint vert_count = 0;

    const float y_height = y2 - y1;
    float curve_val;
    float prev_val = INT_MIN;
    bool skip = false;

    for (int timeline_frame = eval_start; timeline_frame <= eval_end; timeline_frame += eval_step)
    {
      curve_val = evaluate_fcurve(fcu, timeline_frame);
      CLAMP(curve_val, 0.0f, 1.0f);

      /* Avoid adding adjacent verts that have the same value. */
      if (curve_val == prev_val && timeline_frame < eval_end - eval_step) {
        skip = true;
        continue;
      }

      /* If some frames were skipped above, we need to close the shape. */
      if (skip) {
        fcurve_batch_add_verts(
            vbo, y1, y2, y_height, timeline_frame - eval_step, prev_val, &vert_count);
        skip = false;
      }

      fcurve_batch_add_verts(vbo, y1, y2, y_height, timeline_frame, curve_val, &vert_count);
      prev_val = curve_val;
    }

    GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, nullptr, GPU_BATCH_OWNS_VBO);
    GPU_vertbuf_data_len_set(vbo, vert_count);
    GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_UNIFORM_COLOR);
    GPU_batch_uniform_4f(batch, "color", 0.0f, 0.0f, 0.0f, 0.15f);
    GPU_blend(GPU_BLEND_ALPHA);

    if (vert_count > 0) {
      GPU_batch_draw(batch);
    }

    GPU_blend(GPU_BLEND_NONE);
    GPU_batch_discard(batch);
  }
}

/* Draw visible strips. Bounds check are already made. */
static void draw_seq_strip(const bContext *C,
                           SpaceSeq *sseq,
                           Scene *scene,
                           ARegion *region,
                           Sequence *seq,
                           float pixelx,
                           bool seq_active)
{
  Editing *ed = SEQ_editing_get(CTX_data_scene(C));
  ListBase *channels = SEQ_channels_displayed_get(ed);

  View2D *v2d = &region->v2d;
  float x1, x2, y1, y2;
  const float handsize_clamped = sequence_handle_size_get_clamped(scene, seq, pixelx);
  float pixely = BLI_rctf_size_y(&v2d->cur) / BLI_rcti_size_y(&v2d->mask);

  /* Check if we are doing "solo preview". */
  bool is_single_image = char(SEQ_transform_single_image_check(seq));

  /* Use the seq->color_tag to display the tag color. */
  const bool show_strip_color_tag = (sseq->timeline_overlay.flag &
                                     SEQ_TIMELINE_SHOW_STRIP_COLOR_TAG);

  /* Draw strip body. */
  x1 = SEQ_time_has_left_still_frames(scene, seq) ? SEQ_time_start_frame_get(seq) :
                                                    SEQ_time_left_handle_frame_get(scene, seq);
  y1 = seq->machine + SEQ_STRIP_OFSBOTTOM;
  x2 = SEQ_time_has_right_still_frames(scene, seq) ? SEQ_time_content_end_frame_get(scene, seq) :
                                                     SEQ_time_right_handle_frame_get(scene, seq);
  y2 = seq->machine + SEQ_STRIP_OFSTOP;

  /* Limit body to strip bounds. Meta strip can end up with content outside of strip range. */
  x1 = min_ff(x1, SEQ_time_right_handle_frame_get(scene, seq));
  x2 = max_ff(x2, SEQ_time_left_handle_frame_get(scene, seq));

  float text_margin_y;
  /* Whether there is enough space for the strip preview. */
  const bool draw_strip_preview = ((y2 - y1) / pixely) > 20 * UI_SCALE_FAC;
  /* If there is not enough vertical space, don't draw any previews nor the title. */
  const bool strip_content_none = (y2 - y1) < 8 * pixely * UI_SCALE_FAC;
  /* Only a single vertical element is drawn on the strip: the waveform in the case of
   * sound strips, or the strip title for most other strip types. */
  bool strip_content_single;

  if ((sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_NAME) ||
      (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_SOURCE) ||
      (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_DURATION))
  {

    /* Calculate height needed for drawing text on strip. */
    text_margin_y = y2 - min_ff(0.40f, 20 * UI_SCALE_FAC * pixely);

    strip_content_single = !draw_strip_preview;
  }
  else {
    text_margin_y = y2;
    strip_content_single = true;
  }
  /* When a text overlay is enabled and there is space for both the text and the strip content,
   * draw below the text. */
  /* When text is disabled or there is space for only one or the other, use entire strip area. */
  const float strip_preview_y2 = strip_content_single ? y2 : text_margin_y;

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  draw_seq_background(scene, seq, pos, x1, x2, y1, y2, is_single_image, show_strip_color_tag);

  /* Draw a color band inside color strip. */
  if ((sseq->flag & SEQ_SHOW_OVERLAY) && (seq->type == SEQ_TYPE_COLOR)) {
    draw_color_strip_band(scene, channels, seq, pos, strip_preview_y2, y1);
  }

  /* Draw strip offsets when flag is enabled or during "solo preview". */
  if (sseq->flag & SEQ_SHOW_OVERLAY) {
    if (!is_single_image && pixely > 0) {
      if ((sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_OFFSETS) ||
          (seq == special_seq_update)) {
        draw_sequence_extensions_overlay(scene, seq, pos, pixely, show_strip_color_tag);
      }
    }
  }
  immUnbindProgram();

  if (draw_strip_preview && (sseq->flag & SEQ_SHOW_OVERLAY) &&
      ELEM(seq->type, SEQ_TYPE_CROSS, SEQ_TYPE_GAMCROSS, SEQ_TYPE_WIPE))
  {
    draw_seq_transition_strip(
        scene, seq, x1, x2, y1, strip_preview_y2, sseq->timeline_overlay.flag);
  }

  x1 = SEQ_time_left_handle_frame_get(scene, seq);
  x2 = SEQ_time_right_handle_frame_get(scene, seq);

  if (draw_strip_preview && (sseq->flag & SEQ_SHOW_OVERLAY)) {
    if ((seq->type == SEQ_TYPE_META) ||
        ((seq->type == SEQ_TYPE_SCENE) && (seq->flag & SEQ_SCENE_STRIPS)))
    {
      drawmeta_contents(scene, seq, x1, y1, x2, strip_preview_y2, show_strip_color_tag);
    }
  }

  if ((sseq->flag & SEQ_SHOW_OVERLAY) &&
      (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_THUMBNAILS) &&
      ELEM(seq->type, SEQ_TYPE_MOVIE, SEQ_TYPE_IMAGE))
  {
    draw_seq_strip_thumbnail(v2d, C, scene, seq, y1, strip_preview_y2, pixelx, pixely);
  }

  if (draw_strip_preview && (sseq->flag & SEQ_SHOW_OVERLAY) &&
      (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_FCURVES))
  {
    draw_seq_fcurve_overlay(scene, v2d, seq, x1, y1, x2, strip_preview_y2, pixelx);
  }

  /* Draw sound strip waveform. */
  if (seq_draw_waveforms_poll(C, sseq, seq) && !strip_content_none) {
    draw_seq_waveform_overlay(
        C, region, seq, x1, strip_content_single ? y1 : y1 + 0.05f, x2, strip_preview_y2);
  }
  /* Draw locked state. */
  if (SEQ_transform_is_locked(channels, seq)) {
    draw_seq_locked(x1, y1, x2, y2);
  }

  /* Draw Red line on the top of invalid strip (Missing media). */
  if (!SEQ_sequence_has_source(seq)) {
    draw_seq_invalid(x1, x2, y2, text_margin_y);
  }

  pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  if (!SEQ_transform_is_locked(channels, seq)) {
    draw_seq_handle(scene,
                    v2d,
                    seq,
                    handsize_clamped,
                    SEQ_LEFTHANDLE,
                    pos,
                    seq_active,
                    pixelx,
                    draw_strip_preview);
    draw_seq_handle(scene,
                    v2d,
                    seq,
                    handsize_clamped,
                    SEQ_RIGHTHANDLE,
                    pos,
                    seq_active,
                    pixelx,
                    draw_strip_preview);
  }

  draw_seq_outline(scene, seq, pos, x1, x2, y1, y2, pixelx, pixely, seq_active);

  immUnbindProgram();

  /* If a waveform or a color strip is drawn,
   * avoid drawing text when there is not enough vertical space. */
  if (strip_content_single &&
      (seq_draw_waveforms_poll(C, sseq, seq) || seq->type == SEQ_TYPE_COLOR)) {
    return;
  }

  if (sseq->flag & SEQ_SHOW_OVERLAY) {
    /* Draw text only if there is enough horizontal or vertical space. */
    if (((x2 - x1) > 32 * pixelx * UI_SCALE_FAC) && !strip_content_none) {
      calculate_seq_text_offsets(scene, v2d, seq, &x1, &x2, pixelx);
      /* Depending on the vertical space, draw text on top or in the center of strip. */
      draw_seq_text_overlay(scene,
                            v2d,
                            seq,
                            sseq,
                            x1,
                            x2,
                            strip_content_single ? y1 : text_margin_y,
                            y2,
                            seq_active);
    }
  }
}

static void draw_effect_inputs_highlight(const Scene *scene, Sequence *seq)
{
  Sequence *seq1 = seq->seq1;
  Sequence *seq2 = seq->seq2;
  Sequence *seq3 = seq->seq3;
  GPU_blend(GPU_BLEND_ALPHA);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor4ub(255, 255, 255, 48);
  immRectf(pos,
           SEQ_time_left_handle_frame_get(scene, seq1),
           seq1->machine + SEQ_STRIP_OFSBOTTOM,
           SEQ_time_right_handle_frame_get(scene, seq1),
           seq1->machine + SEQ_STRIP_OFSTOP);

  if (seq2 && seq2 != seq1) {
    immRectf(pos,
             SEQ_time_left_handle_frame_get(scene, seq2),
             seq2->machine + SEQ_STRIP_OFSBOTTOM,
             SEQ_time_right_handle_frame_get(scene, seq2),
             seq2->machine + SEQ_STRIP_OFSTOP);
  }
  if (seq3 && !ELEM(seq3, seq1, seq2)) {
    immRectf(pos,
             SEQ_time_left_handle_frame_get(scene, seq3),
             seq3->machine + SEQ_STRIP_OFSBOTTOM,
             SEQ_time_right_handle_frame_get(scene, seq3),
             seq3->machine + SEQ_STRIP_OFSTOP);
  }
  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
}

/* Force redraw, when prefetching and using cache view. */
static void seq_prefetch_wm_notify(const bContext *C, Scene *scene)
{
  if (SEQ_prefetch_need_redraw(CTX_data_main(C), scene)) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, nullptr);
  }
}

static void draw_seq_timeline_channels(View2D *v2d)
{
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_blend(GPU_BLEND_ALPHA);
  immUniformThemeColor(TH_ROW_ALTERNATE);

  /* Alternating horizontal stripes. */
  int i = max_ii(1, int(v2d->cur.ymin) - 1);
  while (i < v2d->cur.ymax) {
    if (i & 1) {
      immRectf(pos, v2d->cur.xmin, i, v2d->cur.xmax, i + 1);
    }
    i++;
  }

  GPU_blend(GPU_BLEND_NONE);
  immUnbindProgram();
}

static void draw_seq_strips(const bContext *C, Editing *ed, ARegion *region)
{
  Scene *scene = CTX_data_scene(C);
  View2D *v2d = &region->v2d;
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  Sequence *last_seq = SEQ_select_active_get(scene);
  int sel = 0, j;
  float pixelx = BLI_rctf_size_x(&v2d->cur) / BLI_rcti_size_x(&v2d->mask);

  /* Loop through twice, first unselected, then selected. */
  for (j = 0; j < 2; j++) {
    /* Loop through strips, checking for those that are visible. */
    LISTBASE_FOREACH (Sequence *, seq, ed->seqbasep) {
      /* Bound-box and selection tests for NOT drawing the strip. */
      if ((seq->flag & SELECT) != sel) {
        continue;
      }
      if (seq == last_seq && (last_seq->flag & SELECT)) {
        continue;
      }
      if (min_ii(SEQ_time_left_handle_frame_get(scene, seq), SEQ_time_start_frame_get(seq)) >
          v2d->cur.xmax)
      {
        continue;
      }
      if (max_ii(SEQ_time_right_handle_frame_get(scene, seq),
                 SEQ_time_content_end_frame_get(scene, seq)) < v2d->cur.xmin)
      {
        continue;
      }
      if (seq->machine + 1.0f < v2d->cur.ymin) {
        continue;
      }
      if (seq->machine > v2d->cur.ymax) {
        continue;
      }

      /* Strip passed all tests, draw it now. */
      draw_seq_strip(C, sseq, scene, region, seq, pixelx, seq == last_seq ? true : false);
    }

    /* Draw selected next time round. */
    sel = SELECT;
  }

  /* When selected draw the last selected (active) strip last,
   * removes some overlapping error. */
  if (last_seq && (last_seq->flag & SELECT)) {
    draw_seq_strip(C, sseq, scene, region, last_seq, pixelx, true);

    /* When active strip is an effect, highlight its inputs. */
    if (SEQ_effect_get_num_inputs(last_seq->type) > 0) {
      draw_effect_inputs_highlight(scene, last_seq);
    }
    /* When active is a Multi-cam strip, highlight its source channel. */
    else if (last_seq->type == SEQ_TYPE_MULTICAM) {
      int channel = last_seq->multicam_source;
      if (channel != 0) {
        GPU_blend(GPU_BLEND_ALPHA);
        uint pos = GPU_vertformat_attr_add(
            immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
        immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

        immUniformColor4ub(255, 255, 255, 48);
        immRectf(pos, v2d->cur.xmin, channel, v2d->cur.xmax, channel + 1);

        immUnbindProgram();
        GPU_blend(GPU_BLEND_NONE);
      }
    }
  }

  /* Draw highlight if "solo preview" is used. */
  if (special_seq_update) {
    const Sequence *seq = special_seq_update;
    GPU_blend(GPU_BLEND_ALPHA);

    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    immUniformColor4ub(255, 255, 255, 48);
    immRectf(pos,
             SEQ_time_left_handle_frame_get(scene, seq),
             seq->machine + SEQ_STRIP_OFSBOTTOM,
             SEQ_time_right_handle_frame_get(scene, seq),
             seq->machine + SEQ_STRIP_OFSTOP);

    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
  }
}

static void seq_draw_sfra_efra(const Scene *scene, View2D *v2d)
{
  const Editing *ed = SEQ_editing_get(scene);
  const int frame_sta = scene->r.sfra;
  const int frame_end = scene->r.efra + 1;

  GPU_blend(GPU_BLEND_ALPHA);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* Draw overlay outside of frame range. */
  immUniformThemeColorShadeAlpha(TH_BACK, -10, -100);

  if (frame_sta < frame_end) {
    immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, float(frame_sta), v2d->cur.ymax);
    immRectf(pos, float(frame_end), v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
  }
  else {
    immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
  }

  immUniformThemeColorShade(TH_BACK, -60);

  /* Draw frame range boundary. */
  immBegin(GPU_PRIM_LINES, 4);

  immVertex2f(pos, frame_sta, v2d->cur.ymin);
  immVertex2f(pos, frame_sta, v2d->cur.ymax);

  immVertex2f(pos, frame_end, v2d->cur.ymin);
  immVertex2f(pos, frame_end, v2d->cur.ymax);

  immEnd();

  /* While in meta strip, draw a checkerboard overlay outside of frame range. */
  if (ed && !BLI_listbase_is_empty(&ed->metastack)) {
    const MetaStack *ms = static_cast<const MetaStack *>(ed->metastack.last);
    immUnbindProgram();

    immBindBuiltinProgram(GPU_SHADER_2D_CHECKER);

    immUniform4f("color1", 0.0f, 0.0f, 0.0f, 0.22f);
    immUniform4f("color2", 1.0f, 1.0f, 1.0f, 0.0f);
    immUniform1i("size", 8);

    immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, ms->disp_range[0], v2d->cur.ymax);
    immRectf(pos, ms->disp_range[1], v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);

    immUnbindProgram();

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformThemeColorShade(TH_BACK, -40);

    immBegin(GPU_PRIM_LINES, 4);

    immVertex2f(pos, ms->disp_range[0], v2d->cur.ymin);
    immVertex2f(pos, ms->disp_range[0], v2d->cur.ymax);

    immVertex2f(pos, ms->disp_range[1], v2d->cur.ymin);
    immVertex2f(pos, ms->disp_range[1], v2d->cur.ymax);

    immEnd();
  }

  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
}

struct CacheDrawData {
  View2D *v2d;
  float stripe_ofs_y;
  float stripe_ht;
  int cache_flag;
  GPUVertBuf *raw_vbo;
  GPUVertBuf *preprocessed_vbo;
  GPUVertBuf *composite_vbo;
  GPUVertBuf *final_out_vbo;
  size_t raw_vert_count;
  size_t preprocessed_vert_count;
  size_t composite_vert_count;
  size_t final_out_vert_count;
};

/* Called as a callback. */
static bool draw_cache_view_init_fn(void *userdata, size_t item_count)
{
  if (item_count == 0) {
    return true;
  }

  CacheDrawData *drawdata = static_cast<CacheDrawData *>(userdata);
  /* We can not get item count per cache type, so using total item count is safe. */
  size_t max_vert_count = item_count * 6;
  GPU_vertbuf_data_alloc(drawdata->raw_vbo, max_vert_count);
  GPU_vertbuf_data_alloc(drawdata->preprocessed_vbo, max_vert_count);
  GPU_vertbuf_data_alloc(drawdata->composite_vbo, max_vert_count);
  GPU_vertbuf_data_alloc(drawdata->final_out_vbo, max_vert_count);

  return false;
}

/* Called as a callback */
static bool draw_cache_view_iter_fn(void *userdata,
                                    Sequence *seq,
                                    int timeline_frame,
                                    int cache_type)
{
  CacheDrawData *drawdata = static_cast<CacheDrawData *>(userdata);
  View2D *v2d = drawdata->v2d;
  float stripe_bot, stripe_top, stripe_ofs_y, stripe_ht;
  GPUVertBuf *vbo;
  size_t *vert_count;

  if ((cache_type & SEQ_CACHE_STORE_FINAL_OUT) &&
      (drawdata->cache_flag & SEQ_CACHE_VIEW_FINAL_OUT)) {
    stripe_ht = UI_view2d_region_to_view_y(v2d, 4.0f * UI_SCALE_FAC * U.pixelsize) - v2d->cur.ymin;
    stripe_bot = UI_view2d_region_to_view_y(v2d, V2D_SCROLL_HANDLE_HEIGHT);
    stripe_top = stripe_bot + stripe_ht;
    vbo = drawdata->final_out_vbo;
    vert_count = &drawdata->final_out_vert_count;
  }
  else if ((cache_type & SEQ_CACHE_STORE_RAW) && (drawdata->cache_flag & SEQ_CACHE_VIEW_RAW)) {
    stripe_ofs_y = drawdata->stripe_ofs_y;
    stripe_ht = drawdata->stripe_ht;
    stripe_bot = seq->machine + SEQ_STRIP_OFSBOTTOM + stripe_ofs_y;
    stripe_top = stripe_bot + stripe_ht;
    vbo = drawdata->raw_vbo;
    vert_count = &drawdata->raw_vert_count;
  }
  else if ((cache_type & SEQ_CACHE_STORE_PREPROCESSED) &&
           (drawdata->cache_flag & SEQ_CACHE_VIEW_PREPROCESSED))
  {
    stripe_ofs_y = drawdata->stripe_ofs_y;
    stripe_ht = drawdata->stripe_ht;
    stripe_bot = seq->machine + SEQ_STRIP_OFSBOTTOM + (stripe_ofs_y + stripe_ht) + stripe_ofs_y;
    stripe_top = stripe_bot + stripe_ht;
    vbo = drawdata->preprocessed_vbo;
    vert_count = &drawdata->preprocessed_vert_count;
  }
  else if ((cache_type & SEQ_CACHE_STORE_COMPOSITE) &&
           (drawdata->cache_flag & SEQ_CACHE_VIEW_COMPOSITE))
  {
    stripe_ofs_y = drawdata->stripe_ofs_y;
    stripe_ht = drawdata->stripe_ht;
    stripe_top = seq->machine + SEQ_STRIP_OFSTOP - stripe_ofs_y;
    stripe_bot = stripe_top - stripe_ht;
    vbo = drawdata->composite_vbo;
    vert_count = &drawdata->composite_vert_count;
  }
  else {
    return false;
  }

  float vert_pos[6][2];
  copy_v2_fl2(vert_pos[0], timeline_frame, stripe_bot);
  copy_v2_fl2(vert_pos[1], timeline_frame, stripe_top);
  copy_v2_fl2(vert_pos[2], timeline_frame + 1, stripe_top);
  copy_v2_v2(vert_pos[3], vert_pos[2]);
  copy_v2_v2(vert_pos[4], vert_pos[0]);
  copy_v2_fl2(vert_pos[5], timeline_frame + 1, stripe_bot);

  for (int i = 0; i < 6; i++) {
    GPU_vertbuf_vert_set(vbo, *vert_count + i, vert_pos[i]);
  }

  *vert_count += 6;
  return false;
}

static void draw_cache_view_batch(
    GPUVertBuf *vbo, size_t vert_count, float col_r, float col_g, float col_b, float col_a)
{
  GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  if (vert_count > 0) {
    GPU_vertbuf_data_len_set(vbo, vert_count);
    GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_UNIFORM_COLOR);
    GPU_batch_uniform_4f(batch, "color", col_r, col_g, col_b, col_a);
    GPU_batch_draw(batch);
  }
  GPU_batch_discard(batch);
}

static void draw_cache_view(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;

  if ((scene->ed->cache_flag & SEQ_CACHE_VIEW_ENABLE) == 0) {
    return;
  }

  GPU_blend(GPU_BLEND_ALPHA);
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  float stripe_bot, stripe_top;
  float stripe_ofs_y = UI_view2d_region_to_view_y(v2d, 1.0f) - v2d->cur.ymin;
  float stripe_ht = UI_view2d_region_to_view_y(v2d, 4.0f * UI_SCALE_FAC * U.pixelsize) -
                    v2d->cur.ymin;

  CLAMP_MAX(stripe_ht, 0.2f);
  CLAMP_MIN(stripe_ofs_y, stripe_ht / 2);

  if (scene->ed->cache_flag & SEQ_CACHE_VIEW_FINAL_OUT) {
    stripe_bot = UI_view2d_region_to_view_y(v2d, V2D_SCROLL_HANDLE_HEIGHT);
    stripe_top = stripe_bot + stripe_ht;
    const float bg_color[4] = {1.0f, 0.4f, 0.2f, 0.1f};

    immUniformColor4f(bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
    immRectf(pos, scene->r.sfra, stripe_bot, scene->r.efra, stripe_top);
  }

  LISTBASE_FOREACH (Sequence *, seq, scene->ed->seqbasep) {
    if (seq->type == SEQ_TYPE_SOUND_RAM) {
      continue;
    }

    if (SEQ_time_left_handle_frame_get(scene, seq) > v2d->cur.xmax ||
        SEQ_time_right_handle_frame_get(scene, seq) < v2d->cur.xmin)
    {
      continue;
    }

    stripe_bot = seq->machine + SEQ_STRIP_OFSBOTTOM + stripe_ofs_y;
    stripe_top = stripe_bot + stripe_ht;

    if (scene->ed->cache_flag & SEQ_CACHE_VIEW_RAW) {
      const float bg_color[4] = {1.0f, 0.1f, 0.02f, 0.1f};
      immUniformColor4f(bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
      immRectf(pos,
               SEQ_time_left_handle_frame_get(scene, seq),
               stripe_bot,
               SEQ_time_right_handle_frame_get(scene, seq),
               stripe_top);
    }

    stripe_bot += stripe_ht + stripe_ofs_y;
    stripe_top = stripe_bot + stripe_ht;

    if (scene->ed->cache_flag & SEQ_CACHE_VIEW_PREPROCESSED) {
      const float bg_color[4] = {0.1f, 0.1f, 0.75f, 0.1f};
      immUniformColor4f(bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
      immRectf(pos,
               SEQ_time_left_handle_frame_get(scene, seq),
               stripe_bot,
               SEQ_time_right_handle_frame_get(scene, seq),
               stripe_top);
    }

    stripe_top = seq->machine + SEQ_STRIP_OFSTOP - stripe_ofs_y;
    stripe_bot = stripe_top - stripe_ht;

    if (scene->ed->cache_flag & SEQ_CACHE_VIEW_COMPOSITE) {
      const float bg_color[4] = {1.0f, 0.6f, 0.0f, 0.1f};
      immUniformColor4f(bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
      immRectf(pos,
               SEQ_time_left_handle_frame_get(scene, seq),
               stripe_bot,
               SEQ_time_right_handle_frame_get(scene, seq),
               stripe_top);
    }
  }

  immUnbindProgram();

  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  CacheDrawData userdata;
  userdata.v2d = v2d;
  userdata.stripe_ofs_y = stripe_ofs_y;
  userdata.stripe_ht = stripe_ht;
  userdata.cache_flag = scene->ed->cache_flag;
  userdata.raw_vert_count = 0;
  userdata.preprocessed_vert_count = 0;
  userdata.composite_vert_count = 0;
  userdata.final_out_vert_count = 0;
  userdata.raw_vbo = GPU_vertbuf_create_with_format(&format);
  userdata.preprocessed_vbo = GPU_vertbuf_create_with_format(&format);
  userdata.composite_vbo = GPU_vertbuf_create_with_format(&format);
  userdata.final_out_vbo = GPU_vertbuf_create_with_format(&format);

  SEQ_cache_iterate(scene, &userdata, draw_cache_view_init_fn, draw_cache_view_iter_fn);

  draw_cache_view_batch(userdata.raw_vbo, userdata.raw_vert_count, 1.0f, 0.1f, 0.02f, 0.4f);
  draw_cache_view_batch(
      userdata.preprocessed_vbo, userdata.preprocessed_vert_count, 0.1f, 0.1f, 0.75f, 0.4f);
  draw_cache_view_batch(
      userdata.composite_vbo, userdata.composite_vert_count, 1.0f, 0.6f, 0.0f, 0.4f);
  draw_cache_view_batch(
      userdata.final_out_vbo, userdata.final_out_vert_count, 1.0f, 0.4f, 0.2f, 0.4f);

  GPU_blend(GPU_BLEND_NONE);
}

/* Draw sequencer timeline. */
static void draw_overlap_frame_indicator(const Scene *scene, const View2D *v2d)
{
  int overlap_frame = (scene->ed->overlay_frame_flag & SEQ_EDIT_OVERLAY_FRAME_ABS) ?
                          scene->ed->overlay_frame_abs :
                          scene->r.cfra + scene->ed->overlay_frame_ofs;

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
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
  immVertex2f(pos, overlap_frame, v2d->cur.ymin);
  immVertex2f(pos, overlap_frame, v2d->cur.ymax);
  immEnd();

  immUnbindProgram();
}

void draw_timeline_seq(const bContext *C, ARegion *region)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  View2D *v2d = &region->v2d;

  seq_prefetch_wm_notify(C, scene);

  GPUViewport *viewport = WM_draw_region_get_viewport(region);
  GPUFrameBuffer *framebuffer_overlay = GPU_viewport_framebuffer_overlay_get(viewport);
  GPU_framebuffer_bind_no_srgb(framebuffer_overlay);
  GPU_depth_test(GPU_DEPTH_NONE);

  UI_ThemeClearColor(TH_BACK);

  UI_view2d_view_ortho(v2d);
  draw_seq_timeline_channels(v2d);

  if ((sseq->flag & SEQ_SHOW_OVERLAY) && (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_GRID)) {
    U.v2d_min_gridsize *= 3;
    UI_view2d_draw_lines_x__discrete_frames_or_seconds(
        v2d, scene, (sseq->flag & SEQ_DRAWFRAMES) == 0, false);
    U.v2d_min_gridsize /= 3;
  }

  /* Only draw backdrop in timeline view. */
  if (sseq->view == SEQ_VIEW_SEQUENCE && sseq->draw_flag & SEQ_DRAW_BACKDROP) {
    int preview_frame = scene->r.cfra;
    if (sequencer_draw_get_transform_preview(sseq, scene)) {
      preview_frame = sequencer_draw_get_transform_preview_frame(scene);
    }

    sequencer_draw_preview(C, scene, region, sseq, preview_frame, 0, false, true);
    UI_view2d_view_ortho(v2d);
  }

  /* Draw attached callbacks. */
  GPU_framebuffer_bind(framebuffer_overlay);
  ED_region_draw_cb_draw(C, region, REGION_DRAW_PRE_VIEW);
  GPU_framebuffer_bind_no_srgb(framebuffer_overlay);

  seq_draw_sfra_efra(scene, v2d);

  if (ed) {
    draw_seq_strips(C, ed, region);
    /* Draw text added in previous function. */
    UI_view2d_text_cache_draw(region);
  }

  UI_view2d_view_ortho(v2d);

  UI_view2d_view_orthoSpecial(region, v2d, true);
  int marker_draw_flag = DRAW_MARKERS_MARGIN;
  if (sseq->flag & SEQ_SHOW_MARKERS) {
    ED_markers_draw(C, marker_draw_flag);
  }

  UI_view2d_view_ortho(v2d);
  ANIM_draw_previewrange(C, v2d, 1);

  if ((sseq->gizmo_flag & SEQ_GIZMO_HIDE) == 0) {
    WM_gizmomap_draw(region->gizmo_map, C, WM_GIZMOMAP_DRAWSTEP_2D);
  }

  /* Draw registered callbacks. */
  GPU_framebuffer_bind(framebuffer_overlay);
  ED_region_draw_cb_draw(C, region, REGION_DRAW_POST_VIEW);
  GPU_framebuffer_bind_no_srgb(framebuffer_overlay);

  UI_view2d_view_restore(C);
  ED_time_scrub_draw(region, scene, !(sseq->flag & SEQ_DRAWFRAMES), true);
}

void draw_timeline_seq_display(const bContext *C, ARegion *region)
{
  const Scene *scene = CTX_data_scene(C);
  const SpaceSeq *sseq = CTX_wm_space_seq(C);
  View2D *v2d = &region->v2d;

  if (scene->ed != nullptr) {
    UI_view2d_view_ortho(v2d);
    draw_cache_view(C);
    if (scene->ed->overlay_frame_flag & SEQ_EDIT_OVERLAY_FRAME_SHOW) {
      draw_overlap_frame_indicator(scene, v2d);
    }
    UI_view2d_view_restore(C);
  }

  ED_time_scrub_draw_current_frame(region, scene, !(sseq->flag & SEQ_DRAWFRAMES));

  const ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(scene));
  SEQ_timeline_boundbox(scene, seqbase, &v2d->tot);
  UI_view2d_scrollers_draw(v2d, nullptr);
}
