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
 * Contributor(s): Blender Foundation, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_SEQUENCER_INTERN_H
#define ED_SEQUENCER_INTERN_H

#include "RNA_access.h"
#include "DNA_sequence_types.h"

/* internal exports only */

struct Sequence;
struct bContext;
struct rctf;
struct SpaceSeq;
struct ScrArea;
struct ARegion;
struct ARegionType;
struct Scene;

/* space_sequencer.c */
struct ARegion *sequencer_has_buttons_region(struct ScrArea *sa);


/* sequencer_draw.c */
void draw_timeline_seq(const struct bContext *C, struct ARegion *ar);
void draw_image_seq(struct Scene *scene, struct ARegion *ar, struct SpaceSeq *sseq);

void seq_reset_imageofs(struct SpaceSeq *sseq);

/* sequencer_edit.c */
struct View2D;
int check_single_seq(struct Sequence *seq);
int seq_tx_get_final_left(struct Sequence *seq, int metaclip);
int seq_tx_get_final_right(struct Sequence *seq, int metaclip);
void seq_rectf(struct Sequence *seq, struct rctf *rectf);
void boundbox_seq(struct Scene *scene, struct rctf *rect);
struct Sequence *find_nearest_seq(struct Scene *scene, struct View2D *v2d, int *hand, short mval[2]);
struct Sequence *find_neighboring_sequence(struct Scene *scene, struct Sequence *test, int lr, int sel);
void deselect_all_seq(struct Scene *scene);
void recurs_sel_seq(struct Sequence *seqm);
int event_to_efftype(int event);
int seq_effect_find_selected(struct Scene *scene, struct Sequence *activeseq, int type, struct Sequence **selseq1, struct Sequence **selseq2, struct Sequence **selseq3, char **error_str);
struct Sequence *alloc_sequence(struct ListBase *lb, int cfra, int machine);

/* externs */
extern EnumPropertyItem sequencer_prop_effect_types[];
extern EnumPropertyItem prop_side_types[];

/* operators */
struct wmOperatorType;
struct wmKeyConfig;

void SEQUENCER_OT_cut(struct wmOperatorType *ot);
void SEQUENCER_OT_mute(struct wmOperatorType *ot);
void SEQUENCER_OT_unmute(struct wmOperatorType *ot);
void SEQUENCER_OT_lock(struct wmOperatorType *ot);
void SEQUENCER_OT_unlock(struct wmOperatorType *ot);
void SEQUENCER_OT_reload(struct wmOperatorType *ot);
void SEQUENCER_OT_refresh_all(struct wmOperatorType *ot);
void SEQUENCER_OT_duplicate(struct wmOperatorType *ot);
void SEQUENCER_OT_delete(struct wmOperatorType *ot);
void SEQUENCER_OT_images_separate(struct wmOperatorType *ot);
void SEQUENCER_OT_meta_toggle(struct wmOperatorType *ot);
void SEQUENCER_OT_meta_make(struct wmOperatorType *ot);
void SEQUENCER_OT_meta_separate(struct wmOperatorType *ot);
void SEQUENCER_OT_snap(struct wmOperatorType *ot);
void SEQUENCER_OT_previous_edit(struct wmOperatorType *ot);
void SEQUENCER_OT_next_edit(struct wmOperatorType *ot);
void SEQUENCER_OT_swap(struct wmOperatorType *ot);
void SEQUENCER_OT_rendersize(struct wmOperatorType *ot);

void SEQUENCER_OT_view_toggle(struct wmOperatorType *ot);
void SEQUENCER_OT_view_all(struct wmOperatorType *ot);
void SEQUENCER_OT_view_selected(struct wmOperatorType *ot);

/* preview specific operators */
void SEQUENCER_OT_view_all_preview(struct wmOperatorType *ot);

/* sequencer_select.c */
void SEQUENCER_OT_select_all_toggle(struct wmOperatorType *ot);
void SEQUENCER_OT_select(struct wmOperatorType *ot);
void SEQUENCER_OT_select_more(struct wmOperatorType *ot);
void SEQUENCER_OT_select_less(struct wmOperatorType *ot);
void SEQUENCER_OT_select_linked(struct wmOperatorType *ot);
void SEQUENCER_OT_select_linked_pick(struct wmOperatorType *ot);
void SEQUENCER_OT_select_handles(struct wmOperatorType *ot);
void SEQUENCER_OT_select_active_side(struct wmOperatorType *ot);
void SEQUENCER_OT_select_border(struct wmOperatorType *ot);
void SEQUENCER_OT_select_inverse(struct wmOperatorType *ot);


/* sequencer_select.c */
void SEQUENCER_OT_scene_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_movie_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_sound_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_image_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_effect_strip_add(struct wmOperatorType *ot);

/* RNA enums, just to be more readable */
enum {
	SEQ_SIDE_NONE=0,
    SEQ_SIDE_LEFT,
    SEQ_SIDE_RIGHT,
	SEQ_SIDE_BOTH,
};
enum {
    SEQ_CUT_SOFT,
    SEQ_CUT_HARD,
};
enum {
    SEQ_SELECTED,
    SEQ_UNSELECTED,
};

/* defines used internally */
#define SEQ_ALLSEL	(SELECT+SEQ_LEFTSEL+SEQ_RIGHTSEL)
#define SEQ_DESEL	~SEQ_ALLSEL
#define SCE_MARKERS 0 // XXX - dummy

/* sequencer_ops.c */
void sequencer_operatortypes(void);
void sequencer_keymap(struct wmKeyConfig *keyconf);

/* sequencer_scope.c */
struct ImBuf *make_waveform_view_from_ibuf(struct ImBuf * ibuf);
struct ImBuf *make_sep_waveform_view_from_ibuf(struct ImBuf * ibuf);
struct ImBuf *make_vectorscope_view_from_ibuf(struct ImBuf * ibuf);
struct ImBuf *make_zebra_view_from_ibuf(struct ImBuf * ibuf, float perc);
struct ImBuf *make_histogram_view_from_ibuf(struct ImBuf * ibuf);

/* sequencer_buttons.c */

void SEQUENCER_OT_properties(struct wmOperatorType *ot);
void sequencer_buttons_register(struct ARegionType *art);

#endif /* ED_SEQUENCER_INTERN_H */

