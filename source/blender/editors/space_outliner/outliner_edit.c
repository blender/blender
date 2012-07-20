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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_outliner/outliner_edit.c
 *  \ingroup spoutliner
 */

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_group_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_material.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_keyframing.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "outliner_intern.h"

/* ************************************************************** */
/* Unused Utilities */
// XXX: where to place these?

/* This is not used anywhere at the moment */
#if 0
/* return 1 when levels were opened */
static int outliner_open_back(SpaceOops *soops, TreeElement *te)
{
	TreeStoreElem *tselem;
	int retval = 0;
	
	for (te = te->parent; te; te = te->parent) {
		tselem = TREESTORE(te);
		if (tselem->flag & TSE_CLOSED) { 
			tselem->flag &= ~TSE_CLOSED;
			retval = 1;
		}
	}
	return retval;
}

static void outliner_open_reveal(SpaceOops *soops, ListBase *lb, TreeElement *teFind, int *found)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		/* check if this tree-element was the one we're seeking */
		if (te == teFind) {
			*found = 1;
			return;
		}
		
		/* try to see if sub-tree contains it then */
		outliner_open_reveal(soops, &te->subtree, teFind, found);
		if (*found) {
			tselem = TREESTORE(te);
			if (tselem->flag & TSE_CLOSED) 
				tselem->flag &= ~TSE_CLOSED;
			return;
		}
	}
}
#endif

/* ************************************************************** */
/* Click Activated */

/* Toggle Open/Closed ------------------------------------------- */

static int do_outliner_item_openclose(bContext *C, SpaceOops *soops, TreeElement *te, int all, const float mval[2])
{
	
	if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
		TreeStoreElem *tselem = TREESTORE(te);
		
		/* all below close/open? */
		if (all) {
			tselem->flag &= ~TSE_CLOSED;
			outliner_set_flag(soops, &te->subtree, TSE_CLOSED, !outliner_has_one_flag(soops, &te->subtree, TSE_CLOSED, 1));
		}
		else {
			if (tselem->flag & TSE_CLOSED) tselem->flag &= ~TSE_CLOSED;
			else tselem->flag |= TSE_CLOSED;
		}
		
		return 1;
	}
	
	for (te = te->subtree.first; te; te = te->next) {
		if (do_outliner_item_openclose(C, soops, te, all, mval)) 
			return 1;
	}
	return 0;
	
}

/* event can enterkey, then it opens/closes */
static int outliner_item_openclose(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te;
	float fmval[2];
	int all = RNA_boolean_get(op->ptr, "all");
	
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], fmval, fmval + 1);
	
	for (te = soops->tree.first; te; te = te->next) {
		if (do_outliner_item_openclose(C, soops, te, all, fmval)) 
			break;
	}

	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_item_openclose(wmOperatorType *ot)
{
	ot->name = "Open/Close Item";
	ot->idname = "OUTLINER_OT_item_openclose";
	ot->description = "Toggle whether item under cursor is enabled or closed";
	
	ot->invoke = outliner_item_openclose;
	
	ot->poll = ED_operator_outliner_active;
	
	RNA_def_boolean(ot->srna, "all", 1, "All", "Close or open all items");
}

/* Rename --------------------------------------------------- */

static void do_item_rename(ARegion *ar, TreeElement *te, TreeStoreElem *tselem, ReportList *reports)
{
	/* can't rename rna datablocks entries */
	if (ELEM3(tselem->type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
		/* do nothing */;
	}
	else if (ELEM10(tselem->type, TSE_ANIM_DATA, TSE_NLA, TSE_DEFGROUP_BASE, TSE_CONSTRAINT_BASE, TSE_MODIFIER_BASE,
	                TSE_SCRIPT_BASE, TSE_POSE_BASE, TSE_POSEGRP_BASE, TSE_R_LAYER_BASE, TSE_R_PASS))
	{
		BKE_report(reports, RPT_WARNING, "Cannot edit builtin name");
	}
	else if (ELEM3(tselem->type, TSE_SEQUENCE, TSE_SEQ_STRIP, TSE_SEQUENCE_DUP)) {
		BKE_report(reports, RPT_WARNING, "Cannot edit sequence name");
	}
	else if (tselem->id->lib) {
		// XXX						error_libdata();
	} 
	else if (te->idcode == ID_LI && te->parent) {
		BKE_report(reports, RPT_WARNING, "Cannot edit the path of an indirectly linked library");
	} 
	else {
		tselem->flag |= TSE_TEXTBUT;
		ED_region_tag_redraw(ar);
	}
}

void item_rename_cb(bContext *C, Scene *UNUSED(scene), TreeElement *te,
                    TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	ARegion *ar = CTX_wm_region(C);
	ReportList *reports = CTX_wm_reports(C); // XXX
	do_item_rename(ar, te, tselem, reports);
}

static int do_outliner_item_rename(bContext *C, ARegion *ar, SpaceOops *soops, TreeElement *te, const float mval[2])
{	
	ReportList *reports = CTX_wm_reports(C); // XXX
	
	if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
		TreeStoreElem *tselem = TREESTORE(te);
		
		/* name and first icon */
		if (mval[0] > te->xs + UI_UNIT_X && mval[0] < te->xend) {
			
			do_item_rename(ar, te, tselem, reports);
		}
		return 1;
	}
	
	for (te = te->subtree.first; te; te = te->next) {
		if (do_outliner_item_rename(C, ar, soops, te, mval)) return 1;
	}
	return 0;
}

static int outliner_item_rename(bContext *C, wmOperator *UNUSED(op), wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te;
	float fmval[2];
	
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], fmval, fmval + 1);
	
	for (te = soops->tree.first; te; te = te->next) {
		if (do_outliner_item_rename(C, ar, soops, te, fmval)) break;
	}
	
	return OPERATOR_FINISHED;
}


void OUTLINER_OT_item_rename(wmOperatorType *ot)
{
	ot->name = "Rename Item";
	ot->idname = "OUTLINER_OT_item_rename";
	ot->description = "Rename item under cursor";
	
	ot->invoke = outliner_item_rename;
	
	ot->poll = ED_operator_outliner_active;
}

/* ************************************************************** */
/* Setting Toggling Operators */

/* =============================================== */
/* Toggling Utilities (Exported) */

/* Apply Settings ------------------------------- */

static int outliner_count_levels(SpaceOops *soops, ListBase *lb, int curlevel)
{
	TreeElement *te;
	int level = curlevel, lev;
	
	for (te = lb->first; te; te = te->next) {
		
		lev = outliner_count_levels(soops, &te->subtree, curlevel + 1);
		if (lev > level) level = lev;
	}
	return level;
}

int outliner_has_one_flag(SpaceOops *soops, ListBase *lb, short flag, short curlevel)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	int level;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		if (tselem->flag & flag) return curlevel;
		
		level = outliner_has_one_flag(soops, &te->subtree, flag, curlevel + 1);
		if (level) return level;
	}
	return 0;
}

void outliner_set_flag(SpaceOops *soops, ListBase *lb, short flag, short set)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		if (set == 0) tselem->flag &= ~flag;
		else tselem->flag |= flag;
		outliner_set_flag(soops, &te->subtree, flag, set);
	}
}

/* Restriction Columns ------------------------------- */

/* same check needed for both object operation and restrict column button func
 * return 0 when in edit mode (cannot restrict view or select)
 * otherwise return 1 */
int common_restrict_check(bContext *C, Object *ob)
{
	/* Don't allow hide an object in edit mode,
	 * check the bug #22153 and #21609, #23977
	 */
	Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit == ob) {
		/* found object is hidden, reset */
		if (ob->restrictflag & OB_RESTRICT_VIEW)
			ob->restrictflag &= ~OB_RESTRICT_VIEW;
		/* found object is unselectable, reset */
		if (ob->restrictflag & OB_RESTRICT_SELECT)
			ob->restrictflag &= ~OB_RESTRICT_SELECT;
		return 0;
	}
	
	return 1;
}

/* =============================================== */
/* Restriction toggles */

/* Toggle Visibility ---------------------------------------- */

void object_toggle_visibility_cb(bContext *C, Scene *scene, TreeElement *te,
                                 TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	Base *base = (Base *)te->directdata;
	Object *ob = (Object *)tselem->id;
	
	/* add check for edit mode */
	if (!common_restrict_check(C, ob)) return;
	
	if (base || (base = BKE_scene_base_find(scene, ob))) {
		if ((base->object->restrictflag ^= OB_RESTRICT_VIEW)) {
			ED_base_object_select(base, BA_DESELECT);
		}
	}
}

void group_toggle_visibility_cb(bContext *UNUSED(C), Scene *scene, TreeElement *UNUSED(te),
                                TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	Group *group = (Group *)tselem->id;
	restrictbutton_gr_restrict_flag(scene, group, OB_RESTRICT_VIEW);
}

static int outliner_toggle_visibility_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	
	outliner_do_object_operation(C, scene, soops, &soops->tree, object_toggle_visibility_cb);
	
	WM_event_add_notifier(C, NC_SCENE | ND_OB_VISIBLE, scene);
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_visibility_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Visibility";
	ot->idname = "OUTLINER_OT_visibility_toggle";
	ot->description = "Toggle the visibility of selected items";
	
	/* callbacks */
	ot->exec = outliner_toggle_visibility_exec;
	ot->poll = ED_operator_outliner_active_no_editobject;
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Toggle Selectability ---------------------------------------- */

void object_toggle_selectability_cb(bContext *UNUSED(C), Scene *scene, TreeElement *te,
                                    TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	Base *base = (Base *)te->directdata;
	
	if (base == NULL) base = BKE_scene_base_find(scene, (Object *)tselem->id);
	if (base) {
		base->object->restrictflag ^= OB_RESTRICT_SELECT;
	}
}

void group_toggle_selectability_cb(bContext *UNUSED(C), Scene *scene, TreeElement *UNUSED(te),
                                   TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	Group *group = (Group *)tselem->id;
	restrictbutton_gr_restrict_flag(scene, group, OB_RESTRICT_SELECT);
}

static int outliner_toggle_selectability_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	
	outliner_do_object_operation(C, scene, soops, &soops->tree, object_toggle_selectability_cb);
	
	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_selectability_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Selectability";
	ot->idname = "OUTLINER_OT_selectability_toggle";
	ot->description = "Toggle the selectability";
	
	/* callbacks */
	ot->exec = outliner_toggle_selectability_exec;
	ot->poll = ED_operator_outliner_active_no_editobject;
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Toggle Renderability ---------------------------------------- */

void object_toggle_renderability_cb(bContext *UNUSED(C), Scene *scene, TreeElement *te,
                                    TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	Base *base = (Base *)te->directdata;
	
	if (base == NULL) base = BKE_scene_base_find(scene, (Object *)tselem->id);
	if (base) {
		base->object->restrictflag ^= OB_RESTRICT_RENDER;
	}
}

void group_toggle_renderability_cb(bContext *UNUSED(C), Scene *scene, TreeElement *UNUSED(te),
                                   TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	Group *group = (Group *)tselem->id;
	restrictbutton_gr_restrict_flag(scene, group, OB_RESTRICT_RENDER);
}

static int outliner_toggle_renderability_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	Scene *scene = CTX_data_scene(C);
	
	outliner_do_object_operation(C, scene, soops, &soops->tree, object_toggle_renderability_cb);
	
	WM_event_add_notifier(C, NC_SCENE | ND_OB_RENDER, scene);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_renderability_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Renderability";
	ot->idname = "OUTLINER_OT_renderability_toggle";
	ot->description = "Toggle the renderability of selected items";
	
	/* callbacks */
	ot->exec = outliner_toggle_renderability_exec;
	ot->poll = ED_operator_outliner_active;
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* =============================================== */
/* Outliner setting toggles */

/* Toggle Expanded (Outliner) ---------------------------------------- */

static int outliner_toggle_expanded_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	
	if (outliner_has_one_flag(soops, &soops->tree, TSE_CLOSED, 1))
		outliner_set_flag(soops, &soops->tree, TSE_CLOSED, 0);
	else 
		outliner_set_flag(soops, &soops->tree, TSE_CLOSED, 1);
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_expanded_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Expand/Collapse All";
	ot->idname = "OUTLINER_OT_expanded_toggle";
	ot->description = "Expand/Collapse all items";
	
	/* callbacks */
	ot->exec = outliner_toggle_expanded_exec;
	ot->poll = ED_operator_outliner_active;
	
	/* no undo or registry, UI option */
}

/* Toggle Selected (Outliner) ---------------------------------------- */

static int outliner_toggle_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	
	if (outliner_has_one_flag(soops, &soops->tree, TSE_SELECTED, 1))
		outliner_set_flag(soops, &soops->tree, TSE_SELECTED, 0);
	else 
		outliner_set_flag(soops, &soops->tree, TSE_SELECTED, 1);
	
	soops->storeflag |= SO_TREESTORE_REDRAW;
	
	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_selected_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Selected";
	ot->idname = "OUTLINER_OT_selected_toggle";
	ot->description = "Toggle the Outliner selection of items";
	
	/* callbacks */
	ot->exec = outliner_toggle_selected_exec;
	ot->poll = ED_operator_outliner_active;
	
	/* no undo or registry, UI option */
}

/* ************************************************************** */
/* Hotkey Only Operators */

/* Show Active --------------------------------------------------- */

static int outliner_show_active_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *so = CTX_wm_space_outliner(C);
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	View2D *v2d = &ar->v2d;
	
	TreeElement *te;
	int xdelta, ytop;
	
	// TODO: make this get this info from context instead...
	if (OBACT == NULL) 
		return OPERATOR_CANCELLED;
	
	te = outliner_find_id(so, &so->tree, (ID *)OBACT);
	if (te) {
		/* make te->ys center of view */
		ytop = (int)(te->ys + (v2d->mask.ymax - v2d->mask.ymin) / 2);
		if (ytop > 0) ytop = 0;
		
		v2d->cur.ymax = (float)ytop;
		v2d->cur.ymin = (float)(ytop - (v2d->mask.ymax - v2d->mask.ymin));
		
		/* make te->xs ==> te->xend center of view */
		xdelta = (int)(te->xs - v2d->cur.xmin);
		v2d->cur.xmin += xdelta;
		v2d->cur.xmax += xdelta;
		
		so->storeflag |= SO_TREESTORE_REDRAW;
	}
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_show_active(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Show Active";
	ot->idname = "OUTLINER_OT_show_active";
	ot->description = "Adjust the view so that the active Object is shown centered";
	
	/* callbacks */
	ot->exec = outliner_show_active_exec;
	ot->poll = ED_operator_outliner_active;
}

/* View Panning --------------------------------------------------- */

static int outliner_scroll_page_exec(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	int dy = ar->v2d.mask.ymax - ar->v2d.mask.ymin;
	int up = 0;
	
	if (RNA_boolean_get(op->ptr, "up"))
		up = 1;

	if (up == 0) dy = -dy;
	ar->v2d.cur.ymin += dy;
	ar->v2d.cur.ymax += dy;
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}


void OUTLINER_OT_scroll_page(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Scroll Page";
	ot->idname = "OUTLINER_OT_scroll_page";
	ot->description = "Scroll page up or down";
	
	/* callbacks */
	ot->exec = outliner_scroll_page_exec;
	ot->poll = ED_operator_outliner_active;
	
	/* properties */
	RNA_def_boolean(ot->srna, "up", 0, "Up", "Scroll up one page");
}

/* Search ------------------------------------------------------- */
// TODO: probably obsolete now with filtering?

#if 0

/* recursive helper for function below */
static void outliner_set_coordinates_element(SpaceOops *soops, TreeElement *te, int startx, int *starty)
{
	TreeStoreElem *tselem = TREESTORE(te);
	
	/* store coord and continue, we need coordinates for elements outside view too */
	te->xs = (float)startx;
	te->ys = (float)(*starty);
	*starty -= UI_UNIT_Y;
	
	if (TSELEM_OPEN(tselem, soops)) {
		TreeElement *ten;
		for (ten = te->subtree.first; ten; ten = ten->next) {
			outliner_set_coordinates_element(soops, ten, startx + UI_UNIT_X, starty);
		}
	}
	
}

/* to retrieve coordinates with redrawing the entire tree */
static void outliner_set_coordinates(ARegion *ar, SpaceOops *soops)
{
	TreeElement *te;
	int starty = (int)(ar->v2d.tot.ymax) - UI_UNIT_Y;
	int startx = 0;
	
	for (te = soops->tree.first; te; te = te->next) {
		outliner_set_coordinates_element(soops, te, startx, &starty);
	}
}

/* find next element that has this name */
static TreeElement *outliner_find_name(SpaceOops *soops, ListBase *lb, char *name, int flags,
                                       TreeElement *prev, int *prevFound)
{
	TreeElement *te, *tes;
	
	for (te = lb->first; te; te = te->next) {
		int found = outliner_filter_has_name(te, name, flags);
		
		if (found) {
			/* name is right, but is element the previous one? */
			if (prev) {
				if ((te != prev) && (*prevFound)) 
					return te;
				if (te == prev) {
					*prevFound = 1;
				}
			}
			else
				return te;
		}
		
		tes = outliner_find_name(soops, &te->subtree, name, flags, prev, prevFound);
		if (tes) return tes;
	}

	/* nothing valid found */
	return NULL;
}

static void outliner_find_panel(Scene *UNUSED(scene), ARegion *ar, SpaceOops *soops, int again, int flags) 
{
	ReportList *reports = NULL; // CTX_wm_reports(C);
	TreeElement *te = NULL;
	TreeElement *last_find;
	TreeStoreElem *tselem;
	int ytop, xdelta, prevFound = 0;
	char name[sizeof(soops->search_string)];
	
	/* get last found tree-element based on stored search_tse */
	last_find = outliner_find_tse(soops, &soops->search_tse);
	
	/* determine which type of search to do */
	if (again && last_find) {
		/* no popup panel - previous + user wanted to search for next after previous */		
		BLI_strncpy(name, soops->search_string, sizeof(name));
		flags = soops->search_flags;
		
		/* try to find matching element */
		te = outliner_find_name(soops, &soops->tree, name, flags, last_find, &prevFound);
		if (te == NULL) {
			/* no more matches after previous, start from beginning again */
			prevFound = 1;
			te = outliner_find_name(soops, &soops->tree, name, flags, last_find, &prevFound);
		}
	}
	else {
		/* pop up panel - no previous, or user didn't want search after previous */
		name[0] = '\0';
// XXX		if (sbutton(name, 0, sizeof(name)-1, "Find: ") && name[0]) {
//			te= outliner_find_name(soops, &soops->tree, name, flags, NULL, &prevFound);
//		}
//		else return; /* XXX RETURN! XXX */
	}

	/* do selection and reveal */
	if (te) {
		tselem = TREESTORE(te);
		if (tselem) {
			/* expand branches so that it will be visible, we need to get correct coordinates */
			if (outliner_open_back(soops, te))
				outliner_set_coordinates(ar, soops);
			
			/* deselect all visible, and select found element */
			outliner_set_flag(soops, &soops->tree, TSE_SELECTED, 0);
			tselem->flag |= TSE_SELECTED;
			
			/* make te->ys center of view */
			ytop = (int)(te->ys + (ar->v2d.mask.ymax - ar->v2d.mask.ymin) / 2);
			if (ytop > 0) ytop = 0;
			ar->v2d.cur.ymax = (float)ytop;
			ar->v2d.cur.ymin = (float)(ytop - (ar->v2d.mask.ymax - ar->v2d.mask.ymin));
			
			/* make te->xs ==> te->xend center of view */
			xdelta = (int)(te->xs - ar->v2d.cur.xmin);
			ar->v2d.cur.xmin += xdelta;
			ar->v2d.cur.xmax += xdelta;
			
			/* store selection */
			soops->search_tse = *tselem;
			
			BLI_strncpy(soops->search_string, name, sizeof(soops->search_string));
			soops->search_flags = flags;
			
			/* redraw */
			soops->storeflag |= SO_TREESTORE_REDRAW;
		}
	}
	else {
		/* no tree-element found */
		BKE_report(reports, RPT_WARNING, "Not found: %s", name);
	}
}
#endif

/* Show One Level ----------------------------------------------- */

/* helper function for Show/Hide one level operator */
static void outliner_openclose_level(SpaceOops *soops, ListBase *lb, int curlevel, int level, int open)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		
		if (open) {
			if (curlevel <= level) tselem->flag &= ~TSE_CLOSED;
		}
		else {
			if (curlevel >= level) tselem->flag |= TSE_CLOSED;
		}
		
		outliner_openclose_level(soops, &te->subtree, curlevel + 1, level, open);
	}
}

static int outliner_one_level_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	int add = RNA_boolean_get(op->ptr, "open");
	int level;
	
	level = outliner_has_one_flag(soops, &soops->tree, TSE_CLOSED, 1);
	if (add == 1) {
		if (level) outliner_openclose_level(soops, &soops->tree, 1, level, 1);
	}
	else {
		if (level == 0) level = outliner_count_levels(soops, &soops->tree, 0);
		if (level) outliner_openclose_level(soops, &soops->tree, 1, level - 1, 0);
	}
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_show_one_level(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Show/Hide One Level";
	ot->idname = "OUTLINER_OT_show_one_level";
	ot->description = "Expand/collapse all entries by one level";
	
	/* callbacks */
	ot->exec = outliner_one_level_exec;
	ot->poll = ED_operator_outliner_active;
	
	/* no undo or registry, UI option */
	
	/* properties */
	RNA_def_boolean(ot->srna, "open", 1, "Open", "Expand all entries one level deep");
}

/* Show Hierarchy ----------------------------------------------- */

/* helper function for tree_element_shwo_hierarchy() - recursively checks whether subtrees have any objects*/
static int subtree_has_objects(SpaceOops *soops, ListBase *lb)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		if (tselem->type == 0 && te->idcode == ID_OB) return 1;
		if (subtree_has_objects(soops, &te->subtree)) return 1;
	}
	return 0;
}

/* recursive helper function for Show Hierarchy operator */
static void tree_element_show_hierarchy(Scene *scene, SpaceOops *soops, ListBase *lb)
{
	TreeElement *te;
	TreeStoreElem *tselem;

	/* open all object elems, close others */
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		
		if (tselem->type == 0) {
			if (te->idcode == ID_SCE) {
				if (tselem->id != (ID *)scene) tselem->flag |= TSE_CLOSED;
				else tselem->flag &= ~TSE_CLOSED;
			}
			else if (te->idcode == ID_OB) {
				if (subtree_has_objects(soops, &te->subtree)) tselem->flag &= ~TSE_CLOSED;
				else tselem->flag |= TSE_CLOSED;
			}
		}
		else tselem->flag |= TSE_CLOSED;
		
		if (TSELEM_OPEN(tselem, soops)) tree_element_show_hierarchy(scene, soops, &te->subtree);
	}
}

/* show entire object level hierarchy */
static int outliner_show_hierarchy_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	
	/* recursively open/close levels */
	tree_element_show_hierarchy(scene, soops, &soops->tree);
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_show_hierarchy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Show Hierarchy";
	ot->idname = "OUTLINER_OT_show_hierarchy";
	ot->description = "Open all object entries and close all others";
	
	/* callbacks */
	ot->exec = outliner_show_hierarchy_exec;
	ot->poll = ED_operator_outliner_active; //  TODO: shouldn't be allowed in RNA views...
	
	/* no undo or registry, UI option */
}

/* ************************************************************** */
/* ANIMATO OPERATIONS */
/* KeyingSet and Driver Creation - Helper functions */

/* specialized poll callback for these operators to work in Datablocks view only */
static int ed_operator_outliner_datablocks_active(bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	if ((sa) && (sa->spacetype == SPACE_OUTLINER)) {
		SpaceOops *so = CTX_wm_space_outliner(C);
		return (so->outlinevis == SO_DATABLOCKS);
	}
	return 0;
}


/* Helper func to extract an RNA path from selected tree element 
 * NOTE: the caller must zero-out all values of the pointers that it passes here first, as
 * this function does not do that yet 
 */
static void tree_element_to_path(SpaceOops *soops, TreeElement *te, TreeStoreElem *tselem, 
                                 ID **id, char **path, int *array_index, short *flag, short *UNUSED(groupmode))
{
	ListBase hierarchy = {NULL, NULL};
	LinkData *ld;
	TreeElement *tem, *temnext, *temsub;
	TreeStoreElem *tse /* , *tsenext */ /* UNUSED */;
	PointerRNA *ptr, *nextptr;
	PropertyRNA *prop;
	char *newpath = NULL;
	
	/* optimize tricks:
	 *	- Don't do anything if the selected item is a 'struct', but arrays are allowed
	 */
	if (tselem->type == TSE_RNA_STRUCT)
		return;
	
	/* Overview of Algorithm:
	 *  1. Go up the chain of parents until we find the 'root', taking note of the
	 *	   levels encountered in reverse-order (i.e. items are added to the start of the list
	 *      for more convenient looping later)
	 *  2. Walk down the chain, adding from the first ID encountered
	 *	   (which will become the 'ID' for the KeyingSet Path), and build a  
	 *      path as we step through the chain
	 */
	 
	/* step 1: flatten out hierarchy of parents into a flat chain */
	for (tem = te->parent; tem; tem = tem->parent) {
		ld = MEM_callocN(sizeof(LinkData), "LinkData for tree_element_to_path()");
		ld->data = tem;
		BLI_addhead(&hierarchy, ld);
	}
	
	/* step 2: step down hierarchy building the path
	 * (NOTE: addhead in previous loop was needed so that we can loop like this) */
	for (ld = hierarchy.first; ld; ld = ld->next) {
		/* get data */
		tem = (TreeElement *)ld->data;
		tse = TREESTORE(tem);
		ptr = &tem->rnaptr;
		prop = tem->directdata;
		
		/* check if we're looking for first ID, or appending to path */
		if (*id) {
			/* just 'append' property to path 
			 * - to prevent memory leaks, we must write to newpath not path, then free old path + swap them
			 */
			if (tse->type == TSE_RNA_PROPERTY) {
				if (RNA_property_type(prop) == PROP_POINTER) {
					/* for pointer we just append property name */
					newpath = RNA_path_append(*path, ptr, prop, 0, NULL);
				}
				else if (RNA_property_type(prop) == PROP_COLLECTION) {
					char buf[128], *name;
					
					temnext = (TreeElement *)(ld->next->data);
					/* tsenext= TREESTORE(temnext); */ /* UNUSED */
					
					nextptr = &temnext->rnaptr;
					name = RNA_struct_name_get_alloc(nextptr, buf, sizeof(buf), NULL);
					
					if (name) {
						/* if possible, use name as a key in the path */
						newpath = RNA_path_append(*path, NULL, prop, 0, name);
						
						if (name != buf)
							MEM_freeN(name);
					}
					else {
						/* otherwise use index */
						int index = 0;
						
						for (temsub = tem->subtree.first; temsub; temsub = temsub->next, index++)
							if (temsub == temnext)
								break;
						
						newpath = RNA_path_append(*path, NULL, prop, index, NULL);
					}
					
					ld = ld->next;
				}
			}
			
			if (newpath) {
				if (*path) MEM_freeN(*path);
				*path = newpath;
				newpath = NULL;
			}
		}
		else {
			/* no ID, so check if entry is RNA-struct, and if that RNA-struct is an ID datablock to extract info from */
			if (tse->type == TSE_RNA_STRUCT) {
				/* ptr->data not ptr->id.data seems to be the one we want,
				 * since ptr->data is sometimes the owner of this ID? */
				if (RNA_struct_is_ID(ptr->type)) {
					*id = (ID *)ptr->data;
					
					/* clear path */
					if (*path) {
						MEM_freeN(*path);
						path = NULL;
					}
				}
			}
		}
	}

	/* step 3: if we've got an ID, add the current item to the path */
	if (*id) {
		/* add the active property to the path */
		ptr = &te->rnaptr;
		prop = te->directdata;
		
		/* array checks */
		if (tselem->type == TSE_RNA_ARRAY_ELEM) {
			/* item is part of an array, so must set the array_index */
			*array_index = te->index;
		}
		else if (RNA_property_array_length(ptr, prop)) {
			/* entire array was selected, so keyframe all */
			*flag |= KSP_FLAG_WHOLE_ARRAY;
		}
		
		/* path */
		newpath = RNA_path_append(*path, NULL, prop, 0, NULL);
		if (*path) MEM_freeN(*path);
		*path = newpath;
	}

	/* free temp data */
	BLI_freelistN(&hierarchy);
}

/* =============================================== */
/* Driver Operations */

/* These operators are only available in databrowser mode for now, as
 * they depend on having RNA paths and/or hierarchies available.
 */
enum {
	DRIVERS_EDITMODE_ADD    = 0,
	DRIVERS_EDITMODE_REMOVE,
} /*eDrivers_EditModes*/;

/* Utilities ---------------------------------- */ 

/* Recursively iterate over tree, finding and working on selected items */
static void do_outliner_drivers_editop(SpaceOops *soops, ListBase *tree, ReportList *reports, short mode)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = tree->first; te; te = te->next) {
		tselem = TREESTORE(te);
		
		/* if item is selected, perform operation */
		if (tselem->flag & TSE_SELECTED) {
			ID *id = NULL;
			char *path = NULL;
			int array_index = 0;
			short flag = 0;
			short groupmode = KSP_GROUP_KSNAME;
			
			/* check if RNA-property described by this selected element is an animatable prop */
			if (ELEM(tselem->type, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM) &&
			    RNA_property_animateable(&te->rnaptr, te->directdata))
			{
				/* get id + path + index info from the selected element */
				tree_element_to_path(soops, te, tselem, 
				                     &id, &path, &array_index, &flag, &groupmode);
			}
			
			/* only if ID and path were set, should we perform any actions */
			if (id && path) {
				short dflags = CREATEDRIVER_WITH_DEFAULT_DVAR;
				int arraylen = 1;
				
				/* array checks */
				if (flag & KSP_FLAG_WHOLE_ARRAY) {
					/* entire array was selected, so add drivers for all */
					arraylen = RNA_property_array_length(&te->rnaptr, te->directdata);
				}
				else
					arraylen = array_index;
				
				/* we should do at least one step */
				if (arraylen == array_index)
					arraylen++;
				
				/* for each array element we should affect, add driver */
				for (; array_index < arraylen; array_index++) {
					/* action depends on mode */
					switch (mode) {
						case DRIVERS_EDITMODE_ADD:
						{
							/* add a new driver with the information obtained (only if valid) */
							ANIM_add_driver(reports, id, path, array_index, dflags, DRIVER_TYPE_PYTHON);
						}
						break;
						case DRIVERS_EDITMODE_REMOVE:
						{
							/* remove driver matching the information obtained (only if valid) */
							ANIM_remove_driver(reports, id, path, array_index, dflags);
						}
						break;
					}
				}
				
				/* free path, since it had to be generated */
				MEM_freeN(path);
			}
			
			
		}
		
		/* go over sub-tree */
		if (TSELEM_OPEN(tselem, soops))
			do_outliner_drivers_editop(soops, &te->subtree, reports, mode);
	}
}

/* Add Operator ---------------------------------- */

static int outliner_drivers_addsel_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soutliner = CTX_wm_space_outliner(C);
	
	/* check for invalid states */
	if (soutliner == NULL)
		return OPERATOR_CANCELLED;
	
	/* recursively go into tree, adding selected items */
	do_outliner_drivers_editop(soutliner, &soutliner->tree, op->reports, DRIVERS_EDITMODE_ADD);
	
	/* send notifiers */
	WM_event_add_notifier(C, NC_ANIMATION | ND_FCURVES_ORDER, NULL); // XXX
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_drivers_add_selected(wmOperatorType *ot)
{
	/* api callbacks */
	ot->idname = "OUTLINER_OT_drivers_add_selected";
	ot->name = "Add Drivers for Selected";
	ot->description = "Add drivers to selected items";
	
	/* api callbacks */
	ot->exec = outliner_drivers_addsel_exec;
	ot->poll = ed_operator_outliner_datablocks_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/* Remove Operator ---------------------------------- */

static int outliner_drivers_deletesel_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soutliner = CTX_wm_space_outliner(C);
	
	/* check for invalid states */
	if (soutliner == NULL)
		return OPERATOR_CANCELLED;
	
	/* recursively go into tree, adding selected items */
	do_outliner_drivers_editop(soutliner, &soutliner->tree, op->reports, DRIVERS_EDITMODE_REMOVE);
	
	/* send notifiers */
	WM_event_add_notifier(C, ND_KEYS, NULL); // XXX
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_drivers_delete_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname = "OUTLINER_OT_drivers_delete_selected";
	ot->name = "Delete Drivers for Selected";
	ot->description = "Delete drivers assigned to selected items";
	
	/* api callbacks */
	ot->exec = outliner_drivers_deletesel_exec;
	ot->poll = ed_operator_outliner_datablocks_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* =============================================== */
/* Keying Set Operations */

/* These operators are only available in databrowser mode for now, as
 * they depend on having RNA paths and/or hierarchies available.
 */
enum {
	KEYINGSET_EDITMODE_ADD  = 0,
	KEYINGSET_EDITMODE_REMOVE,
} /*eKeyingSet_EditModes*/;

/* Utilities ---------------------------------- */ 
 
/* find the 'active' KeyingSet, and add if not found (if adding is allowed) */
// TODO: should this be an API func?
static KeyingSet *verify_active_keyingset(Scene *scene, short add)
{
	KeyingSet *ks = NULL;
	
	/* sanity check */
	if (scene == NULL)
		return NULL;
	
	/* try to find one from scene */
	if (scene->active_keyingset > 0)
		ks = BLI_findlink(&scene->keyingsets, scene->active_keyingset - 1);
		
	/* add if none found */
	// XXX the default settings have yet to evolve
	if ((add) && (ks == NULL)) {
		ks = BKE_keyingset_add(&scene->keyingsets, NULL, NULL, KEYINGSET_ABSOLUTE, 0);
		scene->active_keyingset = BLI_countlist(&scene->keyingsets);
	}
	
	return ks;
}

/* Recursively iterate over tree, finding and working on selected items */
static void do_outliner_keyingset_editop(SpaceOops *soops, KeyingSet *ks, ListBase *tree, short mode)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = tree->first; te; te = te->next) {
		tselem = TREESTORE(te);
		
		/* if item is selected, perform operation */
		if (tselem->flag & TSE_SELECTED) {
			ID *id = NULL;
			char *path = NULL;
			int array_index = 0;
			short flag = 0;
			short groupmode = KSP_GROUP_KSNAME;
			
			/* check if RNA-property described by this selected element is an animatable prop */
			if (ELEM(tselem->type, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM) &&
			    RNA_property_animateable(&te->rnaptr, te->directdata))
			{
				/* get id + path + index info from the selected element */
				tree_element_to_path(soops, te, tselem, 
				                     &id, &path, &array_index, &flag, &groupmode);
			}
			
			/* only if ID and path were set, should we perform any actions */
			if (id && path) {
				/* action depends on mode */
				switch (mode) {
					case KEYINGSET_EDITMODE_ADD:
					{
						/* add a new path with the information obtained (only if valid) */
						/* TODO: what do we do with group name?
						 * for now, we don't supply one, and just let this use the KeyingSet name */
						BKE_keyingset_add_path(ks, id, NULL, path, array_index, flag, groupmode);
						ks->active_path = BLI_countlist(&ks->paths);
					}
					break;
					case KEYINGSET_EDITMODE_REMOVE:
					{
						/* find the relevant path, then remove it from the KeyingSet */
						KS_Path *ksp = BKE_keyingset_find_path(ks, id, NULL, path, array_index, groupmode);
						
						if (ksp) {
							/* free path's data */
							BKE_keyingset_free_path(ks, ksp);

							ks->active_path = 0;
						}
					}
					break;
				}
				
				/* free path, since it had to be generated */
				MEM_freeN(path);
			}
		}
		
		/* go over sub-tree */
		if (TSELEM_OPEN(tselem, soops))
			do_outliner_keyingset_editop(soops, ks, &te->subtree, mode);
	}
}

/* Add Operator ---------------------------------- */

static int outliner_keyingset_additems_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soutliner = CTX_wm_space_outliner(C);
	Scene *scene = CTX_data_scene(C);
	KeyingSet *ks = verify_active_keyingset(scene, 1);
	
	/* check for invalid states */
	if (ks == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Operation requires an Active Keying Set");
		return OPERATOR_CANCELLED;
	}
	if (soutliner == NULL)
		return OPERATOR_CANCELLED;
	
	/* recursively go into tree, adding selected items */
	do_outliner_keyingset_editop(soutliner, ks, &soutliner->tree, KEYINGSET_EDITMODE_ADD);
	
	/* send notifiers */
	WM_event_add_notifier(C, NC_SCENE | ND_KEYINGSET, NULL);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_keyingset_add_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname = "OUTLINER_OT_keyingset_add_selected";
	ot->name = "Keying Set Add Selected";
	ot->description = "Add selected items (blue-gray rows) to active Keying Set";
	
	/* api callbacks */
	ot->exec = outliner_keyingset_additems_exec;
	ot->poll = ed_operator_outliner_datablocks_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/* Remove Operator ---------------------------------- */

static int outliner_keyingset_removeitems_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *soutliner = CTX_wm_space_outliner(C);
	Scene *scene = CTX_data_scene(C);
	KeyingSet *ks = verify_active_keyingset(scene, 1);
	
	/* check for invalid states */
	if (soutliner == NULL)
		return OPERATOR_CANCELLED;
	
	/* recursively go into tree, adding selected items */
	do_outliner_keyingset_editop(soutliner, ks, &soutliner->tree, KEYINGSET_EDITMODE_REMOVE);
	
	/* send notifiers */
	WM_event_add_notifier(C, NC_SCENE | ND_KEYINGSET, NULL);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_keyingset_remove_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname = "OUTLINER_OT_keyingset_remove_selected";
	ot->name = "Keying Set Remove Selected";
	ot->description = "Remove selected items (blue-gray rows) from active Keying Set";
	
	/* api callbacks */
	ot->exec = outliner_keyingset_removeitems_exec;
	ot->poll = ed_operator_outliner_datablocks_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Parent Drop Operator *********************** */

static int parent_drop_exec(bContext *C, wmOperator *op)
{
	Object *par = NULL, *ob = NULL;
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	int partype = -1;
	char parname[MAX_ID_NAME], childname[MAX_ID_NAME];

	partype = RNA_enum_get(op->ptr, "type");
	RNA_string_get(op->ptr, "parent", parname);
	par = (Object *)BKE_libblock_find_name(ID_OB, parname);
	RNA_string_get(op->ptr, "child", childname);
	ob = (Object *)BKE_libblock_find_name(ID_OB, childname);

	ED_object_parent_set(op->reports, bmain, scene, ob, par, partype, FALSE);

	DAG_scene_sort(bmain, scene);
	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);

	return OPERATOR_FINISHED;
}

/* Used for drag and drop parenting */
TreeElement *outliner_dropzone_parent(bContext *C, wmEvent *event, TreeElement *te, float *fmval)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeStoreElem *tselem = TREESTORE(te);

	if ((fmval[1] > te->ys) && (fmval[1] < (te->ys + UI_UNIT_Y))) {
		/* name and first icon */
		if ((fmval[0] > te->xs + UI_UNIT_X) && (fmval[0] < te->xend)) {
			/* always makes active object */
			if (te->idcode == ID_OB && tselem->type == 0) {
				return te;
			}
			else {
				return NULL;
			}
		}
	}

	/* Not it.  Let's look at its children. */
	if ((tselem->flag & TSE_CLOSED) == 0 && (te->subtree.first)) {
		for (te = te->subtree.first; te; te = te->next) {
			TreeElement *te_valid;
			te_valid = outliner_dropzone_parent(C, event, te, fmval);
			if (te_valid) return te_valid;
		}
	}
	return NULL;
}

static int parent_drop_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *par = NULL;
	Object *ob = NULL;
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	Main *bmain = CTX_data_main(C);
	Scene *scene = NULL;
	TreeElement *te = NULL;
	TreeElement *te_found = NULL;
	char childname[MAX_ID_NAME];
	char parname[MAX_ID_NAME];
	int partype = 0;
	float fmval[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	/* Find object hovered over */
	for (te = soops->tree.first; te; te = te->next) {
		te_found = outliner_dropzone_parent(C, event, te, fmval);
		if (te_found) break;
	}

	if (te_found) {
		RNA_string_set(op->ptr, "parent", te_found->name);
		/* Identify parent and child */
		RNA_string_get(op->ptr, "child", childname);
		ob = (Object *)BKE_libblock_find_name(ID_OB, childname);
		RNA_string_get(op->ptr, "parent", parname);
		par = (Object *)BKE_libblock_find_name(ID_OB, parname);
		
		if (ELEM(NULL, ob, par)) {
			if (par == NULL) printf("par==NULL\n");
			return OPERATOR_CANCELLED;
		}
		if (ob == par) {
			return OPERATOR_CANCELLED;
		}
		
		scene = (Scene *)outliner_search_back(soops, te_found, ID_SCE);

		if (scene == NULL) {
			/* currently outlier organized in a way, that if there's no parent scene
			 * element for object it means that all displayed objects belong to
			 * active scene and parenting them is allowed (sergey)
			 */

			scene = CTX_data_scene(C);
		}

		if ((par->type != OB_ARMATURE) && (par->type != OB_CURVE) && (par->type != OB_LATTICE)) {
			if (ED_object_parent_set(op->reports, bmain, scene, ob, par, partype, FALSE)) {
				DAG_scene_sort(bmain, scene);
				DAG_ids_flush_update(bmain, 0);
				WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
				WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);
			}
		}
		else {
			/* Menu creation */
			uiPopupMenu *pup = uiPupMenuBegin(C, IFACE_("Set Parent To"), ICON_NONE);
			uiLayout *layout = uiPupMenuLayout(pup);
			
			PointerRNA ptr;
			
			WM_operator_properties_create(&ptr, "OUTLINER_OT_parent_drop");
			RNA_string_set(&ptr, "parent", parname);
			RNA_string_set(&ptr, "child", childname);
			RNA_enum_set(&ptr, "type", PAR_OBJECT);
			/* Cannot use uiItemEnumO()... have multiple properties to set. */
			uiItemFullO(layout, "OUTLINER_OT_parent_drop", IFACE_("Object"),
			            0, ptr.data, WM_OP_EXEC_DEFAULT, 0);
			
			/* par becomes parent, make the associated menus */
			if (par->type == OB_ARMATURE) {
				WM_operator_properties_create(&ptr, "OUTLINER_OT_parent_drop");
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_ARMATURE);
				uiItemFullO(layout, "OUTLINER_OT_parent_drop", IFACE_("Armature Deform"),
				            0, ptr.data, WM_OP_EXEC_DEFAULT, 0);
				
				WM_operator_properties_create(&ptr, "OUTLINER_OT_parent_drop");
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_ARMATURE_NAME);
				uiItemFullO(layout, "OUTLINER_OT_parent_drop", IFACE_("   With Empty Groups"),
				            0, ptr.data, WM_OP_EXEC_DEFAULT, 0);
				
				WM_operator_properties_create(&ptr, "OUTLINER_OT_parent_drop");
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_ARMATURE_ENVELOPE);
				uiItemFullO(layout, "OUTLINER_OT_parent_drop", IFACE_("   With Envelope Weights"),
				            0, ptr.data, WM_OP_EXEC_DEFAULT, 0);
				
				WM_operator_properties_create(&ptr, "OUTLINER_OT_parent_drop");
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_ARMATURE_AUTO);
				uiItemFullO(layout, "OUTLINER_OT_parent_drop", IFACE_("   With Automatic Weights"),
				            0, ptr.data, WM_OP_EXEC_DEFAULT, 0);
				
				WM_operator_properties_create(&ptr, "OUTLINER_OT_parent_drop");
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_BONE);
				uiItemFullO(layout, "OUTLINER_OT_parent_drop", IFACE_("Bone"),
				            0, ptr.data, WM_OP_EXEC_DEFAULT, 0);
			}
			else if (par->type == OB_CURVE) {
				WM_operator_properties_create(&ptr, "OUTLINER_OT_parent_drop");
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_CURVE);
				uiItemFullO(layout, "OUTLINER_OT_parent_drop", IFACE_("Curve Deform"),
				            0, ptr.data, WM_OP_EXEC_DEFAULT, 0);
				
				WM_operator_properties_create(&ptr, "OUTLINER_OT_parent_drop");
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_FOLLOW);
				uiItemFullO(layout, "OUTLINER_OT_parent_drop", IFACE_("Follow Path"),
				            0, ptr.data, WM_OP_EXEC_DEFAULT, 0);
				
				WM_operator_properties_create(&ptr, "OUTLINER_OT_parent_drop");
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_PATH_CONST);
				uiItemFullO(layout, "OUTLINER_OT_parent_drop", IFACE_("Path Constraint"),
				            0, ptr.data, WM_OP_EXEC_DEFAULT, 0);
			}
			else if (par->type == OB_LATTICE) {
				WM_operator_properties_create(&ptr, "OUTLINER_OT_parent_drop");
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_LATTICE);
				uiItemFullO(layout, "OUTLINER_OT_parent_drop", IFACE_("Lattice Deform"),
				            0, ptr.data, WM_OP_EXEC_DEFAULT, 0);
			}
			
			uiPupMenuEnd(C, pup);
			
			return OPERATOR_CANCELLED;
		}
	}
	else {
		return OPERATOR_CANCELLED;
	}

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_parent_drop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Drop to Set Parent";
	ot->description = "Drag to parent in Outliner";
	ot->idname = "OUTLINER_OT_parent_drop";

	/* api callbacks */
	ot->invoke = parent_drop_invoke;
	ot->exec = parent_drop_exec;

	ot->poll = ED_operator_outliner_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_string(ot->srna, "child", "Object", MAX_ID_NAME, "Child", "Child Object");
	RNA_def_string(ot->srna, "parent", "Object", MAX_ID_NAME, "Parent", "Parent Object");
	RNA_def_enum(ot->srna, "type", prop_make_parent_types, 0, "Type", "");
}

int outliner_dropzone_parent_clear(bContext *C, wmEvent *event, TreeElement *te, float *fmval)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeStoreElem *tselem = TREESTORE(te);

	/* Check for row */
	if ((fmval[1] > te->ys) && (fmval[1] < (te->ys + UI_UNIT_Y))) {
		/* Ignore drop on scene tree elements */
		if ((fmval[0] > te->xs + UI_UNIT_X) && (fmval[0] < te->xend)) {
			if ((te->idcode == ID_SCE) && 
			    !ELEM3(tselem->type, TSE_R_LAYER_BASE, TSE_R_LAYER, TSE_R_PASS))
			{
				return 0;
			}
			// Other codes to ignore?
		}
		
		/* Left or right of: (+), first icon, and name */
		if ((fmval[0] < (te->xs + UI_UNIT_X)) || (fmval[0] > te->xend)) {
			return 1;
		}
		else if (te->idcode != ID_OB || ELEM(tselem->type, TSE_MODIFIER_BASE, TSE_CONSTRAINT_BASE)) {
			return 1;
		}
		
		return 0;       // ID_OB, but mouse in undefined dropzone.
	}

	/* Not this row.  Let's look at its children. */
	if ((tselem->flag & TSE_CLOSED) == 0 && (te->subtree.first)) {
		for (te = te->subtree.first; te; te = te->next) {
			if (outliner_dropzone_parent_clear(C, event, te, fmval)) 
				return 1;
		}
	}
	return 0;
}

static int parent_clear_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = NULL;
	Object *ob = NULL;
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te;
	char obname[MAX_ID_NAME];

	RNA_string_get(op->ptr, "dragged_obj", obname);
	ob = (Object *)BKE_libblock_find_name(ID_OB, obname);

	/* search forwards to find the object */
	te = outliner_find_id(soops, &soops->tree, (ID *)ob);
	/* then search backwards to get the scene */
	scene = (Scene *)outliner_search_back(soops, te, ID_SCE);

	if (scene == NULL) {
		return OPERATOR_CANCELLED;
	}

	ED_object_parent_clear(ob, RNA_enum_get(op->ptr, "type"));

	DAG_scene_sort(bmain, scene);
	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_parent_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Drop to Clear Parent";
	ot->description = "Drag to clear parent in Outliner";
	ot->idname = "OUTLINER_OT_parent_clear";

	/* api callbacks */
	ot->invoke = parent_clear_invoke;

	ot->poll = ED_operator_outliner_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_string(ot->srna, "dragged_obj", "Object", MAX_ID_NAME, "Child", "Child Object");
	RNA_def_enum(ot->srna, "type", prop_clear_parent_types, 0, "Type", "");
}

TreeElement *outliner_dropzone_scene(bContext *C, wmEvent *UNUSED(event), TreeElement *te, float *fmval)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeStoreElem *tselem = TREESTORE(te);

	if ((fmval[1] > te->ys) && (fmval[1] < (te->ys + UI_UNIT_Y))) {
		/* name and first icon */
		if ((fmval[0] > te->xs + UI_UNIT_X) && (fmval[0] < te->xend)) {
			if (te->idcode == ID_SCE && tselem->type == 0) {
				return te;
			}
		}
	}
	return NULL;
}

static int scene_drop_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene = NULL;
	Object *ob = NULL;
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	Main *bmain = CTX_data_main(C);
	TreeElement *te = NULL;
	TreeElement *te_found = NULL;
	char obname[MAX_ID_NAME];
	float fmval[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	/* Find object hovered over */
	for (te = soops->tree.first; te; te = te->next) {
		te_found = outliner_dropzone_scene(C, event, te, fmval);
		if (te_found)
			break;
	}

	if (te_found) {
		Base *base;

		RNA_string_set(op->ptr, "scene", te_found->name);
		scene = (Scene *)BKE_libblock_find_name(ID_SCE, te_found->name);

		RNA_string_get(op->ptr, "object", obname);
		ob = (Object *)BKE_libblock_find_name(ID_OB, obname);

		if (ELEM(NULL, ob, scene) || scene->id.lib != NULL) {
			return OPERATOR_CANCELLED;
		}

		base = ED_object_scene_link(scene, ob);

		if (base == NULL) {
			return OPERATOR_CANCELLED;
		}

		if (scene == CTX_data_scene(C)) {
			/* when linking to an inactive scene don't touch the layer */
			ob->lay = base->lay;
			ED_base_object_select(base, BA_SELECT);
		}

		DAG_scene_sort(bmain, scene);
		DAG_ids_flush_update(bmain, 0);

		WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, scene);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void OUTLINER_OT_scene_drop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Drop Object to Scene";
	ot->description = "Drag object to scene in Outliner";
	ot->idname = "OUTLINER_OT_scene_drop";

	/* api callbacks */
	ot->invoke = scene_drop_invoke;

	ot->poll = ED_operator_outliner_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_string(ot->srna, "object", "Object", MAX_ID_NAME, "Object", "Target Object");
	RNA_def_string(ot->srna, "scene", "Scene", MAX_ID_NAME, "Scene", "Target Scene");
}

static int material_drop_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Main *bmain = CTX_data_main(C);
	Material *ma = NULL;
	Object *ob = NULL;
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	TreeElement *te = NULL;
	TreeElement *te_found = NULL;
	char mat_name[MAX_ID_NAME - 2];
	float fmval[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	/* Find object hovered over */
	for (te = soops->tree.first; te; te = te->next) {
		te_found = outliner_dropzone_parent(C, event, te, fmval);
		if (te_found)
			break;
	}

	if (te_found) {
		RNA_string_set(op->ptr, "object", te_found->name);
		ob = (Object *)BKE_libblock_find_name(ID_OB, te_found->name);

		RNA_string_get(op->ptr, "material", mat_name);
		ma = (Material *)BKE_libblock_find_name(ID_MA, mat_name);

		if (ELEM(NULL, ob, ma)) {
			return OPERATOR_CANCELLED;
		}

		assign_material(ob, ma, ob->totcol + 1);

		DAG_ids_flush_update(bmain, 0);
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));		
		WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING, ma);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void OUTLINER_OT_material_drop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Drop Material on Object";
	ot->description = "Drag material to object in Outliner";
	ot->idname = "OUTLINER_OT_material_drop";

	/* api callbacks */
	ot->invoke = material_drop_invoke;

	ot->poll = ED_operator_outliner_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_string(ot->srna, "object", "Object", MAX_ID_NAME, "Object", "Target Object");
	RNA_def_string(ot->srna, "material", "Material", MAX_ID_NAME, "Material", "Target Material");
}

