/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ImBuf;
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
typedef struct SeqRenderState {
  struct LinkNode *scene_parents;
} SeqRenderState;

void seq_render_state_init(SeqRenderState *state);

struct ImBuf *seq_render_give_ibuf_seqbase(const struct SeqRenderData *context,
                                           float timeline_frame,
                                           int chan_shown,
                                           struct ListBase *seqbasep);
struct ImBuf *seq_render_effect_execute_threaded(struct SeqEffectHandle *sh,
                                                 const struct SeqRenderData *context,
                                                 struct Sequence *seq,
                                                 float timeline_frame,
                                                 float fac,
                                                 struct ImBuf *ibuf1,
                                                 struct ImBuf *ibuf2,
                                                 struct ImBuf *ibuf3);
void seq_imbuf_to_sequencer_space(struct Scene *scene, struct ImBuf *ibuf, bool make_float);
int seq_get_shown_sequences(struct ListBase *seqbase,
                            int timeline_frame,
                            int chanshown,
                            struct Sequence **r_seq_arr);
struct ImBuf *seq_render_strip(const struct SeqRenderData *context,
                               struct SeqRenderState *state,
                               struct Sequence *seq,
                               float timeline_frame);
struct ImBuf *seq_render_mask(const struct SeqRenderData *context,
                              struct Mask *mask,
                              float frame_index,
                              bool make_float);
void seq_imbuf_assign_spaces(struct Scene *scene, struct ImBuf *ibuf);

#ifdef __cplusplus
}
#endif
