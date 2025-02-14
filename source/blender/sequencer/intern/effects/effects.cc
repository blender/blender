/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2024 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

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
    /* Hmm, global float option? */
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

static void init_noop(Strip * /*strip*/) {}

static void load_noop(Strip * /*strip*/) {}

static void free_noop(Strip * /*strip*/, const bool /*do_id_user*/) {}

static int num_inputs_default()
{
  return 2;
}

static StripEarlyOut early_out_noop(const Strip * /*strip*/, float /*fac*/)
{
  return StripEarlyOut::DoEffect;
}

StripEarlyOut early_out_fade(const Strip * /*strip*/, float fac)
{
  if (fac == 0.0f) {
    return StripEarlyOut::UseInput1;
  }
  if (fac == 1.0f) {
    return StripEarlyOut::UseInput2;
  }
  return StripEarlyOut::DoEffect;
}

StripEarlyOut early_out_mul_input2(const Strip * /*strip*/, float fac)
{
  if (fac == 0.0f) {
    return StripEarlyOut::UseInput1;
  }
  return StripEarlyOut::DoEffect;
}

StripEarlyOut early_out_mul_input1(const Strip * /*strip*/, float fac)
{
  if (fac == 0.0f) {
    return StripEarlyOut::UseInput2;
  }
  return StripEarlyOut::DoEffect;
}

static void get_default_fac_noop(const Scene * /*scene*/,
                                 const Strip * /*strip*/,
                                 float /*timeline_frame*/,
                                 float *fac)
{
  *fac = 1.0f;
}

void get_default_fac_fade(const Scene *scene, const Strip *strip, float timeline_frame, float *fac)
{
  *fac = float(timeline_frame - SEQ_time_left_handle_frame_get(scene, strip));
  *fac /= SEQ_time_strip_length_get(scene, strip);
  *fac = math::clamp(*fac, 0.0f, 1.0f);
}

SeqEffectHandle get_sequence_effect_impl(int strip_type)
{
  SeqEffectHandle rval;

  rval.init = init_noop;
  rval.num_inputs = num_inputs_default;
  rval.load = load_noop;
  rval.free = free_noop;
  rval.early_out = early_out_noop;
  rval.get_default_fac = get_default_fac_noop;
  rval.execute = nullptr;
  rval.copy = nullptr;

  switch (strip_type) {
    case STRIP_TYPE_CROSS:
      cross_effect_get_handle(rval);
      break;
    case STRIP_TYPE_GAMCROSS:
      gamma_cross_effect_get_handle(rval);
      break;
    case STRIP_TYPE_ADD:
      add_effect_get_handle(rval);
      break;
    case STRIP_TYPE_SUB:
      sub_effect_get_handle(rval);
      break;
    case STRIP_TYPE_MUL:
      mul_effect_get_handle(rval);
      break;
    case STRIP_TYPE_SCREEN:
    case STRIP_TYPE_OVERLAY:
    case STRIP_TYPE_COLOR_BURN:
    case STRIP_TYPE_LINEAR_BURN:
    case STRIP_TYPE_DARKEN:
    case STRIP_TYPE_LIGHTEN:
    case STRIP_TYPE_DODGE:
    case STRIP_TYPE_SOFT_LIGHT:
    case STRIP_TYPE_HARD_LIGHT:
    case STRIP_TYPE_PIN_LIGHT:
    case STRIP_TYPE_LIN_LIGHT:
    case STRIP_TYPE_VIVID_LIGHT:
    case STRIP_TYPE_BLEND_COLOR:
    case STRIP_TYPE_HUE:
    case STRIP_TYPE_SATURATION:
    case STRIP_TYPE_VALUE:
    case STRIP_TYPE_DIFFERENCE:
    case STRIP_TYPE_EXCLUSION:
      blend_mode_effect_get_handle(rval);
      break;
    case STRIP_TYPE_COLORMIX:
      color_mix_effect_get_handle(rval);
      break;
    case STRIP_TYPE_ALPHAOVER:
      alpha_over_effect_get_handle(rval);
      break;
    case STRIP_TYPE_OVERDROP:
      over_drop_effect_get_handle(rval);
      break;
    case STRIP_TYPE_ALPHAUNDER:
      alpha_under_effect_get_handle(rval);
      break;
    case STRIP_TYPE_WIPE:
      wipe_effect_get_handle(rval);
      break;
    case STRIP_TYPE_GLOW:
      glow_effect_get_handle(rval);
      break;
    case STRIP_TYPE_TRANSFORM:
      transform_effect_get_handle(rval);
      break;
    case STRIP_TYPE_SPEED:
      speed_effect_get_handle(rval);
      break;
    case STRIP_TYPE_COLOR:
      solid_color_effect_get_handle(rval);
      break;
    case STRIP_TYPE_MULTICAM:
      multi_camera_effect_get_handle(rval);
      break;
    case STRIP_TYPE_ADJUSTMENT:
      adjustment_effect_get_handle(rval);
      break;
    case STRIP_TYPE_GAUSSIAN_BLUR:
      gaussian_blur_effect_get_handle(rval);
      break;
    case STRIP_TYPE_TEXT:
      text_effect_get_handle(rval);
      break;
  }

  return rval;
}

SeqEffectHandle SEQ_effect_handle_get(Strip *strip)
{
  SeqEffectHandle rval = {};

  if (strip->type & STRIP_TYPE_EFFECT) {
    rval = get_sequence_effect_impl(strip->type);
    if ((strip->flag & SEQ_EFFECT_NOT_LOADED) != 0) {
      rval.load(strip);
      strip->flag &= ~SEQ_EFFECT_NOT_LOADED;
    }
  }

  return rval;
}

SeqEffectHandle strip_effect_get_sequence_blend(Strip *strip)
{
  SeqEffectHandle rval = {};

  if (strip->blend_mode != 0) {
    if ((strip->flag & SEQ_EFFECT_NOT_LOADED) != 0) {
      /* load the effect first */
      rval = get_sequence_effect_impl(strip->type);
      rval.load(strip);
    }

    rval = get_sequence_effect_impl(strip->blend_mode);
    if ((strip->flag & SEQ_EFFECT_NOT_LOADED) != 0) {
      /* now load the blend and unset unloaded flag */
      rval.load(strip);
      strip->flag &= ~SEQ_EFFECT_NOT_LOADED;
    }
  }

  return rval;
}

int SEQ_effect_get_num_inputs(int strip_type)
{
  SeqEffectHandle rval = get_sequence_effect_impl(strip_type);

  int count = rval.num_inputs();
  if (rval.execute) {
    return count;
  }
  return 0;
}
