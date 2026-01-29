/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BKE_fcurve.hh"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "IMB_imbuf.hh"

#include "RNA_prototypes.hh"

#include "SEQ_render.hh"
#include "SEQ_time.hh"

#include "effects.hh"
#include "render.hh"

namespace blender::seq {

static void init_speed_effect(Strip *strip)
{
  SpeedControlVars *data = MEM_new<SpeedControlVars>("speedcontrolvars");
  strip->effectdata = data;
  data->speed_control_type = SEQ_SPEED_STRETCH;
  data->speed_fader = 1.0f;
  data->speed_fader_length = 0.0f;
  data->speed_fader_frame_number = 0.0f;
}

static void free_speed_effect(Strip *strip, const bool /*do_id_user*/)
{
  if (strip->effectdata) {
    SpeedControlVars *v = static_cast<SpeedControlVars *>(strip->effectdata);
    if (v->frameMap) {
      MEM_delete(v->frameMap);
    }
    MEM_delete(v);
    strip->effectdata = nullptr;
  }
}

static int num_inputs_speed()
{
  return 1;
}

static void copy_speed_effect(Strip *dst, const Strip *src, const int /*flag*/)
{
  SpeedControlVars *v = MEM_dupalloc(static_cast<SpeedControlVars *>(src->effectdata));
  v->frameMap = nullptr;
  dst->effectdata = v;
}

static StripEarlyOut early_out_speed(const Strip * /*strip*/, float /*fac*/)
{
  return StripEarlyOut::DoEffect;
}

static FCurve *strip_effect_speed_speed_factor_curve_get(Scene *scene, Strip *strip)
{
  return id_data_find_fcurve(&scene->id, strip, RNA_Strip, "speed_factor", 0, nullptr);
}

void strip_effect_speed_rebuild_map(Scene *scene, Strip *strip)
{
  const int effect_strip_length = strip->right_handle(scene) - strip->left_handle();

  if ((strip->input1 == nullptr) || (effect_strip_length < 1)) {
    return; /* Make COVERITY happy and check for (CID 598) input strip. */
  }

  const FCurve *fcu = strip_effect_speed_speed_factor_curve_get(scene, strip);
  if (fcu == nullptr) {
    return;
  }

  SpeedControlVars *v = static_cast<SpeedControlVars *>(strip->effectdata);
  if (v->frameMap) {
    MEM_delete(v->frameMap);
  }

  v->frameMap = MEM_new_array_uninitialized<float>(size_t(effect_strip_length), __func__);
  v->frameMap[0] = 0.0f;

  float target_frame = 0;
  for (int frame_index = 1; frame_index < effect_strip_length; frame_index++) {
    target_frame += evaluate_fcurve(fcu, strip->left_handle() + frame_index);
    const int target_frame_max = strip->input1->length(scene);
    CLAMP(target_frame, 0, target_frame_max);
    v->frameMap[frame_index] = target_frame;
  }
}

static void strip_effect_speed_frame_map_ensure(Scene *scene, Strip *strip)
{
  const SpeedControlVars *v = static_cast<SpeedControlVars *>(strip->effectdata);
  if (v->frameMap != nullptr) {
    return;
  }

  strip_effect_speed_rebuild_map(scene, strip);
}

float strip_speed_effect_target_frame_get(Scene *scene,
                                          Strip *strip_speed,
                                          float timeline_frame,
                                          int input)
{
  if (strip_speed->input1 == nullptr) {
    return 0.0f;
  }

  strip_effect_handle_get(strip_speed); /* Ensure, that data are initialized. */
  int frame_index = round_fl_to_int(give_frame_index(scene, strip_speed, timeline_frame));
  SpeedControlVars *s = static_cast<SpeedControlVars *>(strip_speed->effectdata);
  const Strip *source = strip_speed->input1;

  float target_frame = 0.0f;
  switch (s->speed_control_type) {
    case SEQ_SPEED_STRETCH: {
      /* Only right handle controls effect speed! */
      const float target_content_length = source->length(scene) - source->startofs;
      const float speed_effetct_length = strip_speed->right_handle(scene) -
                                         strip_speed->left_handle();
      const float ratio = frame_index / speed_effetct_length;
      target_frame = target_content_length * ratio;
      break;
    }
    case SEQ_SPEED_MULTIPLY: {
      const FCurve *fcu = strip_effect_speed_speed_factor_curve_get(scene, strip_speed);
      if (fcu != nullptr) {
        strip_effect_speed_frame_map_ensure(scene, strip_speed);
        target_frame = s->frameMap[frame_index];
      }
      else {
        target_frame = frame_index * s->speed_fader;
        if (s->speed_fader < 0) {
          /* Treat `target_frame` as a negative offset from the last frame of the strip. */
          target_frame += source->length(scene);
        }
      }
      break;
    }
    case SEQ_SPEED_LENGTH:
      target_frame = source->length(scene) * (s->speed_fader_length / 100.0f);
      break;
    case SEQ_SPEED_FRAME_NUMBER:
      target_frame = s->speed_fader_frame_number;
      break;
  }

  CLAMP(target_frame, 0, source->length(scene));
  target_frame += strip_speed->start;

  /* No interpolation. */
  if ((s->flags & SEQ_SPEED_USE_INTERPOLATION) == 0) {
    return target_frame;
  }

  /* Interpolation is used, switch between current and next frame based on which input is
   * requested. */
  return input == 0 ? target_frame : ceil(target_frame);
}

static float speed_effect_interpolation_ratio_get(Scene *scene,
                                                  Strip *strip_speed,
                                                  float timeline_frame)
{
  const float target_frame = strip_speed_effect_target_frame_get(
      scene, strip_speed, timeline_frame, 0);
  return target_frame - floor(target_frame);
}

static ImBuf *do_speed_effect(const RenderData *context,
                              SeqRenderState *state,
                              Strip *strip,
                              float timeline_frame,
                              float fac,
                              ImBuf *ibuf1,
                              ImBuf *ibuf2)
{
  const SpeedControlVars *s = static_cast<SpeedControlVars *>(strip->effectdata);
  EffectHandle cross_effect = effect_handle_get(STRIP_TYPE_CROSS);

  if (s->flags & SEQ_SPEED_USE_INTERPOLATION) {
    fac = speed_effect_interpolation_ratio_get(context->scene, strip, timeline_frame);
    /* Current frame is ibuf1, next frame is ibuf2. */
    ImBuf *out = cross_effect.execute(context, state, nullptr, timeline_frame, fac, ibuf1, ibuf2);
    return out;
  }

  /* No interpolation. */
  return IMB_dupImBuf(ibuf1);
}

void speed_effect_get_handle(EffectHandle &rval)
{
  rval.init = init_speed_effect;
  rval.num_inputs = num_inputs_speed;
  rval.free = free_speed_effect;
  rval.copy = copy_speed_effect;
  rval.execute = do_speed_effect;
  rval.early_out = early_out_speed;
}

}  // namespace blender::seq
