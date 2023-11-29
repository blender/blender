/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct ImBuf;
struct LinkNode;
struct ListBase;
struct Scene;
struct SeqEffectHandle;
struct SeqRenderData;
struct Sequence;

#define EARLY_NO_INPUT -1
#define EARLY_DO_EFFECT 0
#define EARLY_USE_INPUT_1 1
#define EARLY_USE_INPUT_2 2

/* mutable state for sequencer */
struct SeqRenderState {
  LinkNode *scene_parents;
};

void seq_render_state_init(SeqRenderState *state);

ImBuf *seq_render_give_ibuf_seqbase(const SeqRenderData *context,
                                    float timeline_frame,
                                    int chan_shown,
                                    ListBase *channels,
                                    ListBase *seqbasep);
ImBuf *seq_render_effect_execute_threaded(SeqEffectHandle *sh,
                                          const SeqRenderData *context,
                                          Sequence *seq,
                                          float timeline_frame,
                                          float fac,
                                          ImBuf *ibuf1,
                                          ImBuf *ibuf2,
                                          ImBuf *ibuf3);
void seq_imbuf_to_sequencer_space(Scene *scene, ImBuf *ibuf, bool make_float);
int seq_get_shown_sequences(const Scene *scene,
                            ListBase *channels,
                            ListBase *seqbase,
                            int timeline_frame,
                            int chanshown,
                            Sequence **r_seq_arr);
ImBuf *seq_render_strip(const SeqRenderData *context,
                        SeqRenderState *state,
                        Sequence *seq,
                        float timeline_frame);
ImBuf *seq_render_mask(const SeqRenderData *context,
                       Mask *mask,
                       float frame_index,
                       bool make_float);
void seq_imbuf_assign_spaces(Scene *scene, ImBuf *ibuf);
