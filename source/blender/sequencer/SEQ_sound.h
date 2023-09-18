/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct Scene;
struct Sequence;
struct bSound;
struct SequencerSoundEqualizer;
struct BlendWriter;
struct BlendDataReader;
struct ListBase;
struct SoundEqualizerModifierData;

#define SOUND_EQUALIZER_DEFAULT_MIN_FREQ 30.0
#define SOUND_EQUALIZER_DEFAULT_MAX_FREQ 20000.0
#define SOUND_EQUALIZER_DEFAULT_MAX_DB 35.0
#define SOUND_EQUALIZER_SIZE_CONVERSION 2048
#define SOUND_EQUALIZER_SIZE_DEFINITION 1000

void SEQ_sound_update_bounds_all(struct Scene *scene);
void SEQ_sound_update_bounds(struct Scene *scene, struct Sequence *seq);
void SEQ_sound_update(struct Scene *scene, struct bSound *sound);
void SEQ_sound_update_length(struct Main *bmain, struct Scene *scene);
float SEQ_sound_pitch_get(const struct Scene *scene, const struct Sequence *seq);
struct EQCurveMappingData *SEQ_sound_equalizer_add(struct SoundEqualizerModifierData *semd,
                                                   float minX,
                                                   float maxX);
void SEQ_sound_blend_write(struct BlendWriter *writer, struct ListBase *soundbase);
void SEQ_sound_blend_read_data(struct BlendDataReader *reader, struct ListBase *lb);

void *SEQ_sound_modifier_recreator(struct Sequence *seq,
                                   struct SequenceModifierData *smd,
                                   void *sound);

void SEQ_sound_equalizermodifier_init_data(struct SequenceModifierData *smd);
void SEQ_sound_equalizermodifier_free(struct SequenceModifierData *smd);
void SEQ_sound_equalizermodifier_copy_data(struct SequenceModifierData *target,
                                           struct SequenceModifierData *smd);
void *SEQ_sound_equalizermodifier_recreator(struct Sequence *seq,
                                            struct SequenceModifierData *smd,
                                            void *sound);
void SEQ_sound_equalizermodifier_set_graphs(struct SoundEqualizerModifierData *semd, int number);
const struct SoundModifierWorkerInfo *SEQ_sound_modifier_worker_info_get(int type);
struct EQCurveMappingData *SEQ_sound_equalizermodifier_add_graph(
    struct SoundEqualizerModifierData *semd, float min_freq, float max_freq);
void SEQ_sound_equalizermodifier_remove_graph(struct SoundEqualizerModifierData *semd,
                                              struct EQCurveMappingData *gsed);

typedef struct SoundModifierWorkerInfo {
  int type;
  void *(*recreator)(struct Sequence *seq, struct SequenceModifierData *smd, void *sound);
} SoundModifierWorkerInfo;

#ifdef __cplusplus
}
#endif
