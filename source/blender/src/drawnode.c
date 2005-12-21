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
#include "DNA_userdef_types.h"

#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_language.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_screen.h"

#include "BSE_drawipo.h"
#include "BSE_node.h"
#include "BSE_view.h"

#include "BMF_Api.h"

#include "blendef.h"
#include "interface.h"	/* urm...  for rasterpos_safe, roundbox */

#include "MEM_guardedalloc.h"

/* **************  Draw callbacks *********** */



static void node_shader_draw_value(SpaceNode *snode, bNode *node)
{
	
	if(snode->block) {
		uiBut *bt;
		
		bt= uiDefButF(snode->block, NUM, B_NODE_EXEC, "", 
					  node->prv.xmin, node->prv.ymin, node->prv.xmax-node->prv.xmin, node->prv.ymax-node->prv.ymin, 
					  node->ns.vec, 0.0f, 1.0f, 100, 2, "");
		
	}
}

static void node_shader_draw_rgb(SpaceNode *snode, bNode *node)
{
	
	if(snode->block) {
		
		/* enforce square box drawing */
		uiBlockSetEmboss(snode->block, UI_EMBOSSP);
		
		uiDefButF(snode->block, HSVCUBE, B_NODE_EXEC, "", 
					  node->prv.xmin, node->prv.ymin, node->prv.xmax-node->prv.xmin, 10.0f, 
					  node->ns.vec, 0.0f, 1.0f, 3, 0, "");
		uiDefButF(snode->block, HSVCUBE, B_NODE_EXEC, "", 
					  node->prv.xmin, node->prv.ymin+14.0f, node->prv.xmax-node->prv.xmin, node->prv.ymax-node->prv.ymin-14.0f, 
					  node->ns.vec, 0.0f, 1.0f, 2, 0, "");
		
		uiDefButF(snode->block, COL, B_NOP, "",		
				  node->prv.xmin, node->prv.ymax+10.0f, node->prv.xmax-node->prv.xmin, 15.0f, 
				  node->ns.vec, 0.0, 0.0, -1, 0, "");
					/* the -1 above prevents col button to popup a color picker */
		
		uiBlockSetEmboss(snode->block, UI_EMBOSS);
	}
}

static void node_shader_draw_show_rgb(SpaceNode *snode, bNode *node)
{
	
	if(snode->block) {
		
		/* enforce square box drawing */
		uiBlockSetEmboss(snode->block, UI_EMBOSSP);
		
		uiDefButF(snode->block, COL, B_NOP, "", 
					  node->prv.xmin, node->prv.ymin-NODE_DY, node->prv.xmax-node->prv.xmin, NODE_DY, 
					  node->ns.vec, 0.0f, 0.0f, -1, 0, "");
					/* the -1 above prevents col button to popup a color picker */
		uiBlockSetEmboss(snode->block, UI_EMBOSS);
	}
}


/* exported to editnode.c */
void node_shader_set_drawfunc(bNode *node)
{
	switch(node->type) {
		case SH_NODE_TEST:
			node->drawfunc= NULL;
			break;
		case SH_NODE_VALUE:
			node->drawfunc= node_shader_draw_value;
			break;
		case SH_NODE_RGB:
			node->drawfunc= node_shader_draw_rgb;
			break;
		case SH_NODE_SHOW_RGB:
			node->drawfunc= node_shader_draw_show_rgb;
			break;
		case SH_NODE_MIX_RGB:
			node->drawfunc= NULL;
			break;
	}
}

/* ******* init draw callbacks for all tree types ************* */

static void ntree_init_callbacks(bNodeTree *ntree)
{
	bNode *node;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(ntree->type==NTREE_SHADER)
			node_shader_set_drawfunc(node);
	}
	ntree->init |= NTREE_DRAW_SET;
}	

/* ************** Generic drawing ************** */

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

static void nodeshadow(rctf *rct, int select)
{
	int a;
	char alpha= 2;
	
	uiSetRoundBox(15);
	glEnable(GL_BLEND);
	
	if(select) a= 10; else a=7;
	for(; a>0; a-=1) {
		/* alpha ranges from 2 to 20 or so */
		glColor4ub(0, 0, 0, alpha);
		alpha+= 2;
		
		gl_round_box(GL_POLYGON, rct->xmin - a, rct->ymin - a, rct->xmax + a, rct->ymax-10.0f + a, 8.0f+a);
	}
	
	/* outline emphasis */
	glEnable( GL_LINE_SMOOTH );
	glColor4ub(0, 0, 0, 100);
	gl_round_box(GL_LINE_LOOP, rct->xmin-0.5f, rct->ymin-0.5f, rct->xmax+0.5f, rct->ymax+0.5f, 8.0f);
	glDisable( GL_LINE_SMOOTH );
	
	glDisable(GL_BLEND);
}

/* nice AA filled circle */
static void socket_circle_draw(float x, float y, float size, int type, int select)
{
	/* 16 values of sin function */
	static float si[16] = {
		0.00000000, 0.39435585,0.72479278,0.93775213,
		0.99871650,0.89780453,0.65137248,0.29936312,
		-0.10116832,-0.48530196,-0.79077573,-0.96807711,
		-0.98846832,-0.84864425,-0.57126821,-0.20129852
	};
	/* 16 values of cos function */
	static float co[16] ={
		1.00000000,0.91895781,0.68896691,0.34730525,
		-0.05064916,-0.44039415,-0.75875812,-0.95413925,
		-0.99486932,-0.87434661,-0.61210598,-0.25065253,
		0.15142777,0.52896401,0.82076344,0.97952994,
	};
	int a;
	
	if(select==0) {
		if(type==SOCK_VALUE)
			glColor3ub(160, 160, 160);
		else if(type==SOCK_VECTOR)
			glColor3ub(100, 100, 200);
		else if(type==SOCK_RGBA)
			glColor3ub(200, 200, 40);
		else 
			glColor3ub(100, 200, 100);
	}
	else {
		if(type==SOCK_VALUE)
			glColor3ub(200, 200, 200);
		else if(type==SOCK_VECTOR)
			glColor3ub(140, 140, 240);
		else if(type==SOCK_RGBA)
			glColor3ub(240, 240, 100);
		else 
			glColor3ub(140, 240, 140);
	}
	
	glBegin(GL_POLYGON);
	for(a=0; a<16; a++)
		glVertex2f(x+size*si[a], y+size*co[a]);
	glEnd();
	
	glColor4ub(0, 0, 0, 150);
	glEnable(GL_BLEND);
	glEnable( GL_LINE_SMOOTH );
	glBegin(GL_LINE_LOOP);
	for(a=0; a<16; a++)
		glVertex2f(x+size*si[a], y+size*co[a]);
	glEnd();
	glDisable( GL_LINE_SMOOTH );
	glDisable(GL_BLEND);
}

static int node_basis_draw(SpaceNode *snode, bNode *node)
{
	bNodeSocket *sock;
	rctf *rct= &node->tot;
	float slen;
	int trans= (U.transopts & USER_TR_BUTTONS);
	
	nodeshadow(rct, node->flag & SELECT);
	
	BIF_ThemeColorShade(TH_HEADER, 0);
	uiSetRoundBox(3);
	uiRoundBox(rct->xmin, rct->ymax-NODE_DY, rct->xmax, rct->ymax, 8);
	
	BIF_ThemeColorShade(TH_HEADER, 20);
	uiSetRoundBox(12);
	uiRoundBox(rct->xmin, rct->ymin, rct->xmax, rct->ymax-NODE_DY, 8);
	
	ui_rasterpos_safe(rct->xmin+4.0f, rct->ymax-NODE_DY+5.0f, snode->aspect);
	
	if(node->flag & SELECT) 
		BIF_ThemeColor(TH_TEXT_HI);
	else
		BIF_ThemeColor(TH_TEXT);
	
	BIF_DrawString(snode->curfont, node->name, trans);
	
	for(sock= node->inputs.first; sock; sock= sock->next) {
		socket_circle_draw(sock->locx, sock->locy, NODE_SOCK, sock->type, sock->flag & SELECT);
		
		BIF_ThemeColor(TH_TEXT);
		ui_rasterpos_safe(sock->locx+8.0f, sock->locy-5.0f, snode->aspect);
		BIF_DrawString(snode->curfont, sock->name, trans);
	}
	
	for(sock= node->outputs.first; sock; sock= sock->next) {
		socket_circle_draw(sock->locx, sock->locy, NODE_SOCK, sock->type, sock->flag & SELECT);
		
		BIF_ThemeColor(TH_TEXT);
		slen= snode->aspect*BIF_GetStringWidth(snode->curfont, sock->name, trans);
		ui_rasterpos_safe(sock->locx-8.0f-slen, sock->locy-5.0f, snode->aspect);
		BIF_DrawString(snode->curfont, sock->name, trans);
	}
	
	return 0;
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
		if(link->fromnode->level >= link->tonode->level && link->tonode->level!=0xFFF)
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
	get_nodetree(snode);	/* editor context */
	
	if(snode->nodetree) {
		bNode *node;
		bNodeLink *link;
		
		if((snode->nodetree->init & NTREE_DRAW_SET)==0)
			ntree_init_callbacks(snode->nodetree);
		
		/* node lines */
		glEnable(GL_BLEND);
		glEnable( GL_LINE_SMOOTH );
		for(link= snode->nodetree->links.first; link; link= link->next)
			node_draw_link(snode, link);
		glDisable(GL_BLEND);
		glDisable( GL_LINE_SMOOTH );
		
		/* not selected */
		snode->block= uiNewBlock(&sa->uiblocks, "node buttons1", UI_EMBOSS, UI_HELV, sa->win);
		uiBlockSetFlag(snode->block, UI_BLOCK_NO_HILITE);
		
		for(node= snode->nodetree->nodes.first; node; node= node->next) {
			if(!(node->flag & SELECT)) {
				node_basis_draw(snode, node);
				if(node->drawfunc) node->drawfunc(snode, node);
			}
		}
		uiDrawBlock(snode->block);
		
		/* selected */
		snode->block= uiNewBlock(&sa->uiblocks, "node buttons2", UI_EMBOSS, UI_HELV, sa->win);
		uiBlockSetFlag(snode->block, UI_BLOCK_NO_HILITE);
		
		for(node= snode->nodetree->nodes.first; node; node= node->next) {
			if(node->flag & SELECT) {
				node_basis_draw(snode, node);
				if(node->drawfunc) node->drawfunc(snode, node);
			}
		}

		uiDrawBlock(snode->block);
	}
	
	/* restore viewport (not needed yet) */
	mywinset(sa->win);

	/* ortho at pixel level curarea */
	myortho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);

	draw_area_emboss(sa);
	curarea->win_swap= WIN_BACK_OK;
}
