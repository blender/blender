/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_sequencer/sequencer_intern.h
 *  \ingroup spseq
 */

#ifndef __SEQUENCER_INTERN_H__
#define __SEQUENCER_INTERN_H__

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
struct Main;
struct SequencePreview;

/* space_sequencer.c */
struct ARegion *sequencer_has_buttons_region(struct ScrArea *sa);


/* sequencer_draw.c */
void draw_timeline_seq(const struct bContext *C, struct ARegion *ar);
void draw_image_seq(const struct bContext *C, struct Scene *scene, struct  ARegion *ar, struct SpaceSeq *sseq, int cfra, int offset, bool draw_overlay, bool draw_backdrop);
void color3ubv_from_seq(struct Scene *curscene, struct Sequence *seq, unsigned char col[3]);
void draw_shadedstrip(struct Sequence *seq, unsigned char col[3], float x1, float y1, float x2, float y2);

/* UNUSED */
// void seq_reset_imageofs(struct SpaceSeq *sseq);

struct ImBuf *sequencer_ibuf_get(struct Main *bmain, struct Scene *scene, struct SpaceSeq *sseq, int cfra, int frame_ofs);

/* sequencer_edit.c */
struct View2D;
void seq_rectf(struct Sequence *seq, struct rctf *rectf);
void boundbox_seq(struct Scene *scene, struct rctf *rect);
struct Sequence *find_nearest_seq(struct Scene *scene, struct View2D *v2d, int *hand, const int mval[2]);
struct Sequence *find_neighboring_sequence(struct Scene *scene, struct Sequence *test, int lr, int sel);
void recurs_sel_seq(struct Sequence *seqm);
int seq_effect_find_selected(struct Scene *scene, struct Sequence *activeseq, int type, struct Sequence **selseq1, struct Sequence **selseq2, struct Sequence **selseq3, const char **error_str);

/* operator helpers */
int sequencer_edit_poll(struct bContext *C);
/* UNUSED */
//int sequencer_strip_poll(struct bContext *C);
int sequencer_strip_has_path_poll(struct bContext *C);
int sequencer_view_poll(struct bContext *C);

/* externs */
extern EnumPropertyItem sequencer_prop_effect_types[];
extern EnumPropertyItem prop_side_types[];

/* operators */
struct wmOperatorType;
struct wmKeyConfig;

void SEQUENCER_OT_cut(struct wmOperatorType *ot);
void SEQUENCER_OT_slip(struct wmOperatorType *ot);
void SEQUENCER_OT_mute(struct wmOperatorType *ot);
void SEQUENCER_OT_unmute(struct wmOperatorType *ot);
void SEQUENCER_OT_lock(struct wmOperatorType *ot);
void SEQUENCER_OT_unlock(struct wmOperatorType *ot);
void SEQUENCER_OT_reload(struct wmOperatorType *ot);
void SEQUENCER_OT_refresh_all(struct wmOperatorType *ot);
void SEQUENCER_OT_reassign_inputs(struct wmOperatorType *ot);
void SEQUENCER_OT_swap_inputs(struct wmOperatorType *ot);
void SEQUENCER_OT_duplicate(struct wmOperatorType *ot);
void SEQUENCER_OT_delete(struct wmOperatorType *ot);
void SEQUENCER_OT_offset_clear(struct wmOperatorType *ot);
void SEQUENCER_OT_images_separate(struct wmOperatorType *ot);
void SEQUENCER_OT_meta_toggle(struct wmOperatorType *ot);
void SEQUENCER_OT_meta_make(struct wmOperatorType *ot);
void SEQUENCER_OT_meta_separate(struct wmOperatorType *ot);

void SEQUENCER_OT_gap_remove(struct wmOperatorType *ot);
void SEQUENCER_OT_gap_insert(struct wmOperatorType *ot);
void SEQUENCER_OT_snap(struct wmOperatorType *ot);

void SEQUENCER_OT_strip_jump(struct wmOperatorType *ot);
void SEQUENCER_OT_swap(struct wmOperatorType *ot);
void SEQUENCER_OT_swap_data(struct wmOperatorType *ot);
void SEQUENCER_OT_rendersize(struct wmOperatorType *ot);

void SEQUENCER_OT_view_toggle(struct wmOperatorType *ot);
void SEQUENCER_OT_view_all(struct wmOperatorType *ot);
void SEQUENCER_OT_view_selected(struct wmOperatorType *ot);
void SEQUENCER_OT_view_zoom_ratio(struct wmOperatorType *ot);
void SEQUENCER_OT_view_ghost_border(struct wmOperatorType *ot);

void SEQUENCER_OT_change_effect_input(struct wmOperatorType *ot);
void SEQUENCER_OT_change_effect_type(struct wmOperatorType *ot);
void SEQUENCER_OT_change_path(struct wmOperatorType *ot);

void SEQUENCER_OT_copy(struct wmOperatorType *ot);
void SEQUENCER_OT_paste(struct wmOperatorType *ot);

void SEQUENCER_OT_rebuild_proxy(struct wmOperatorType *ot);

/* preview specific operators */
void SEQUENCER_OT_view_all_preview(struct wmOperatorType *ot);

/* sequencer_select.c */
void SEQUENCER_OT_select_all(struct wmOperatorType *ot);
void SEQUENCER_OT_select(struct wmOperatorType *ot);
void SEQUENCER_OT_select_more(struct wmOperatorType *ot);
void SEQUENCER_OT_select_less(struct wmOperatorType *ot);
void SEQUENCER_OT_select_linked(struct wmOperatorType *ot);
void SEQUENCER_OT_select_linked_pick(struct wmOperatorType *ot);
void SEQUENCER_OT_select_handles(struct wmOperatorType *ot);
void SEQUENCER_OT_select_active_side(struct wmOperatorType *ot);
void SEQUENCER_OT_select_border(struct wmOperatorType *ot);
void SEQUENCER_OT_select_inverse(struct wmOperatorType *ot);
void SEQUENCER_OT_select_grouped(struct wmOperatorType *ot);

/* sequencer_select.c */
void SEQUENCER_OT_scene_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_movie_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_movieclip_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_mask_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_sound_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_image_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_effect_strip_add(struct wmOperatorType *ot);

enum {
	SEQ_CUT_SOFT,
	SEQ_CUT_HARD
};
enum {
	SEQ_SELECTED,
	SEQ_UNSELECTED
};

enum {
	SEQ_SELECT_LR_NONE = 0,
	SEQ_SELECT_LR_MOUSE,
	SEQ_SELECT_LR_LEFT,
	SEQ_SELECT_LR_RIGHT	
};

/* defines used internally */
#define SCE_MARKERS 0 // XXX - dummy

/* sequencer_ops.c */
void sequencer_operatortypes(void);
void sequencer_keymap(struct wmKeyConfig *keyconf);

/* sequencer_scope.c */
struct ImBuf *make_waveform_view_from_ibuf(struct ImBuf *ibuf);
struct ImBuf *make_sep_waveform_view_from_ibuf(struct ImBuf *ibuf);
struct ImBuf *make_vectorscope_view_from_ibuf(struct ImBuf *ibuf);
struct ImBuf *make_zebra_view_from_ibuf(struct ImBuf *ibuf, float perc);
struct ImBuf *make_histogram_view_from_ibuf(struct ImBuf *ibuf);

/* sequencer_buttons.c */
void sequencer_buttons_register(struct ARegionType *art);
void SEQUENCER_OT_properties(struct wmOperatorType *ot);

/* sequencer_modifiers.c */
void SEQUENCER_OT_strip_modifier_add(struct wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_remove(struct wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_move(struct wmOperatorType *ot);

/* sequencer_view.c */
void SEQUENCER_OT_sample(struct wmOperatorType *ot);

/* sequencer_preview.c */
void sequencer_preview_add_sound(const struct bContext *C, struct Sequence *seq);

#endif /* __SEQUENCER_INTERN_H__ */

