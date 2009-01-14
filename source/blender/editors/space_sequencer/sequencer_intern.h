/**
 * $Id:
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_SEQUENCER_INTERN_H
#define ED_SEQUENCER_INTERN_H

/* internal exports only */

struct Sequence;
struct bContext;
struct rctf;
struct SpaceSeq;
struct ARegion;
struct Scene;

/* sequencer_header.c */
void sequencer_header_buttons(const struct bContext *C, struct ARegion *ar);

/* sequencer_draw.c */
void drawseqspace(const struct bContext *C, struct ARegion *ar);

/* sequencer_edit.c */
int check_single_seq(struct Sequence *seq);
int seq_tx_get_final_left(struct Sequence *seq, int metaclip);
int seq_tx_get_final_right(struct Sequence *seq, int metaclip);
void boundbox_seq(struct Scene *scene, struct rctf *rect);
struct Sequence *get_last_seq(struct Scene *scene);

/* sequencer_scope.c */
struct ImBuf *make_waveform_view_from_ibuf(struct ImBuf * ibuf);
struct ImBuf *make_sep_waveform_view_from_ibuf(struct ImBuf * ibuf);
struct ImBuf *make_vectorscope_view_from_ibuf(struct ImBuf * ibuf);
struct ImBuf *make_zebra_view_from_ibuf(struct ImBuf * ibuf, float perc);
struct ImBuf *make_histogram_view_from_ibuf(struct ImBuf * ibuf);

#endif /* ED_SEQUENCER_INTERN_H */

