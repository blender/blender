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
 * The Original Code is: all of this file.
 
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/metaball/mball_edit.c
 *  \ingroup edmeta
 */


#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "DNA_defs.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_define.h"
#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "BKE_depsgraph.h"
#include "BKE_context.h"
#include "BKE_mball.h"

#include "ED_mball.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "mball_intern.h"

/* This function is used to free all MetaElems from MetaBall */
void free_editMball(Object *obedit)
{
	MetaBall *mb = (MetaBall *)obedit->data;

	mb->editelems = NULL;
	mb->lastelem = NULL;
}

/* This function is called, when MetaBall Object is
 * switched from object mode to edit mode */
void make_editMball(Object *obedit)
{
	MetaBall *mb = (MetaBall *)obedit->data;
	MetaElem *ml; /*, *newml;*/

	ml = mb->elems.first;
	
	while (ml) {
		if (ml->flag & SELECT) mb->lastelem = ml;
		ml = ml->next;
	}

	mb->editelems = &mb->elems;
}

/* This function is called, when MetaBall Object switched from
 * edit mode to object mode. List of MetaElements is copied
 * from object->data->edit_elems to object->data->elems. */
void load_editMball(Object *UNUSED(obedit))
{
}

/* Add metaelem primitive to metaball object (which is in edit mode) */
MetaElem *add_metaball_primitive(bContext *UNUSED(C), Object *obedit, float mat[4][4], float dia, int type)
{
	MetaBall *mball = (MetaBall *)obedit->data;
	MetaElem *ml;

	/* Deselect all existing metaelems */
	ml = mball->editelems->first;
	while (ml) {
		ml->flag &= ~SELECT;
		ml = ml->next;
	}
	
	ml = BKE_mball_element_add(mball, type);
	ml->rad *= dia;
	mball->wiresize *= dia;
	mball->rendersize *= dia;
	copy_v3_v3(&ml->x, mat[3]);

	ml->flag |= SELECT;
	mball->lastelem = ml;
	return ml;
}

/***************************** Select/Deselect operator *****************************/

/* Select or deselect all MetaElements */
static int mball_select_all_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	MetaBall *mb = (MetaBall *)obedit->data;
	MetaElem *ml;
	int action = RNA_enum_get(op->ptr, "action");

	if (BLI_listbase_is_empty(mb->editelems))
		return OPERATOR_CANCELLED;

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		for (ml = mb->editelems->first; ml; ml = ml->next) {
			if (ml->flag & SELECT) {
				action = SEL_DESELECT;
				break;
			}
		}
	}

	switch (action) {
		case SEL_SELECT:
			BKE_mball_select_all(mb);
			break;
		case SEL_DESELECT:
			BKE_mball_deselect_all(mb);
			break;
		case SEL_INVERT:
			BKE_mball_select_swap(mb);
			break;
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, mb);

	return OPERATOR_FINISHED;
}

void MBALL_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All";
	ot->description = "Change selection of all meta elements";
	ot->idname = "MBALL_OT_select_all";

	/* callback functions */
	ot->exec = mball_select_all_exec;
	ot->poll = ED_operator_editmball;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}


/* -------------------------------------------------------------------- */
/* Select Similar */

enum {
	SIMMBALL_TYPE = 1,
	SIMMBALL_RADIUS,
	SIMMBALL_STIFFNESS,
	SIMMBALL_ROTATION
};

static EnumPropertyItem prop_similar_types[] = {
	{SIMMBALL_TYPE, "TYPE", 0, "Type", ""},
	{SIMMBALL_RADIUS, "RADIUS", 0, "Radius", ""},
	{SIMMBALL_STIFFNESS, "STIFFNESS", 0, "Stiffness", ""},
	{SIMMBALL_ROTATION, "ROTATION", 0, "Rotation", ""},
	{0, NULL, 0, NULL, NULL}
};

static bool mball_select_similar_type(MetaBall *mb)
{
	MetaElem *ml;
	bool changed = false;

	for (ml = mb->editelems->first; ml; ml = ml->next) {
		if (ml->flag & SELECT) {
			MetaElem *ml_iter;

			for (ml_iter = mb->editelems->first; ml_iter; ml_iter = ml_iter->next) {
				if ((ml_iter->flag & SELECT) == 0) {
					if (ml->type == ml_iter->type) {
						ml_iter->flag |= SELECT;
						changed = true;
					}
				}
			}
		}
	}

	return changed;
}

static bool mball_select_similar_radius(MetaBall *mb, const float thresh)
{
	MetaElem *ml;
	bool changed = false;

	for (ml = mb->editelems->first; ml; ml = ml->next) {
		if (ml->flag & SELECT) {
			MetaElem *ml_iter;

			for (ml_iter = mb->editelems->first; ml_iter; ml_iter = ml_iter->next) {
				if ((ml_iter->flag & SELECT) == 0) {
					if (fabsf(ml_iter->rad - ml->rad) <= (thresh * ml->rad)) {
						ml_iter->flag |= SELECT;
						changed = true;
					}
				}
			}
		}
	}

	return changed;
}

static bool mball_select_similar_stiffness(MetaBall *mb, const float thresh)
{
	MetaElem *ml;
	bool changed = false;

	for (ml = mb->editelems->first; ml; ml = ml->next) {
		if (ml->flag & SELECT) {
			MetaElem *ml_iter;

			for (ml_iter = mb->editelems->first; ml_iter; ml_iter = ml_iter->next) {
				if ((ml_iter->flag & SELECT) == 0) {
					if (fabsf(ml_iter->s - ml->s) <= thresh) {
						ml_iter->flag |= SELECT;
						changed = true;
					}
				}
			}
		}
	}

	return changed;
}

static bool mball_select_similar_rotation(MetaBall *mb, const float thresh)
{
	const float thresh_rad = thresh * (float)M_PI_2;
	MetaElem *ml;
	bool changed = false;

	for (ml = mb->editelems->first; ml; ml = ml->next) {
		if (ml->flag & SELECT) {
			MetaElem *ml_iter;

			float ml_mat[3][3];

			unit_m3(ml_mat);
			mul_qt_v3(ml->quat, ml_mat[0]);
			mul_qt_v3(ml->quat, ml_mat[1]);
			mul_qt_v3(ml->quat, ml_mat[2]);
			normalize_m3(ml_mat);

			for (ml_iter = mb->editelems->first; ml_iter; ml_iter = ml_iter->next) {
				if ((ml_iter->flag & SELECT) == 0) {
					float ml_iter_mat[3][3];

					unit_m3(ml_iter_mat);
					mul_qt_v3(ml_iter->quat, ml_iter_mat[0]);
					mul_qt_v3(ml_iter->quat, ml_iter_mat[1]);
					mul_qt_v3(ml_iter->quat, ml_iter_mat[2]);
					normalize_m3(ml_iter_mat);

					if ((angle_normalized_v3v3(ml_mat[0], ml_iter_mat[0]) +
					     angle_normalized_v3v3(ml_mat[1], ml_iter_mat[1]) +
					     angle_normalized_v3v3(ml_mat[2], ml_iter_mat[2])) < thresh_rad)
					{
						ml_iter->flag |= SELECT;
						changed = true;
					}
				}
			}
		}
	}

	return changed;
}

static int mball_select_similar_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	MetaBall *mb = (MetaBall *)obedit->data;

	int type = RNA_enum_get(op->ptr, "type");
	float thresh = RNA_float_get(op->ptr, "threshold");
	bool changed = false;

	switch (type) {
		case SIMMBALL_TYPE:
			changed = mball_select_similar_type(mb);
			break;
		case SIMMBALL_RADIUS:
			changed = mball_select_similar_radius(mb, thresh);
			break;
		case SIMMBALL_STIFFNESS:
			changed = mball_select_similar_stiffness(mb, thresh);
			break;
		case SIMMBALL_ROTATION:
			changed = mball_select_similar_rotation(mb, thresh);
			break;
		default:
			BLI_assert(0);
			break;
	}

	if (changed) {
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, mb);
	}

	return OPERATOR_FINISHED;
}

void MBALL_OT_select_similar(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Similar";
	ot->idname = "MBALL_OT_select_similar";

	/* callback functions */
	ot->invoke = WM_menu_invoke;
	ot->exec = mball_select_similar_exec;
	ot->poll = ED_operator_editmball;
	ot->description = "Select similar metaballs by property types";

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_similar_types, 0, "Type", "");

	RNA_def_float(ot->srna, "threshold", 0.1, 0.0, 1.0, "Threshold", "", 0.01, 1.0);
}


/***************************** Select random operator *****************************/

/* Random metaball selection */
static int select_random_metaelems_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	MetaBall *mb = (MetaBall *)obedit->data;
	MetaElem *ml;
	const bool select = (RNA_enum_get(op->ptr, "action") == SEL_SELECT);
	float percent = RNA_float_get(op->ptr, "percent") / 100.0f;
	
	for (ml = mb->editelems->first; ml; ml = ml->next) {
		if (BLI_frand() < percent) {
			if (select)
				ml->flag |= SELECT;
			else
				ml->flag &= ~SELECT;
		}
	}
	
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, mb);
	
	return OPERATOR_FINISHED;
}


void MBALL_OT_select_random_metaelems(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Random";
	ot->description = "Randomly select metaelements";
	ot->idname = "MBALL_OT_select_random_metaelems";
	
	/* callback functions */
	ot->exec = select_random_metaelems_exec;
	ot->poll = ED_operator_editmball;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_float_percentage(ot->srna, "percent", 50.f, 0.0f, 100.0f, "Percent", "Percentage of elements to select randomly", 0.f, 100.0f);
	WM_operator_properties_select_action_simple(ot, SEL_SELECT);
}

/***************************** Duplicate operator *****************************/

/* Duplicate selected MetaElements */
static int duplicate_metaelems_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	MetaBall *mb = (MetaBall *)obedit->data;
	MetaElem *ml, *newml;
	
	ml = mb->editelems->last;
	if (ml) {
		while (ml) {
			if (ml->flag & SELECT) {
				newml = MEM_dupallocN(ml);
				BLI_addtail(mb->editelems, newml);
				mb->lastelem = newml;
				ml->flag &= ~SELECT;
			}
			ml = ml->prev;
		}
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, mb);
		DAG_id_tag_update(obedit->data, 0);
	}

	return OPERATOR_FINISHED;
}

void MBALL_OT_duplicate_metaelems(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Metaelements";
	ot->description = "Duplicate selected metaelement(s)";
	ot->idname = "MBALL_OT_duplicate_metaelems";

	/* callback functions */
	ot->exec = duplicate_metaelems_exec;
	ot->poll = ED_operator_editmball;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************************** Delete operator *****************************/

/* Delete all selected MetaElems (not MetaBall) */
static int delete_metaelems_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	MetaBall *mb = (MetaBall *)obedit->data;
	MetaElem *ml, *next;
	
	ml = mb->editelems->first;
	if (ml) {
		while (ml) {
			next = ml->next;
			if (ml->flag & SELECT) {
				if (mb->lastelem == ml) mb->lastelem = NULL;
				BLI_remlink(mb->editelems, ml);
				MEM_freeN(ml);
			}
			ml = next;
		}
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, mb);
		DAG_id_tag_update(obedit->data, 0);
	}

	return OPERATOR_FINISHED;
}

void MBALL_OT_delete_metaelems(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete";
	ot->description = "Delete selected metaelement(s)";
	ot->idname = "MBALL_OT_delete_metaelems";

	/* callback functions */
	ot->exec = delete_metaelems_exec;
	ot->poll = ED_operator_editmball;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************************** Hide operator *****************************/

/* Hide selected MetaElems */
static int hide_metaelems_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	MetaBall *mb = (MetaBall *)obedit->data;
	MetaElem *ml;
	const bool invert = RNA_boolean_get(op->ptr, "unselected") ? SELECT : 0;

	ml = mb->editelems->first;

	if (ml) {
		while (ml) {
			if ((ml->flag & SELECT) != invert)
				ml->flag |= MB_HIDE;
			ml = ml->next;
		}
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, mb);
		DAG_id_tag_update(obedit->data, 0);
	}

	return OPERATOR_FINISHED;
}

void MBALL_OT_hide_metaelems(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hide";
	ot->description = "Hide (un)selected metaelement(s)";
	ot->idname = "MBALL_OT_hide_metaelems";

	/* callback functions */
	ot->exec = hide_metaelems_exec;
	ot->poll = ED_operator_editmball;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected");
}

/***************************** Unhide operator *****************************/

/* Unhide all edited MetaElems */
static int reveal_metaelems_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	MetaBall *mb = (MetaBall *)obedit->data;
	MetaElem *ml;

	ml = mb->editelems->first;

	if (ml) {
		while (ml) {
			ml->flag &= ~MB_HIDE;
			ml = ml->next;
		}
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, mb);
		DAG_id_tag_update(obedit->data, 0);
	}
	
	return OPERATOR_FINISHED;
}

void MBALL_OT_reveal_metaelems(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reveal";
	ot->description = "Reveal all hidden metaelements";
	ot->idname = "MBALL_OT_reveal_metaelems";
	
	/* callback functions */
	ot->exec = reveal_metaelems_exec;
	ot->poll = ED_operator_editmball;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Select MetaElement with mouse click (user can select radius circle or
 * stiffness circle) */
bool mouse_mball(bContext *C, const int mval[2], bool extend, bool deselect, bool toggle)
{
	static MetaElem *startelem = NULL;
	Object *obedit = CTX_data_edit_object(C);
	ViewContext vc;
	MetaBall *mb = (MetaBall *)obedit->data;
	MetaElem *ml, *ml_act = NULL;
	int a, hits;
	unsigned int buffer[4 * MAXPICKBUF];
	rcti rect;

	view3d_set_viewcontext(C, &vc);

	rect.xmin = mval[0] - 12;
	rect.xmax = mval[0] + 12;
	rect.ymin = mval[1] - 12;
	rect.ymax = mval[1] + 12;

	hits = view3d_opengl_select(&vc, buffer, MAXPICKBUF, &rect, true);

	/* does startelem exist? */
	ml = mb->editelems->first;
	while (ml) {
		if (ml == startelem) break;
		ml = ml->next;
	}

	if (ml == NULL) startelem = mb->editelems->first;
	
	if (hits > 0) {
		ml = startelem;
		while (ml) {
			for (a = 0; a < hits; a++) {
				/* index converted for gl stuff */
				if (ml->selcol1 == buffer[4 * a + 3]) {
					ml->flag |= MB_SCALE_RAD;
					ml_act = ml;
				}
				if (ml->selcol2 == buffer[4 * a + 3]) {
					ml->flag &= ~MB_SCALE_RAD;
					ml_act = ml;
				}
			}
			if (ml_act) break;
			ml = ml->next;
			if (ml == NULL) ml = mb->editelems->first;
			if (ml == startelem) break;
		}
		
		/* When some metaelem was found, then it is necessary to select or
		 * deselect it. */
		if (ml_act) {
			if (extend) {
				ml_act->flag |= SELECT;
			}
			else if (deselect) {
				ml_act->flag &= ~SELECT;
			}
			else if (toggle) {
				if (ml_act->flag & SELECT)
					ml_act->flag &= ~SELECT;
				else
					ml_act->flag |= SELECT;
			}
			else {
				/* Deselect all existing metaelems */
				BKE_mball_deselect_all(mb);

				/* Select only metaelem clicked on */
				ml_act->flag |= SELECT;
			}
			
			mb->lastelem = ml_act;
			
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, mb);

			return true;
		}
	}

	return false;
}


/*  ************* undo for MetaBalls ************* */

/* free all MetaElems from ListBase */
static void freeMetaElemlist(ListBase *lb)
{
	MetaElem *ml;

	if (lb == NULL) return;

	while ((ml = BLI_pophead(lb))) {
		MEM_freeN(ml);
	}
}


static void undoMball_to_editMball(void *lbu, void *lbe, void *UNUSED(obe))
{
	ListBase *lb = lbu;
	ListBase *editelems = lbe;
	MetaElem *ml, *newml;
	
	freeMetaElemlist(editelems);

	/* copy 'undo' MetaElems to 'edit' MetaElems */
	ml = lb->first;
	while (ml) {
		newml = MEM_dupallocN(ml);
		BLI_addtail(editelems, newml);
		ml = ml->next;
	}
	
}

static void *editMball_to_undoMball(void *lbe, void *UNUSED(obe))
{
	ListBase *editelems = lbe;
	ListBase *lb;
	MetaElem *ml, *newml;

	/* allocate memory for undo ListBase */
	lb = MEM_callocN(sizeof(ListBase), "listbase undo");
	
	/* copy contents of current ListBase to the undo ListBase */
	ml = editelems->first;
	while (ml) {
		newml = MEM_dupallocN(ml);
		BLI_addtail(lb, newml);
		ml = ml->next;
	}
	
	return lb;
}

/* free undo ListBase of MetaElems */
static void free_undoMball(void *lbv)
{
	ListBase *lb = lbv;
	
	freeMetaElemlist(lb);
	MEM_freeN(lb);
}

static ListBase *metaball_get_editelems(Object *ob)
{
	if (ob && ob->type == OB_MBALL) {
		struct MetaBall *mb = (struct MetaBall *)ob->data;
		return mb->editelems;
	}
	return NULL;
}


static void *get_data(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	return metaball_get_editelems(obedit);
}

/* this is undo system for MetaBalls */
void undo_push_mball(bContext *C, const char *name)
{
	undo_editmode_push(C, name, get_data, free_undoMball, undoMball_to_editMball, editMball_to_undoMball, NULL);
}
