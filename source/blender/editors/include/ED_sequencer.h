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
 * The Original Code is Copyright (C) 2009, Blender Foundation
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_SEQUENCER_H__
#define __ED_SEQUENCER_H__

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
void ED_sequencer_deselect_all(struct Scene *scene);

bool ED_space_sequencer_maskedit_mask_poll(struct bContext *C);
bool ED_space_sequencer_check_show_maskedit(struct SpaceSeq *sseq, struct Scene *scene);
bool ED_space_sequencer_maskedit_poll(struct bContext *C);

bool ED_space_sequencer_check_show_imbuf(struct SpaceSeq *sseq);
bool ED_space_sequencer_check_show_strip(struct SpaceSeq *sseq);

void ED_operatormacros_sequencer(void);

Sequence *ED_sequencer_special_preview_get(void);
void ED_sequencer_special_preview_set(struct bContext *C, const int mval[2]);
void ED_sequencer_special_preview_clear(void);

#ifdef __cplusplus
}
#endif

#endif /*  __ED_SEQUENCER_H__ */
