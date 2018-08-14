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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2003-2009, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_sequencer/sequencer_select.c
 *  \ingroup spseq
 */


#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_sequencer.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"

/* for menu/popup icons etc etc*/

#include "ED_screen.h"
#include "ED_sequencer.h"
#include "ED_select_utils.h"

#include "UI_view2d.h"

/* own include */
#include "sequencer_intern.h"
static void *find_nearest_marker(int UNUSED(d1), int UNUSED(d2))
{
	return NULL;
}

static void select_surrounding_handles(Scene *scene, Sequence *test) /* XXX BRING BACK */
{
	Sequence *neighbor;

	neighbor = find_neighboring_sequence(scene, test, SEQ_SIDE_LEFT, -1);
	if (neighbor) {
		/* Only select neighbor handle if matching handle from test seq is also selected, or if neighbor
		 * was not selected at all up till now.
		 * Otherwise, we get odd mismatch when shift-alt-rmb selecting neighbor strips... */
		if (!(neighbor->flag & SELECT) || (test->flag & SEQ_LEFTSEL)) {
			neighbor->flag |= SEQ_RIGHTSEL;
		}
		neighbor->flag |= SELECT;
		recurs_sel_seq(neighbor);
	}
	neighbor = find_neighboring_sequence(scene, test, SEQ_SIDE_RIGHT, -1);
	if (neighbor) {
		if (!(neighbor->flag & SELECT) || (test->flag & SEQ_RIGHTSEL)) {  /* See comment above. */
			neighbor->flag |= SEQ_LEFTSEL;
		}
		neighbor->flag |= SELECT;
		recurs_sel_seq(neighbor);
	}
}

/* used for mouse selection and for SEQUENCER_OT_select_active_side() */
static void select_active_side(ListBase *seqbase, int sel_side, int channel, int frame)
{
	Sequence *seq;

	for (seq = seqbase->first; seq; seq = seq->next) {
		if (channel == seq->machine) {
			switch (sel_side) {
				case SEQ_SIDE_LEFT:
					if (frame > (seq->startdisp)) {
						seq->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
						seq->flag |= SELECT;
					}
					break;
				case SEQ_SIDE_RIGHT:
					if (frame < (seq->startdisp)) {
						seq->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
						seq->flag |= SELECT;
					}
					break;
				case SEQ_SIDE_BOTH:
					seq->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
					seq->flag |= SELECT;
					break;
			}
		}
	}
}

/* used for mouse selection and for SEQUENCER_OT_select_active_side() */
static void select_linked_time(ListBase *seqbase, Sequence *seq_link)
{
	Sequence *seq;

	for (seq = seqbase->first; seq; seq = seq->next) {
		if (seq_link->machine != seq->machine) {
			int left_match = (seq->startdisp == seq_link->startdisp) ? 1 : 0;
			int right_match = (seq->enddisp == seq_link->enddisp) ? 1 : 0;

			if (left_match && right_match) {
				/* a direct match, copy the selection settinhs */
				seq->flag &= ~(SELECT | SEQ_LEFTSEL | SEQ_RIGHTSEL);
				seq->flag |= seq_link->flag & (SELECT | SEQ_LEFTSEL | SEQ_RIGHTSEL);

				recurs_sel_seq(seq);
			}
			else if (seq_link->flag & SELECT && (left_match || right_match)) {

				/* clear for reselection */
				seq->flag &= ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);

				if (left_match && seq_link->flag & SEQ_LEFTSEL)
					seq->flag |= SELECT | SEQ_LEFTSEL;

				if (right_match && seq_link->flag & SEQ_RIGHTSEL)
					seq->flag |= SELECT | SEQ_RIGHTSEL;

				recurs_sel_seq(seq);
			}
		}
	}
}

#if 0 // BRING BACK
void select_surround_from_last(Scene *scene)
{
	Sequence *seq = get_last_seq(scene);

	if (seq == NULL)
		return;

	select_surrounding_handles(scene, seq);
}
#endif

void ED_sequencer_select_sequence_single(Scene *scene, Sequence *seq, bool deselect_all)
{
	Editing *ed = BKE_sequencer_editing_get(scene, false);

	if (deselect_all)
		ED_sequencer_deselect_all(scene);

	BKE_sequencer_active_set(scene, seq);

	if ((seq->type == SEQ_TYPE_IMAGE) || (seq->type == SEQ_TYPE_MOVIE)) {
		if (seq->strip)
			BLI_strncpy(ed->act_imagedir, seq->strip->dir, FILE_MAXDIR);
	}
	else if (seq->type == SEQ_TYPE_SOUND_RAM) {
		if (seq->strip)
			BLI_strncpy(ed->act_sounddir, seq->strip->dir, FILE_MAXDIR);
	}
	seq->flag |= SELECT;
	recurs_sel_seq(seq);
}

#if 0
static void select_neighbor_from_last(Scene *scene, int lr)
{
	Sequence *seq = BKE_sequencer_active_get(scene);
	Sequence *neighbor;
	bool changed = false;
	if (seq) {
		neighbor = find_neighboring_sequence(scene, seq, lr, -1);
		if (neighbor) {
			switch (lr) {
				case SEQ_SIDE_LEFT:
					neighbor->flag |= SELECT;
					recurs_sel_seq(neighbor);
					neighbor->flag |= SEQ_RIGHTSEL;
					seq->flag |= SEQ_LEFTSEL;
					break;
				case SEQ_SIDE_RIGHT:
					neighbor->flag |= SELECT;
					recurs_sel_seq(neighbor);
					neighbor->flag |= SEQ_LEFTSEL;
					seq->flag |= SEQ_RIGHTSEL;
					break;
			}
			seq->flag |= SELECT;
			changed = true;
		}
	}
	if (changed) {
	}
}
#endif

/* (de)select operator */
static int sequencer_de_select_all_exec(bContext *C, wmOperator *op)
{
	int action = RNA_enum_get(op->ptr, "action");

	Scene *scene = CTX_data_scene(C);
	Editing *ed = BKE_sequencer_editing_get(scene, false);
	Sequence *seq;

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		for (seq = ed->seqbasep->first; seq; seq = seq->next) {
			if (seq->flag & SEQ_ALLSEL) {
				action = SEL_DESELECT;
				break;
			}
		}
	}

	for (seq = ed->seqbasep->first; seq; seq = seq->next) {
		switch (action) {
			case SEL_SELECT:
				seq->flag &= ~(SEQ_LEFTSEL + SEQ_RIGHTSEL);
				seq->flag |= SELECT;
				break;
			case SEL_DESELECT:
				seq->flag &= ~SEQ_ALLSEL;
				break;
			case SEL_INVERT:
				if (seq->flag & SEQ_ALLSEL) {
					seq->flag &= ~SEQ_ALLSEL;
				}
				else {
					seq->flag &= ~(SEQ_LEFTSEL + SEQ_RIGHTSEL);
					seq->flag |= SELECT;
				}
				break;
		}
	}

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_all(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All";
	ot->idname = "SEQUENCER_OT_select_all";
	ot->description = "Select or deselect all strips";

	/* api callbacks */
	ot->exec = sequencer_de_select_all_exec;
	ot->poll = sequencer_edit_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}


/* (de)select operator */
static int sequencer_select_inverse_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	Editing *ed = BKE_sequencer_editing_get(scene, false);
	Sequence *seq;

	for (seq = ed->seqbasep->first; seq; seq = seq->next) {
		if (seq->flag & SELECT) {
			seq->flag &= ~SEQ_ALLSEL;
		}
		else {
			seq->flag &= ~(SEQ_LEFTSEL + SEQ_RIGHTSEL);
			seq->flag |= SELECT;
		}
	}

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_inverse(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Inverse";
	ot->idname = "SEQUENCER_OT_select_inverse";
	ot->description = "Select unselected strips";

	/* api callbacks */
	ot->exec = sequencer_select_inverse_exec;
	ot->poll = sequencer_edit_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int sequencer_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	View2D *v2d = UI_view2d_fromcontext(C);
	Scene *scene = CTX_data_scene(C);
	Editing *ed = BKE_sequencer_editing_get(scene, false);
	const bool extend = RNA_boolean_get(op->ptr, "extend");
	const bool linked_handle = RNA_boolean_get(op->ptr, "linked_handle");
	const bool linked_time = RNA_boolean_get(op->ptr, "linked_time");
	int left_right = RNA_enum_get(op->ptr, "left_right");

	Sequence *seq, *neighbor, *act_orig;
	int hand, sel_side;
	TimeMarker *marker;

	if (ed == NULL)
		return OPERATOR_CANCELLED;

	marker = find_nearest_marker(SCE_MARKERS, 1); //XXX - dummy function for now

	seq = find_nearest_seq(scene, v2d, &hand, event->mval);

	// XXX - not nice, Ctrl+RMB needs to do left_right only when not over a strip
	if (seq && linked_time && (left_right == SEQ_SELECT_LR_MOUSE))
		left_right = SEQ_SELECT_LR_NONE;


	if (marker) {
		int oldflag;
		/* select timeline marker */
		if (extend) {
			oldflag = marker->flag;
			if (oldflag & SELECT)
				marker->flag &= ~SELECT;
			else
				marker->flag |= SELECT;
		}
		else {
			/* deselect_markers(0, 0); */ /* XXX, in 2.4x, seq selection used to deselect all, need to re-thnik this for 2.5 */
			marker->flag |= SELECT;
		}

	}
	else if (left_right != SEQ_SELECT_LR_NONE) {
		/* use different logic for this */
		float x;
		ED_sequencer_deselect_all(scene);

		switch (left_right) {
			case SEQ_SELECT_LR_MOUSE:
				x = UI_view2d_region_to_view_x(v2d, event->mval[0]);
				break;
			case SEQ_SELECT_LR_LEFT:
				x = CFRA - 1.0f;
				break;
			case SEQ_SELECT_LR_RIGHT:
			default:
				x = CFRA;
				break;
		}

		SEQP_BEGIN (ed, seq)
		{
			if (((x <  CFRA) && (seq->enddisp   <= CFRA)) ||
			    ((x >= CFRA) && (seq->startdisp >= CFRA)))
			{
				seq->flag |= SELECT;
				recurs_sel_seq(seq);
			}
		}
		SEQ_END
		{
			SpaceSeq *sseq = CTX_wm_space_seq(C);
			if (sseq && sseq->flag & SEQ_MARKER_TRANS) {
				TimeMarker *tmarker;

				for (tmarker = scene->markers.first; tmarker; tmarker = tmarker->next) {
					if (((x <  CFRA) && (tmarker->frame <= CFRA)) ||
					    ((x >= CFRA) && (tmarker->frame >= CFRA)))
					{
						tmarker->flag |= SELECT;
					}
					else {
						tmarker->flag &= ~SELECT;
					}
				}
			}
		}
	}
	else {
		// seq = find_nearest_seq(scene, v2d, &hand, mval);

		act_orig = ed->act_seq;

		if (extend == 0 && linked_handle == 0)
			ED_sequencer_deselect_all(scene);

		if (seq) {
			BKE_sequencer_active_set(scene, seq);

			if ((seq->type == SEQ_TYPE_IMAGE) || (seq->type == SEQ_TYPE_MOVIE)) {
				if (seq->strip) {
					BLI_strncpy(ed->act_imagedir, seq->strip->dir, FILE_MAXDIR);
				}
			}
			else if (seq->type == SEQ_TYPE_SOUND_RAM) {
				if (seq->strip) {
					BLI_strncpy(ed->act_sounddir, seq->strip->dir, FILE_MAXDIR);
				}
			}

			/* On Alt selection, select the strip and bordering handles */
			if (linked_handle) {
				if (!ELEM(hand, SEQ_SIDE_LEFT, SEQ_SIDE_RIGHT)) {
					/* First click selects the strip and its adjacent handles (if valid).
					 * Second click selects the strip, both of its handles and its adjacent handles (if valid).
					 */
					const bool is_striponly_selected = ((seq->flag & SEQ_ALLSEL) == SELECT);

					if (!extend) {
						ED_sequencer_deselect_all(scene);
					}
					seq->flag &= ~SEQ_ALLSEL;
					seq->flag |= is_striponly_selected ? SEQ_ALLSEL : SELECT;
					select_surrounding_handles(scene, seq);
				}
				else {
					/* always select the strip under the cursor */
					seq->flag |= SELECT;

					/* First click selects adjacent handles on that side.
					 * Second click selects all strips in that direction.
					 * If there are no adjacent strips, it just selects all in that direction.
					 */
					sel_side = hand;
					neighbor = find_neighboring_sequence(scene, seq, sel_side, -1);
					if (neighbor) {
						switch (sel_side) {
							case SEQ_SIDE_LEFT:
								if ((seq->flag & SEQ_LEFTSEL) && (neighbor->flag & SEQ_RIGHTSEL)) {
									if (extend == 0) ED_sequencer_deselect_all(scene);
									seq->flag |= SELECT;

									select_active_side(ed->seqbasep, SEQ_SIDE_LEFT, seq->machine, seq->startdisp);
								}
								else {
									if (extend == 0) ED_sequencer_deselect_all(scene);
									seq->flag |= SELECT;

									neighbor->flag |= SELECT;
									recurs_sel_seq(neighbor);
									neighbor->flag |= SEQ_RIGHTSEL;
									seq->flag |= SEQ_LEFTSEL;
								}
								break;
							case SEQ_SIDE_RIGHT:
								if ((seq->flag & SEQ_RIGHTSEL) && (neighbor->flag & SEQ_LEFTSEL)) {
									if (extend == 0) ED_sequencer_deselect_all(scene);
									seq->flag |= SELECT;

									select_active_side(ed->seqbasep, SEQ_SIDE_RIGHT, seq->machine, seq->startdisp);
								}
								else {
									if (extend == 0) ED_sequencer_deselect_all(scene);
									seq->flag |= SELECT;

									neighbor->flag |= SELECT;
									recurs_sel_seq(neighbor);
									neighbor->flag |= SEQ_LEFTSEL;
									seq->flag |= SEQ_RIGHTSEL;
								}
								break;
						}
					}
					else {
						if (extend == 0) ED_sequencer_deselect_all(scene);
						select_active_side(ed->seqbasep, sel_side, seq->machine, seq->startdisp);
					}
				}
			}
			else {
				if (extend && (seq->flag & SELECT) && ed->act_seq == act_orig) {
					switch (hand) {
						case SEQ_SIDE_NONE:
							if (linked_handle == 0)
								seq->flag &= ~SEQ_ALLSEL;
							break;
						case SEQ_SIDE_LEFT:
							seq->flag ^= SEQ_LEFTSEL;
							break;
						case SEQ_SIDE_RIGHT:
							seq->flag ^= SEQ_RIGHTSEL;
							break;
					}
				}
				else {
					seq->flag |= SELECT;
					if (hand == SEQ_SIDE_LEFT) seq->flag |= SEQ_LEFTSEL;
					if (hand == SEQ_SIDE_RIGHT) seq->flag |= SEQ_RIGHTSEL;
				}
			}

			recurs_sel_seq(seq);

			if (linked_time) {
				select_linked_time(ed->seqbasep, seq);
			}
		}
	}

	/* marker transform */
#if 0 // XXX probably need to redo this differently for 2.5
	if (marker) {
		int mval[2], xo, yo;
//		getmouseco_areawin(mval);
		xo = mval[0];
		yo = mval[1];

		while (get_mbut()) {
//			getmouseco_areawin(mval);
			if (abs(mval[0] - xo) + abs(mval[1] - yo) > 4) {
				transform_markers('g', 0);
				return;
			}
		}
	}
#endif

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

	/* allowing tweaks */
	return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
}

void SEQUENCER_OT_select(wmOperatorType *ot)
{
	static const EnumPropertyItem sequencer_select_left_right_types[] = {
		{SEQ_SELECT_LR_NONE, "NONE", 0, "None", "Don't do left-right selection"},
		{SEQ_SELECT_LR_MOUSE, "MOUSE", 0, "Mouse", "Use mouse position for selection"},
		{SEQ_SELECT_LR_LEFT, "LEFT", 0, "Left", "Select left"},
		{SEQ_SELECT_LR_RIGHT, "RIGHT", 0, "Right", "Select right"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Select";
	ot->idname = "SEQUENCER_OT_select";
	ot->description = "Select a strip (last selected becomes the \"active strip\")";

	/* api callbacks */
	ot->invoke = sequencer_select_invoke;
	ot->poll = ED_operator_sequencer_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the selection");
	RNA_def_boolean(ot->srna, "linked_handle", 0, "Linked Handle", "Select handles next to the active strip");
	/* for animation this is an enum but atm having an enum isn't useful for us */
	RNA_def_enum(ot->srna, "left_right", sequencer_select_left_right_types, 0, "Left/Right", "Select based on the current frame side the cursor is on");
	RNA_def_boolean(ot->srna, "linked_time", 0, "Linked Time", "Select other strips at the same time");
}


/* run recursively to select linked */
static bool select_more_less_seq__internal(Scene *scene, bool sel, const bool linked)
{
	Editing *ed = BKE_sequencer_editing_get(scene, false);
	Sequence *seq, *neighbor;
	bool changed = false;
	int isel;

	if (ed == NULL)
		return changed;

	if (sel) {
		sel = SELECT;
		isel = 0;
	}
	else {
		sel = 0;
		isel = SELECT;
	}

	if (!linked) {
		/* if not linked we only want to touch each seq once, newseq */
		for (seq = ed->seqbasep->first; seq; seq = seq->next) {
			seq->tmp = NULL;
		}
	}

	for (seq = ed->seqbasep->first; seq; seq = seq->next) {
		if ((seq->flag & SELECT) == sel) {
			if (linked || (seq->tmp == NULL)) {
				/* only get unselected neighbors */
				neighbor = find_neighboring_sequence(scene, seq, SEQ_SIDE_LEFT, isel);
				if (neighbor) {
					if (sel) {
						neighbor->flag |= SELECT;
						recurs_sel_seq(neighbor);
					}
					else {
						neighbor->flag &= ~SELECT;
					}
					if (!linked) {
						neighbor->tmp = (Sequence *)1;
					}
					changed = true;
				}
				neighbor = find_neighboring_sequence(scene, seq, SEQ_SIDE_RIGHT, isel);
				if (neighbor) {
					if (sel) {
						neighbor->flag |= SELECT;
						recurs_sel_seq(neighbor);
					}
					else {
						neighbor->flag &= ~SELECT;
					}
					if (!linked) {
						neighbor->tmp = (Sequence *)1;
					}
					changed = true;
				}
			}
		}
	}

	return changed;
}



/* select more operator */
static int sequencer_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);

	if (!select_more_less_seq__internal(scene, true, false))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select More";
	ot->idname = "SEQUENCER_OT_select_more";
	ot->description = "Select more strips adjacent to the current selection";

	/* api callbacks */
	ot->exec = sequencer_select_more_exec;
	ot->poll = sequencer_edit_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
}


/* select less operator */
static int sequencer_select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);

	if (!select_more_less_seq__internal(scene, false, false))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Less";
	ot->idname = "SEQUENCER_OT_select_less";
	ot->description = "Shrink the current selection of adjacent selected strips";

	/* api callbacks */
	ot->exec = sequencer_select_less_exec;
	ot->poll = sequencer_edit_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
}


/* select pick linked operator (uses the mouse) */
static int sequencer_select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Scene *scene = CTX_data_scene(C);
	View2D *v2d = UI_view2d_fromcontext(C);

	bool extend = RNA_boolean_get(op->ptr, "extend");

	Sequence *mouse_seq;
	int selected, hand;

	/* this works like UV, not mesh */
	mouse_seq = find_nearest_seq(scene, v2d, &hand, event->mval);
	if (!mouse_seq)
		return OPERATOR_FINISHED;  /* user error as with mesh?? */

	if (extend == 0)
		ED_sequencer_deselect_all(scene);

	mouse_seq->flag |= SELECT;
	recurs_sel_seq(mouse_seq);

	selected = 1;
	while (selected) {
		selected = select_more_less_seq__internal(scene, 1, 1);
	}

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_linked_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Pick Linked";
	ot->idname = "SEQUENCER_OT_select_linked_pick";
	ot->description = "Select a chain of linked strips nearest to the mouse pointer";

	/* api callbacks */
	ot->invoke = sequencer_select_linked_pick_invoke;
	ot->poll = ED_operator_sequencer_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the selection");
}


/* select linked operator */
static int sequencer_select_linked_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	bool selected;

	selected = true;
	while (selected) {
		selected = select_more_less_seq__internal(scene, true, true);
	}

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked";
	ot->idname = "SEQUENCER_OT_select_linked";
	ot->description = "Select all strips adjacent to the current selection";

	/* api callbacks */
	ot->exec = sequencer_select_linked_exec;
	ot->poll = sequencer_edit_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
}


/* select handles operator */
static int sequencer_select_handles_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Editing *ed = BKE_sequencer_editing_get(scene, false);
	Sequence *seq;
	int sel_side = RNA_enum_get(op->ptr, "side");


	for (seq = ed->seqbasep->first; seq; seq = seq->next) {
		if (seq->flag & SELECT) {
			switch (sel_side) {
				case SEQ_SIDE_LEFT:
					seq->flag &= ~SEQ_RIGHTSEL;
					seq->flag |= SEQ_LEFTSEL;
					break;
				case SEQ_SIDE_RIGHT:
					seq->flag &= ~SEQ_LEFTSEL;
					seq->flag |= SEQ_RIGHTSEL;
					break;
				case SEQ_SIDE_BOTH:
					seq->flag |= SEQ_LEFTSEL | SEQ_RIGHTSEL;
					break;
			}
		}
	}

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_handles(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Handles";
	ot->idname = "SEQUENCER_OT_select_handles";
	ot->description = "Select gizmo handles on the sides of the selected strip";

	/* api callbacks */
	ot->exec = sequencer_select_handles_exec;
	ot->poll = sequencer_edit_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "side", prop_side_types, SEQ_SIDE_BOTH, "Side", "The side of the handle that is selected");
}

/* select side operator */
static int sequencer_select_active_side_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Editing *ed = BKE_sequencer_editing_get(scene, false);
	Sequence *seq_act = BKE_sequencer_active_get(scene);

	if (ed == NULL || seq_act == NULL)
		return OPERATOR_CANCELLED;

	seq_act->flag |= SELECT;

	select_active_side(ed->seqbasep, RNA_enum_get(op->ptr, "side"), seq_act->machine, seq_act->startdisp);

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_active_side(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Active Side";
	ot->idname = "SEQUENCER_OT_select_active_side";
	ot->description = "Select strips on the nominated side of the active strip";

	/* api callbacks */
	ot->exec = sequencer_select_active_side_exec;
	ot->poll = sequencer_edit_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "side", prop_side_types, SEQ_SIDE_BOTH, "Side", "The side of the handle that is selected");
}


/* borderselect operator */
static int sequencer_borderselect_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Editing *ed = BKE_sequencer_editing_get(scene, false);
	View2D *v2d = UI_view2d_fromcontext(C);

	Sequence *seq;
	rctf rectf, rq;
	const bool select = !RNA_boolean_get(op->ptr, "deselect");
	const bool extend = RNA_boolean_get(op->ptr, "extend");

	if (ed == NULL)
		return OPERATOR_CANCELLED;

	WM_operator_properties_border_to_rctf(op, &rectf);
	UI_view2d_region_to_view_rctf(v2d, &rectf, &rectf);

	for (seq = ed->seqbasep->first; seq; seq = seq->next) {
		seq_rectf(seq, &rq);

		if (BLI_rctf_isect(&rq, &rectf, NULL)) {
			if (select) seq->flag |= SELECT;
			else seq->flag &= ~SEQ_ALLSEL;
			recurs_sel_seq(seq);
		}
		else if (!extend) {
			seq->flag &= ~SEQ_ALLSEL;
			recurs_sel_seq(seq);
		}
	}

	WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

	return OPERATOR_FINISHED;
}


/* ****** Border Select ****** */
void SEQUENCER_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Border Select";
	ot->idname = "SEQUENCER_OT_select_border";
	ot->description = "Select strips using border selection";

	/* api callbacks */
	ot->invoke = WM_gesture_border_invoke;
	ot->exec = sequencer_borderselect_exec;
	ot->modal = WM_gesture_border_modal;
	ot->cancel = WM_gesture_border_cancel;

	ot->poll = ED_operator_sequencer_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* rna */
	WM_operator_properties_gesture_border_select(ot);
}

/* ****** Selected Grouped ****** */

enum {
	SEQ_SELECT_GROUP_TYPE,
	SEQ_SELECT_GROUP_TYPE_BASIC,
	SEQ_SELECT_GROUP_TYPE_EFFECT,
	SEQ_SELECT_GROUP_DATA,
	SEQ_SELECT_GROUP_EFFECT,
	SEQ_SELECT_GROUP_EFFECT_LINK,
	SEQ_SELECT_GROUP_OVERLAP,
};

static const EnumPropertyItem sequencer_prop_select_grouped_types[] = {
	{SEQ_SELECT_GROUP_TYPE, "TYPE", 0, "Type", "Shared strip type"},
	{SEQ_SELECT_GROUP_TYPE_BASIC, "TYPE_BASIC", 0, "Global Type", "All strips of same basic type (Graphical or Sound)"},
	{SEQ_SELECT_GROUP_TYPE_EFFECT, "TYPE_EFFECT", 0, "Effect Type",
	 "Shared strip effect type (if active strip is not an effect one, select all non-effect strips)"},
	{SEQ_SELECT_GROUP_DATA, "DATA", 0, "Data", "Shared data (scene, image, sound, etc.)"},
	{SEQ_SELECT_GROUP_EFFECT, "EFFECT", 0, "Effect", "Shared effects"},
	{SEQ_SELECT_GROUP_EFFECT_LINK, "EFFECT_LINK", 0, "Effect/Linked",
	 "Other strips affected by the active one (sharing some time, and below or effect-assigned)"},
	{SEQ_SELECT_GROUP_OVERLAP, "OVERLAP", 0, "Overlap", "Overlapping time"},
	{0, NULL, 0, NULL, NULL}
};

#define SEQ_IS_SOUND(_seq) ((_seq->type & SEQ_TYPE_SOUND_RAM) && !(_seq->type & SEQ_TYPE_EFFECT))

#define SEQ_IS_EFFECT(_seq) ((_seq->type & SEQ_TYPE_EFFECT) != 0)

#define SEQ_USE_DATA(_seq) (ELEM(_seq->type, SEQ_TYPE_SCENE, SEQ_TYPE_MOVIECLIP, SEQ_TYPE_MASK) || SEQ_HAS_PATH(_seq))

#define SEQ_CHANNEL_CHECK(_seq, _chan) (ELEM((_chan), 0, (_seq)->machine))

static bool select_grouped_type(Editing *ed, Sequence *actseq, const int channel)
{
	Sequence *seq;
	bool changed = false;

	SEQP_BEGIN (ed, seq)
	{
		if (SEQ_CHANNEL_CHECK(seq, channel) && seq->type == actseq->type) {
			seq->flag |= SELECT;
			changed = true;
		}
	}
	SEQ_END;

	return changed;
}

static bool select_grouped_type_basic(Editing *ed, Sequence *actseq, const int channel)
{
	Sequence *seq;
	bool changed = false;
	const bool is_sound = SEQ_IS_SOUND(actseq);

	SEQP_BEGIN (ed, seq)
	{
		if (SEQ_CHANNEL_CHECK(seq, channel) && (is_sound ? SEQ_IS_SOUND(seq) : !SEQ_IS_SOUND(seq))) {
			seq->flag |= SELECT;
			changed = true;
		}
	}
	SEQ_END;

	return changed;
}

static bool select_grouped_type_effect(Editing *ed, Sequence *actseq, const int channel)
{
	Sequence *seq;
	bool changed = false;
	const bool is_effect = SEQ_IS_EFFECT(actseq);

	SEQP_BEGIN (ed, seq)
	{
		if (SEQ_CHANNEL_CHECK(seq, channel) && (is_effect ? SEQ_IS_EFFECT(seq) : !SEQ_IS_EFFECT(seq))) {
			seq->flag |= SELECT;
			changed = true;
		}
	}
	SEQ_END;

	return changed;
}

static bool select_grouped_data(Editing *ed, Sequence *actseq, const int channel)
{
	Sequence *seq;
	bool changed = false;
	const char *dir = actseq->strip ? actseq->strip->dir : NULL;

	if (!SEQ_USE_DATA(actseq))
		return changed;

	if (SEQ_HAS_PATH(actseq) && dir) {
		SEQP_BEGIN (ed, seq)
		{
			if (SEQ_CHANNEL_CHECK(seq, channel) && SEQ_HAS_PATH(seq) && seq->strip && STREQ(seq->strip->dir, dir)) {
				seq->flag |= SELECT;
				changed = true;
			}
		}
		SEQ_END;
	}
	else if (actseq->type == SEQ_TYPE_SCENE) {
		Scene *sce = actseq->scene;
		SEQP_BEGIN (ed, seq)
		{
			if (SEQ_CHANNEL_CHECK(seq, channel) && seq->type == SEQ_TYPE_SCENE && seq->scene == sce) {
				seq->flag |= SELECT;
				changed = true;
			}
		}
		SEQ_END;
	}
	else if (actseq->type == SEQ_TYPE_MOVIECLIP) {
		MovieClip *clip = actseq->clip;
		SEQP_BEGIN (ed, seq)
		{
			if (SEQ_CHANNEL_CHECK(seq, channel) && seq->type == SEQ_TYPE_MOVIECLIP && seq->clip == clip) {
				seq->flag |= SELECT;
				changed = true;
			}
		}
		SEQ_END;
	}
	else if (actseq->type == SEQ_TYPE_MASK) {
		struct Mask *mask = actseq->mask;
		SEQP_BEGIN (ed, seq)
		{
			if (SEQ_CHANNEL_CHECK(seq, channel) && seq->type == SEQ_TYPE_MASK && seq->mask == mask) {
				seq->flag |= SELECT;
				changed = true;
			}
		}
		SEQ_END;
	}

	return changed;
}

static bool select_grouped_effect(Editing *ed, Sequence *actseq, const int channel)
{
	Sequence *seq;
	bool changed = false;
	bool effects[SEQ_TYPE_MAX + 1];
	int i;

	for (i = 0; i <= SEQ_TYPE_MAX; i++)
		effects[i] = false;

	SEQP_BEGIN (ed, seq)
	{
		if (SEQ_CHANNEL_CHECK(seq, channel) && (seq->type & SEQ_TYPE_EFFECT) &&
		    ELEM(actseq, seq->seq1, seq->seq2, seq->seq3))
		{
			effects[seq->type] = true;
		}
	}
	SEQ_END;

	SEQP_BEGIN (ed, seq)
	{
		if (SEQ_CHANNEL_CHECK(seq, channel) && effects[seq->type]) {
			if (seq->seq1) seq->seq1->flag |= SELECT;
			if (seq->seq2) seq->seq2->flag |= SELECT;
			if (seq->seq3) seq->seq3->flag |= SELECT;
			changed = true;
		}
	}
	SEQ_END;

	return changed;
}

static bool select_grouped_time_overlap(Editing *ed, Sequence *actseq)
{
	Sequence *seq;
	bool changed = false;

	SEQP_BEGIN (ed, seq)
	{
		if (!((seq->startdisp >= actseq->enddisp) || (seq->enddisp < actseq->startdisp))) {
			seq->flag |= SELECT;
			changed = true;
		}
	}
	SEQ_END;

	return changed;
}

static bool select_grouped_effect_link(Editing *ed, Sequence *actseq, const int channel)
{
	Sequence *seq = NULL;
	bool changed = false;
	const bool is_audio = ((actseq->type == SEQ_TYPE_META) || SEQ_IS_SOUND(actseq));
	int startdisp = actseq->startdisp;
	int enddisp   = actseq->enddisp;
	int machine   = actseq->machine;
	SeqIterator iter;

	SEQP_BEGIN (ed, seq)
	{
		seq->tmp = NULL;
	}
	SEQ_END;

	actseq->tmp = SET_INT_IN_POINTER(true);

	for (BKE_sequence_iterator_begin(ed, &iter, true); iter.valid; BKE_sequence_iterator_next(&iter)) {
		seq = iter.seq;

		/* Ignore all seqs already selected! */
		/* Ignore all seqs not sharing some time with active one. */
		/* Ignore all seqs of incompatible types (audio vs video). */
		if (!SEQ_CHANNEL_CHECK(seq, channel) ||
		    (seq->flag & SELECT) || (seq->startdisp >= enddisp) || (seq->enddisp < startdisp) ||
		    (!is_audio && SEQ_IS_SOUND(seq)) ||
		    (is_audio && !((seq->type == SEQ_TYPE_META) || SEQ_IS_SOUND(seq))))
		{
			continue;
		}

		/* If the seq is an effect one, we need extra cheking! */
		if (SEQ_IS_EFFECT(seq) && ((seq->seq1 && seq->seq1->tmp) ||
		                           (seq->seq2 && seq->seq2->tmp) ||
		                           (seq->seq3 && seq->seq3->tmp)))
		{
			if (startdisp > seq->startdisp) startdisp = seq->startdisp;
			if (enddisp < seq->enddisp) enddisp = seq->enddisp;
			if (machine < seq->machine) machine = seq->machine;

			seq->tmp = SET_INT_IN_POINTER(true);

			seq->flag |= SELECT;
			changed = true;

			/* Unfortunately, we must restart checks from the beginning. */
			BKE_sequence_iterator_end(&iter);
			BKE_sequence_iterator_begin(ed, &iter, true);
		}

		/* Video strips bellow active one, or any strip for audio (order do no matters here!). */
		else if (seq->machine < machine || is_audio) {
			seq->flag |= SELECT;
			changed = true;
		}
	}
	BKE_sequence_iterator_end(&iter);

	return changed;
}

#undef SEQ_IS_SOUND
#undef SEQ_IS_EFFECT
#undef SEQ_USE_DATA

static int sequencer_select_grouped_exec(bContext *C, wmOperator *op)
{
	Scene *scene  = CTX_data_scene(C);
	Editing *ed   = BKE_sequencer_editing_get(scene, false);
	Sequence *seq, *actseq = BKE_sequencer_active_get(scene);

	const int type = RNA_enum_get(op->ptr, "type");
	const int channel = RNA_boolean_get(op->ptr, "use_active_channel") ? actseq->machine : 0;
	const bool extend = RNA_boolean_get(op->ptr, "extend");

	bool changed = false;

	if (actseq == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active sequence!");
		return OPERATOR_CANCELLED;
	}

	if (!extend) {
		SEQP_BEGIN (ed, seq)
		{
			seq->flag &= ~SELECT;
			changed = true;
		}
		SEQ_END;
	}

	switch (type) {
		case SEQ_SELECT_GROUP_TYPE:
			changed |= select_grouped_type(ed, actseq, channel);
			break;
		case SEQ_SELECT_GROUP_TYPE_BASIC:
			changed |= select_grouped_type_basic(ed, actseq, channel);
			break;
		case SEQ_SELECT_GROUP_TYPE_EFFECT:
			changed |= select_grouped_type_effect(ed, actseq, channel);
			break;
		case SEQ_SELECT_GROUP_DATA:
			changed |= select_grouped_data(ed, actseq, channel);
			break;
		case SEQ_SELECT_GROUP_EFFECT:
			changed |= select_grouped_effect(ed, actseq, channel);
			break;
		case SEQ_SELECT_GROUP_EFFECT_LINK:
			changed |= select_grouped_effect_link(ed, actseq, channel);
			break;
		case SEQ_SELECT_GROUP_OVERLAP:
			changed |= select_grouped_time_overlap(ed, actseq);
			break;
		default:
			BLI_assert(0);
			break;
	}

	if (changed) {
		WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void SEQUENCER_OT_select_grouped(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Grouped";
	ot->description = "Select all strips grouped by various properties";
	ot->idname = "SEQUENCER_OT_select_grouped";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = sequencer_select_grouped_exec;
	ot->poll = sequencer_edit_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", sequencer_prop_select_grouped_types, 0, "Type", "");
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection instead of deselecting everything first");
	RNA_def_boolean(ot->srna, "use_active_channel", false, "Same Channel",
	                "Only consider strips on the same channel as the active one");
}
