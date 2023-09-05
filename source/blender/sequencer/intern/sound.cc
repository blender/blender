/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLO_read_write.hh"

#include "BKE_colortools.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_sound.h"

#ifdef WITH_AUDASPACE
#  include "AUD_Sound.h"
#endif

#include "SEQ_sound.h"
#include "SEQ_time.h"

#include "sequencer.h"
#include "strip_time.h"

/* Unlike _update_sound_ functions,
 * these ones take info from audaspace to update sequence length! */
const SoundModifierWorkerInfo workersSoundModifiers[] = {
    {seqModifierType_SoundEqualizer, SEQ_sound_equalizermodifier_recreator}, {0, nullptr}};

#ifdef WITH_AUDASPACE
static bool sequencer_refresh_sound_length_recursive(Main *bmain, Scene *scene, ListBase *seqbase)
{
  bool changed = false;

  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (seq->type == SEQ_TYPE_META) {
      if (sequencer_refresh_sound_length_recursive(bmain, scene, &seq->seqbase)) {
        changed = true;
      }
    }
    else if (seq->type == SEQ_TYPE_SOUND_RAM && seq->sound) {
      SoundInfo info;
      if (!BKE_sound_info_get(bmain, seq->sound, &info)) {
        continue;
      }

      int old = seq->len;
      float fac;

      seq->len = MAX2(1, round((info.length - seq->sound->offset_time) * FPS));
      fac = float(seq->len) / float(old);
      old = seq->startofs;
      seq->startofs *= fac;
      seq->endofs *= fac;
      seq->start += (old - seq->startofs); /* So that visual/"real" start frame does not change! */

      changed = true;
    }
  }
  return changed;
}
#endif

void SEQ_sound_update_length(Main *bmain, Scene *scene)
{
#ifdef WITH_AUDASPACE
  if (scene->ed) {
    sequencer_refresh_sound_length_recursive(bmain, scene, &scene->ed->seqbase);
  }
#else
  UNUSED_VARS(bmain, scene);
#endif
}

void SEQ_sound_update_bounds_all(Scene *scene)
{
  Editing *ed = scene->ed;

  if (ed) {
    LISTBASE_FOREACH (Sequence *, seq, &ed->seqbase) {
      if (seq->type == SEQ_TYPE_META) {
        seq_update_sound_bounds_recursive(scene, seq);
      }
      else if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SCENE)) {
        SEQ_sound_update_bounds(scene, seq);
      }
    }
  }
}

void SEQ_sound_update_bounds(Scene *scene, Sequence *seq)
{
  if (seq->type == SEQ_TYPE_SCENE) {
    if (seq->scene && seq->scene_sound) {
      /* We have to take into account start frame of the sequence's scene! */
      int startofs = seq->startofs + seq->anim_startofs + seq->scene->r.sfra;

      BKE_sound_move_scene_sound(scene,
                                 seq->scene_sound,
                                 SEQ_time_left_handle_frame_get(scene, seq),
                                 SEQ_time_right_handle_frame_get(scene, seq),
                                 startofs,
                                 0.0);
    }
  }
  else {
    BKE_sound_move_scene_sound_defaults(scene, seq);
  }
  /* mute is set in seq_update_muting_recursive */
}

static void seq_update_sound_recursive(Scene *scene, ListBase *seqbasep, bSound *sound)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbasep) {
    if (seq->type == SEQ_TYPE_META) {
      seq_update_sound_recursive(scene, &seq->seqbase, sound);
    }
    else if (seq->type == SEQ_TYPE_SOUND_RAM) {
      if (seq->scene_sound && sound == seq->sound) {
        BKE_sound_update_scene_sound(seq->scene_sound, sound);
      }
    }
  }
}

void SEQ_sound_update(Scene *scene, bSound *sound)
{
  if (scene->ed) {
    seq_update_sound_recursive(scene, &scene->ed->seqbase, sound);
  }
}

float SEQ_sound_pitch_get(const Scene *scene, const Sequence *seq)
{
  const Sequence *meta_parent = seq_sequence_lookup_meta_by_seq(scene, seq);
  if (meta_parent != nullptr) {
    return seq->speed_factor * SEQ_sound_pitch_get(scene, meta_parent);
  }
  return seq->speed_factor;
}

EQCurveMappingData *SEQ_sound_equalizer_add(SoundEqualizerModifierData *semd,
                                            float minX,
                                            float maxX)
{
  EQCurveMappingData *eqcmd;

  if (maxX < 0)
    maxX = SOUND_EQUALIZER_DEFAULT_MAX_FREQ;
  if (minX < 0)
    minX = 0.0;
  /* It's the same as BKE_curvemapping_add , but changing the name */
  eqcmd = MEM_cnew<EQCurveMappingData>("Equalizer");
  BKE_curvemapping_set_defaults(&eqcmd->curve_mapping,
                                1, /* tot*/
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

  BKE_curvemap_reset(&eqcmd->curve_mapping.cm[0], &clipr, CURVE_PRESET_CONSTANT_MEDIAN, 0);

  BLI_addtail(&semd->graphics, eqcmd);

  return eqcmd;
}

void SEQ_sound_equalizermodifier_set_graphs(SoundEqualizerModifierData *semd, int number)
{
  SEQ_sound_equalizermodifier_free((SequenceModifierData *)semd);
  if (number == 1) {
    SEQ_sound_equalizer_add(
        semd, SOUND_EQUALIZER_DEFAULT_MIN_FREQ, SOUND_EQUALIZER_DEFAULT_MAX_FREQ);
  }
  else if (number == 2) {
    SEQ_sound_equalizer_add(semd, 30.0, 2000.0);
    SEQ_sound_equalizer_add(semd, 2000.1, 20000.0);
  }
  else if (number == 3) {
    SEQ_sound_equalizer_add(semd, 30.0, 1000.0);
    SEQ_sound_equalizer_add(semd, 1000.1, 5000.0);
    SEQ_sound_equalizer_add(semd, 5000.1, 20000.0);
  }
}

EQCurveMappingData *SEQ_sound_equalizermodifier_add_graph(SoundEqualizerModifierData *semd,
                                                          float min_freq,
                                                          float max_freq)
{
  if (min_freq < 0.0)
    return nullptr;
  if (max_freq < 0.0)
    return nullptr;
  if (max_freq <= min_freq)
    return nullptr;
  return SEQ_sound_equalizer_add(semd, min_freq, max_freq);
}

void SEQ_sound_equalizermodifier_remove_graph(SoundEqualizerModifierData *semd,
                                              EQCurveMappingData *eqcmd)
{
  BLI_remlink_safe(&semd->graphics, eqcmd);
  MEM_freeN(eqcmd);
}

void SEQ_sound_equalizermodifier_init_data(SequenceModifierData *smd)
{
  SoundEqualizerModifierData *semd = (SoundEqualizerModifierData *)smd;

  SEQ_sound_equalizer_add(
      semd, SOUND_EQUALIZER_DEFAULT_MIN_FREQ, SOUND_EQUALIZER_DEFAULT_MAX_FREQ);
}

void SEQ_sound_equalizermodifier_free(SequenceModifierData *smd)
{
  SoundEqualizerModifierData *semd = (SoundEqualizerModifierData *)smd;
  LISTBASE_FOREACH_MUTABLE (EQCurveMappingData *, eqcmd, &semd->graphics) {
    BKE_curvemapping_free_data(&eqcmd->curve_mapping);
    MEM_freeN(eqcmd);
  }
  BLI_listbase_clear(&semd->graphics);
}

void SEQ_sound_equalizermodifier_copy_data(SequenceModifierData *target, SequenceModifierData *smd)
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

void *SEQ_sound_equalizermodifier_recreator(Sequence *seq, SequenceModifierData *smd, void *sound)
{
#ifdef WITH_AUDASPACE
  UNUSED_VARS(seq);

  SoundEqualizerModifierData *semd = (SoundEqualizerModifierData *)smd;

  // No Equalizer definition
  if (BLI_listbase_is_empty(&semd->graphics)) {
    return sound;
  }

  float *buf = (float *)MEM_callocN(sizeof(float) * SOUND_EQUALIZER_SIZE_DEFINITION,
                                    "eqrecreator");

  CurveMapping *eq_mapping;
  CurveMap *cm;
  float minX;
  float maxX;
  float interval = SOUND_EQUALIZER_DEFAULT_MAX_FREQ / float(SOUND_EQUALIZER_SIZE_DEFINITION);

  // Visit all equalizer definitions
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
      if (fabs(val) > SOUND_EQUALIZER_DEFAULT_MAX_DB)
        val = (val / fabs(val)) * SOUND_EQUALIZER_DEFAULT_MAX_DB;
      buf[i] = val;
      /* To soften lower limit, but not the first position which is the constant value */
      if (i == idx && i > 2) {
        buf[i - 1] = 0.5 * (buf[i] + buf[i - 1]);
      }
    }
    /* To soften higher limit */
    if (i < SOUND_EQUALIZER_SIZE_DEFINITION)
      buf[i] = 0.5 * (buf[i] + buf[i - 1]);
  }

  AUD_Sound *equ = AUD_Sound_equalize(sound,
                                      buf,
                                      SOUND_EQUALIZER_SIZE_DEFINITION,
                                      SOUND_EQUALIZER_DEFAULT_MAX_FREQ,
                                      SOUND_EQUALIZER_SIZE_CONVERSION);

  MEM_freeN(buf);

  return equ;
#else
  UNUSED_VARS(seq, smd, sound);
  return nullptr;
#endif
}

const SoundModifierWorkerInfo *SEQ_sound_modifier_worker_info_get(int type)
{
  for (int i = 0; workersSoundModifiers[i].type > 0; i++) {
    if (workersSoundModifiers[i].type == type)
      return &workersSoundModifiers[i];
  }
  return nullptr;
}

void *SEQ_sound_modifier_recreator(Sequence *seq, SequenceModifierData *smd, void *sound)
{

  if (!(smd->flag & SEQUENCE_MODIFIER_MUTE)) {
    const SoundModifierWorkerInfo *smwi = SEQ_sound_modifier_worker_info_get(smd->type);
    return smwi->recreator(seq, smd, sound);
  }
  return sound;
}
