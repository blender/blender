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

#include "BIF_editview.h"
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
#include "BSE_node.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BDR_editobject.h"

#include "blendef.h"
#include "interface.h"	/* urm...  for rasterpos_safe, roundbox */
#include "PIL_time.h"
#include "mydevice.h"


/* **************** NODE draw callbacks ************* */

#define NODE_DY		20
#define NODE_SOCK	5

#define SOCK_IN		1
#define SOCK_OUT	2

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
		socket_circle_draw(sock->locx, sock->locy, NODE_SOCK);
		
		BIF_ThemeColor(TH_TEXT);
		ui_rasterpos_safe(sock->locx+8.0f, sock->locy-5.0f, snode->aspect);
		BIF_DrawString(snode->curfont, sock->name, trans);
	}
	
	for(sock= node->outputs.first; sock; sock= sock->next) {
		socket_circle_draw(sock->locx, sock->locy, NODE_SOCK);
		
		BIF_ThemeColor(TH_TEXT);
		slen= snode->aspect*BIF_GetStringWidth(snode->curfont, sock->name, trans);
		ui_rasterpos_safe(sock->locx-8.0f-slen, sock->locy-5.0f, snode->aspect);
		BIF_DrawString(snode->curfont, sock->name, trans);
	}
	
	return 0;
}

/* ************************** Node generic ************** */

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

/* ********************* transform ****************** */

/* releases on event, only intern (for extern see below) */
static void transform_nodes(SpaceNode *snode, char *undostr)
{
	bNode *node;
	float mxstart, mystart, mx, my, *oldlocs, *ol;
	int cont=1, tot=0, cancel=0, firsttime=1;
	short mval[2], mvalo[2];
	char str[64];
	
	/* count total */
	for(node= snode->nodetree->nodes.first; node; node= node->next)
		if(node->flag & SELECT) tot++;
	
	if(tot==0) return;
	
	/* store oldlocs */
	ol= oldlocs= MEM_mallocN(sizeof(float)*2*tot, "oldlocs transform");
	for(node= snode->nodetree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			ol[0]= node->locx; ol[1]= node->locy;
			ol+= 2;
		}
	}
	
	getmouseco_areawin(mvalo);
	areamouseco_to_ipoco(G.v2d, mvalo, &mxstart, &mystart);
	
	while(cont) {
		
		getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || firsttime) {
			firsttime= 0;
			
			areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			for(ol= oldlocs, node= snode->nodetree->nodes.first; node; node= node->next) {
				if(node->flag & SELECT) {
					node->locx= ol[0] + mx-mxstart;
					node->locy= ol[1] + my-mystart;
					node_update(snode->nodetree, node);
					ol+= 2;
				}
			}
			
			sprintf(str, "X: %.1f Y: %.1f", mx-mxstart, my-mystart);
			headerprint(str);
			force_draw(0);
		}
		else
			PIL_sleep_ms(10);
		
		while (qtest()) {
			short val;
			unsigned short event= extern_qread(&val);
			
			switch (event) {
				case LEFTMOUSE:
				case SPACEKEY:
				case RETKEY:
					cont=0;
					break;
				case ESCKEY:
				case RIGHTMOUSE:
					if(val) {
						cancel=1;
						cont=0;
					}
					break;
				default:
					if(val) arrows_move_cursor(event);
					break;
			}
		}
		
	}
	
	if(cancel) {
		for(ol= oldlocs, node= snode->nodetree->nodes.first; node; node= node->next) {
			if(node->flag & SELECT) {
				node->locx= ol[0];
				node->locy= ol[1];
				ol+= 2;
				node_update(snode->nodetree, node);
			}
		}
		
	}
	else
		BIF_undo_push(undostr);
	
	allqueue(REDRAWNODE, 1);
	MEM_freeN(oldlocs);
}

/* external call, also for callback */
void node_transform_ext(int mode, int unused)
{
	transform_nodes(curarea->spacedata.first, "Translate node");
}

/* ********************** select ******************** */

/* no undo here! */
void node_deselectall(SpaceNode *snode, int swap)
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


static void node_mouse_select(SpaceNode *snode)
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
			if(G.qual & LR_SHIFTKEY) {
				if(node->flag & SELECT)
					node->flag &= ~SELECT;
				else
					node->flag |= SELECT;
			}
			else 
				node->flag |= SELECT;
			
			break;
		}
	}
	
	/* not so nice (no event), but function below delays redraw otherwise */
	force_draw(0);
	
	std_rmouse_transform(node_transform_ext);	/* does undo push for select */
}

/* ****************** Add *********************** */

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
		
		/* fake links */
//		if(snode->nodetree->nodes.first!=snode->nodetree->nodes.last) {
//			bNode *first= snode->nodetree->nodes.first;
//			
//			nodeAddLink(snode->nodetree, first, first->outputs.first, node, node->inputs.first);
//		}
		
		allqueue(REDRAWNODE, 0);
	}
	
	BIF_undo_push("Add Node");

}

void node_adduplicate(SpaceNode *snode)
{
	bNode *node, *nnode;
	
	/* backwards, we add to list end */
	for(node= snode->nodetree->nodes.last; node; node= node->prev) {
		if(node->flag & SELECT) {
			nnode= nodeCopyNode(snode->nodetree, node);
			node->flag &= ~SELECT;
			nnode->flag |= SELECT;
		}
	}
	
	transform_nodes(snode, "Duplicate");
}

/* checks mouse position, and returns found node/socket */
/* type is SOCK_IN and/or SOCK_OUT */
static int find_indicated_socket(SpaceNode *snode, bNode **nodep, bNodeSocket **sockp, int type)
{
	bNode *node;
	bNodeSocket *sock;
	rctf rect;
	short mval[2];
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &rect.xmin, &rect.ymin);
	
	rect.xmin -= NODE_SOCK+3;
	rect.ymin -= NODE_SOCK+3;
	rect.xmax = rect.xmin + 2*NODE_SOCK+6;
	rect.ymax = rect.ymin + 2*NODE_SOCK+6;
	
	/* check if we click in a socket */
	for(node= snode->nodetree->nodes.first; node; node= node->next) {
		if(type & SOCK_IN) {
			for(sock= node->inputs.first; sock; sock= sock->next) {
				if(BLI_in_rctf(&rect, sock->locx, sock->locy)) {
					*nodep= node;
					*sockp= sock;
					return 1;
				}
			}
		}
		if(type & SOCK_OUT) {
			for(sock= node->outputs.first; sock; sock= sock->next) {
				if(BLI_in_rctf(&rect, sock->locx, sock->locy)) {
					*nodep= node;
					*sockp= sock;
					return 1;
				}
			}
		}
	}
	return 0;
}

/* loop that adds a link node, called by function below though */
static int node_draw_link_drag(SpaceNode *snode, bNode *node, bNodeSocket *sock, int type)
{
	bNode *tnode;
	bNodeSocket *tsock;
	bNodeLink *link= NULL;
	short mval[2], mvalo[2];
	
	/* we make a temporal link */
	if(type==SOCK_OUT)
		link= nodeAddLink(snode->nodetree, node, sock, NULL, NULL);
	else
		link= nodeAddLink(snode->nodetree, NULL, NULL, node, sock);
	
	getmouseco_areawin(mvalo);
	while (get_mbut() & L_MOUSE) {
		
		getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1]) {

			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			if(type==SOCK_OUT) {
				if(find_indicated_socket(snode, &tnode, &tsock, SOCK_IN)) {
					if(nodeFindLink(snode->nodetree, sock, tsock)==NULL) {
						if(tnode!=node) {
							link->tonode= tnode;
							link->tosock= tsock;
						}
					}
				}
				else {
					link->tonode= NULL;
					link->tosock= NULL;
				}
			}
			else {
				if(find_indicated_socket(snode, &tnode, &tsock, SOCK_OUT)) {
					if(nodeFindLink(snode->nodetree, sock, tsock)==NULL) {
						if(tnode!=node) {
							link->fromnode= tnode;
							link->fromsock= tsock;
						}
					}
				}
				else {
					link->fromnode= NULL;
					link->fromsock= NULL;
				}
			}
			
			force_draw(0);
		}
		else BIF_wait_for_statechange();		
	}
	
	if(link->tonode==NULL || link->fromnode==NULL) {
		BLI_remlink(&snode->nodetree->links, link);
		MEM_freeN(link);
	}
	
	allqueue(REDRAWNODE, 0);
	
	return 1;
}

static int node_draw_link(SpaceNode *snode)
{
	bNode *node;
	bNodeSocket *sock;
	
	/* we're going to draw an output */
	if(find_indicated_socket(snode, &node, &sock, SOCK_OUT)) {
		return node_draw_link_drag(snode, node, sock, SOCK_OUT);
	}
	if(find_indicated_socket(snode, &node, &sock, SOCK_IN)) {
		bNodeLink *link;
		
		/* find if we break a link */
		for(link= snode->nodetree->links.first; link; link= link->next) {
			if(link->tosock==sock)
				break;
		}
		if(link) {
			node= link->fromnode;
			sock= link->fromsock;
			BLI_remlink(&snode->nodetree->links, link);
			MEM_freeN(link);
			return node_draw_link_drag(snode, node, sock, SOCK_OUT);
		}
		else {
			/* link from input to output */
			return node_draw_link_drag(snode, node, sock, SOCK_IN);
		}
	}
	
	return 0;
}

/* ******************** main event loop ****************** */

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
			if(node_draw_link(snode)==0)
				node_mouse_select(snode);
			break;
			
		case RIGHTMOUSE: 
			node_mouse_select(snode);

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
			else if(G.qual==0) {
				node_deselectall(snode, 1);
				BIF_undo_push("Deselect all nodes");
			}
			break;
		case DKEY:
			if(G.qual==LR_SHIFTKEY)
				node_adduplicate(snode);
			break;
		case CKEY:
			break;
		case GKEY:
			transform_nodes(snode, "Translate Node");
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


