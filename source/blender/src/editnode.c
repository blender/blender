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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_material.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_language.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "blendef.h"
#include "interface.h"	/* urm...  for rasterpos_safe, roundbox */

#include "mydevice.h"


/* ***************************** */

#define NODE_DY		20

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
static void socket_circle_draw(float x, float y, float size)
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
	
	glColor3ub(200, 200, 40);
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
	
	BIF_ThemeColorShade(TH_HEADER, +30);
	uiSetRoundBox(3);
	uiRoundBox(rct->xmin, rct->ymax-NODE_DY, rct->xmax, rct->ymax, 8);
	
	BIF_ThemeColorShade(TH_HEADER, +10);
	uiSetRoundBox(12);
	uiRoundBox(rct->xmin, rct->ymin, rct->xmax, rct->ymax-NODE_DY, 8);
	
	ui_rasterpos_safe(rct->xmin+4.0f, rct->ymax-NODE_DY+5.0f, snode->aspect);
	
	if(node->flag & SELECT) 
		BIF_ThemeColor(TH_TEXT_HI);
	else
		BIF_ThemeColor(TH_TEXT);
		
	BIF_DrawString(snode->curfont, node->name, trans);
	
	for(sock= node->inputs.first; sock; sock= sock->next) {
		socket_circle_draw(sock->locx, sock->locy, 5.0f);
		
		BIF_ThemeColor(TH_TEXT);
		ui_rasterpos_safe(sock->locx+8.0f, sock->locy-5.0f, snode->aspect);
		BIF_DrawString(snode->curfont, sock->name, trans);
	}
	
	for(sock= node->outputs.first; sock; sock= sock->next) {
		socket_circle_draw(sock->locx, sock->locy, 5.0f);
		
		BIF_ThemeColor(TH_TEXT);
		slen= snode->aspect*BIF_GetStringWidth(snode->curfont, sock->name, trans);
		ui_rasterpos_safe(sock->locx-8.0f-slen, sock->locy-5.0f, snode->aspect);
		BIF_DrawString(snode->curfont, sock->name, trans);
	}
	
	return 0;
}

static void node_deselectall(SpaceNode *snode, int swap)
{
	bNode *node;
	
	if(swap) {
		for(node= snode->nodetree->nodes.first; node; node= node->next)
			if(node->flag & SELECT)
				break;
		if(node==NULL) {
			for(node= snode->nodetree->nodes.first; node; node= node->next)
				node->flag |= SELECT;
			allqueue(REDRAWNODE, 0);
			return;
		}
		/* else pass on to deselect */
	}
	
	for(node= snode->nodetree->nodes.first; node; node= node->next)
		node->flag &= ~SELECT;
	
	allqueue(REDRAWNODE, 0);
}


/* based on settings in tree and node, 
   - it fills it with appropriate callbacks 
   - sets drawing rect info */
void node_update(bNodeTree *ntree, bNode *node)
{
	bNodeSocket *nsock;
	float dy= 0.0f;
	float width= 80.0f;	/* width custom? */
	
	node->drawfunc= node_basis_draw;
	
	/* input connectors */
	for(nsock= node->inputs.last; nsock; nsock= nsock->prev) {
		nsock->locx= node->locx;
		nsock->locy= node->locy + dy + NODE_DY/2;
		dy+= NODE_DY;
	}
	
	/* spacer */
	dy+= NODE_DY/2;
	
	/* preview rect? */
	
	/* spacer */
	dy+= NODE_DY/2;
	
	/* output connectors */
	for(nsock= node->outputs.last; nsock; nsock= nsock->prev) {
		nsock->locx= node->locx + width;
		nsock->locy= node->locy + dy + NODE_DY/2;
		dy+= NODE_DY;
	}
	
	/* header */
	dy+= NODE_DY;

	node->tot.xmin= node->locx;
	node->tot.xmax= node->locx + width;
	node->tot.ymin= node->locy;
	node->tot.ymax= node->locy + dy;
}

/* editor context */
static void node_add_menu(SpaceNode *snode)
{
	short event, mval[2];
	
	getmouseco_areawin(mval);
	
	event= pupmenu("Add Node%t|Testnode%x1");
	if(event<1) return;
	
	node_deselectall(snode, 0);
	
	if(event==1) {
		bNodeSocket *sock;
		bNode *node= nodeAddNode(snode->nodetree, "TestNode");
		
		areamouseco_to_ipoco(G.v2d, mval, &node->locx, &node->locy);
		node->flag= SELECT;
		
		/* add fake sockets */
		sock= MEM_callocN(sizeof(bNodeSocket), "sock");
		strcpy(sock->name, "Col");
		BLI_addtail(&node->inputs, sock);
		sock= MEM_callocN(sizeof(bNodeSocket), "sock");
		strcpy(sock->name, "Spec");
		BLI_addtail(&node->inputs, sock);
		
		sock= MEM_callocN(sizeof(bNodeSocket), "sock");
		strcpy(sock->name, "Diffuse");
		BLI_addtail(&node->outputs, sock);
		
		node_update(snode->nodetree, node);
		
		allqueue(REDRAWNODE, 0);
	}
}

static void node_select(SpaceNode *snode)
{
	bNode *node;
	float mx, my;
	short mval[2];
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
	
	if((G.qual & LR_SHIFTKEY)==0)
		node_deselectall(snode, 0);
	
	for(node= snode->nodetree->nodes.first; node; node= node->next) {
		if(BLI_in_rctf(&node->tot, mx, my)) {
			node->flag |= SELECT;
		}
	}
	allqueue(REDRAWNODE, 0);
}


void winqreadnodespace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	SpaceNode *snode= spacedata;
	float dx;
	unsigned short event= evt->event;
	short val= evt->val, doredraw=0;
	
	if(sa->win==0) return;

	if(val) {
		
		if( uiDoBlocks(&sa->uiblocks, event)!=UI_NOTHING ) event= 0;

		switch(event) {
		case LEFTMOUSE:
			node_select(snode);
			break;
			
		case RIGHTMOUSE: 
			node_select(snode);

			break;
		case MIDDLEMOUSE:
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			view2dmove(event);	/* in drawipo.c */
			break;
		case PADPLUSKEY:
			dx= (float)(0.1154*(G.v2d->cur.xmax-G.v2d->cur.xmin));
			G.v2d->cur.xmin+= dx;
			G.v2d->cur.xmax-= dx;
			test_view2d(G.v2d, sa->winx, sa->winy);
			doredraw= 1;
			break;
		case PADMINUS:
			dx= (float)(0.15*(G.v2d->cur.xmax-G.v2d->cur.xmin));
			G.v2d->cur.xmin-= dx;
			G.v2d->cur.xmax+= dx;
			test_view2d(G.v2d, sa->winx, sa->winy);
			doredraw= 1;
			break;
		case HOMEKEY:
			doredraw= 1;
			break;
			
		case AKEY:
			if(G.qual==LR_SHIFTKEY)
				node_add_menu(snode);
			else if(G.qual==0)
				node_deselectall(snode, 1);
			break;
		case DKEY:
			break;
		case CKEY:
			break;
		case GKEY:
			break;
		case DELKEY:
		case XKEY:
			if( okee("Erase selected")==0 ) break;
			break;
		}
	}

	if(doredraw)
		scrarea_queue_winredraw(sa);
}


