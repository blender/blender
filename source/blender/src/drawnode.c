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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_action_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_screen.h"

#include "BSE_drawipo.h"
#include "BSE_view.h"
#include "BMF_Api.h"

#include "blendef.h"
#include "MEM_guardedalloc.h"

static void draw_nodespace_grid(SpaceNode *snode)
{
//	float fac, step= 20.0f;
	
	/* window is 'pixel size', like buttons */
	
	BIF_ThemeColorShade(TH_BACK, 10);
	
	glRectf(0.0f, 0.0f, curarea->winx, curarea->winy);
}


/* get from assigned ID */
static void get_nodetree(SpaceNode *snode)
{
	/* note: once proper coded, remove free from freespacelist() */
	if(snode->nodetree==NULL) {
		snode->nodetree= MEM_callocN(sizeof(bNodeTree), "new node tree");
	}
	
}

void drawnodespace(ScrArea *sa, void *spacedata)
{
	SpaceNode *snode= sa->spacedata.first;
	float col[3];
	
	BIF_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	calc_scrollrcts(sa, &(snode->v2d), sa->winx, sa->winy);
	
	myortho2(snode->v2d.cur.xmin, snode->v2d.cur.xmax, snode->v2d.cur.ymin, snode->v2d.cur.ymax);
	bwin_clear_viewmat(sa->win);	/* clear buttons view */
	glLoadIdentity();
	
	/* only set once */
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	/* aspect+font, set each time */
	snode->aspect= (snode->v2d.cur.xmax - snode->v2d.cur.xmin)/((float)sa->winx);
	snode->curfont= uiSetCurFont_ext(snode->aspect);

	/* backdrop */
	draw_nodespace_grid(snode);
	
	/* nodes */
	get_nodetree(snode);
	if(snode->nodetree) {
		bNode *node;
		for(node= snode->nodetree->nodes.first; node; node= node->next) {
			node->drawfunc(snode, node);
		}
	}
	
	/* restore viewport */
	mywinset(sa->win);

	/* ortho at pixel level curarea */
	myortho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);

	draw_area_emboss(sa);
	curarea->win_swap= WIN_BACK_OK;
}
