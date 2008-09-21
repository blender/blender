/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BIF_EDITSEQ_H
#define BIF_EDITSEQ_H

struct Sequence;

void				add_duplicate_seq(void);
void				add_sequence(int type);
void				borderselect_seq(void);
void				boundbox_seq(void);
void				change_sequence(void);
void				reload_sequence(void);
void				update_seq_ipo_rect(struct Sequence * seq);
void				update_seq_icu_rects(struct Sequence * seq);
struct Sequence*	get_last_seq();
struct Sequence*	get_forground_frame_seq( int frame );
void				set_last_seq(struct Sequence * seq);
void				clear_last_seq();
void				del_seq(void);
void				enter_meta(void);
void				exit_meta(void);
struct Sequence*	find_neighboring_sequence(struct Sequence *test, int lr, int sel);
struct Sequence*	find_next_prev_sequence(struct Sequence *test, int lr, int sel);
struct Sequence*	find_nearest_seq(int *hand);
int					insert_gap(int gap, int cfra);
void				make_meta(void);
void				select_single_seq(struct Sequence *seq, int deselect_all);
void				select_channel_direction(struct Sequence *test,int lr);
void				select_more_seq(void);
void				select_less_seq(void);
void				mouse_select_seq(void);
void				no_gaps(void);
void				seq_snap(short event);
void				seq_snap_menu(void);
void				seq_mute_sel( int mute );
void                            seq_lock_sel(int lock);
void				set_filter_seq(void);
void				swap_select_seq(void);
void				touch_seq_files(void);
void				seq_remap_paths(void);
void				transform_seq(int mode, int context);
void				transform_seq_nomarker(int mode, int context);
void				un_meta(void);
void				seq_cut(int cutframe, int hard_cut);
void				seq_separate_images(void);
void				reassign_inputs_seq_effect(void);
void				select_surrounding_handles(struct Sequence *test);
void				select_surround_from_last();
void				select_dir_from_last(int lr);
void				select_neighbor_from_last(int lr);
void				select_linked_seq(int mode);
int					test_overlap_seq(struct Sequence *test);
void				shuffle_seq(struct Sequence *test);
struct Sequence*	alloc_sequence(ListBase *lb, int cfra, int machine); /*used from python*/
int 				check_single_seq(struct Sequence *seq);

/* seq funcs for transform
 notice the difference between start/end and left/right.
 
 left and right are the bounds at which the setuence is rendered,
start and end are from the start and fixed length of the sequence.
*/

/* sequence transform functions, for internal used */
int seq_tx_get_start(struct Sequence *seq);
int seq_tx_get_end(struct Sequence *seq);

int seq_tx_get_final_left(struct Sequence *seq, int metaclip);
int seq_tx_get_final_right(struct Sequence *seq, int metaclip);

void seq_tx_set_final_left(struct Sequence *seq, int i);
void seq_tx_set_final_right(struct Sequence *seq, int i);

/* check if one side can be transformed */
int seq_tx_check_left(struct Sequence *seq);
int seq_tx_check_right(struct Sequence *seq);

#define SEQ_DEBUG_INFO(seq) printf("seq into '%s' -- len:%i  start:%i  startstill:%i  endstill:%i  startofs:%i  endofs:%i depth:%i\n",\
		    seq->name, seq->len, seq->start, seq->startstill, seq->endstill, seq->startofs, seq->endofs, seq->depth)


#endif

