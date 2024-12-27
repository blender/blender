/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2024 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_metadata.hh"

#include "RE_pipeline.h"

#include "SEQ_render.hh"
#include "SEQ_time.hh"

#include "effects.hh"
#include "render.hh"

using namespace blender;

void slice_get_byte_buffers(const SeqRenderData *context,
                            const ImBuf *ibuf1,
                            const ImBuf *ibuf2,
                            const ImBuf *out,
                            int start_line,
                            uchar **rect1,
                            uchar **rect2,
                            uchar **rect_out)
{
  int offset = 4 * start_line * context->rectx;

  *rect1 = ibuf1->byte_buffer.data + offset;
  *rect_out = out->byte_buffer.data + offset;

  if (ibuf2) {
    *rect2 = ibuf2->byte_buffer.data + offset;
  }
}

void slice_get_float_buffers(const SeqRenderData *context,
                             const ImBuf *ibuf1,
                             const ImBuf *ibuf2,
                             const ImBuf *out,
                             int start_line,
                             float **rect1,
                             float **rect2,
                             float **rect_out)
{
  int offset = 4 * start_line * context->rectx;

  *rect1 = ibuf1->float_buffer.data + offset;
  *rect_out = out->float_buffer.data + offset;

  if (ibuf2) {
    *rect2 = ibuf2->float_buffer.data + offset;
  }
}

ImBuf *prepare_effect_imbufs(const SeqRenderData *context,
                             ImBuf *ibuf1,
                             ImBuf *ibuf2,
                             bool uninitialized_pixels)
{
  ImBuf *out;
  Scene *scene = context->scene;
  int x = context->rectx;
  int y = context->recty;
  int base_flags = uninitialized_pixels ? IB_uninitialized_pixels : 0;

  if (!ibuf1 && !ibuf2) {
    /* hmmm, global float option ? */
    out = IMB_allocImBuf(x, y, 32, IB_rect | base_flags);
  }
  else if ((ibuf1 && ibuf1->float_buffer.data) || (ibuf2 && ibuf2->float_buffer.data)) {
    /* if any inputs are float, output is float too */
    out = IMB_allocImBuf(x, y, 32, IB_rectfloat | base_flags);
  }
  else {
    out = IMB_allocImBuf(x, y, 32, IB_rect | base_flags);
  }

  if (out->float_buffer.data) {
    if (ibuf1 && !ibuf1->float_buffer.data) {
      seq_imbuf_to_sequencer_space(scene, ibuf1, true);
    }

    if (ibuf2 && !ibuf2->float_buffer.data) {
      seq_imbuf_to_sequencer_space(scene, ibuf2, true);
    }

    IMB_colormanagement_assign_float_colorspace(out, scene->sequencer_colorspace_settings.name);
  }
  else {
    if (ibuf1 && !ibuf1->byte_buffer.data) {
      IMB_rect_from_float(ibuf1);
    }

    if (ibuf2 && !ibuf2->byte_buffer.data) {
      IMB_rect_from_float(ibuf2);
    }
  }

  /* If effect only affecting a single channel, forward input's metadata to the output. */
  if (ibuf1 != nullptr && ibuf1 == ibuf2) {
    IMB_metadata_copy(out, ibuf1);
  }

  return out;
}

Array<float> make_gaussian_blur_kernel(float rad, int size)
{
  int n = 2 * size + 1;
  Array<float> gaussian(n);

  float sum = 0.0f;
  float fac = (rad > 0.0f ? 1.0f / rad : 0.0f);
  for (int i = -size; i <= size; i++) {
    float val = RE_filter_value(R_FILTER_GAUSS, float(i) * fac);
    sum += val;
    gaussian[i + size] = val;
  }

  float inv_sum = 1.0f / sum;
  for (int i = 0; i < n; i++) {
    gaussian[i] *= inv_sum;
  }

  return gaussian;
}

static void init_noop(Sequence * /*seq*/) {}

static void load_noop(Sequence * /*seq*/) {}

static void free_noop(Sequence * /*seq*/, const bool /*do_id_user*/) {}

static int num_inputs_default()
{
  return 2;
}

static StripEarlyOut early_out_noop(const Sequence * /*seq*/, float /*fac*/)
{
  return StripEarlyOut::DoEffect;
}

StripEarlyOut early_out_fade(const Sequence * /*seq*/, float fac)
{
  if (fac == 0.0f) {
    return StripEarlyOut::UseInput1;
  }
  if (fac == 1.0f) {
    return StripEarlyOut::UseInput2;
  }
  return StripEarlyOut::DoEffect;
}

StripEarlyOut early_out_mul_input2(const Sequence * /*seq*/, float fac)
{
  if (fac == 0.0f) {
    return StripEarlyOut::UseInput1;
  }
  return StripEarlyOut::DoEffect;
}

StripEarlyOut early_out_mul_input1(const Sequence * /*seq*/, float fac)
{
  if (fac == 0.0f) {
    return StripEarlyOut::UseInput2;
  }
  return StripEarlyOut::DoEffect;
}

static void get_default_fac_noop(const Scene * /*scene*/,
                                 const Sequence * /*seq*/,
                                 float /*timeline_frame*/,
                                 float *fac)
{
  *fac = 1.0f;
}

void get_default_fac_fade(const Scene *scene,
                          const Sequence *seq,
                          float timeline_frame,
                          float *fac)
{
  *fac = float(timeline_frame - SEQ_time_left_handle_frame_get(scene, seq));
  *fac /= SEQ_time_strip_length_get(scene, seq);
  *fac = math::clamp(*fac, 0.0f, 1.0f);
}

static ImBuf *init_execution(const SeqRenderData *context, ImBuf *ibuf1, ImBuf *ibuf2)
{
  ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2);
  return out;
}

SeqEffectHandle get_sequence_effect_impl(int seq_type)
{
  SeqEffectHandle rval;
  int sequence_type = seq_type;

  rval.multithreaded = false;
  rval.init = init_noop;
  rval.num_inputs = num_inputs_default;
  rval.load = load_noop;
  rval.free = free_noop;
  rval.early_out = early_out_noop;
  rval.get_default_fac = get_default_fac_noop;
  rval.execute = nullptr;
  rval.init_execution = init_execution;
  rval.execute_slice = nullptr;
  rval.copy = nullptr;

  switch (sequence_type) {
    case SEQ_TYPE_CROSS:
      cross_effect_get_handle(rval);
      break;
    case SEQ_TYPE_GAMCROSS:
      gamma_cross_effect_get_handle(rval);
      break;
    case SEQ_TYPE_ADD:
      add_effect_get_handle(rval);
      break;
    case SEQ_TYPE_SUB:
      sub_effect_get_handle(rval);
      break;
    case SEQ_TYPE_MUL:
      mul_effect_get_handle(rval);
      break;
    case SEQ_TYPE_SCREEN:
    case SEQ_TYPE_OVERLAY:
    case SEQ_TYPE_COLOR_BURN:
    case SEQ_TYPE_LINEAR_BURN:
    case SEQ_TYPE_DARKEN:
    case SEQ_TYPE_LIGHTEN:
    case SEQ_TYPE_DODGE:
    case SEQ_TYPE_SOFT_LIGHT:
    case SEQ_TYPE_HARD_LIGHT:
    case SEQ_TYPE_PIN_LIGHT:
    case SEQ_TYPE_LIN_LIGHT:
    case SEQ_TYPE_VIVID_LIGHT:
    case SEQ_TYPE_BLEND_COLOR:
    case SEQ_TYPE_HUE:
    case SEQ_TYPE_SATURATION:
    case SEQ_TYPE_VALUE:
    case SEQ_TYPE_DIFFERENCE:
    case SEQ_TYPE_EXCLUSION:
      blend_mode_effect_get_handle(rval);
      break;
    case SEQ_TYPE_COLORMIX:
      color_mix_effect_get_handle(rval);
      break;
    case SEQ_TYPE_ALPHAOVER:
      alpha_over_effect_get_handle(rval);
      break;
    case SEQ_TYPE_OVERDROP:
      over_drop_effect_get_handle(rval);
      break;
    case SEQ_TYPE_ALPHAUNDER:
      alpha_under_effect_get_handle(rval);
      break;
    case SEQ_TYPE_WIPE:
      wipe_effect_get_handle(rval);
      break;
    case SEQ_TYPE_GLOW:
      glow_effect_get_handle(rval);
      break;
    case SEQ_TYPE_TRANSFORM:
      transform_effect_get_handle(rval);
      break;
    case SEQ_TYPE_SPEED:
      speed_effect_get_handle(rval);
      break;
    case SEQ_TYPE_COLOR:
      solid_color_effect_get_handle(rval);
      break;
    case SEQ_TYPE_MULTICAM:
      multi_camera_effect_get_handle(rval);
      break;
    case SEQ_TYPE_ADJUSTMENT:
      adjustment_effect_get_handle(rval);
      break;
    case SEQ_TYPE_GAUSSIAN_BLUR:
      gaussian_blur_effect_get_handle(rval);
      break;
    case SEQ_TYPE_TEXT:
      text_effect_get_handle(rval);
      break;
  }

  return rval;
}

SeqEffectHandle SEQ_effect_handle_get(Sequence *seq)
{
  SeqEffectHandle rval = {};

  if (seq->type & SEQ_TYPE_EFFECT) {
    rval = get_sequence_effect_impl(seq->type);
    if ((seq->flag & SEQ_EFFECT_NOT_LOADED) != 0) {
      rval.load(seq);
      seq->flag &= ~SEQ_EFFECT_NOT_LOADED;
    }
  }

  return rval;
}

SeqEffectHandle seq_effect_get_sequence_blend(Sequence *seq)
{
  SeqEffectHandle rval = {};

  if (seq->blend_mode != 0) {
    if ((seq->flag & SEQ_EFFECT_NOT_LOADED) != 0) {
      /* load the effect first */
      rval = get_sequence_effect_impl(seq->type);
      rval.load(seq);
    }

    rval = get_sequence_effect_impl(seq->blend_mode);
    if ((seq->flag & SEQ_EFFECT_NOT_LOADED) != 0) {
      /* now load the blend and unset unloaded flag */
      rval.load(seq);
      seq->flag &= ~SEQ_EFFECT_NOT_LOADED;
    }
  }

  return rval;
}

int SEQ_effect_get_num_inputs(int seq_type)
{
  SeqEffectHandle rval = get_sequence_effect_impl(seq_type);

  int count = rval.num_inputs();
  if (rval.execute || (rval.execute_slice && rval.init_execution)) {
    return count;
  }
  return 0;
}
