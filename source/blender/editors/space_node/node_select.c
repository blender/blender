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
 * Contributor(s): Blender Foundation, Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/node_select.c
 *  \ingroup spnode
 */

#include <stdlib.h>

#include "DNA_node_types.h"

#include "BLI_utildefines.h"
#include "BLI_rect.h"
#include "BLI_lasso.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "ED_node.h"  /* own include */
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "MEM_guardedalloc.h"

#include "node_intern.h"  /* own include */

/* ****** helpers ****** */

static bNode *node_under_mouse_select(bNodeTree *ntree, int mx, int my)
{
	bNode *node;
	
	for (node = ntree->nodes.last; node; node = node->prev) {
		if (node->typeinfo->select_area_func) {
			if (node->typeinfo->select_area_func(node, mx, my))
				return node;
		}
	}
	return NULL;
}

static bNode *node_under_mouse_tweak(bNodeTree *ntree, int mx, int my)
{
	bNode *node;
	
	for (node = ntree->nodes.last; node; node = node->prev) {
		if (node->typeinfo->tweak_area_func) {
			if (node->typeinfo->tweak_area_func(node, mx, my))
				return node;
		}
	}
	return NULL;
}

static void node_toggle(bNode *node)
{
	nodeSetSelected(node, !(node->flag & SELECT));
}

void node_socket_select(bNode *node, bNodeSocket *sock)
{
	sock->flag |= SELECT;
	
	/* select node too */
	if (node)
		node->flag |= SELECT;
}

void node_socket_deselect(bNode *node, bNodeSocket *sock, const bool deselect_node)
{
	sock->flag &= ~SELECT;
	
	if (node && deselect_node) {
		bool sel = 0;
		
		/* if no selected sockets remain, also deselect the node */
		for (sock = node->inputs.first; sock; sock = sock->next) {
			if (sock->flag & SELECT) {
				sel = 1;
				break;
			}
		}
		for (sock = node->outputs.first; sock; sock = sock->next) {
			if (sock->flag & SELECT) {
				sel = 1;
				break;
			}
		}
		
		if (!sel)
			node->flag &= ~SELECT;
	}
}

static void node_socket_toggle(bNode *node, bNodeSocket *sock, int deselect_node)
{
	if (sock->flag & SELECT)
		node_socket_deselect(node, sock, deselect_node);
	else
		node_socket_select(node, sock);
}

/* no undo here! */
void node_deselect_all(SpaceNode *snode)
{
	bNode *node;
	
	for (node = snode->edittree->nodes.first; node; node = node->next)
		nodeSetSelected(node, false);
}

void node_deselect_all_input_sockets(SpaceNode *snode, const bool deselect_nodes)
{
	bNode *node;
	bNodeSocket *sock;
	
	/* XXX not calling node_socket_deselect here each time, because this does iteration
	 * over all node sockets internally to check if the node stays selected.
	 * We can do that more efficiently here.
	 */
	
	for (node = snode->edittree->nodes.first; node; node = node->next) {
		int sel = 0;
		
		for (sock = node->inputs.first; sock; sock = sock->next)
			sock->flag &= ~SELECT;
		
		/* if no selected sockets remain, also deselect the node */
		if (deselect_nodes) {
			for (sock = node->outputs.first; sock; sock = sock->next) {
				if (sock->flag & SELECT) {
					sel = 1;
					break;
				}
			}
			
			if (!sel)
				node->flag &= ~SELECT;
		}
	}
}

void node_deselect_all_output_sockets(SpaceNode *snode, const bool deselect_nodes)
{
	bNode *node;
	bNodeSocket *sock;
	
	/* XXX not calling node_socket_deselect here each time, because this does iteration
	 * over all node sockets internally to check if the node stays selected.
	 * We can do that more efficiently here.
	 */
	
	for (node = snode->edittree->nodes.first; node; node = node->next) {
		bool sel = false;
		
		for (sock = node->outputs.first; sock; sock = sock->next)
			sock->flag &= ~SELECT;
		
		/* if no selected sockets remain, also deselect the node */
		if (deselect_nodes) {
			for (sock = node->inputs.first; sock; sock = sock->next) {
				if (sock->flag & SELECT) {
					sel = 1;
					break;
				}
			}
			
			if (!sel)
				node->flag &= ~SELECT;
		}
	}
}

/* Return true if we need redraw, otherwise false. */

static bool node_select_grouped_type(SpaceNode *snode, bNode *node_act)
{
	bNode *node;
	bool changed = false;

	for (node = snode->edittree->nodes.first; node; node = node->next) {
		if ((node->flag & SELECT) == 0) {
			if (node->type == node_act->type) {
				nodeSetSelected(node, true);
				changed = true;
			}
		}
	}

	return changed;
}

static bool node_select_grouped_color(SpaceNode *snode, bNode *node_act)
{
	bNode *node;
	bool changed = false;

	for (node = snode->edittree->nodes.first; node; node = node->next) {
		if ((node->flag & SELECT) == 0) {
			if (compare_v3v3(node->color, node_act->color, 0.005f)) {
				nodeSetSelected(node, true);
				changed = true;
			}
		}
	}

	return changed;
}

static bool node_select_grouped_name(SpaceNode *snode, bNode *node_act, const bool from_right)
{
	bNode *node;
	bool changed = false;
	const unsigned int delims[] = {'.', '-', '_', '\0'};
	size_t pref_len_act, pref_len_curr;
	char *sep, *suf_act, *suf_curr;

	pref_len_act = BLI_str_partition_ex_utf8(node_act->name, delims, &sep, &suf_act, from_right);

	/* Note: in case we are searching for suffix, and found none, use whole name as suffix. */
	if (from_right && !(sep && suf_act)) {
		pref_len_act = 0;
		suf_act = node_act->name;
	}

	for (node = snode->edittree->nodes.first; node; node = node->next) {
		if (node->flag & SELECT) {
			continue;
		}
		pref_len_curr = BLI_str_partition_ex_utf8(node->name, delims, &sep, &suf_curr, from_right);

		/* Same as with active node name! */
		if (from_right && !(sep && suf_curr)) {
			pref_len_curr = 0;
			suf_curr = node->name;
		}

		if ((from_right && STREQ(suf_act, suf_curr)) ||
		    (!from_right && (pref_len_act == pref_len_curr) && STREQLEN(node_act->name, node->name, pref_len_act)))
		{
			nodeSetSelected(node, true);
			changed = true;
		}
	}

	return changed;
}

enum {
	NODE_SELECT_GROUPED_TYPE   = 0,
	NODE_SELECT_GROUPED_COLOR  = 1,
	NODE_SELECT_GROUPED_PREFIX = 2,
	NODE_SELECT_GROUPED_SUFIX  = 3,
};

static int node_select_grouped_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *node_act = nodeGetActive(snode->edittree);
	bNode *node;
	bool changed = false;
	const bool extend = RNA_boolean_get(op->ptr, "extend");
	const int type = RNA_enum_get(op->ptr, "type");

	if (!extend) {
		for (node = snode->edittree->nodes.first; node; node = node->next) {
			nodeSetSelected(node, false);
		}
	}
	nodeSetSelected(node_act, true);

	switch (type) {
		case NODE_SELECT_GROUPED_TYPE:
			changed = node_select_grouped_type(snode, node_act);
			break;
		case NODE_SELECT_GROUPED_COLOR:
			changed = node_select_grouped_color(snode, node_act);
			break;
		case NODE_SELECT_GROUPED_PREFIX:
			changed = node_select_grouped_name(snode, node_act, false);
			break;
		case NODE_SELECT_GROUPED_SUFIX:
			changed = node_select_grouped_name(snode, node_act, true);
			break;
		default:
			break;
	}

	if (changed) {
		ED_node_sort(snode->edittree);
		WM_event_add_notifier(C, NC_NODE | NA_SELECTED, NULL);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void NODE_OT_select_grouped(wmOperatorType *ot)
{
	static EnumPropertyItem prop_select_grouped_types[] = {
		{NODE_SELECT_GROUPED_TYPE, "TYPE", 0, "Type", ""},
		{NODE_SELECT_GROUPED_COLOR, "COLOR", 0, "Color", ""},
		{NODE_SELECT_GROUPED_PREFIX, "PREFIX", 0, "Prefix", ""},
		{NODE_SELECT_GROUPED_SUFIX, "SUFFIX", 0, "Suffix", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Select Grouped";
	ot->description = "Select nodes with similar properties";
	ot->idname = "NODE_OT_select_grouped";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = node_select_grouped_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection instead of deselecting everything first");
	ot->prop = RNA_def_enum(ot->srna, "type", prop_select_grouped_types, 0, "Type", "");
}

void node_select_single(bContext *C, bNode *node)
{
	Main *bmain = CTX_data_main(C);
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *tnode;
	
	for (tnode = snode->edittree->nodes.first; tnode; tnode = tnode->next)
		if (tnode != node)
			nodeSetSelected(tnode, false);
	nodeSetSelected(node, true);
	
	ED_node_set_active(bmain, snode->edittree, node);
	ED_node_set_active_viewer_key(snode);
	
	ED_node_sort(snode->edittree);
	
	WM_event_add_notifier(C, NC_NODE | NA_SELECTED, NULL);
}

/* ****** Click Select ****** */
 
static int node_mouse_select(Main *bmain, SpaceNode *snode, ARegion *ar, const int mval[2], short extend)
{
	bNode *node, *tnode;
	bNodeSocket *sock, *tsock;
	float cursor[2];
	int selected = 0;
	
	/* get mouse coordinates in view2d space */
	UI_view2d_region_to_view(&ar->v2d, mval[0], mval[1], &cursor[0], &cursor[1]);
	
	if (extend) {
		/* first do socket selection, these generally overlap with nodes.
		 * socket selection only in extend mode.
		 */
		if (node_find_indicated_socket(snode, &node, &sock, cursor, SOCK_IN)) {
			node_socket_toggle(node, sock, 1);
			selected = 1;
		}
		else if (node_find_indicated_socket(snode, &node, &sock, cursor, SOCK_OUT)) {
			if (sock->flag & SELECT) {
				node_socket_deselect(node, sock, 1);
			}
			else {
				/* only allow one selected output per node, for sensible linking.
				 * allows selecting outputs from different nodes though.
				 */
				if (node) {
					for (tsock = node->outputs.first; tsock; tsock = tsock->next)
						node_socket_deselect(node, tsock, 1);
				}
				node_socket_select(node, sock);
			}
			selected = 1;
		}
		else {
			/* find the closest visible node */
			node = node_under_mouse_select(snode->edittree, cursor[0], cursor[1]);
			
			if (node) {
				if ((node->flag & SELECT) && (node->flag & NODE_ACTIVE) == 0) {
					/* if node is selected but not active make it active
					 * before it'll be desleected
					 */
					ED_node_set_active(bmain, snode->edittree, node);
				}
				else {
					node_toggle(node);
					ED_node_set_active(bmain, snode->edittree, node);
				}

				selected = 1;
			}
		}
	}
	else {  /* extend == 0 */
		
		/* find the closest visible node */
		node = node_under_mouse_select(snode->edittree, cursor[0], cursor[1]);
		
		if (node) {
			for (tnode = snode->edittree->nodes.first; tnode; tnode = tnode->next) {
				nodeSetSelected(tnode, false);
			}
			nodeSetSelected(node, true);
			ED_node_set_active(bmain, snode->edittree, node);
			selected = 1;
		}
	}
	
	/* update node order */
	if (selected) {
		ED_node_set_active_viewer_key(snode);
		ED_node_sort(snode->edittree);
	}
	
	return selected;
}

static int node_select_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	SpaceNode *snode = CTX_wm_space_node(C);
	ARegion *ar = CTX_wm_region(C);
	int mval[2];
	short extend;
	
	/* get settings from RNA properties for operator */
	mval[0] = RNA_int_get(op->ptr, "mouse_x");
	mval[1] = RNA_int_get(op->ptr, "mouse_y");
	
	extend = RNA_boolean_get(op->ptr, "extend");
	
	/* perform the select */
	if (node_mouse_select(bmain, snode, ar, mval, extend)) {
		/* send notifiers */
		WM_event_add_notifier(C, NC_NODE | NA_SELECTED, NULL);
		
		/* allow tweak event to work too */
		return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
	}
	else {
		/* allow tweak event to work too */
		return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
	}
}

static int node_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	RNA_int_set(op->ptr, "mouse_x", event->mval[0]);
	RNA_int_set(op->ptr, "mouse_y", event->mval[1]);

	return node_select_exec(C, op);
}


void NODE_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select";
	ot->idname = "NODE_OT_select";
	ot->description = "Select the node under the cursor";
	
	/* api callbacks */
	ot->invoke = node_select_invoke;
	ot->exec = node_select_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "mouse_x", 0, INT_MIN, INT_MAX, "Mouse X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "mouse_y", 0, INT_MIN, INT_MAX, "Mouse Y", "", INT_MIN, INT_MAX);
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "");
}

/* ****** Border Select ****** */

static int node_borderselect_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	ARegion *ar = CTX_wm_region(C);
	bNode *node;
	rctf rectf;
	int gesture_mode = RNA_int_get(op->ptr, "gesture_mode");
	const bool extend = RNA_boolean_get(op->ptr, "extend");
	
	WM_operator_properties_border_to_rctf(op, &rectf);
	UI_view2d_region_to_view_rctf(&ar->v2d, &rectf, &rectf);
	
	for (node = snode->edittree->nodes.first; node; node = node->next) {
		if (BLI_rctf_isect(&rectf, &node->totr, NULL)) {
			nodeSetSelected(node, (gesture_mode == GESTURE_MODAL_SELECT));
		}
		else if (!extend) {
			nodeSetSelected(node, false);
		}
	}
	
	ED_node_sort(snode->edittree);
	
	WM_event_add_notifier(C, NC_NODE | NA_SELECTED, NULL);

	return OPERATOR_FINISHED;
}

static int node_border_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	const bool tweak = RNA_boolean_get(op->ptr, "tweak");
	
	if (tweak) {
		/* prevent initiating the border select if the mouse is over a node */
		/* this allows border select on empty space, but drag-translate on nodes */
		SpaceNode *snode = CTX_wm_space_node(C);
		ARegion *ar = CTX_wm_region(C);
		float mx, my;

		UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &mx, &my);
		
		if (node_under_mouse_tweak(snode->edittree, mx, my))
			return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
	}
	
	return WM_border_select_invoke(C, op, event);
}

void NODE_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Border Select";
	ot->idname = "NODE_OT_select_border";
	ot->description = "Use box selection to select nodes";
	
	/* api callbacks */
	ot->invoke = node_border_select_invoke;
	ot->exec = node_borderselect_exec;
	ot->modal = WM_border_select_modal;
	ot->cancel = WM_border_select_cancel;
	
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* rna */
	WM_operator_properties_gesture_border(ot, true);
	RNA_def_boolean(ot->srna, "tweak", 0, "Tweak", "Only activate when mouse is not over a node - useful for tweak gesture");
}

/* ****** Circle Select ****** */

static int node_circleselect_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	ARegion *ar = CTX_wm_region(C);
	bNode *node;

	int x, y, radius, gesture_mode;
	float offset[2];

	float zoom  = (float)(BLI_rcti_size_x(&ar->winrct)) / (float)(BLI_rctf_size_x(&ar->v2d.cur));

	gesture_mode = RNA_int_get(op->ptr, "gesture_mode");

	/* get operator properties */
	x = RNA_int_get(op->ptr, "x");
	y = RNA_int_get(op->ptr, "y");
	radius = RNA_int_get(op->ptr, "radius");

	UI_view2d_region_to_view(&ar->v2d, x, y, &offset[0], &offset[1]);

	for (node = snode->edittree->nodes.first; node; node = node->next) {
		if (BLI_rctf_isect_circle(&node->totr, offset, radius / zoom)) {
			nodeSetSelected(node, (gesture_mode == GESTURE_MODAL_SELECT));
		}
	}

	WM_event_add_notifier(C, NC_NODE | NA_SELECTED, NULL);

	return OPERATOR_FINISHED;
}

void NODE_OT_select_circle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Circle Select";
	ot->idname = "NODE_OT_select_circle";
	ot->description = "Use circle selection to select nodes";

	/* api callbacks */
	ot->invoke = WM_gesture_circle_invoke;
	ot->exec = node_circleselect_exec;
	ot->modal = WM_gesture_circle_modal;

	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* rna */
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "radius", 1, 1, INT_MAX, "Radius", "", 1, INT_MAX);
	RNA_def_int(ot->srna, "gesture_mode", 0, INT_MIN, INT_MAX, "Gesture Mode", "", INT_MIN, INT_MAX);
}

/* ****** Lasso Select ****** */

static bool do_lasso_select_node(bContext *C, const int mcords[][2], short moves, short select)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *node;

	ARegion *ar = CTX_wm_region(C);

	rcti rect;
	bool changed = false;

	/* get rectangle from operator */
	BLI_lasso_boundbox(&rect, mcords, moves);

	/* do actual selection */
	for (node = snode->edittree->nodes.first; node; node = node->next) {
		int screen_co[2];
		const float cent[2] = {BLI_rctf_cent_x(&node->totr),
		                       BLI_rctf_cent_y(&node->totr)};

		/* marker in screen coords */
		if (UI_view2d_view_to_region_clip(&ar->v2d, cent[0], cent[1], &screen_co[0], &screen_co[1]) &&
		    BLI_rcti_isect_pt(&rect, screen_co[0], screen_co[1]) &&
		    BLI_lasso_is_point_inside(mcords, moves, screen_co[0], screen_co[1], INT_MAX))
		{
			nodeSetSelected(node, select);
			changed = true;
		}
	}

	if (changed) {
		WM_event_add_notifier(C, NC_NODE | NA_SELECTED, NULL);
	}

	return changed;
}

static int node_lasso_select_exec(bContext *C, wmOperator *op)
{
	int mcords_tot;
	const int (*mcords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcords_tot);

	if (mcords) {
		short select;

		select = !RNA_boolean_get(op->ptr, "deselect");
		do_lasso_select_node(C, mcords, mcords_tot, select);

		MEM_freeN((void *)mcords);

		return OPERATOR_FINISHED;
	}
	return OPERATOR_PASS_THROUGH;
}

void NODE_OT_select_lasso(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Lasso Select";
	ot->description = "Select nodes using lasso selection";
	ot->idname = "NODE_OT_select_lasso";

	/* api callbacks */
	ot->invoke = WM_gesture_lasso_invoke;
	ot->modal = WM_gesture_lasso_modal;
	ot->exec = node_lasso_select_exec;
	ot->poll = ED_operator_node_active;
	ot->cancel = WM_gesture_lasso_cancel;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Deselect rather than select items");
	RNA_def_boolean(ot->srna, "extend", 1, "Extend", "Extend selection instead of deselecting everything first");
}

/* ****** Select/Deselect All ****** */

static int node_select_all_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	ListBase *node_lb = &snode->edittree->nodes;
	bNode *node;
	int action = RNA_enum_get(op->ptr, "action");

	if (action == SEL_TOGGLE) {
		if (ED_node_select_check(node_lb))
			action = SEL_DESELECT;
		else
			action = SEL_SELECT;
	}

	for (node = node_lb->first; node; node = node->next) {
		switch (action) {
			case SEL_SELECT:
				nodeSetSelected(node, true);
				break;
			case SEL_DESELECT:
				nodeSetSelected(node, false);
				break;
			case SEL_INVERT:
				nodeSetSelected(node, !(node->flag & SELECT));
				break;
		}
	}

	ED_node_sort(snode->edittree);
	
	WM_event_add_notifier(C, NC_NODE | NA_SELECTED, NULL);
	return OPERATOR_FINISHED;
}

void NODE_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All";
	ot->description = "(De)select all nodes";
	ot->idname = "NODE_OT_select_all";
	
	/* api callbacks */
	ot->exec = node_select_all_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

/* ****** Select Linked To ****** */

static int node_select_linked_to_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeLink *link;
	bNode *node;
	
	for (node = snode->edittree->nodes.first; node; node = node->next)
		node->flag &= ~NODE_TEST;

	for (link = snode->edittree->links.first; link; link = link->next) {
		if (nodeLinkIsHidden(link))
			continue;
		if (link->fromnode && link->tonode && (link->fromnode->flag & NODE_SELECT))
			link->tonode->flag |= NODE_TEST;
	}
	
	for (node = snode->edittree->nodes.first; node; node = node->next) {
		if (node->flag & NODE_TEST)
			nodeSetSelected(node, true);
	}
	
	ED_node_sort(snode->edittree);
	
	WM_event_add_notifier(C, NC_NODE | NA_SELECTED, NULL);
	return OPERATOR_FINISHED;
}

void NODE_OT_select_linked_to(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked To";
	ot->description = "Select nodes linked to the selected ones";
	ot->idname = "NODE_OT_select_linked_to";
	
	/* api callbacks */
	ot->exec = node_select_linked_to_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****** Select Linked From ****** */

static int node_select_linked_from_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeLink *link;
	bNode *node;
	
	for (node = snode->edittree->nodes.first; node; node = node->next)
		node->flag &= ~NODE_TEST;

	for (link = snode->edittree->links.first; link; link = link->next) {
		if (nodeLinkIsHidden(link))
			continue;
		if (link->fromnode && link->tonode && (link->tonode->flag & NODE_SELECT))
			link->fromnode->flag |= NODE_TEST;
	}
	
	for (node = snode->edittree->nodes.first; node; node = node->next) {
		if (node->flag & NODE_TEST)
			nodeSetSelected(node, true);
	}
	
	ED_node_sort(snode->edittree);
	
	WM_event_add_notifier(C, NC_NODE | NA_SELECTED, NULL);
	return OPERATOR_FINISHED;
}

void NODE_OT_select_linked_from(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked From";
	ot->description = "Select nodes linked from the selected ones";
	ot->idname = "NODE_OT_select_linked_from";
	
	/* api callbacks */
	ot->exec = node_select_linked_from_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int node_select_same_type_step_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	ARegion *ar = CTX_wm_region(C);
	bNode **node_array;
	bNode *active = nodeGetActive(snode->edittree);
	int totnodes;
	const bool revert = RNA_boolean_get(op->ptr, "prev");
	const bool same_type = 1;
	
	ntreeGetDependencyList(snode->edittree, &node_array, &totnodes);
	
	if (totnodes > 1) {
		int a;
		
		for (a = 0; a < totnodes; a++) {
			if (node_array[a] == active)
				break;
		}
		
		if (same_type) {
			bNode *node = NULL;
			
			while (node == NULL) {
				if (revert) a--;
				else a++;
				
				if (a < 0 || a >= totnodes)
					break;
				
				node = node_array[a];
				
				if (node->type == active->type)
					break;
				else node = NULL;
			}
			if (node)
				active = node;
		}
		else {
			if (revert) {
				if (a == 0)
					active = node_array[totnodes - 1];
				else
					active = node_array[a - 1];
			}
			else {
				if (a == totnodes - 1)
					active = node_array[0];
				else
					active = node_array[a + 1];
			}
		}
		
		node_select_single(C, active);

		/* is note outside view? */
		if (active->totr.xmax < ar->v2d.cur.xmin || active->totr.xmin > ar->v2d.cur.xmax ||
		    active->totr.ymax < ar->v2d.cur.ymin || active->totr.ymin > ar->v2d.cur.ymax)
		{
			const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
			space_node_view_flag(C, snode, ar, NODE_SELECT, smooth_viewtx);
		}
	}
	
	if (node_array)
		MEM_freeN(node_array);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_select_same_type_step(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Activate Same Type Next/Prev";
	ot->description = "Activate and view same node type, step by step";
	ot->idname = "NODE_OT_select_same_type_step";
	
	/* api callbacks */
	ot->exec = node_select_same_type_step_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "prev", 0, "Previous", "");

}

/* *************** find a node **************** */

/* generic  search invoke */
static void node_find_cb(const struct bContext *C, void *UNUSED(arg), const char *str, uiSearchItems *items)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *node;
	
	for (node = snode->edittree->nodes.first; node; node = node->next) {
		
		if (BLI_strcasestr(node->name, str) || BLI_strcasestr(node->label, str)) {
			char name[256];
			
			if (node->label[0])
				BLI_snprintf(name, 256, "%s (%s)", node->name, node->label);
			else
				BLI_strncpy(name, node->name, 256);
			if (false == uiSearchItemAdd(items, name, node, 0))
				break;
		}
	}
}

static void node_find_call_cb(struct bContext *C, void *UNUSED(arg1), void *arg2)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *active = arg2;
	
	if (active) {
		ARegion *ar = CTX_wm_region(C);
		node_select_single(C, active);
		
		/* is note outside view? */
		if (active->totr.xmax < ar->v2d.cur.xmin || active->totr.xmin > ar->v2d.cur.xmax ||
		    active->totr.ymax < ar->v2d.cur.ymin || active->totr.ymin > ar->v2d.cur.ymax)
		{
			space_node_view_flag(C, snode, ar, NODE_SELECT, U.smooth_viewtx);
		}

	}
}

static uiBlock *node_find_menu(bContext *C, ARegion *ar, void *arg_op)
{
	static char search[256] = "";
	wmEvent event;
	wmWindow *win = CTX_wm_window(C);
	uiBlock *block;
	uiBut *but;
	wmOperator *op = (wmOperator *)arg_op;
	
	block = uiBeginBlock(C, ar, "_popup", UI_EMBOSS);
	uiBlockSetFlag(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);
	
	but = uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, sizeof(search), 10, 10, 9 * UI_UNIT_X, UI_UNIT_Y, 0, 0, "");
	uiButSetSearchFunc(but, node_find_cb, op->type, node_find_call_cb, NULL);
	
	/* fake button, it holds space for search items */
	uiDefBut(block, LABEL, 0, "", 10, 10 - uiSearchBoxHeight(), uiSearchBoxWidth(), uiSearchBoxHeight(), NULL, 0, 0, 0, 0, NULL);
	
	uiPopupBoundsBlock(block, 6, 0, -UI_UNIT_Y); /* move it downwards, mouse over button */
	
	//	uiButActiveOnly(C, ar, block, but); XXX using this here makes Blender hang - investigate
	wm_event_init_from_window(win, &event);
	event.type = EVT_BUT_OPEN;
	event.val = KM_PRESS;
	event.customdata = but;
	event.customdatafree = false;
	wm_event_add(win, &event);
	
	return block;
}


static int node_find_node_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	uiPupBlock(C, node_find_menu, op);
	return OPERATOR_CANCELLED;
}


void NODE_OT_find_node(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Find Node";
	ot->description = "Search for named node and allow to select and activate it";
	ot->idname = "NODE_OT_find_node";
	
	/* api callbacks */
	ot->invoke = node_find_node_invoke;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "prev", 0, "Previous", "");
	
}


