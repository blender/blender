/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct EQCurveMappingData;
struct Main;
struct Scene;
struct Strip;
struct bSound;
struct SequenceModifierData;
struct SoundModifierWorkerInfo;
struct BlendWriter;
struct BlendDataReader;
struct ListBase;
struct SoundEqualizerModifierData;

#define SOUND_EQUALIZER_DEFAULT_MIN_FREQ 30.0
#define SOUND_EQUALIZER_DEFAULT_MAX_FREQ 20000.0
#define SOUND_EQUALIZER_DEFAULT_MAX_DB 35.0
#define SOUND_EQUALIZER_SIZE_CONVERSION 2048
#define SOUND_EQUALIZER_SIZE_DEFINITION 1000

void SEQ_sound_update_bounds_all(Scene *scene);
void SEQ_sound_update_bounds(Scene *scene, Strip *strip);
void SEQ_sound_update(Scene *scene, bSound *sound);
void SEQ_sound_update_length(Main *bmain, Scene *scene);
float SEQ_sound_pitch_get(const Scene *scene, const Strip *strip);
EQCurveMappingData *SEQ_sound_equalizer_add(SoundEqualizerModifierData *semd,
                                            float minX,
                                            float maxX);
void SEQ_sound_blend_write(BlendWriter *writer, ListBase *soundbase);
void SEQ_sound_blend_read_data(BlendDataReader *reader, ListBase *lb);

void *SEQ_sound_modifier_recreator(Strip *strip, SequenceModifierData *smd, void *sound);

void SEQ_sound_equalizermodifier_init_data(SequenceModifierData *smd);
void SEQ_sound_equalizermodifier_free(SequenceModifierData *smd);
void SEQ_sound_equalizermodifier_copy_data(SequenceModifierData *target,
                                           SequenceModifierData *smd);
void *SEQ_sound_equalizermodifier_recreator(Strip *strip, SequenceModifierData *smd, void *sound);
void SEQ_sound_equalizermodifier_set_graphs(SoundEqualizerModifierData *semd, int number);
const SoundModifierWorkerInfo *SEQ_sound_modifier_worker_info_get(int type);
EQCurveMappingData *SEQ_sound_equalizermodifier_add_graph(SoundEqualizerModifierData *semd,
                                                          float min_freq,
                                                          float max_freq);
void SEQ_sound_equalizermodifier_remove_graph(SoundEqualizerModifierData *semd,
                                              EQCurveMappingData *eqcmd);

struct SoundModifierWorkerInfo {
  int type;
  void *(*recreator)(Strip *strip, SequenceModifierData *smd, void *sound);
};
