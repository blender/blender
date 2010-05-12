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

#include <stdio.h>

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_node.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "BLI_rect.h"

#include "ED_screen.h"
#include "ED_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"
 
#include "node_intern.h"

/* ****** helpers ****** */

static bNode *node_under_mouse(bNodeTree *ntree, int mx, int my)
{
	bNode *node;
	
	for(next_node(ntree); (node=next_node(NULL));) {
		/* node body (header and scale are in other operators) */
		if (BLI_in_rctf(&node->totr, mx, my))
			return node;
	}
	return NULL;
}

/* ****** Click Select ****** */
 
static bNode *node_mouse_select(SpaceNode *snode, ARegion *ar, short *mval, short extend)
{
	bNode *node;
	float mx, my;
	
	/* get mouse coordinates in view2d space */
	mx= (float)mval[0];
	my= (float)mval[1];
	
	UI_view2d_region_to_view(&ar->v2d, mval[0], mval[1], &mx, &my);
	
	/* find the closest visible node */
	node = node_under_mouse(snode->edittree, mx, my);
	
	if (node) {
		if (extend == 0) {
			node_deselectall(snode);
			node->flag |= SELECT;
		}
		else
			node->flag ^= SELECT;
			
		node_set_active(snode, node);
	}

	return node;
}

static int node_select_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	short mval[2];
	short extend;
	bNode *node= NULL;
	
	/* get settings from RNA properties for operator */
	mval[0] = RNA_int_get(op->ptr, "mouse_x");
	mval[1] = RNA_int_get(op->ptr, "mouse_y");
	
	extend = RNA_boolean_get(op->ptr, "extend");
	
	/* perform the select */
	node= node_mouse_select(snode, ar, mval, extend);

	/* WATCH THIS, there are a few other ways to change the active material */
	if(node) {
		if (node->id && ELEM(GS(node->id->name), ID_MA, ID_TE)) {
			WM_event_add_notifier(C, NC_MATERIAL|ND_SHADING_DRAW, node->id);
		}
	}
	
	/* send notifiers */
	WM_event_add_notifier(C, NC_NODE|ND_NODE_SELECT, NULL);
	
	/* allow tweak event to work too */
	return OPERATOR_FINISHED|OPERATOR_PASS_THROUGH;
}

static int node_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	short mval[2];	
	
	mval[0]= event->x - ar->winrct.xmin;
	mval[1]= event->y - ar->winrct.ymin;
	
	RNA_int_set(op->ptr, "mouse_x", mval[0]);
	RNA_int_set(op->ptr, "mouse_y", mval[1]);

	return node_select_exec(C,op);
}


void NODE_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select";
	ot->idname= "NODE_OT_select";
	
	/* api callbacks */
	ot->invoke= node_select_invoke;
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "mouse_x", 0, INT_MIN, INT_MAX, "Mouse X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "mouse_y", 0, INT_MIN, INT_MAX, "Mouse Y", "", INT_MIN, INT_MAX);
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "");
}

/* ****** Border Select ****** */

static int node_borderselect_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	bNode *node;
	rcti rect;
	rctf rectf;
	int gesture_mode= RNA_int_get(op->ptr, "gesture_mode");
	
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	UI_view2d_region_to_view(&ar->v2d, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
	
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
	UI_view2d_region_to_view(&ar->v2d, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);
	
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(BLI_isect_rctf(&rectf, &node->totr, NULL)) {
			if(gesture_mode==GESTURE_MODAL_SELECT)
				node->flag |= SELECT;
			else
				node->flag &= ~SELECT;
		}
	}
	
	WM_event_add_notifier(C, NC_NODE|ND_NODE_SELECT, NULL);

	return OPERATOR_FINISHED;
}

static int node_border_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	int tweak = RNA_boolean_get(op->ptr, "tweak");
	
	if (tweak) {
		/* prevent initiating the border select if the mouse is over a node */
		/* this allows border select on empty space, but drag-translate on nodes */
		SpaceNode *snode= CTX_wm_space_node(C);
		ARegion *ar= CTX_wm_region(C);
		short mval[2];
		float mx, my;
		
		mval[0]= event->x - ar->winrct.xmin;
		mval[1]= event->y - ar->winrct.ymin;
		
		/* get mouse coordinates in view2d space */
		mx= (float)mval[0];
		my= (float)mval[1];
		
		UI_view2d_region_to_view(&ar->v2d, mval[0], mval[1], &mx, &my);
		
		if (node_under_mouse(snode->edittree, mx, my))
			return OPERATOR_CANCELLED|OPERATOR_PASS_THROUGH;
	}
	
	return WM_border_select_invoke(C, op, event);
}

void NODE_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->idname= "NODE_OT_select_border";
	
	/* api callbacks */
	ot->invoke= node_border_select_invoke;
	ot->exec= node_borderselect_exec;
	ot->modal= WM_border_select_modal;
	
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna */
	WM_operator_properties_gesture_border(ot, FALSE);
	RNA_def_boolean(ot->srna, "tweak", 0, "Tweak", "Only activate when mouse is not over a node - useful for tweak gesture");
}

/* ****** Select/Deselect All ****** */

static int node_select_all_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *first = snode->edittree->nodes.first;
	bNode *node;
	int count= 0;

	for(node=first; node; node=node->next)
		if(node->flag & NODE_SELECT)
			count++;

	if(count) {
		for(node=first; node; node=node->next)
			node->flag &= ~NODE_SELECT;
	}
	else {
		for(node=first; node; node=node->next)
			node->flag |= NODE_SELECT;
	}
	
	WM_event_add_notifier(C, NC_NODE|ND_NODE_SELECT, NULL);
	return OPERATOR_FINISHED;
}

void NODE_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select/Deselect All";
	ot->description = "(De)select all nodes";
	ot->idname = "NODE_OT_select_all";
	
	/* api callbacks */
	ot->exec = node_select_all_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****** Select Linked To ****** */

static int node_select_linked_to_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeLink *link;
	bNode *node;
	
	for (node=snode->edittree->nodes.first; node; node=node->next)
		node->flag &= ~NODE_TEST;

	for (link=snode->edittree->links.first; link; link=link->next) {
		if (link->fromnode->flag & NODE_SELECT)
			link->tonode->flag |= NODE_TEST;
	}
	
	for (node=snode->edittree->nodes.first; node; node=node->next) {
		if (node->flag & NODE_TEST)
			node->flag |= NODE_SELECT;
	}
	
	WM_event_add_notifier(C, NC_NODE|ND_NODE_SELECT, NULL);
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
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****** Select Linked From ****** */

static int node_select_linked_from_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeLink *link;
	bNode *node;
	
	for(node=snode->edittree->nodes.first; node; node=node->next)
		node->flag &= ~NODE_TEST;

	for(link=snode->edittree->links.first; link; link=link->next) {
		if(link->tonode->flag & NODE_SELECT)
			link->fromnode->flag |= NODE_TEST;
	}
	
	for(node=snode->edittree->nodes.first; node; node=node->next) {
		if(node->flag & NODE_TEST)
			node->flag |= NODE_SELECT;
	}
	
	WM_event_add_notifier(C, NC_NODE|ND_NODE_SELECT, NULL);
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
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****** Select Same Type ****** */

static int node_select_same_type_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);

	node_select_same_type(snode);
	WM_event_add_notifier(C, NC_NODE|ND_NODE_SELECT, NULL);
	return OPERATOR_FINISHED;
}

void NODE_OT_select_same_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Same Type";
	ot->description = "Select all the same type";
	ot->idname = "NODE_OT_select_same_type";
	
	/* api callbacks */
	ot->exec = node_select_same_type_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}
