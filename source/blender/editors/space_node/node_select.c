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
 * Contributor(s): Blender Foundation, Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>

#include "DNA_node_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_node.h"
#include "BKE_global.h"

#include "BLI_rect.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"
 
#include "node_intern.h"
 
static void node_mouse_select(SpaceNode *snode, ARegion *ar, short *mval, short extend)
{
	bNode *node;
	float mx, my;
	
	mx= (float)mval[0];
	my= (float)mval[1];
	
	UI_view2d_region_to_view(&ar->v2d, mval[0], mval[1], &mx, &my);
	
	for(next_node(snode->edittree); (node=next_node(NULL));) {
		
		/* first check for the headers or scaling widget */
		/* XXX if(node->flag & NODE_HIDDEN) {
			if(do_header_hidden_node(snode, node, mx, my))
				return 1;
		}
		else {
			if(do_header_node(snode, node, mx, my))
				return 1;
		}*/
		
		/* node body */
		if(BLI_in_rctf(&node->totr, mx, my))
			break;
	}
	if(node) {
		if((extend & KM_SHIFT)==0)
			node_deselectall(snode);
		
		if(extend & KM_SHIFT) {
			if(node->flag & SELECT)
				node->flag &= ~SELECT;
			else
				node->flag |= SELECT;
		}
		else
			node->flag |= SELECT;
		
		node_set_active(snode, node);
		
		/* viewer linking */
		if(extend & KM_CTRL)
			;//	node_link_viewer(snode, node);
		
		//std_rmouse_transform(node_transform_ext);	/* does undo push for select */
	}
}

static int node_select_exec(bContext *C, wmOperator *op)
{
	// XXX wmWindow *window=  CTX_wm_window(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	int select_type;
	short mval[2];
	short extend;

	select_type = RNA_enum_get(op->ptr, "select_type");
	
	switch (select_type) {
		case NODE_SELECT_MOUSE:
			mval[0] = RNA_int_get(op->ptr, "mouse_x");
			mval[1] = RNA_int_get(op->ptr, "mouse_y");
			extend = RNA_boolean_get(op->ptr, "extend");
			node_mouse_select(snode, ar, mval, extend);
			break;
	}

	/* need refresh/a notifier vs compo notifier */
	// XXX WM_event_add_notifier(C, NC_SCENE|ND_NODES, NULL); /* Do we need to pass the scene? */
	ED_region_tag_redraw(ar);
	
	/* allow tweak event to work too */
	return OPERATOR_FINISHED|OPERATOR_PASS_THROUGH;
}

static int node_select_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	/* execute the events */
	switch (event->type) {
		case MOUSEMOVE:
			printf("%d %d\n", event->x, event->y);
			break;
		case SELECTMOUSE:
			//if (event->val==0) {
				/* calculate overall delta mouse-movement for redo */
				printf("done translating\n");
				//WM_cursor_restore(CTX_wm_window(C));
				
				return OPERATOR_FINISHED;
			//}
			break;
	}

	return OPERATOR_RUNNING_MODAL;
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

static int node_extend_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	RNA_boolean_set(op->ptr, "extend", KM_SHIFT);

	return node_select_invoke(C, op, event);
}

/* operators */

static EnumPropertyItem prop_select_items[] = {
	{NODE_SELECT_MOUSE, "NORMAL", 0, "Normal Select", "Select using the mouse"},
	{0, NULL, 0, NULL, NULL}};

void NODE_OT_select_extend(wmOperatorType *ot)
{
	// XXX - Todo - This should just be a toggle option for NODE_OT_select not its own op
	/* identifiers */
	ot->name= "Activate/Select (Shift)";
	ot->idname= "NODE_OT_select_extend";
	
	/* api callbacks */
	ot->invoke= node_extend_select_invoke;
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "select_type", prop_select_items, 0, "Select Type", "");
	
	RNA_def_int(ot->srna, "mouse_x", 0, INT_MIN, INT_MAX, "Mouse X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "mouse_y", 0, INT_MIN, INT_MAX, "Mouse Y", "", INT_MIN, INT_MAX);
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "");
}

void NODE_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Activate/Select";
	ot->idname= "NODE_OT_select";
	
	/* api callbacks */
	ot->invoke= node_select_invoke;
	ot->poll= ED_operator_node_active;
	ot->modal= node_select_modal;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
	
	RNA_def_enum(ot->srna, "select_type", prop_select_items, 0, "Select Type", "");
	
	RNA_def_int(ot->srna, "mouse_x", 0, INT_MIN, INT_MAX, "Mouse X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "mouse_y", 0, INT_MIN, INT_MAX, "Mouse Y", "", INT_MIN, INT_MAX);
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "");
}

/* ****** Border Select ****** */

static EnumPropertyItem prop_select_types[] = {
	{NODE_EXCLUSIVE, "EXCLUSIVE", 0, "Exclusive", ""}, /* right mouse */
	{NODE_EXTEND, "EXTEND", 0, "Extend", ""}, /* left mouse */
	{0, NULL, 0, NULL, NULL}
};

static int node_borderselect_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	bNode *node;
	rcti rect;
	rctf rectf;
	short val;
	
	val= RNA_int_get(op->ptr, "event_type");
	
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	UI_view2d_region_to_view(&ar->v2d, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
	
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
	UI_view2d_region_to_view(&ar->v2d, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);
	
	if (snode->edittree == NULL) // XXX should this be in poll()? - campbell
		return OPERATOR_FINISHED;

	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(BLI_isect_rctf(&rectf, &node->totr, NULL)) {
			if(val==NODE_EXTEND)
				node->flag |= SELECT;
			else
				node->flag &= ~SELECT;
		}
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_NODES, NULL); /* Do we need to pass the scene? */

	return OPERATOR_FINISHED;
}

void NODE_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->idname= "NODE_OT_select_border";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= node_borderselect_exec;
	ot->modal= WM_border_select_modal;
	
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna */
	RNA_def_int(ot->srna, "event_type", 0, INT_MIN, INT_MAX, "Event Type", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmin", 0, INT_MIN, INT_MAX, "X Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmax", 0, INT_MIN, INT_MAX, "X Max", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymin", 0, INT_MIN, INT_MAX, "Y Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymax", 0, INT_MIN, INT_MAX, "Y Max", "", INT_MIN, INT_MAX);

	RNA_def_enum(ot->srna, "type", prop_select_types, 0, "Type", "");
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
	
	WM_event_add_notifier(C, NC_SCENE|ND_NODES, NULL);
	return OPERATOR_FINISHED;
}

void NODE_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select/Deselect All";
	ot->description = "(De)select all nodes.";
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

	for (link=snode->edittree->links.first; link; link=link->next)
		if (link->fromnode->flag & NODE_SELECT)
			link->tonode->flag |= NODE_TEST;
	
	for (node=snode->edittree->nodes.first; node; node=node->next)
		if (node->flag & NODE_TEST)
			node->flag |= NODE_SELECT;
	
	WM_event_add_notifier(C, NC_SCENE|ND_NODES, NULL);
	return OPERATOR_FINISHED;
}

void NODE_OT_select_linked_to(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked To";
	ot->description = "Select nodes linked to the selected ones.";
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

	for(link=snode->edittree->links.first; link; link=link->next)
		if(link->tonode->flag & NODE_SELECT)
			link->fromnode->flag |= NODE_TEST;
	
	for(node=snode->edittree->nodes.first; node; node=node->next)
		if(node->flag & NODE_TEST)
			node->flag |= NODE_SELECT;
	
	WM_event_add_notifier(C, NC_SCENE|ND_NODES, NULL);
	return OPERATOR_FINISHED;
}

void NODE_OT_select_linked_from(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked From";
	ot->description = "Select nodes linked from the selected ones.";
	ot->idname = "NODE_OT_select_linked_from";
	
	/* api callbacks */
	ot->exec = node_select_linked_from_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

