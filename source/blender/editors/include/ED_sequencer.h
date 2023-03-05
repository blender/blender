/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 Blender Foundation. */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Scene;
struct Sequence;
struct SpaceSeq;
struct bContext;

void ED_sequencer_select_sequence_single(struct Scene *scene,
                                         struct Sequence *seq,
                                         bool deselect_all);
/**
 * Iterates over a scene's sequences and deselects all of them.
 *
 * \param scene: scene containing sequences to be deselected.
 * \return true if any sequences were deselected; false otherwise.
 */
bool ED_sequencer_deselect_all(struct Scene *scene);

bool ED_space_sequencer_maskedit_mask_poll(struct bContext *C);
bool ED_space_sequencer_check_show_maskedit(struct SpaceSeq *sseq, struct Scene *scene);
bool ED_space_sequencer_maskedit_poll(struct bContext *C);

/**
 * Are we displaying the seq output (not channels or histogram).
 */
bool ED_space_sequencer_check_show_imbuf(struct SpaceSeq *sseq);

bool ED_space_sequencer_check_show_strip(struct SpaceSeq *sseq);
/**
 * Check if there is animation shown during playback.
 *
 * - Colors of color strips are displayed on the strip itself.
 * - Backdrop is drawn.
 */
bool ED_space_sequencer_has_playback_animation(const struct SpaceSeq *sseq,
                                               const struct Scene *scene);

void ED_operatormacros_sequencer(void);

Sequence *ED_sequencer_special_preview_get(void);
void ED_sequencer_special_preview_set(struct bContext *C, const int mval[2]);
void ED_sequencer_special_preview_clear(void);

#ifdef __cplusplus
}
#endif
