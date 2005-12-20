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

static void node_draw_link(SpaceNode *snode, bNodeLink *link)
{
	float vec[4][3];
	float dist, spline_step, mx=0.0f, my=0.0f;
	int curve_res;
	
	if(link->fromnode==NULL && link->tonode==NULL)
		return;
	
	/* this is dragging link */
	if(link->fromnode==NULL || link->tonode==NULL) {
		short mval[2];
		getmouseco_areawin(mval);
		areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
		
		BIF_ThemeColor(TH_WIRE);
	}
	else {
		/* check cyclic */
		if(link->fromnode->level >= link->tonode->level)
			BIF_ThemeColor(TH_WIRE);
		else
			BIF_ThemeColor(TH_REDALERT);
	}
	
	vec[0][2]= vec[1][2]= vec[2][2]= vec[3][2]= 0.0; /* only 2d spline, set the Z to 0*/
	
	/* in v0 and v3 we put begin/end points */
	if(link->fromnode) {
		vec[0][0]= link->fromsock->locx;
		vec[0][1]= link->fromsock->locy;
	}
	else {
		vec[0][0]= mx;
		vec[0][1]= my;
	}
	if(link->tonode) {
		vec[3][0]= link->tosock->locx;
		vec[3][1]= link->tosock->locy;
	}
	else {
		vec[3][0]= mx;
		vec[3][1]= my;
	}
	
	dist= 0.5*VecLenf(vec[0], vec[3]);
	
	/* check direction later, for top sockets */
	vec[1][0]= vec[0][0]+dist;
	vec[1][1]= vec[0][1];
	
	vec[2][0]= vec[3][0]-dist;
	vec[2][1]= vec[3][1];
	
	if( MIN4(vec[0][0], vec[1][0], vec[2][0], vec[3][0]) > G.v2d->cur.xmax); /* clipped */	
	else if ( MAX4(vec[0][0], vec[1][0], vec[2][0], vec[3][0]) < G.v2d->cur.xmin); /* clipped */
	else {
		curve_res = 24;
		
		/* we can reuse the dist variable here to increment the GL curve eval amount*/
		dist = 1.0f/curve_res;
		spline_step = 0.0f;
		
		glMap1f(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 4, vec[0]);
		glBegin(GL_LINE_STRIP);
		while (spline_step < 1.000001f) {
			glEvalCoord1f(spline_step);
			spline_step += dist;
		}
		glEnd();
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
	glEnable(GL_MAP1_VERTEX_3);

	/* aspect+font, set each time */
	snode->aspect= (snode->v2d.cur.xmax - snode->v2d.cur.xmin)/((float)sa->winx);
	snode->curfont= uiSetCurFont_ext(snode->aspect);

	/* backdrop */
	draw_nodespace_grid(snode);
	
	/* nodes */
	get_nodetree(snode);
	if(snode->nodetree) {
		bNode *node;
		bNodeLink *link;
		
		/* node lines */
		glEnable(GL_BLEND);
		glEnable( GL_LINE_SMOOTH );
		for(link= snode->nodetree->links.first; link; link= link->next)
			node_draw_link(snode, link);
		glDisable(GL_BLEND);
		glDisable( GL_LINE_SMOOTH );
		
		/* not selected */
		snode->block= uiNewBlock(&sa->uiblocks, "node buttons1", UI_EMBOSS, UI_HELV, sa->win);
		
		for(node= snode->nodetree->nodes.first; node; node= node->next)
			if(!(node->flag & SELECT)) 
				node->drawfunc(snode, node);
		
		uiDrawBlock(snode->block);
		
		/* selected */
		snode->block= uiNewBlock(&sa->uiblocks, "node buttons2", UI_EMBOSS, UI_HELV, sa->win);
		
		for(node= snode->nodetree->nodes.first; node; node= node->next)
			if(node->flag & SELECT) 
				node->drawfunc(snode, node);
		
		uiDrawBlock(snode->block);
	}
	
	/* restore viewport (not needed yet) */
	mywinset(sa->win);

	/* ortho at pixel level curarea */
	myortho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);

	draw_area_emboss(sa);
	curarea->win_swap= WIN_BACK_OK;
}
