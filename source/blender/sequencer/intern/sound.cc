/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_colortools.hh"
#include "BKE_sound.hh"

#ifdef WITH_CONVOLUTION
#  include "AUD_Sound.h"
#endif

#include "SEQ_sequencer.hh"
#include "SEQ_sound.hh"
#include "SEQ_time.hh"

#include "strip_time.hh"

namespace blender::seq {

/* Unlike _update_sound_ functions,
 * these ones take info from audaspace to update sequence length! */
const SoundModifierWorkerInfo workersSoundModifiers[] = {
    {eSeqModifierType_SoundEqualizer, sound_equalizermodifier_recreator}, {0, nullptr}};

#ifdef WITH_CONVOLUTION
static bool sequencer_refresh_sound_length_recursive(Main *bmain, Scene *scene, ListBase *seqbase)
{
  bool changed = false;

  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if (strip->type == STRIP_TYPE_META) {
      if (sequencer_refresh_sound_length_recursive(bmain, scene, &strip->seqbase)) {
        changed = true;
      }
    }
    else if (strip->type == STRIP_TYPE_SOUND_RAM && strip->sound) {
      SoundInfo info;
      if (!BKE_sound_info_get(bmain, strip->sound, &info)) {
        continue;
      }

      int old = strip->len;
      float fac;

      strip->len = std::max(
          1, int(round((info.length - strip->sound->offset_time) * scene->frames_per_second())));
      fac = float(strip->len) / float(old);
      old = strip->startofs;
      strip->startofs *= fac;
      strip->endofs *= fac;
      strip->start += (old -
                       strip->startofs); /* So that visual/"real" start frame does not change! */

      changed = true;
    }
  }
  return changed;
}
#endif

void sound_update_length(Main *bmain, Scene *scene)
{
#ifdef WITH_CONVOLUTION
  if (scene->ed) {
    sequencer_refresh_sound_length_recursive(bmain, scene, &scene->ed->seqbase);
  }
#else
  UNUSED_VARS(bmain, scene);
#endif
}

void sound_update_bounds_all(Scene *scene)
{
  Editing *ed = scene->ed;

  if (ed) {
    LISTBASE_FOREACH (Strip *, strip, &ed->seqbase) {
      if (strip->type == STRIP_TYPE_META) {
        strip_update_sound_bounds_recursive(scene, strip);
      }
      else if (ELEM(strip->type, STRIP_TYPE_SOUND_RAM, STRIP_TYPE_SCENE)) {
        sound_update_bounds(scene, strip);
      }
    }
  }
}

void sound_update_bounds(Scene *scene, Strip *strip)
{
  if (strip->type == STRIP_TYPE_SCENE) {
    if (strip->scene && strip->scene_sound) {
      /* We have to take into account start frame of the sequence's scene! */
      int startofs = strip->startofs + strip->anim_startofs + strip->scene->r.sfra;

      BKE_sound_move_scene_sound(scene,
                                 strip->scene_sound,
                                 time_left_handle_frame_get(scene, strip),
                                 time_right_handle_frame_get(scene, strip),
                                 startofs,
                                 0.0);
    }
  }
  else {
    BKE_sound_move_scene_sound_defaults(scene, strip);
  }
  /* mute is set in strip_update_muting_recursive */
}

static void strip_update_sound_recursive(Scene *scene, ListBase *seqbasep, bSound *sound)
{
  LISTBASE_FOREACH (Strip *, strip, seqbasep) {
    if (strip->type == STRIP_TYPE_META) {
      strip_update_sound_recursive(scene, &strip->seqbase, sound);
    }
    else if (strip->type == STRIP_TYPE_SOUND_RAM) {
      if (strip->scene_sound && sound == strip->sound) {
        BKE_sound_update_scene_sound(strip->scene_sound, sound);
      }
    }
  }
}

void sound_update(Scene *scene, bSound *sound)
{
  if (scene->ed) {
    strip_update_sound_recursive(scene, &scene->ed->seqbase, sound);
  }
}

float sound_pitch_get(const Scene *scene, const Strip *strip)
{
  const Strip *meta_parent = lookup_meta_by_strip(scene->ed, strip);
  if (meta_parent != nullptr) {
    return strip->speed_factor * sound_pitch_get(scene, meta_parent);
  }
  return strip->speed_factor;
}

EQCurveMappingData *sound_equalizer_add(SoundEqualizerModifierData *semd, float minX, float maxX)
{
  EQCurveMappingData *eqcmd;

  if (maxX < 0) {
    maxX = SOUND_EQUALIZER_DEFAULT_MAX_FREQ;
  }
  if (minX < 0) {
    minX = 0.0;
  }
  /* It's the same as #BKE_curvemapping_add, but changing the name. */
  eqcmd = MEM_callocN<EQCurveMappingData>("Equalizer");
  BKE_curvemapping_set_defaults(&eqcmd->curve_mapping,
                                1, /* Total. */
                                minX,
                                -SOUND_EQUALIZER_DEFAULT_MAX_DB, /* Min x, y */
                                maxX,
                                SOUND_EQUALIZER_DEFAULT_MAX_DB, /* Max x, y */
                                HD_AUTO_ANIM);

  eqcmd->curve_mapping.preset = CURVE_PRESET_CONSTANT_MEDIAN;

  rctf clipr;
  clipr.xmin = minX;
  clipr.xmax = maxX;
  clipr.ymin = 0.0;
  clipr.ymax = 0.0;

  BKE_curvemap_reset(&eqcmd->curve_mapping.cm[0],
                     &clipr,
                     CURVE_PRESET_CONSTANT_MEDIAN,
                     CurveMapSlopeType::Negative);

  BLI_addtail(&semd->graphics, eqcmd);

  return eqcmd;
}

void sound_equalizermodifier_set_graphs(SoundEqualizerModifierData *semd, int number)
{
  sound_equalizermodifier_free((StripModifierData *)semd);
  if (number == 1) {
    sound_equalizer_add(semd, SOUND_EQUALIZER_DEFAULT_MIN_FREQ, SOUND_EQUALIZER_DEFAULT_MAX_FREQ);
  }
  else if (number == 2) {
    sound_equalizer_add(semd, 30.0, 2000.0);
    sound_equalizer_add(semd, 2000.1, 20000.0);
  }
  else if (number == 3) {
    sound_equalizer_add(semd, 30.0, 1000.0);
    sound_equalizer_add(semd, 1000.1, 5000.0);
    sound_equalizer_add(semd, 5000.1, 20000.0);
  }
}

EQCurveMappingData *sound_equalizermodifier_add_graph(SoundEqualizerModifierData *semd,
                                                      float min_freq,
                                                      float max_freq)
{
  if (min_freq < 0.0) {
    return nullptr;
  }
  if (max_freq < 0.0) {
    return nullptr;
  }
  if (max_freq <= min_freq) {
    return nullptr;
  }
  return sound_equalizer_add(semd, min_freq, max_freq);
}

void sound_equalizermodifier_remove_graph(SoundEqualizerModifierData *semd,
                                          EQCurveMappingData *eqcmd)
{
  BLI_remlink_safe(&semd->graphics, eqcmd);
  MEM_freeN(eqcmd);
}

void sound_equalizermodifier_init_data(StripModifierData *smd)
{
  SoundEqualizerModifierData *semd = (SoundEqualizerModifierData *)smd;

  sound_equalizer_add(semd, SOUND_EQUALIZER_DEFAULT_MIN_FREQ, SOUND_EQUALIZER_DEFAULT_MAX_FREQ);
}

void sound_equalizermodifier_free(StripModifierData *smd)
{
  SoundEqualizerModifierData *semd = (SoundEqualizerModifierData *)smd;
  LISTBASE_FOREACH_MUTABLE (EQCurveMappingData *, eqcmd, &semd->graphics) {
    BKE_curvemapping_free_data(&eqcmd->curve_mapping);
    MEM_freeN(eqcmd);
  }
  BLI_listbase_clear(&semd->graphics);
  if (smd->runtime.last_buf) {
    MEM_freeN(smd->runtime.last_buf);
  }
}

void sound_equalizermodifier_copy_data(StripModifierData *target, StripModifierData *smd)
{
  SoundEqualizerModifierData *semd = (SoundEqualizerModifierData *)smd;
  SoundEqualizerModifierData *semd_target = (SoundEqualizerModifierData *)target;
  EQCurveMappingData *eqcmd_n;

  BLI_listbase_clear(&semd_target->graphics);

  LISTBASE_FOREACH (EQCurveMappingData *, eqcmd, &semd->graphics) {
    eqcmd_n = static_cast<EQCurveMappingData *>(MEM_dupallocN(eqcmd));
    BKE_curvemapping_copy_data(&eqcmd_n->curve_mapping, &eqcmd->curve_mapping);

    eqcmd_n->next = eqcmd_n->prev = nullptr;
    BLI_addtail(&semd_target->graphics, eqcmd_n);
  }
}

void *sound_equalizermodifier_recreator(Strip *strip,
                                        StripModifierData *smd,
                                        void *sound_in,
                                        bool &needs_update)
{
#ifdef WITH_CONVOLUTION
  UNUSED_VARS(strip);

  SoundEqualizerModifierData *semd = (SoundEqualizerModifierData *)smd;

  /* No equalizer definition. */
  if (BLI_listbase_is_empty(&semd->graphics)) {
    return sound_in;
  }

  float *buf = MEM_calloc_arrayN<float>(SOUND_EQUALIZER_SIZE_DEFINITION, "eqrecreator");

  CurveMapping *eq_mapping;
  CurveMap *cm;
  float minX;
  float maxX;
  float interval = SOUND_EQUALIZER_DEFAULT_MAX_FREQ / float(SOUND_EQUALIZER_SIZE_DEFINITION);

  /* Visit all equalizer definitions. */
  LISTBASE_FOREACH (EQCurveMappingData *, mapping, &semd->graphics) {
    eq_mapping = &mapping->curve_mapping;
    BKE_curvemapping_init(eq_mapping);
    cm = eq_mapping->cm;
    minX = eq_mapping->curr.xmin;
    maxX = eq_mapping->curr.xmax;
    int idx = int(ceil(minX / interval));
    int i = idx;
    for (; i * interval <= maxX && i < SOUND_EQUALIZER_SIZE_DEFINITION; i++) {
      float freq = i * interval;
      float val = BKE_curvemap_evaluateF(eq_mapping, cm, freq);
      if (fabs(val) > SOUND_EQUALIZER_DEFAULT_MAX_DB) {
        val = (val / fabs(val)) * SOUND_EQUALIZER_DEFAULT_MAX_DB;
      }
      buf[i] = val;
      /* To soften lower limit, but not the first position which is the constant value */
      if (i == idx && i > 2) {
        buf[i - 1] = 0.5 * (buf[i] + buf[i - 1]);
      }
    }
    /* To soften higher limit */
    if (i < SOUND_EQUALIZER_SIZE_DEFINITION) {
      buf[i] = 0.5 * (buf[i] + buf[i - 1]);
    }
  }

  /* Only make new sound when necessary. It is faster and it prevents audio glitches. */
  if (!needs_update && smd->runtime.last_sound_in == sound_in &&
      smd->runtime.last_buf != nullptr &&
      std::memcmp(buf, smd->runtime.last_buf, SOUND_EQUALIZER_SIZE_DEFINITION) == 0)
  {
    MEM_freeN(buf);
    return smd->runtime.last_sound_out;
  }

  AUD_Sound *sound_out = AUD_Sound_equalize(sound_in,
                                            buf,
                                            SOUND_EQUALIZER_SIZE_DEFINITION,
                                            SOUND_EQUALIZER_DEFAULT_MAX_FREQ,
                                            SOUND_EQUALIZER_SIZE_CONVERSION);

  needs_update = true;
  smd->runtime.last_buf = buf;
  smd->runtime.last_sound_in = sound_in;
  smd->runtime.last_sound_out = sound_out;

  return sound_out;
#else
  UNUSED_VARS(strip, smd, sound_in, needs_update);
  return nullptr;
#endif
}

const SoundModifierWorkerInfo *sound_modifier_worker_info_get(int type)
{
  for (int i = 0; workersSoundModifiers[i].type > 0; i++) {
    if (workersSoundModifiers[i].type == type) {
      return &workersSoundModifiers[i];
    }
  }
  return nullptr;
}

void *sound_modifier_recreator(Strip *strip,
                               StripModifierData *smd,
                               void *sound,
                               bool &needs_update)
{

  if (!(smd->flag & STRIP_MODIFIER_FLAG_MUTE)) {
    const SoundModifierWorkerInfo *smwi = sound_modifier_worker_info_get(smd->type);
    return smwi->recreator(strip, smd, sound, needs_update);
  }
  return sound;
}

}  // namespace blender::seq
