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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup spseq
 */

#include <math.h>
#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_string_utils.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf_types.h"

#include "DNA_anim_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
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

#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"
#include "GPU_vertex_buffer.h"
#include "GPU_viewport.h"

#include "ED_anim_api.h"
#include "ED_gpencil.h"
#include "ED_markers.h"
#include "ED_mask.h"
#include "ED_screen.h"
#include "ED_sequencer.h"
#include "ED_space_api.h"
#include "ED_time_scrub_ui.h"
#include "ED_util.h"

#include "BIF_glutil.h"

#include "SEQ_effects.h"
#include "SEQ_prefetch.h"
#include "SEQ_proxy.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_select.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"
#include "SEQ_utils.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BLF_api.h"

#include "MEM_guardedalloc.h"

/* Own include. */
#include "sequencer_intern.h"

#define SEQ_LEFTHANDLE 1
#define SEQ_RIGHTHANDLE 2
#define SEQ_HANDLE_SIZE 8.0f
#define SEQ_SCROLLER_TEXT_OFFSET 8
#define MUTE_ALPHA 120

/* Note, Don't use SEQ_ALL_BEGIN/SEQ_ALL_END while drawing!
 * it messes up transform. */
#undef SEQ_ALL_BEGIN
#undef SEQ_ALL_END
#undef SEQ_CURRENT_BEGIN
#undef SEQ_CURRENT_END

static Sequence *special_seq_update = NULL;

void color3ubv_from_seq(Scene *curscene, Sequence *seq, uchar col[3])
{
  uchar blendcol[3];

  switch (seq->type) {
    case SEQ_TYPE_IMAGE:
      UI_GetThemeColor3ubv(TH_SEQ_IMAGE, col);
      break;

    case SEQ_TYPE_META:
      UI_GetThemeColor3ubv(TH_SEQ_META, col);
      break;

    case SEQ_TYPE_MOVIE:
      UI_GetThemeColor3ubv(TH_SEQ_MOVIE, col);
      break;

    case SEQ_TYPE_MOVIECLIP:
      UI_GetThemeColor3ubv(TH_SEQ_MOVIECLIP, col);
      break;

    case SEQ_TYPE_MASK:
      UI_GetThemeColor3ubv(TH_SEQ_MASK, col);
      break;

    case SEQ_TYPE_SCENE:
      UI_GetThemeColor3ubv(TH_SEQ_SCENE, col);

      if (seq->scene == curscene) {
        UI_GetColorPtrShade3ubv(col, col, 20);
      }
      break;

    /* Transitions use input colors, fallback for when the input is a transition itself. */
    case SEQ_TYPE_CROSS:
    case SEQ_TYPE_GAMCROSS:
    case SEQ_TYPE_WIPE:
      col[0] = 130;
      col[1] = 130;
      col[2] = 130;
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
      UI_GetThemeColor3ubv(TH_SEQ_EFFECT, col);

      /* Slightly offset hue to distinguish different effects. */
      if (seq->type == SEQ_TYPE_ADD) {
        rgb_byte_set_hue_float_offset(col, 0.03);
      }
      else if (seq->type == SEQ_TYPE_SUB) {
        rgb_byte_set_hue_float_offset(col, 0.06);
      }
      else if (seq->type == SEQ_TYPE_MUL) {
        rgb_byte_set_hue_float_offset(col, 0.13);
      }
      else if (seq->type == SEQ_TYPE_ALPHAOVER) {
        rgb_byte_set_hue_float_offset(col, 0.16);
      }
      else if (seq->type == SEQ_TYPE_ALPHAUNDER) {
        rgb_byte_set_hue_float_offset(col, 0.23);
      }
      else if (seq->type == SEQ_TYPE_OVERDROP) {
        rgb_byte_set_hue_float_offset(col, 0.26);
      }
      else if (seq->type == SEQ_TYPE_COLORMIX) {
        rgb_byte_set_hue_float_offset(col, 0.33);
      }
      else if (seq->type == SEQ_TYPE_GAUSSIAN_BLUR) {
        rgb_byte_set_hue_float_offset(col, 0.43);
      }
      else if (seq->type == SEQ_TYPE_GLOW) {
        rgb_byte_set_hue_float_offset(col, 0.46);
      }
      else if (seq->type == SEQ_TYPE_ADJUSTMENT) {
        rgb_byte_set_hue_float_offset(col, 0.55);
      }
      else if (seq->type == SEQ_TYPE_SPEED) {
        rgb_byte_set_hue_float_offset(col, 0.65);
      }
      else if (seq->type == SEQ_TYPE_TRANSFORM) {
        rgb_byte_set_hue_float_offset(col, 0.75);
      }
      else if (seq->type == SEQ_TYPE_MULTICAM) {
        rgb_byte_set_hue_float_offset(col, 0.85);
      }
      break;

    case SEQ_TYPE_COLOR:
      UI_GetThemeColor3ubv(TH_SEQ_COLOR, col);
      break;

    case SEQ_TYPE_SOUND_RAM:
      UI_GetThemeColor3ubv(TH_SEQ_AUDIO, col);
      blendcol[0] = blendcol[1] = blendcol[2] = 128;
      if (seq->flag & SEQ_MUTE) {
        UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.5, 20);
      }
      break;

    case SEQ_TYPE_TEXT:
      UI_GetThemeColor3ubv(TH_SEQ_TEXT, col);
      break;

    default:
      col[0] = 10;
      col[1] = 255;
      col[2] = 40;
      break;
  }
}

/**
 * \param x1, x2, y1, y2: The starting and end X value to draw the wave, same for y1 and y2.
 * \param stepsize: The width of a pixel.
 */
static void draw_seq_waveform_overlay(View2D *v2d,
                                      const bContext *C,
                                      SpaceSeq *sseq,
                                      Scene *scene,
                                      Sequence *seq,
                                      float x1,
                                      float y1,
                                      float x2,
                                      float y2,
                                      float stepsize)
{
  /* Offset x1 and x2 values, to match view min/max, if strip is out of bounds. */
  int x1_offset = max_ff(v2d->cur.xmin, x1);
  int x2_offset = min_ff(v2d->cur.xmax + 1.0f, x2);

  if (seq->sound && ((sseq->flag & SEQ_ALL_WAVEFORMS) || (seq->flag & SEQ_AUDIO_DRAW_WAVEFORM))) {
    int length = floor((x2_offset - x1_offset) / stepsize) + 1;
    float ymid = (y1 + y2) / 2.0f;
    float yscale = (y2 - y1) / 2.0f;
    float samplestep;
    float startsample, endsample;
    float volume = seq->volume;
    float value1, value2;
    bSound *sound = seq->sound;
    SoundWaveform *waveform;

    if (length < 2) {
      return;
    }

    BLI_spin_lock(sound->spinlock);
    if (!sound->waveform) {
      if (!(sound->tags & SOUND_TAGS_WAVEFORM_LOADING)) {
        /* Prevent sounds from reloading. */
        sound->tags |= SOUND_TAGS_WAVEFORM_LOADING;
        BLI_spin_unlock(sound->spinlock);
        sequencer_preview_add_sound(C, seq);
      }
      else {
        BLI_spin_unlock(sound->spinlock);
      }
      return; /* Nothing to draw. */
    }
    BLI_spin_unlock(sound->spinlock);

    waveform = sound->waveform;

    /* Waveform could not be built. */
    if (waveform->length == 0) {
      return;
    }

    startsample = floor((seq->startofs + seq->anim_startofs) / FPS *
                        SOUND_WAVE_SAMPLES_PER_SECOND);
    endsample = ceil((seq->startofs + seq->anim_startofs + seq->enddisp - seq->startdisp) / FPS *
                     SOUND_WAVE_SAMPLES_PER_SECOND);
    samplestep = (endsample - startsample) * stepsize / (x2 - x1);

    length = min_ii(
        floor((waveform->length - startsample) / samplestep - (x1_offset - x1) / stepsize),
        length);

    if (length < 2) {
      return;
    }

    /* F-curve lookup is quite expensive, so do this after precondition. */
    FCurve *fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, "volume", 0, NULL);

    GPU_blend(GPU_BLEND_ALPHA);
    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    uint col = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);
    immBegin(GPU_PRIM_TRI_STRIP, length * 2);

    for (int i = 0; i < length; i++) {
      float sampleoffset = startsample + ((x1_offset - x1) / stepsize + i) * samplestep;
      int p = sampleoffset;

      value1 = waveform->data[p * 3];
      value2 = waveform->data[p * 3 + 1];

      if (samplestep > 1.0f) {
        for (int j = p + 1; (j < waveform->length) && (j < p + samplestep); j++) {
          if (value1 > waveform->data[j * 3]) {
            value1 = waveform->data[j * 3];
          }

          if (value2 < waveform->data[j * 3 + 1]) {
            value2 = waveform->data[j * 3 + 1];
          }
        }
      }
      else if (p + 1 < waveform->length) {
        /* Use simple linear interpolation. */
        float f = sampleoffset - p;
        value1 = (1.0f - f) * value1 + f * waveform->data[p * 3 + 3];
        value2 = (1.0f - f) * value2 + f * waveform->data[p * 3 + 4];
      }

      if (fcu && !BKE_fcurve_is_empty(fcu)) {
        float evaltime = x1_offset + (i * stepsize);
        volume = evaluate_fcurve(fcu, evaltime);
        CLAMP_MIN(volume, 0.0f);
      }
      value1 *= volume;
      value2 *= volume;

      if (value2 > 1 || value1 < -1) {
        immAttr4f(col, 1.0f, 0.0f, 0.0f, 0.5f);

        CLAMP_MAX(value2, 1.0f);
        CLAMP_MIN(value1, -1.0f);
      }
      else {
        immAttr4f(col, 1.0f, 1.0f, 1.0f, 0.5f);
      }

      immVertex2f(pos, x1_offset + i * stepsize, ymid + value1 * yscale);
      immVertex2f(pos, x1_offset + i * stepsize, ymid + value2 * yscale);
    }

    immEnd();
    immUnbindProgram();
    GPU_blend(GPU_BLEND_NONE);
  }
}

static void drawmeta_contents(Scene *scene, Sequence *seqm, float x1, float y1, float x2, float y2)
{
  /* Don't use SEQ_ALL_BEGIN/SEQ_ALL_END here,
   * because it changes seq->depth, which is needed for transform. */
  Sequence *seq;
  uchar col[4];

  int chan_min = MAXSEQ;
  int chan_max = 0;
  int chan_range = 0;
  float draw_range = y2 - y1;
  float draw_height;
  ListBase *seqbase;
  int offset;

  seqbase = SEQ_get_seqbase_from_sequence(seqm, &offset);
  if (!seqbase || BLI_listbase_is_empty(seqbase)) {
    return;
  }

  if (seqm->type == SEQ_TYPE_SCENE) {
    offset = seqm->start - offset;
  }
  else {
    offset = 0;
  }

  GPU_blend(GPU_BLEND_ALPHA);

  for (seq = seqbase->first; seq; seq = seq->next) {
    chan_min = min_ii(chan_min, seq->machine);
    chan_max = max_ii(chan_max, seq->machine);
  }

  chan_range = (chan_max - chan_min) + 1;
  draw_height = draw_range / chan_range;

  col[3] = 196; /* Alpha, used for all meta children. */

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* Draw only immediate children (1 level depth). */
  for (seq = seqbase->first; seq; seq = seq->next) {
    const int startdisp = seq->startdisp + offset;
    const int enddisp = seq->enddisp + offset;

    if ((startdisp > x2 || enddisp < x1) == 0) {
      float y_chan = (seq->machine - chan_min) / (float)(chan_range)*draw_range;
      float x1_chan = startdisp;
      float x2_chan = enddisp;
      float y1_chan, y2_chan;

      if (seq->type == SEQ_TYPE_COLOR) {
        SolidColorVars *colvars = (SolidColorVars *)seq->effectdata;
        rgb_float_to_uchar(col, colvars->col);
      }
      else {
        color3ubv_from_seq(scene, seq, col);
      }

      if ((seqm->flag & SEQ_MUTE) || (seq->flag & SEQ_MUTE)) {
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

/* Get handle width in pixels. */
float sequence_handle_size_get_clamped(Sequence *seq, const float pixelx)
{
  const float maxhandle = (pixelx * SEQ_HANDLE_SIZE) * U.pixelsize;

  /* Ensure that handle is not wider, than half of strip. */
  return min_ff(maxhandle, ((float)(seq->enddisp - seq->startdisp) / 2.0f) / pixelx);
}

/* Draw a handle, on left or right side of strip. */
static void draw_seq_handle(View2D *v2d,
                            Sequence *seq,
                            const float handsize_clamped,
                            const short direction,
                            uint pos,
                            bool seq_active,
                            float pixelx,
                            bool y_threshold)
{
  float rx1 = 0, rx2 = 0;
  float x1, x2, y1, y2;
  uint whichsel = 0;
  uchar col[4];

  x1 = seq->startdisp;
  x2 = seq->enddisp;

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
  if (y_threshold &&
      (((seq->flag & SELECT) && (G.moving & G_TRANSFORM_SEQ)) || (seq->flag & whichsel))) {

    char numstr[64];
    size_t numstr_len;
    const int fontid = BLF_default();
    BLF_set_default();

    /* Calculate if strip is wide enough for showing the labels. */
    numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%d%d", seq->startdisp, seq->enddisp);
    float tot_width = BLF_width(fontid, numstr, numstr_len);

    if ((x2 - x1) / pixelx > 20 + tot_width) {
      col[0] = col[1] = col[2] = col[3] = 255;
      float text_margin = 1.2f * handsize_clamped;

      if (direction == SEQ_LEFTHANDLE) {
        numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%d", seq->startdisp);
        x1 += text_margin;
        y1 += 0.09f;
      }
      else {
        numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%d", seq->enddisp - 1);
        x1 = x2 - (text_margin + pixelx * BLF_width(fontid, numstr, numstr_len));
        y1 += 0.09f;
      }
      UI_view2d_text_cache_add(v2d, x1, y1, numstr, numstr_len, col);
    }
  }
}

static void draw_seq_outline(Sequence *seq,
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
  if ((G.moving & G_TRANSFORM_SEQ) && (seq->flag & SELECT)) {
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
  /* XXX: some platforms don't support OpenGL lines wider than 1px (see T57570),
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

static void draw_seq_text_get_source(Sequence *seq, char *r_source, size_t source_len)
{
  *r_source = '\0';

  /* Set source for the most common types. */
  switch (seq->type) {
    case SEQ_TYPE_IMAGE:
    case SEQ_TYPE_MOVIE: {
      BLI_join_dirfile(r_source, source_len, seq->strip->dir, seq->strip->stripdata->name);
      break;
    }
    case SEQ_TYPE_SOUND_RAM: {
      if (seq->sound != NULL) {
        BLI_strncpy(r_source, seq->sound->filepath, source_len);
      }
      break;
    }
    case SEQ_TYPE_MULTICAM: {
      BLI_snprintf(r_source, source_len, "Channel: %d", seq->multicam_source);
      break;
    }
    case SEQ_TYPE_TEXT: {
      const TextVars *textdata = seq->effectdata;
      BLI_strncpy(r_source, textdata->text, source_len);
      break;
    }
    case SEQ_TYPE_SCENE: {
      if (seq->scene != NULL) {
        if (seq->scene_camera != NULL) {
          BLI_snprintf(r_source,
                       source_len,
                       "%s (%s)",
                       seq->scene->id.name + 2,
                       seq->scene_camera->id.name + 2);
        }
        else {
          BLI_strncpy(r_source, seq->scene->id.name + 2, source_len);
        }
      }
      break;
    }
    case SEQ_TYPE_MOVIECLIP: {
      if (seq->clip != NULL) {
        BLI_strncpy(r_source, seq->clip->id.name + 2, source_len);
      }
      break;
    }
    case SEQ_TYPE_MASK: {
      if (seq->mask != NULL) {
        BLI_strncpy(r_source, seq->mask->id.name + 2, source_len);
      }
      break;
    }
  }
}

static size_t draw_seq_text_get_overlay_string(SpaceSeq *sseq,
                                               Sequence *seq,
                                               char *r_overlay_string,
                                               size_t overlay_string_len)
{
  const char *text_sep = " | ";
  const char *text_array[5];
  int i = 0;

  if (sseq->flag & SEQ_SHOW_STRIP_NAME) {
    text_array[i++] = draw_seq_text_get_name(seq);
  }

  char source[FILE_MAX];
  if (sseq->flag & SEQ_SHOW_STRIP_SOURCE) {
    draw_seq_text_get_source(seq, source, sizeof(source));
    if (source[0] != '\0') {
      if (i != 0) {
        text_array[i++] = text_sep;
      }
      text_array[i++] = source;
    }
  }

  char strip_duration_text[16];
  if (sseq->flag & SEQ_SHOW_STRIP_DURATION) {
    const int strip_duration = seq->enddisp - seq->startdisp;
    SNPRINTF(strip_duration_text, "%d", strip_duration);
    if (i != 0) {
      text_array[i++] = text_sep;
    }
    text_array[i++] = strip_duration_text;
  }

  BLI_assert(i <= ARRAY_SIZE(text_array));

  return BLI_string_join_array(r_overlay_string, overlay_string_len, text_array, i) -
         r_overlay_string;
}

/* Draw info text on a sequence strip. */
static void draw_seq_text_overlay(View2D *v2d,
                                  Sequence *seq,
                                  SpaceSeq *sseq,
                                  float x1,
                                  float x2,
                                  float y1,
                                  float y2,
                                  bool seq_active)
{
  char overlay_string[FILE_MAX];
  size_t overlay_string_len = draw_seq_text_get_overlay_string(
      sseq, seq, overlay_string, sizeof(overlay_string));

  if (overlay_string_len == 0) {
    return;
  }

  /* White text for the active strip. */
  uchar col[4];
  col[0] = col[1] = col[2] = seq_active ? 255 : 10;
  col[3] = 255;

  /* Make the text duller when the strip is muted. */
  if (seq->flag & SEQ_MUTE) {
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

static void draw_sequence_extensions_overlay(Scene *scene, Sequence *seq, uint pos, float pixely)
{
  float x1, x2, y1, y2;
  uchar col[4], blend_col[3];

  x1 = seq->startdisp;
  x2 = seq->enddisp;

  y1 = seq->machine + SEQ_STRIP_OFSBOTTOM;
  y2 = seq->machine + SEQ_STRIP_OFSTOP;

  GPU_blend(GPU_BLEND_ALPHA);

  color3ubv_from_seq(scene, seq, col);
  if (seq->flag & SELECT) {
    UI_GetColorPtrShade3ubv(col, col, 50);
  }
  col[3] = seq->flag & SEQ_MUTE ? MUTE_ALPHA : 200;
  UI_GetColorPtrShade3ubv(col, blend_col, 10);

  if (seq->startofs) {
    immUniformColor4ubv(col);
    immRectf(pos, (float)(seq->start), y1 - pixely, x1, y1 - SEQ_STRIP_OFSBOTTOM);

    /* Outline. */
    immUniformColor3ubv(blend_col);
    imm_draw_box_wire_2d(pos, x1, y1 - pixely, (float)(seq->start), y1 - SEQ_STRIP_OFSBOTTOM);
  }
  if (seq->endofs) {
    immUniformColor4ubv(col);
    immRectf(pos, x2, y2 + pixely, (float)(seq->start + seq->len), y2 + SEQ_STRIP_OFSBOTTOM);

    /* Outline. */
    immUniformColor3ubv(blend_col);
    imm_draw_box_wire_2d(
        pos, x2, y2 + pixely, (float)(seq->start + seq->len), y2 + SEQ_STRIP_OFSBOTTOM);
  }
  GPU_blend(GPU_BLEND_NONE);
}

static void draw_color_strip_band(Sequence *seq, uint pos, float text_margin_y, float y1)
{
  uchar col[4];
  SolidColorVars *colvars = (SolidColorVars *)seq->effectdata;

  rgb_float_to_uchar(col, colvars->col);
  if (seq->flag & SEQ_MUTE) {
    GPU_blend(GPU_BLEND_ALPHA);
    col[3] = MUTE_ALPHA;
  }
  else {
    col[3] = 255;
  }

  immUniformColor4ubv(col);

  immRectf(pos, seq->startdisp, y1, seq->enddisp, text_margin_y);

  /* 1px line to better separate the color band. */
  UI_GetColorPtrShade3ubv(col, col, -20);
  immUniformColor4ubv(col);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(pos, seq->startdisp, text_margin_y);
  immVertex2f(pos, seq->enddisp, text_margin_y);
  immEnd();

  if (seq->flag & SEQ_MUTE) {
    GPU_blend(GPU_BLEND_NONE);
  }
}

static void draw_seq_background(Scene *scene,
                                Sequence *seq,
                                uint pos,
                                float x1,
                                float x2,
                                float y1,
                                float y2,
                                bool is_single_image)
{
  uchar col[4];

  /* Get the correct color per strip type, transitions use their inputs ones. */
  if (ELEM(seq->type, SEQ_TYPE_CROSS, SEQ_TYPE_GAMCROSS, SEQ_TYPE_WIPE)) {
    Sequence *seq1 = seq->seq1;
    if (seq1->type == SEQ_TYPE_COLOR) {
      SolidColorVars *colvars = (SolidColorVars *)seq1->effectdata;
      rgb_float_to_uchar(col, colvars->col);
    }
    else {
      color3ubv_from_seq(scene, seq1, col);
    }
  }
  else {
    color3ubv_from_seq(scene, seq, col);
  }

  if (seq->flag & SEQ_MUTE) {
    GPU_blend(GPU_BLEND_ALPHA);

    col[3] = MUTE_ALPHA;
  }
  else {
    col[3] = 255;
  }
  immUniformColor4ubv(col);

  /* Draw the main strip body. */
  if (is_single_image) {
    immRectf(pos,
             SEQ_transform_get_left_handle_frame(seq, false),
             y1,
             SEQ_transform_get_right_handle_frame(seq, false),
             y2);
  }
  else {
    immRectf(pos, x1, y1, x2, y2);
  }

  /* Draw background for hold still regions. */
  if (!is_single_image && (seq->startstill || seq->endstill)) {
    UI_GetColorPtrShade3ubv(col, col, -35);
    immUniformColor4ubv(col);

    if (seq->startstill) {
      immRectf(pos, seq->startdisp, y1, (float)(seq->start), y2);
    }
    if (seq->endstill) {
      immRectf(pos, (float)(seq->start + seq->len), y1, seq->enddisp, y2);
    }
  }

  /* Draw right half of transition strips. */
  if (ELEM(seq->type, SEQ_TYPE_CROSS, SEQ_TYPE_GAMCROSS, SEQ_TYPE_WIPE)) {
    float vert_pos[3][2];
    Sequence *seq1 = seq->seq1;
    Sequence *seq2 = seq->seq2;

    if (seq2->type == SEQ_TYPE_COLOR) {
      SolidColorVars *colvars = (SolidColorVars *)seq2->effectdata;
      rgb_float_to_uchar(col, colvars->col);
    }
    else {
      color3ubv_from_seq(scene, seq2, col);
      /* If the transition inputs are of the same type, draw the right side slightly darker. */
      if (seq1->type == seq2->type) {
        UI_GetColorPtrShade3ubv(col, col, -15);
      }
    }
    immUniformColor4ubv(col);

    copy_v2_fl2(vert_pos[0], x1, y2);
    copy_v2_fl2(vert_pos[1], x2, y2);
    copy_v2_fl2(vert_pos[2], x2, y1);

    immBegin(GPU_PRIM_TRIS, 3);
    immVertex2fv(pos, vert_pos[0]);
    immVertex2fv(pos, vert_pos[1]);
    immVertex2fv(pos, vert_pos[2]);
    immEnd();
  }

  if (seq->flag & SEQ_MUTE) {
    GPU_blend(GPU_BLEND_NONE);
  }
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
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor4f(1.0f, 0.0f, 0.0f, 0.9f);
  immRectf(pos, x1, y2, x2, text_margin_y);

  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
}

static void calculate_seq_text_offsets(
    View2D *v2d, Sequence *seq, float *x1, float *x2, float pixelx)
{
  const float handsize_clamped = sequence_handle_size_get_clamped(seq, pixelx);
  float text_margin = 2.0f * handsize_clamped;

  *x1 += text_margin;
  *x2 -= text_margin;

  float scroller_vert_xoffs = (V2D_SCROLL_HANDLE_WIDTH + SEQ_SCROLLER_TEXT_OFFSET) * pixelx;

  /* Info text on the strip. */
  if (*x1 < v2d->cur.xmin + scroller_vert_xoffs) {
    *x1 = v2d->cur.xmin + scroller_vert_xoffs;
  }
  else if (*x1 > v2d->cur.xmax) {
    *x1 = v2d->cur.xmax;
  }
  if (*x2 < v2d->cur.xmin) {
    *x2 = v2d->cur.xmin;
  }
  else if (*x2 > v2d->cur.xmax) {
    *x2 = v2d->cur.xmax;
  }
}

static void fcurve_batch_add_verts(GPUVertBuf *vbo,
                                   float y1,
                                   float y2,
                                   float y_height,
                                   int timeline_frame,
                                   float curve_val,
                                   unsigned int *vert_count)
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
    fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, "volume", 0, NULL);
  }
  else {
    fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, "blend_alpha", 0, NULL);
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

    for (int timeline_frame = eval_start; timeline_frame <= eval_end;
         timeline_frame += eval_step) {
      curve_val = evaluate_fcurve(fcu, timeline_frame);
      CLAMP(curve_val, 0.0f, 1.0f);

      /* Avoid adding adjacent verts that have the same value. */
      if (curve_val == prev_val && timeline_frame < eval_end - eval_step) {
        skip = true;
        continue;
      }

      /* If some frames were skipped above, we need to close the shape.  */
      if (skip) {
        fcurve_batch_add_verts(
            vbo, y1, y2, y_height, timeline_frame - eval_step, prev_val, &vert_count);
        skip = false;
      }

      fcurve_batch_add_verts(vbo, y1, y2, y_height, timeline_frame, curve_val, &vert_count);
      prev_val = curve_val;
    }

    GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
    GPU_vertbuf_data_len_set(vbo, vert_count);
    GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_UNIFORM_COLOR);
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
  View2D *v2d = &region->v2d;
  float x1, x2, y1, y2;
  const float handsize_clamped = sequence_handle_size_get_clamped(seq, pixelx);
  float pixely = BLI_rctf_size_y(&v2d->cur) / BLI_rcti_size_y(&v2d->mask);

  /* Check if we are doing "solo preview". */
  bool is_single_image = (char)SEQ_transform_single_image_check(seq);

  /* Draw strip body. */
  x1 = (seq->startstill) ? seq->start : seq->startdisp;
  y1 = seq->machine + SEQ_STRIP_OFSBOTTOM;
  x2 = (seq->endstill) ? (seq->start + seq->len) : seq->enddisp;
  y2 = seq->machine + SEQ_STRIP_OFSTOP;

  float text_margin_y;
  bool y_threshold;
  if ((sseq->flag & SEQ_SHOW_STRIP_NAME) || (sseq->flag & SEQ_SHOW_STRIP_SOURCE) ||
      (sseq->flag & SEQ_SHOW_STRIP_DURATION)) {

    /* Calculate height needed for drawing text on strip. */
    text_margin_y = y2 - min_ff(0.40f, 20 * U.dpi_fac * pixely);

    /* Is there enough space for drawing something else than text? */
    y_threshold = ((y2 - y1) / pixely) > 20 * U.dpi_fac;
  }
  else {
    text_margin_y = y2;
    y_threshold = 1;
  }

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  draw_seq_background(scene, seq, pos, x1, x2, y1, y2, is_single_image);

  /* Draw a color band inside color strip. */
  if (seq->type == SEQ_TYPE_COLOR && y_threshold) {
    draw_color_strip_band(seq, pos, text_margin_y, y1);
  }

  /* Draw strip offsets when flag is enabled or during "solo preview". */
  if ((sseq->flag & SEQ_SHOW_STRIP_OVERLAY)) {
    if (!is_single_image && (seq->startofs || seq->endofs) && pixely > 0) {
      if ((sseq->draw_flag & SEQ_DRAW_OFFSET_EXT) || (seq == special_seq_update)) {
        draw_sequence_extensions_overlay(scene, seq, pos, pixely);
      }
    }
  }
  immUnbindProgram();

  x1 = seq->startdisp;
  x2 = seq->enddisp;

  if ((seq->type == SEQ_TYPE_META) ||
      ((seq->type == SEQ_TYPE_SCENE) && (seq->flag & SEQ_SCENE_STRIPS))) {
    drawmeta_contents(scene, seq, x1, y1, x2, y2);
  }

  if ((sseq->flag & SEQ_SHOW_STRIP_OVERLAY) && (sseq->flag & SEQ_SHOW_FCURVES)) {
    draw_seq_fcurve_overlay(scene, v2d, seq, x1, y1, x2, y2, pixelx);
  }

  /* Draw sound strip waveform. */
  if ((seq->type == SEQ_TYPE_SOUND_RAM) && ((sseq->flag & SEQ_SHOW_STRIP_OVERLAY)) &&
      (sseq->flag & SEQ_NO_WAVEFORMS) == 0) {
    draw_seq_waveform_overlay(v2d,
                              C,
                              sseq,
                              scene,
                              seq,
                              x1,
                              y_threshold ? y1 + 0.05f : y1,
                              x2,
                              y_threshold ? text_margin_y : y2,
                              BLI_rctf_size_x(&region->v2d.cur) / region->winx);
  }
  /* Draw locked state. */
  if (seq->flag & SEQ_LOCK) {
    draw_seq_locked(x1, y1, x2, y2);
  }

  /* Draw Red line on the top of invalid strip (Missing media). */
  if (!SEQ_sequence_has_source(seq)) {
    draw_seq_invalid(x1, x2, y2, text_margin_y);
  }

  pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  if ((seq->flag & SEQ_LOCK) == 0) {
    draw_seq_handle(
        v2d, seq, handsize_clamped, SEQ_LEFTHANDLE, pos, seq_active, pixelx, y_threshold);
    draw_seq_handle(
        v2d, seq, handsize_clamped, SEQ_RIGHTHANDLE, pos, seq_active, pixelx, y_threshold);
  }

  draw_seq_outline(seq, pos, x1, x2, y1, y2, pixelx, pixely, seq_active);

  immUnbindProgram();

  calculate_seq_text_offsets(v2d, seq, &x1, &x2, pixelx);

  /* If a waveform is drawn, avoid drawing text when there is not enough vertical space. */
  if (seq->type == SEQ_TYPE_SOUND_RAM) {
    if (!y_threshold && (sseq->flag & SEQ_NO_WAVEFORMS) == 0 &&
        ((sseq->flag & SEQ_ALL_WAVEFORMS) || (seq->flag & SEQ_AUDIO_DRAW_WAVEFORM))) {
      return;
    }
  }

  if (sseq->flag & SEQ_SHOW_STRIP_OVERLAY) {
    /* Don't draw strip if there is not enough vertical or horizontal space. */
    if (((x2 - x1) > 32 * pixelx * U.dpi_fac) && ((y2 - y1) > 8 * pixely * U.dpi_fac)) {
      /* Depending on the vertical space, draw text on top or in the center of strip. */
      draw_seq_text_overlay(
          v2d, seq, sseq, x1, x2, y_threshold ? text_margin_y : y1, y2, seq_active);
    }
  }
}

static void draw_effect_inputs_highlight(Sequence *seq)
{
  Sequence *seq1 = seq->seq1;
  Sequence *seq2 = seq->seq2;
  Sequence *seq3 = seq->seq3;
  GPU_blend(GPU_BLEND_ALPHA);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor4ub(255, 255, 255, 48);
  immRectf(pos,
           seq1->startdisp,
           seq1->machine + SEQ_STRIP_OFSBOTTOM,
           seq1->enddisp,
           seq1->machine + SEQ_STRIP_OFSTOP);

  if (seq2 && seq2 != seq1) {
    immRectf(pos,
             seq2->startdisp,
             seq2->machine + SEQ_STRIP_OFSBOTTOM,
             seq2->enddisp,
             seq2->machine + SEQ_STRIP_OFSTOP);
  }
  if (seq3 && !ELEM(seq3, seq1, seq2)) {
    immRectf(pos,
             seq3->startdisp,
             seq3->machine + SEQ_STRIP_OFSBOTTOM,
             seq3->enddisp,
             seq3->machine + SEQ_STRIP_OFSTOP);
  }
  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
}

void sequencer_special_update_set(Sequence *seq)
{
  special_seq_update = seq;
}

Sequence *ED_sequencer_special_preview_get(void)
{
  return special_seq_update;
}

void ED_sequencer_special_preview_set(bContext *C, const int mval[2])
{
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  int hand;
  Sequence *seq;
  seq = find_nearest_seq(scene, &region->v2d, &hand, mval);
  sequencer_special_update_set(seq);
}

void ED_sequencer_special_preview_clear(void)
{
  sequencer_special_update_set(NULL);
}

/**
 * Rendering using opengl will change the current viewport/context.
 * This is why we need the \a region, to set back the render area.
 *
 * TODO: do not rely on such hack and just update the \a ibuf outside of
 * the UI drawing code.
 */
ImBuf *sequencer_ibuf_get(struct Main *bmain,
                          ARegion *region,
                          struct Depsgraph *depsgraph,
                          Scene *scene,
                          SpaceSeq *sseq,
                          int timeline_frame,
                          int frame_ofs,
                          const char *viewname)
{
  SeqRenderData context = {0};
  ImBuf *ibuf;
  int rectx, recty;
  double render_size;
  short is_break = G.is_break;

  if (sseq->render_size == SEQ_RENDER_SIZE_NONE) {
    return NULL;
  }

  if (sseq->render_size == SEQ_RENDER_SIZE_SCENE) {
    render_size = scene->r.size / 100.0;
  }
  else {
    render_size = SEQ_rendersize_to_scale_factor(sseq->render_size);
  }

  rectx = roundf(render_size * scene->r.xsch);
  recty = roundf(render_size * scene->r.ysch);

  SEQ_render_new_render_data(
      bmain, depsgraph, scene, rectx, recty, sseq->render_size, false, &context);
  context.view_id = BKE_scene_multiview_view_id_get(&scene->r, viewname);

  /* Sequencer could start rendering, in this case we need to be sure it wouldn't be canceled
   * by Escape pressed somewhere in the past. */
  G.is_break = false;

  GPUViewport *viewport = WM_draw_region_get_bound_viewport(region);
  GPUFrameBuffer *fb = GPU_framebuffer_active_get();
  if (viewport) {
    /* Unbind viewport to release the DRW context. */
    GPU_viewport_unbind(viewport);
  }
  else {
    /* Rendering can change OGL context. Save & Restore frame-buffer. */
    GPU_framebuffer_restore();
  }

  if (special_seq_update) {
    ibuf = SEQ_render_give_ibuf_direct(&context, timeline_frame + frame_ofs, special_seq_update);
  }
  else {
    ibuf = SEQ_render_give_ibuf(&context, timeline_frame + frame_ofs, sseq->chanshown);
  }

  if (viewport) {
    /* Follows same logic as wm_draw_window_offscreen to make sure to restore the same viewport. */
    int view = (sseq->multiview_eye == STEREO_RIGHT_ID) ? 1 : 0;
    GPU_viewport_bind(viewport, view, &region->winrct);
  }
  else if (fb) {
    GPU_framebuffer_bind(fb);
  }

  /* Restore state so real rendering would be canceled if needed. */
  G.is_break = is_break;

  return ibuf;
}

static void sequencer_check_scopes(SequencerScopes *scopes, ImBuf *ibuf)
{
  if (scopes->reference_ibuf != ibuf) {
    if (scopes->zebra_ibuf) {
      IMB_freeImBuf(scopes->zebra_ibuf);
      scopes->zebra_ibuf = NULL;
    }

    if (scopes->waveform_ibuf) {
      IMB_freeImBuf(scopes->waveform_ibuf);
      scopes->waveform_ibuf = NULL;
    }

    if (scopes->sep_waveform_ibuf) {
      IMB_freeImBuf(scopes->sep_waveform_ibuf);
      scopes->sep_waveform_ibuf = NULL;
    }

    if (scopes->vector_ibuf) {
      IMB_freeImBuf(scopes->vector_ibuf);
      scopes->vector_ibuf = NULL;
    }

    if (scopes->histogram_ibuf) {
      IMB_freeImBuf(scopes->histogram_ibuf);
      scopes->histogram_ibuf = NULL;
    }
  }
}

static ImBuf *sequencer_make_scope(Scene *scene, ImBuf *ibuf, ImBuf *(*make_scope_fn)(ImBuf *ibuf))
{
  ImBuf *display_ibuf = IMB_dupImBuf(ibuf);
  ImBuf *scope;

  IMB_colormanagement_imbuf_make_display_space(
      display_ibuf, &scene->view_settings, &scene->display_settings);

  scope = make_scope_fn(display_ibuf);

  IMB_freeImBuf(display_ibuf);

  return scope;
}

static void sequencer_display_size(Scene *scene, float r_viewrect[2])
{
  r_viewrect[0] = (float)scene->r.xsch;
  r_viewrect[1] = (float)scene->r.ysch;

  r_viewrect[0] *= scene->r.xasp / scene->r.yasp;
}

static void sequencer_draw_gpencil_overlay(const bContext *C)
{
  /* Draw grease-pencil (image aligned). */
  ED_annotation_draw_2dimage(C);

  /* Orthographic at pixel level. */
  UI_view2d_view_restore(C);

  /* Draw grease-pencil (screen aligned). */
  ED_annotation_draw_view2d(C, 0);
}

/* Draw content and safety borders borders. */
static void sequencer_draw_borders_overlay(const SpaceSeq *sseq,
                                           const View2D *v2d,
                                           const Scene *scene)
{
  float x1 = v2d->tot.xmin;
  float y1 = v2d->tot.ymin;
  float x2 = v2d->tot.xmax;
  float y2 = v2d->tot.ymax;

  GPU_line_width(1.0f);

  /* Draw border. */
  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

  immUniformThemeColor(TH_BACK);
  immUniform1i("colors_len", 0); /* Simple dashes. */
  immUniform1f("dash_width", 6.0f);
  immUniform1f("dash_factor", 0.5f);

  imm_draw_box_wire_2d(shdr_pos, x1 - 0.5f, y1 - 0.5f, x2 + 0.5f, y2 + 0.5f);

  /* Draw safety border. */
  if (sseq->flag & SEQ_SHOW_SAFE_MARGINS) {
    immUniformThemeColorBlend(TH_VIEW_OVERLAY, TH_BACK, 0.25f);

    UI_draw_safe_areas(shdr_pos,
                       &(const rctf){
                           .xmin = x1,
                           .xmax = x2,
                           .ymin = y1,
                           .ymax = y2,
                       },
                       scene->safe_areas.title,
                       scene->safe_areas.action);

    if (sseq->flag & SEQ_SHOW_SAFE_CENTER) {
      UI_draw_safe_areas(shdr_pos,
                         &(const rctf){
                             .xmin = x1,
                             .xmax = x2,
                             .ymin = y1,
                             .ymax = y2,
                         },
                         scene->safe_areas.title_center,
                         scene->safe_areas.action_center);
    }
  }

  immUnbindProgram();
}

#if 0
void sequencer_draw_maskedit(const bContext *C, Scene *scene, ARegion *region, SpaceSeq *sseq)
{
  /* NOTE: sequencer mask editing isn't finished, the draw code is working but editing not.
   * For now just disable drawing since the strip frame will likely be offset. */

  // if (sc->mode == SC_MODE_MASKEDIT)
  if (0 && sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
    Mask *mask = SEQ_active_mask_get(scene);

    if (mask) {
      int width, height;
      float aspx = 1.0f, aspy = 1.0f;
      // ED_mask_get_size(C, &width, &height);

      //Scene *scene = CTX_data_scene(C);
      width = (scene->r.size * scene->r.xsch) / 100;
      height = (scene->r.size * scene->r.ysch) / 100;

      ED_mask_draw_region(mask,
                          region,
                          0,
                          0,
                          0, /* TODO */
                          width,
                          height,
                          aspx,
                          aspy,
                          false,
                          true,
                          NULL,
                          C);
    }
  }
}
#endif

/* Force redraw, when prefetching and using cache view. */
static void seq_prefetch_wm_notify(const bContext *C, Scene *scene)
{
  if (SEQ_prefetch_need_redraw(CTX_data_main(C), scene)) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, NULL);
  }
}

static void *sequencer_OCIO_transform_ibuf(const bContext *C,
                                           ImBuf *ibuf,
                                           bool *r_glsl_used,
                                           eGPUTextureFormat *r_format,
                                           eGPUDataFormat *r_data)
{
  void *display_buffer;
  void *cache_handle = NULL;
  bool force_fallback = false;
  *r_glsl_used = false;
  force_fallback |= (ED_draw_imbuf_method(ibuf) != IMAGE_DRAW_METHOD_GLSL);
  force_fallback |= (ibuf->dither != 0.0f);

  /* Default */
  *r_format = GPU_RGBA8;
  *r_data = GPU_DATA_UBYTE;

  /* Fallback to CPU based color space conversion. */
  if (force_fallback) {
    *r_glsl_used = false;
    display_buffer = NULL;
  }
  else if (ibuf->rect_float) {
    display_buffer = ibuf->rect_float;

    *r_data = GPU_DATA_FLOAT;
    if (ibuf->channels == 4) {
      *r_format = GPU_RGBA16F;
    }
    else if (ibuf->channels == 3) {
      /* Alpha is implicitly 1. */
      *r_format = GPU_RGB16F;
    }
    else {
      BLI_assert(!"Incompatible number of channels for float buffer in sequencer");
      *r_format = GPU_RGBA16F;
      display_buffer = NULL;
    }

    if (ibuf->float_colorspace) {
      *r_glsl_used = IMB_colormanagement_setup_glsl_draw_from_space_ctx(
          C, ibuf->float_colorspace, ibuf->dither, true);
    }
    else {
      *r_glsl_used = IMB_colormanagement_setup_glsl_draw_ctx(C, ibuf->dither, true);
    }
  }
  else if (ibuf->rect) {
    display_buffer = ibuf->rect;

    *r_glsl_used = IMB_colormanagement_setup_glsl_draw_from_space_ctx(
        C, ibuf->rect_colorspace, ibuf->dither, false);
  }
  else {
    display_buffer = NULL;
  }

  /* There is data to be displayed, but GLSL is not initialized
   * properly, in this case we fallback to CPU-based display transform. */
  if ((ibuf->rect || ibuf->rect_float) && !*r_glsl_used) {
    display_buffer = IMB_display_buffer_acquire_ctx(C, ibuf, &cache_handle);
    *r_format = GPU_RGBA8;
    *r_data = GPU_DATA_UBYTE;
  }
  if (cache_handle) {
    IMB_display_buffer_release(cache_handle);
  }

  return display_buffer;
}

static void sequencer_stop_running_jobs(const bContext *C, Scene *scene)
{
  if (G.is_rendering == false && (scene->r.seq_prev_type) == OB_RENDER) {
    /* Stop all running jobs, except screen one. Currently previews frustrate Render.
     * Need to make so sequencers rendering doesn't conflict with compositor. */
    WM_jobs_kill_type(CTX_wm_manager(C), NULL, WM_JOB_TYPE_COMPOSITE);

    /* In case of final rendering used for preview, kill all previews,
     * otherwise threading conflict will happen in rendering module. */
    WM_jobs_kill_type(CTX_wm_manager(C), NULL, WM_JOB_TYPE_RENDER_PREVIEW);
  }
}

static void sequencer_preview_clear(void)
{
  UI_ThemeClearColor(TH_SEQ_PREVIEW);
}

static void sequencer_preview_get_rect(rctf *preview,
                                       Scene *scene,
                                       ARegion *region,
                                       SpaceSeq *sseq,
                                       bool draw_overlay,
                                       bool draw_backdrop)
{
  struct View2D *v2d = &region->v2d;
  float viewrect[2];

  sequencer_display_size(scene, viewrect);
  BLI_rctf_init(preview, -1.0f, 1.0f, -1.0f, 1.0f);

  if (draw_overlay && sseq->overlay_type == SEQ_DRAW_OVERLAY_RECT) {
    preview->xmax = v2d->tot.xmin +
                    (fabsf(BLI_rctf_size_x(&v2d->tot)) * scene->ed->over_border.xmax);
    preview->xmin = v2d->tot.xmin +
                    (fabsf(BLI_rctf_size_x(&v2d->tot)) * scene->ed->over_border.xmin);
    preview->ymax = v2d->tot.ymin +
                    (fabsf(BLI_rctf_size_y(&v2d->tot)) * scene->ed->over_border.ymax);
    preview->ymin = v2d->tot.ymin +
                    (fabsf(BLI_rctf_size_y(&v2d->tot)) * scene->ed->over_border.ymin);
  }
  else if (draw_backdrop) {
    float aspect = BLI_rcti_size_x(&region->winrct) / (float)BLI_rcti_size_y(&region->winrct);
    float image_aspect = viewrect[0] / viewrect[1];

    if (aspect >= image_aspect) {
      preview->xmax = image_aspect / aspect;
      preview->xmin = -preview->xmax;
    }
    else {
      preview->ymax = aspect / image_aspect;
      preview->ymin = -preview->ymax;
    }
  }
  else {
    *preview = v2d->tot;
  }
}

static void sequencer_draw_display_buffer(const bContext *C,
                                          Scene *scene,
                                          ARegion *region,
                                          SpaceSeq *sseq,
                                          ImBuf *ibuf,
                                          ImBuf *scope,
                                          bool draw_overlay,
                                          bool draw_backdrop)
{
  void *display_buffer;

  if (sseq->mainb == SEQ_DRAW_IMG_IMBUF && sseq->flag & SEQ_USE_ALPHA) {
    GPU_blend(GPU_BLEND_ALPHA);
  }

  /* Format needs to be created prior to any #immBindShader call.
   * Do it here because OCIO binds its own shader. */
  eGPUTextureFormat format;
  eGPUDataFormat data;
  bool glsl_used = false;
  GPUVertFormat *imm_format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(imm_format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint texCoord = GPU_vertformat_attr_add(
      imm_format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  if (scope) {
    ibuf = scope;

    if (ibuf->rect_float && ibuf->rect == NULL) {
      IMB_rect_from_float(ibuf);
    }

    display_buffer = (uchar *)ibuf->rect;
    format = GPU_RGBA8;
    data = GPU_DATA_UBYTE;
  }
  else {
    display_buffer = sequencer_OCIO_transform_ibuf(C, ibuf, &glsl_used, &format, &data);
  }

  if (draw_backdrop) {
    GPU_matrix_push();
    GPU_matrix_identity_set();
    GPU_matrix_push_projection();
    GPU_matrix_identity_projection_set();
  }

  GPUTexture *texture = GPU_texture_create_2d(
      "seq_display_buf", ibuf->x, ibuf->y, 1, format, NULL);
  GPU_texture_update(texture, data, display_buffer);
  GPU_texture_filter_mode(texture, false);

  GPU_texture_bind(texture, 0);

  if (!glsl_used) {
    immBindBuiltinProgram(GPU_SHADER_2D_IMAGE_COLOR);
    immUniformColor3f(1.0f, 1.0f, 1.0f);
    immUniform1i("image", 0);
  }

  immBegin(GPU_PRIM_TRI_FAN, 4);

  rctf preview;
  rctf canvas;
  sequencer_preview_get_rect(&preview, scene, region, sseq, draw_overlay, draw_backdrop);

  if (draw_overlay && sseq->overlay_type == SEQ_DRAW_OVERLAY_RECT) {
    canvas = scene->ed->over_border;
  }
  else {
    BLI_rctf_init(&canvas, 0.0f, 1.0f, 0.0f, 1.0f);
  }

  immAttr2f(texCoord, canvas.xmin, canvas.ymin);
  immVertex2f(pos, preview.xmin, preview.ymin);

  immAttr2f(texCoord, canvas.xmin, canvas.ymax);
  immVertex2f(pos, preview.xmin, preview.ymax);

  immAttr2f(texCoord, canvas.xmax, canvas.ymax);
  immVertex2f(pos, preview.xmax, preview.ymax);

  immAttr2f(texCoord, canvas.xmax, canvas.ymin);
  immVertex2f(pos, preview.xmax, preview.ymin);

  immEnd();

  GPU_texture_unbind(texture);
  GPU_texture_free(texture);

  if (!glsl_used) {
    immUnbindProgram();
  }
  else {
    IMB_colormanagement_finish_glsl_draw();
  }

  if (sseq->mainb == SEQ_DRAW_IMG_IMBUF && sseq->flag & SEQ_USE_ALPHA) {
    GPU_blend(GPU_BLEND_NONE);
  }

  if (draw_backdrop) {
    GPU_matrix_pop();
    GPU_matrix_pop_projection();
  }
}

static ImBuf *sequencer_get_scope(Scene *scene, SpaceSeq *sseq, ImBuf *ibuf, bool draw_backdrop)
{
  struct ImBuf *scope = NULL;
  SequencerScopes *scopes = &sseq->scopes;

  if (!draw_backdrop && (sseq->mainb != SEQ_DRAW_IMG_IMBUF || sseq->zebra != 0)) {
    sequencer_check_scopes(scopes, ibuf);

    switch (sseq->mainb) {
      case SEQ_DRAW_IMG_IMBUF:
        if (!scopes->zebra_ibuf) {
          ImBuf *display_ibuf = IMB_dupImBuf(ibuf);

          if (display_ibuf->rect_float) {
            IMB_colormanagement_imbuf_make_display_space(
                display_ibuf, &scene->view_settings, &scene->display_settings);
          }
          scopes->zebra_ibuf = make_zebra_view_from_ibuf(display_ibuf, sseq->zebra);
          IMB_freeImBuf(display_ibuf);
        }
        scope = scopes->zebra_ibuf;
        break;
      case SEQ_DRAW_IMG_WAVEFORM:
        if ((sseq->flag & SEQ_DRAW_COLOR_SEPARATED) != 0) {
          if (!scopes->sep_waveform_ibuf) {
            scopes->sep_waveform_ibuf = sequencer_make_scope(
                scene, ibuf, make_sep_waveform_view_from_ibuf);
          }
          scope = scopes->sep_waveform_ibuf;
        }
        else {
          if (!scopes->waveform_ibuf) {
            scopes->waveform_ibuf = sequencer_make_scope(
                scene, ibuf, make_waveform_view_from_ibuf);
          }
          scope = scopes->waveform_ibuf;
        }
        break;
      case SEQ_DRAW_IMG_VECTORSCOPE:
        if (!scopes->vector_ibuf) {
          scopes->vector_ibuf = sequencer_make_scope(scene, ibuf, make_vectorscope_view_from_ibuf);
        }
        scope = scopes->vector_ibuf;
        break;
      case SEQ_DRAW_IMG_HISTOGRAM:
        if (!scopes->histogram_ibuf) {
          scopes->histogram_ibuf = sequencer_make_scope(
              scene, ibuf, make_histogram_view_from_ibuf);
        }
        scope = scopes->histogram_ibuf;
        break;
    }

    /* Future files may have new scopes we don't catch above. */
    if (scope) {
      scopes->reference_ibuf = ibuf;
    }
  }
  return scope;
}

void sequencer_draw_preview(const bContext *C,
                            Scene *scene,
                            ARegion *region,
                            SpaceSeq *sseq,
                            int timeline_frame,
                            int offset,
                            bool draw_overlay,
                            bool draw_backdrop)
{
  struct Main *bmain = CTX_data_main(C);
  struct Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
  struct View2D *v2d = &region->v2d;
  struct ImBuf *ibuf = NULL;
  struct ImBuf *scope = NULL;
  float viewrect[2];
  const bool show_imbuf = ED_space_sequencer_check_show_imbuf(sseq);
  const bool draw_gpencil = ((sseq->flag & SEQ_SHOW_GPENCIL) && sseq->gpd);
  const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};

  sequencer_stop_running_jobs(C, scene);
  if (G.is_rendering) {
    return;
  }

  /* Get image. */
  ibuf = sequencer_ibuf_get(
      bmain, region, depsgraph, scene, sseq, timeline_frame, offset, names[sseq->multiview_eye]);

  /* Setup off-screen buffers. */
  GPUViewport *viewport = WM_draw_region_get_viewport(region);
  GPUFrameBuffer *framebuffer_overlay = GPU_viewport_framebuffer_overlay_get(viewport);
  GPU_framebuffer_bind_no_srgb(framebuffer_overlay);
  GPU_depth_test(GPU_DEPTH_NONE);

  if (sseq->render_size == SEQ_RENDER_SIZE_NONE) {
    sequencer_preview_clear();
    return;
  }

  /* Setup view. */
  sequencer_display_size(scene, viewrect);
  UI_view2d_totRect_set(v2d, roundf(viewrect[0] + 0.5f), roundf(viewrect[1] + 0.5f));
  UI_view2d_curRect_validate(v2d);
  UI_view2d_view_ortho(v2d);

  /* Draw background. */
  if (!draw_backdrop && (!draw_overlay || sseq->overlay_type == SEQ_DRAW_OVERLAY_REFERENCE)) {
    sequencer_preview_clear();

    if (sseq->flag & SEQ_USE_ALPHA) {
      imm_draw_box_checker_2d(v2d->tot.xmin, v2d->tot.ymin, v2d->tot.xmax, v2d->tot.ymax);
    }
  }

  if (ibuf) {
    scope = sequencer_get_scope(scene, sseq, ibuf, draw_backdrop);

    /* Draw image. */
    sequencer_draw_display_buffer(
        C, scene, region, sseq, ibuf, scope, draw_overlay, draw_backdrop);

    /* Draw over image. */
    if (sseq->flag & SEQ_SHOW_METADATA && sseq->flag & SEQ_SHOW_STRIP_OVERLAY) {
      ED_region_image_metadata_draw(0.0, 0.0, ibuf, &v2d->tot, 1.0, 1.0);
    }
  }

  if (show_imbuf && (sseq->flag & SEQ_SHOW_STRIP_OVERLAY)) {
    sequencer_draw_borders_overlay(sseq, v2d, scene);
  }

  if (draw_gpencil && show_imbuf && (sseq->flag & SEQ_SHOW_STRIP_OVERLAY)) {
    sequencer_draw_gpencil_overlay(C);
  }
#if 0
  sequencer_draw_maskedit(C, scene, region, sseq);
#endif

  /* Scope is freed in sequencer_check_scopes when `ibuf` changes and redraw is needed. */
  if (ibuf) {
    IMB_freeImBuf(ibuf);
  }

  UI_view2d_view_restore(C);
  seq_prefetch_wm_notify(C, scene);
}

/* Draw backdrop in sequencer timeline. */
static void draw_seq_backdrop(View2D *v2d)
{
  int i;

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* View backdrop. */
  immUniformThemeColorShade(TH_BACK, -25);
  immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);

  /* Darker overlay over the view backdrop. */
  immUniformThemeColorShade(TH_BACK, -20);
  immRectf(pos, v2d->cur.xmin, -1.0, v2d->cur.xmax, 1.0);

  /* Alternating horizontal stripes. */
  i = max_ii(1, ((int)v2d->cur.ymin) - 1);

  GPU_blend(GPU_BLEND_ALPHA);
  immUniformThemeColor(TH_ROW_ALTERNATE);

  while (i < v2d->cur.ymax) {
    if (i & 1) {
      immRectf(pos, v2d->cur.xmin, i, v2d->cur.xmax, i + 1);
    }
    i++;
  }

  GPU_blend(GPU_BLEND_NONE);

  /* Lines separating the horizontal bands. */
  i = max_ii(1, ((int)v2d->cur.ymin) - 1);
  int line_len = (int)v2d->cur.ymax - i + 1;
  immUniformThemeColor(TH_GRID);
  immBegin(GPU_PRIM_LINES, line_len * 2);
  while (line_len--) {
    immVertex2f(pos, v2d->cur.xmax, i);
    immVertex2f(pos, v2d->cur.xmin, i);
    i++;
  }
  immEnd();

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
    Sequence *seq;
    /* Loop through strips, checking for those that are visible. */
    for (seq = ed->seqbasep->first; seq; seq = seq->next) {
      /* Bound-box and selection tests for NOT drawing the strip. */
      if ((seq->flag & SELECT) != sel) {
        continue;
      }
      if (seq == last_seq && (last_seq->flag & SELECT)) {
        continue;
      }
      if (min_ii(seq->startdisp, seq->start) > v2d->cur.xmax) {
        continue;
      }
      if (max_ii(seq->enddisp, seq->start + seq->len) < v2d->cur.xmin) {
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
      draw_effect_inputs_highlight(last_seq);
    }
    /* When active is a Multi-cam strip, highlight its source channel. */
    else if (last_seq->type == SEQ_TYPE_MULTICAM) {
      int channel = last_seq->multicam_source;
      if (channel != 0) {
        GPU_blend(GPU_BLEND_ALPHA);
        uint pos = GPU_vertformat_attr_add(
            immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
        immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

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
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    immUniformColor4ub(255, 255, 255, 48);
    immRectf(pos,
             seq->startdisp,
             seq->machine + SEQ_STRIP_OFSBOTTOM,
             seq->enddisp,
             seq->machine + SEQ_STRIP_OFSTOP);

    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
  }
}

static void seq_draw_sfra_efra(Scene *scene, View2D *v2d)
{
  const Editing *ed = SEQ_editing_get(scene, false);
  const int frame_sta = scene->r.sfra;
  const int frame_end = scene->r.efra + 1;

  GPU_blend(GPU_BLEND_ALPHA);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* Draw overlay outside of frame range. */
  immUniformThemeColorShadeAlpha(TH_BACK, -25, -100);

  if (frame_sta < frame_end) {
    immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, (float)frame_sta, v2d->cur.ymax);
    immRectf(pos, (float)frame_end, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
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
    MetaStack *ms = ed->metastack.last;
    immUnbindProgram();

    immBindBuiltinProgram(GPU_SHADER_2D_CHECKER);

    immUniform4f("color1", 0.0f, 0.0f, 0.0f, 0.22f);
    immUniform4f("color2", 1.0f, 1.0f, 1.0f, 0.0f);
    immUniform1i("size", 8);

    immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, ms->disp_range[0], v2d->cur.ymax);
    immRectf(pos, ms->disp_range[1], v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);

    immUnbindProgram();

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
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

typedef struct CacheDrawData {
  struct View2D *v2d;
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
} CacheDrawData;

/* Called as a callback. */
static bool draw_cache_view_init_fn(void *userdata, size_t item_count)
{
  if (item_count == 0) {
    return true;
  }

  CacheDrawData *drawdata = userdata;
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
                                    struct Sequence *seq,
                                    int timeline_frame,
                                    int cache_type)
{
  CacheDrawData *drawdata = userdata;
  struct View2D *v2d = drawdata->v2d;
  float stripe_bot, stripe_top, stripe_ofs_y, stripe_ht;
  GPUVertBuf *vbo;
  size_t *vert_count;

  if ((cache_type & SEQ_CACHE_STORE_FINAL_OUT) &&
      (drawdata->cache_flag & SEQ_CACHE_VIEW_FINAL_OUT)) {
    stripe_ht = UI_view2d_region_to_view_y(v2d, 4.0f * UI_DPI_FAC * U.pixelsize) - v2d->cur.ymin;
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
           (drawdata->cache_flag & SEQ_CACHE_VIEW_PREPROCESSED)) {
    stripe_ofs_y = drawdata->stripe_ofs_y;
    stripe_ht = drawdata->stripe_ht;
    stripe_bot = seq->machine + SEQ_STRIP_OFSBOTTOM + (stripe_ofs_y + stripe_ht) + stripe_ofs_y;
    stripe_top = stripe_bot + stripe_ht;
    vbo = drawdata->preprocessed_vbo;
    vert_count = &drawdata->preprocessed_vert_count;
  }
  else if ((cache_type & SEQ_CACHE_STORE_COMPOSITE) &&
           (drawdata->cache_flag & SEQ_CACHE_VIEW_COMPOSITE)) {
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
  GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
  if (vert_count > 0) {
    GPU_vertbuf_data_len_set(vbo, vert_count);
    GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_UNIFORM_COLOR);
    GPU_batch_uniform_4f(batch, "color", col_r, col_g, col_b, col_a);
    GPU_batch_draw(batch);
  }
  GPU_batch_discard(batch);
}

static void draw_cache_view(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  struct View2D *v2d = &region->v2d;

  if ((scene->ed->cache_flag & SEQ_CACHE_VIEW_ENABLE) == 0) {
    return;
  }

  GPU_blend(GPU_BLEND_ALPHA);
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  float stripe_bot, stripe_top;
  float stripe_ofs_y = UI_view2d_region_to_view_y(v2d, 1.0f) - v2d->cur.ymin;
  float stripe_ht = UI_view2d_region_to_view_y(v2d, 4.0f * UI_DPI_FAC * U.pixelsize) -
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

  for (Sequence *seq = scene->ed->seqbasep->first; seq != NULL; seq = seq->next) {
    if (seq->type == SEQ_TYPE_SOUND_RAM) {
      continue;
    }

    if (seq->startdisp > v2d->cur.xmax || seq->enddisp < v2d->cur.xmin) {
      continue;
    }

    stripe_bot = seq->machine + SEQ_STRIP_OFSBOTTOM + stripe_ofs_y;
    stripe_top = stripe_bot + stripe_ht;

    if (scene->ed->cache_flag & SEQ_CACHE_VIEW_RAW) {
      const float bg_color[4] = {1.0f, 0.1f, 0.02f, 0.1f};
      immUniformColor4f(bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
      immRectf(pos, seq->startdisp, stripe_bot, seq->enddisp, stripe_top);
    }

    stripe_bot += stripe_ht + stripe_ofs_y;
    stripe_top = stripe_bot + stripe_ht;

    if (scene->ed->cache_flag & SEQ_CACHE_VIEW_PREPROCESSED) {
      const float bg_color[4] = {0.1f, 0.1f, 0.75f, 0.1f};
      immUniformColor4f(bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
      immRectf(pos, seq->startdisp, stripe_bot, seq->enddisp, stripe_top);
    }

    stripe_top = seq->machine + SEQ_STRIP_OFSTOP - stripe_ofs_y;
    stripe_bot = stripe_top - stripe_ht;

    if (scene->ed->cache_flag & SEQ_CACHE_VIEW_COMPOSITE) {
      const float bg_color[4] = {1.0f, 0.6f, 0.0f, 0.1f};
      immUniformColor4f(bg_color[0], bg_color[1], bg_color[2], bg_color[3]);
      immRectf(pos, seq->startdisp, stripe_bot, seq->enddisp, stripe_top);
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
void draw_timeline_seq(const bContext *C, ARegion *region)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  View2D *v2d = &region->v2d;
  short cfra_flag = 0;
  float col[3];

  seq_prefetch_wm_notify(C, scene);

  GPUViewport *viewport = WM_draw_region_get_viewport(region);
  GPUFrameBuffer *framebuffer_overlay = GPU_viewport_framebuffer_overlay_get(viewport);
  GPU_framebuffer_bind_no_srgb(framebuffer_overlay);
  GPU_depth_test(GPU_DEPTH_NONE);

  UI_GetThemeColor3fv(TH_BACK, col);
  if (ed && ed->metastack.first) {
    GPU_clear_color(col[0], col[1], col[2] - 0.1f, 0.0f);
  }
  else {
    GPU_clear_color(col[0], col[1], col[2], 0.0f);
  }

  UI_view2d_view_ortho(v2d);
  /* Get timeline bound-box, needed for the scroll-bars. */
  SEQ_timeline_boundbox(scene, SEQ_active_seqbase_get(ed), &v2d->tot);
  draw_seq_backdrop(v2d);
  UI_view2d_constant_grid_draw(v2d, FPS);

  /* Only draw backdrop in timeline view. */
  if (sseq->view == SEQ_VIEW_SEQUENCE && sseq->draw_flag & SEQ_DRAW_BACKDROP) {
    sequencer_draw_preview(C, scene, region, sseq, scene->r.cfra, 0, false, true);
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
  if ((sseq->flag & SEQ_DRAWFRAMES) == 0) {
    cfra_flag |= DRAWCFRA_UNIT_SECONDS;
  }

  /* Draw overlap frame frame indicator. */
  if (scene->ed && scene->ed->over_flag & SEQ_EDIT_OVERLAY_SHOW) {
    int overlap_frame = (scene->ed->over_flag & SEQ_EDIT_OVERLAY_ABS) ?
                            scene->ed->over_cfra :
                            scene->r.cfra + scene->ed->over_ofs;

    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);
    float viewport_size[4];
    GPU_viewport_size_get_f(viewport_size);
    immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);
    /* Shader may have color set from past usage - reset it. */
    immUniform1i("colors_len", 0);
    immUniform1f("dash_width", 20.0f * U.pixelsize);
    immUniform1f("dash_factor", 0.5f);
    immUniformThemeColor(TH_CFRAME);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, overlap_frame, v2d->cur.ymin);
    immVertex2f(pos, overlap_frame, v2d->cur.ymax);
    immEnd();

    immUnbindProgram();
  }

  UI_view2d_view_orthoSpecial(region, v2d, 1);
  int marker_draw_flag = DRAW_MARKERS_MARGIN;
  if (sseq->flag & SEQ_SHOW_MARKERS) {
    ED_markers_draw(C, marker_draw_flag);
  }

  UI_view2d_view_ortho(v2d);

  if (ed) {
    draw_cache_view(C);
  }

  ANIM_draw_previewrange(C, v2d, 1);

  /* Draw registered callbacks. */
  GPU_framebuffer_bind(framebuffer_overlay);
  ED_region_draw_cb_draw(C, region, REGION_DRAW_POST_VIEW);
  GPU_framebuffer_bind_no_srgb(framebuffer_overlay);

  UI_view2d_view_restore(C);
  ED_time_scrub_draw(region, scene, !(sseq->flag & SEQ_DRAWFRAMES), true);

  /* Draw channel numbers. */
  {
    rcti rect;
    BLI_rcti_init(
        &rect, 0, 15 * UI_DPI_FAC, 15 * UI_DPI_FAC, region->winy - UI_TIME_SCRUB_MARGIN_Y);
    UI_view2d_draw_scale_y__block(region, v2d, &rect, TH_SCROLL_TEXT);
  }
}

void draw_timeline_seq_display(const bContext *C, ARegion *region)
{
  const Scene *scene = CTX_data_scene(C);
  const SpaceSeq *sseq = CTX_wm_space_seq(C);
  View2D *v2d = &region->v2d;

  ED_time_scrub_draw_current_frame(region, scene, !(sseq->flag & SEQ_DRAWFRAMES), true);
  UI_view2d_scrollers_draw(v2d, NULL);
}
