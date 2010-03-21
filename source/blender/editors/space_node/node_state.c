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

/* **************** Node Header Buttons ************** */

static void node_hide_unhide_sockets(SpaceNode *snode, bNode *node)
{
	bNodeSocket *sock;
	
	/* unhide all */
	if( node_has_hidden_sockets(node) ) {
		for(sock= node->inputs.first; sock; sock= sock->next)
			sock->flag &= ~SOCK_HIDDEN;
		for(sock= node->outputs.first; sock; sock= sock->next)
			sock->flag &= ~SOCK_HIDDEN;
	}
	else {
		bNode *gnode= node_tree_get_editgroup(snode->nodetree);
		
		/* hiding inside group should not break links in other group users */
		if(gnode) {
			nodeGroupSocketUseFlags((bNodeTree *)gnode->id);
			for(sock= node->inputs.first; sock; sock= sock->next)
				if(!(sock->flag & SOCK_IN_USE))
					if(sock->link==NULL)
						sock->flag |= SOCK_HIDDEN;
			for(sock= node->outputs.first; sock; sock= sock->next)
				if(!(sock->flag & SOCK_IN_USE))
					if(nodeCountSocketLinks(snode->edittree, sock)==0)
						sock->flag |= SOCK_HIDDEN;
		}
		else {
			/* hide unused sockets */
			for(sock= node->inputs.first; sock; sock= sock->next) {
				if(sock->link==NULL)
					sock->flag |= SOCK_HIDDEN;
			}
			for(sock= node->outputs.first; sock; sock= sock->next) {
				if(nodeCountSocketLinks(snode->edittree, sock)==0)
					sock->flag |= SOCK_HIDDEN;
			}
		}
	}

	node_tree_verify_groups(snode->nodetree);
}

static int do_header_node(SpaceNode *snode, bNode *node, float mx, float my)
{
	rctf totr= node->totr;
	
	totr.ymin= totr.ymax-20.0f;
	
	totr.xmax= totr.xmin+15.0f;
	if(BLI_in_rctf(&totr, mx, my)) {
		node->flag |= NODE_HIDDEN;
		return 1;
	}	
	
	totr.xmax= node->totr.xmax;
	totr.xmin= totr.xmax-18.0f;
	if(node->typeinfo->flag & NODE_PREVIEW) {
		if(BLI_in_rctf(&totr, mx, my)) {
			node->flag ^= NODE_PREVIEW;
			return 1;
		}
		totr.xmin-=18.0f;
	}
	if(node->type == NODE_GROUP) {
		if(BLI_in_rctf(&totr, mx, my)) {
			snode_make_group_editable(snode, node);
			return 1;
		}
		totr.xmin-=18.0f;
	}
	if(node->typeinfo->flag & NODE_OPTIONS) {
		if(BLI_in_rctf(&totr, mx, my)) {
			node->flag ^= NODE_OPTIONS;
			return 1;
		}
		totr.xmin-=18.0f;
	}
	/* hide unused sockets */
	if(BLI_in_rctf(&totr, mx, my)) {
		node_hide_unhide_sockets(snode, node);
	}
	
	return 0;
}

static int do_header_hidden_node(SpaceNode *snode, bNode *node, float mx, float my)
{
	rctf totr= node->totr;
	
	totr.xmax= totr.xmin+15.0f;
	if(BLI_in_rctf(&totr, mx, my)) {
		node->flag &= ~NODE_HIDDEN;
		return 1;
	}	
	return 0;
}

static int node_toggle_visibility(SpaceNode *snode, ARegion *ar, short *mval)
{
	bNode *node;
	float mx, my;
	
	mx= (float)mval[0];
	my= (float)mval[1];
	
	UI_view2d_region_to_view(&ar->v2d, mval[0], mval[1], &mx, &my);
	
	for(next_node(snode->edittree); (node=next_node(NULL));) {
		if(node->flag & NODE_HIDDEN) {
			if(do_header_hidden_node(snode, node, mx, my)) {
				ED_region_tag_redraw(ar);
				return 1;
			}
		}
		else {
			if(do_header_node(snode, node, mx, my)) {
				ED_region_tag_redraw(ar);
				return 1;
			}
		}
	}
	return 0;
}

static int node_toggle_visibility_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ARegion *ar= CTX_wm_region(C);
	short mval[2];

	mval[0] = RNA_int_get(op->ptr, "mouse_x");
	mval[1] = RNA_int_get(op->ptr, "mouse_y");
	if(node_toggle_visibility(snode, ar, mval))
		return OPERATOR_FINISHED;
	else
		return OPERATOR_CANCELLED|OPERATOR_PASS_THROUGH;
}

static int node_toggle_visibility_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	short mval[2];	
	
	mval[0]= event->x - ar->winrct.xmin;
	mval[1]= event->y - ar->winrct.ymin;
	
	RNA_int_set(op->ptr, "mouse_x", mval[0]);
	RNA_int_set(op->ptr, "mouse_y", mval[1]);

	return node_toggle_visibility_exec(C,op);
}

void NODE_OT_visibility_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Visibility";
	ot->idname= "NODE_OT_visibility_toggle";
	ot->description= "Handle clicks on node header buttons";
	
	/* api callbacks */
	ot->invoke= node_toggle_visibility_invoke;
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "mouse_x", 0, INT_MIN, INT_MAX, "Mouse X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "mouse_y", 0, INT_MIN, INT_MAX, "Mouse Y", "", INT_MIN, INT_MAX);
}

/* **************** View All Operator ************** */

static void snode_home(ScrArea *sa, ARegion *ar, SpaceNode* snode)
{
	bNode *node;
	rctf *cur, *tot;
	float oldwidth, oldheight, width, height;
	int first= 1;
	
	cur= &ar->v2d.cur;
	tot= &ar->v2d.tot;
	
	oldwidth= cur->xmax - cur->xmin;
	oldheight= cur->ymax - cur->ymin;
	
	cur->xmin= cur->ymin= 0.0f;
	cur->xmax=ar->winx;
	cur->xmax= ar->winy;
	
	if(snode->edittree) {
		for(node= snode->edittree->nodes.first; node; node= node->next) {
			if(first) {
				first= 0;
				ar->v2d.cur= node->totr;
			}
			else {
				BLI_union_rctf(cur, &node->totr);
			}
		}
	}
	
	snode->xof= 0;
	snode->yof= 0;
	width= cur->xmax - cur->xmin;
	height= cur->ymax- cur->ymin;
	if(width > height) {
		float newheight;
		newheight= oldheight * width/oldwidth;
		cur->ymin= cur->ymin - newheight/4;
		cur->ymax= cur->ymin + newheight;
	}
	else {
		float newwidth;
		newwidth= oldwidth * height/oldheight;
		cur->xmin= cur->xmin - newwidth/4;
		cur->xmax= cur->xmin + newwidth;
	}
	
	ar->v2d.tot= ar->v2d.cur;
	UI_view2d_curRect_validate(&ar->v2d);
}

static int node_view_all_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	SpaceNode *snode= CTX_wm_space_node(C);
	
	snode_home(sa, ar, snode);
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_view_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View All";
	ot->idname= "NODE_OT_view_all";
	
	/* api callbacks */
	ot->exec= node_view_all_exec;
	ot->poll= ED_operator_node_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
