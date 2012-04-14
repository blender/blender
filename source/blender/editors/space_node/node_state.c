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

/** \file blender/editors/space_node/node_state.c
 *  \ingroup spnode
 */


#include <stdio.h>

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_node.h"

#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"
 
#include "node_intern.h"


/* **************** View All Operator ************** */

static void snode_home(ScrArea *UNUSED(sa), ARegion *ar, SpaceNode* snode)
{
	bNode *node;
	rctf *cur;
	float oldwidth, oldheight, width, height;
	int first= 1;
	
	cur= &ar->v2d.cur;
	
	oldwidth= cur->xmax - cur->xmin;
	oldheight= cur->ymax - cur->ymin;
	
	cur->xmin = cur->ymin = 0.0f;
	cur->xmax=ar->winx;
	cur->ymax=ar->winy;
	
	if (snode->edittree) {
		for (node= snode->edittree->nodes.first; node; node= node->next) {
			if (first) {
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

	if (width > height) {
		float newheight;
		newheight= oldheight * width/oldwidth;
		cur->ymin = cur->ymin - newheight/4;
		cur->ymax = cur->ymax + newheight/4;
	}
	else {
		float newwidth;
		newwidth= oldwidth * height/oldheight;
		cur->xmin = cur->xmin - newwidth/4;
		cur->xmax = cur->xmax + newwidth/4;
	}

	ar->v2d.tot= ar->v2d.cur;
	UI_view2d_curRect_validate(&ar->v2d);
}

static int node_view_all_exec(bContext *C, wmOperator *UNUSED(op))
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
	ot->name = "View All";
	ot->idname = "NODE_OT_view_all";
	ot->description = "Resize view so you can see all nodes";
	
	/* api callbacks */
	ot->exec = node_view_all_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}
