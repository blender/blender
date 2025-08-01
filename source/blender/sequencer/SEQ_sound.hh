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
struct StripModifierData;
struct BlendWriter;
struct BlendDataReader;
struct ListBase;
struct SoundEqualizerModifierData;

namespace blender::seq {

struct SoundModifierWorkerInfo {
  int type;
  void *(*recreator)(Strip *strip, StripModifierData *smd, void *sound, bool &needs_update);
};

#define SOUND_EQUALIZER_DEFAULT_MIN_FREQ 30.0
#define SOUND_EQUALIZER_DEFAULT_MAX_FREQ 20000.0
#define SOUND_EQUALIZER_DEFAULT_MAX_DB 35.0
#define SOUND_EQUALIZER_SIZE_CONVERSION 2048
#define SOUND_EQUALIZER_SIZE_DEFINITION 1000

void sound_update_bounds_all(Scene *scene);
void sound_update_bounds(Scene *scene, Strip *strip);
void sound_update(Scene *scene, bSound *sound);
void sound_update_length(Main *bmain, Scene *scene);
float sound_pitch_get(const Scene *scene, const Strip *strip);
EQCurveMappingData *sound_equalizer_add(SoundEqualizerModifierData *semd, float minX, float maxX);
void sound_blend_write(BlendWriter *writer, ListBase *soundbase);
void sound_blend_read_data(BlendDataReader *reader, ListBase *lb);

void *sound_modifier_recreator(Strip *strip,
                               StripModifierData *smd,
                               void *sound,
                               bool &needs_update);

void sound_equalizermodifier_init_data(StripModifierData *smd);
void sound_equalizermodifier_free(StripModifierData *smd);
void sound_equalizermodifier_copy_data(StripModifierData *target, StripModifierData *smd);
void *sound_equalizermodifier_recreator(Strip *strip,
                                        StripModifierData *smd,
                                        void *sound,
                                        bool &needs_update);
void sound_equalizermodifier_set_graphs(SoundEqualizerModifierData *semd, int number);
const SoundModifierWorkerInfo *sound_modifier_worker_info_get(int type);
EQCurveMappingData *sound_equalizermodifier_add_graph(SoundEqualizerModifierData *semd,
                                                      float min_freq,
                                                      float max_freq);
void sound_equalizermodifier_remove_graph(SoundEqualizerModifierData *semd,
                                          EQCurveMappingData *eqcmd);

}  // namespace blender::seq
