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

/* drawseq.c */
void do_seqbuttons(short);

#endif

