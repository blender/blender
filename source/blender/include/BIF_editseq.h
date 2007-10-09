/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BIF_EDITSEQ_H
#define BIF_EDITSEQ_H

struct Sequence;

void				add_duplicate_seq(void);
void				add_sequence(int type);
void				borderselect_seq(void);
void				boundbox_seq(void);
void				change_sequence(void);
void				update_seq_ipo_rect(struct Sequence * seq);
void				update_seq_icu_rects(struct Sequence * seq);
struct Sequence*	get_last_seq();
void				set_last_seq(struct Sequence * seq);
void				clear_last_seq();
void				del_seq(void);
void				enter_meta(void);
void				exit_meta(void);
struct Sequence*	find_neighboring_sequence(struct Sequence *test, int lr);
struct Sequence*	find_nearest_seq(int *hand);
int					insert_gap(int gap, int cfra);
void				make_meta(void);
void				select_channel_direction(struct Sequence *test,int lr);
void				mouse_select_seq(void);
void				no_gaps(void);
void				seq_snap(short event);
void				seq_snap_menu(void);
void				set_filter_seq(void);
void				swap_select_seq(void);
void				touch_seq_files(void);
void				seq_remap_paths(void);
void				transform_seq(int mode, int context);
void				un_meta(void);
void				seq_cut(int cutframe);
void				reassign_inputs_seq_effect(void);
void				select_surrounding_handles(struct Sequence *test);
void				select_surround_from_last();
void				select_dir_from_last(int lr);
void				select_neighbor_from_last(int lr);
struct Sequence*	alloc_sequence(ListBase *lb, int cfra, int machine); /*used from python*/
int 				check_single_image_seq(struct Sequence *seq);

/* sequence transform functions, for internal used */
int seq_tx_get_start(struct Sequence *seq);
int seq_tx_get_end(struct Sequence *seq);

int seq_tx_get_final_left(struct Sequence *seq);
int seq_tx_get_final_right(struct Sequence *seq);

void seq_tx_set_final_left(struct Sequence *seq, int i);
void seq_tx_set_final_right(struct Sequence *seq, int i);

#define SEQ_DEBUG_INFO(seq) printf("seq into '%s' -- len:%i  start:%i  startstill:%i  endstill:%i  startofs:%i  endofs:%i\n",\
		    seq->name, seq->len, seq->start, seq->startstill, seq->endstill, seq->startofs, seq->endofs)

/* seq macro's for transform
 notice the difference between start/end and left/right.
 
 left and right are the bounds at which the setuence is rendered,
start and end are from the start and fixed length of the sequence.
*/
/*
#define SEQ_GET_START(seq)	(seq->start)
#define SEQ_GET_END(seq)	(seq->start+seq->len)

#define SEQ_GET_FINAL_LEFT(seq)		((seq->start - seq->startstill) + seq->startofs)
#define SEQ_GET_FINAL_RIGHT(seq)	(((seq->start+seq->len) + seq->endstill) - seq->endofs)

#define SEQ_SET_FINAL_LEFT(seq, val) \
	if (val < (seq)->start) { \
		(seq)->startstill = abs(val - (seq)->start); \
		(seq)->startofs = 0; \
} else { \
		(seq)->startofs = abs(val - (seq)->start); \
		(seq)->startstill = 0; \
}

#define SEQ_SET_FINAL_RIGHT(seq, val) \
	if (val > (seq)->start + (seq)->len) { \
		(seq)->endstill = abs(val - ((seq)->start + (seq)->len)); \
		(seq)->endofs = 0; \
} else { \
		(seq)->endofs = abs(val - ((seq)->start + (seq)->len)); \
		(seq)->endstill = 0; \
}
*/
/* drawseq.c */
void do_seqbuttons(short);

#endif

