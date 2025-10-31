/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2024 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BKE_fcurve.hh"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_metadata.hh"

#include "RE_pipeline.h"

#include "RNA_prototypes.hh"

#include "SEQ_render.hh"
#include "SEQ_time.hh"

#include "effects.hh"
#include "render.hh"

namespace blender::seq {

ImBuf *prepare_effect_imbufs(const RenderData *context,
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
    out = IMB_allocImBuf(x, y, 32, IB_byte_data | base_flags);
  }
  else if ((ibuf1 && ibuf1->float_buffer.data) || (ibuf2 && ibuf2->float_buffer.data)) {
    /* if any inputs are float, output is float too */
    out = IMB_allocImBuf(x, y, 32, IB_float_data | base_flags);
  }
  else {
    out = IMB_allocImBuf(x, y, 32, IB_byte_data | base_flags);
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
      IMB_byte_from_float(ibuf1);
    }

    if (ibuf2 && !ibuf2->byte_buffer.data) {
      IMB_byte_from_float(ibuf2);
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

static void free_default(Strip *strip, const bool /*do_id_user*/)
{
  MEM_SAFE_FREE(strip->effectdata);
}

static int num_inputs_default()
{
  return 2;
}

static void copy_effect_default(Strip *dst, const Strip *src, const int /*flag*/)
{
  dst->effectdata = MEM_dupallocN(src->effectdata);
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

void effect_ensure_initialized(Strip *strip)
{
  if (strip->effectdata == nullptr) {
    EffectHandle h = strip_effect_handle_get(strip);
    if (h.init != nullptr) {
      h.init(strip);
    }
  }
}

void effect_free(Strip *strip)
{
  EffectHandle h = strip_effect_handle_get(strip);
  if (h.free != nullptr) {
    h.free(strip, true);
    BLI_assert(strip->effectdata == nullptr);
  }
}

EffectHandle effect_handle_get(StripType strip_type)
{
  EffectHandle rval;

  rval.init = init_noop;
  rval.num_inputs = num_inputs_default;
  rval.load = load_noop;
  rval.free = free_default;
  rval.early_out = early_out_noop;
  rval.execute = nullptr;
  rval.copy = copy_effect_default;

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
    case STRIP_TYPE_COLORMIX:
      color_mix_effect_get_handle(rval);
      break;
    case STRIP_TYPE_ALPHAOVER:
      alpha_over_effect_get_handle(rval);
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
    default:
      break;
  }

  return rval;
}

static EffectHandle effect_handle_for_blend_mode_get(StripBlendMode blend)
{
  EffectHandle rval;

  rval.init = init_noop;
  rval.num_inputs = num_inputs_default;
  rval.load = load_noop;
  rval.free = free_default;
  rval.early_out = early_out_noop;
  rval.execute = nullptr;
  rval.copy = nullptr;

  switch (blend) {
    case STRIP_BLEND_CROSS:
      cross_effect_get_handle(rval);
      break;
    case STRIP_BLEND_ADD:
      add_effect_get_handle(rval);
      break;
    case STRIP_BLEND_SUB:
      sub_effect_get_handle(rval);
      break;
    case STRIP_BLEND_ALPHAOVER:
      alpha_over_effect_get_handle(rval);
      break;
    case STRIP_BLEND_ALPHAUNDER:
      alpha_under_effect_get_handle(rval);
      break;
    case STRIP_BLEND_GAMCROSS:
      gamma_cross_effect_get_handle(rval);
      break;
    case STRIP_BLEND_MUL:
      mul_effect_get_handle(rval);
      break;
    case STRIP_BLEND_SCREEN:
    case STRIP_BLEND_LIGHTEN:
    case STRIP_BLEND_DODGE:
    case STRIP_BLEND_DARKEN:
    case STRIP_BLEND_COLOR_BURN:
    case STRIP_BLEND_LINEAR_BURN:
    case STRIP_BLEND_OVERLAY:
    case STRIP_BLEND_HARD_LIGHT:
    case STRIP_BLEND_SOFT_LIGHT:
    case STRIP_BLEND_PIN_LIGHT:
    case STRIP_BLEND_LIN_LIGHT:
    case STRIP_BLEND_VIVID_LIGHT:
    case STRIP_BLEND_HUE:
    case STRIP_BLEND_SATURATION:
    case STRIP_BLEND_VALUE:
    case STRIP_BLEND_BLEND_COLOR:
    case STRIP_BLEND_DIFFERENCE:
    case STRIP_BLEND_EXCLUSION:
      blend_mode_effect_get_handle(rval);
      break;
    default:
      break;
  }

  return rval;
}

EffectHandle strip_effect_handle_get(Strip *strip)
{
  EffectHandle rval = {};

  if (strip->is_effect()) {
    rval = effect_handle_get(StripType(strip->type));
    if ((strip->runtime.flag & STRIP_EFFECT_NOT_LOADED) != 0) {
      rval.load(strip);
      strip->runtime.flag &= ~STRIP_EFFECT_NOT_LOADED;
    }
  }

  return rval;
}

EffectHandle strip_blend_mode_handle_get(Strip *strip)
{
  EffectHandle rval = {};

  if (strip->blend_mode != STRIP_BLEND_REPLACE) {
    if ((strip->runtime.flag & STRIP_EFFECT_NOT_LOADED) != 0) {
      /* load the effect first */
      rval = effect_handle_get(StripType(strip->type));
      rval.load(strip);
    }

    rval = effect_handle_for_blend_mode_get(StripBlendMode(strip->blend_mode));
    if ((strip->runtime.flag & STRIP_EFFECT_NOT_LOADED) != 0) {
      /* now load the blend and unset unloaded flag */
      rval.load(strip);
      strip->runtime.flag &= ~STRIP_EFFECT_NOT_LOADED;
    }
  }

  return rval;
}

static float transition_fader_calc(const Scene *scene, const Strip *strip, float timeline_frame)
{
  float fac = float(timeline_frame - time_left_handle_frame_get(scene, strip));
  fac /= time_strip_length_get(scene, strip);
  fac = math::clamp(fac, 0.0f, 1.0f);
  return fac;
}

float effect_fader_calc(Scene *scene, Strip *strip, float timeline_frame)
{
  if (strip->flag & SEQ_USE_EFFECT_DEFAULT_FADE) {
    if (effect_is_transition(StripType(strip->type))) {
      return transition_fader_calc(scene, strip, timeline_frame);
    }
    return 1.0f;
  }

  const FCurve *fcu = id_data_find_fcurve(
      &scene->id, strip, &RNA_Strip, "effect_fader", 0, nullptr);
  if (fcu) {
    return evaluate_fcurve(fcu, timeline_frame);
  }
  return strip->effect_fader;
}

int effect_get_num_inputs(int strip_type)
{
  EffectHandle rval = effect_handle_get(StripType(strip_type));
  if (rval.execute == nullptr) {
    return 0;
  }
  return rval.num_inputs();
}

bool effect_is_transition(StripType type)
{
  return ELEM(type, STRIP_TYPE_CROSS, STRIP_TYPE_GAMCROSS, STRIP_TYPE_WIPE);
}

}  // namespace blender::seq
