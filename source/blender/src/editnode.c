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
#include "BSE_edit.h"
#include "BSE_headerbuttons.h"
#include "BSE_node.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BDR_editobject.h"

#include "blendef.h"
#include "interface.h"	/* urm...  for rasterpos_safe, roundbox */
#include "PIL_time.h"
#include "mydevice.h"

#define NODE_DY		20
#define NODE_DYS	10
#define NODE_SOCK	5

/* **************** NODE draw callbacks ************* */

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

/* ************************** Node generic ************** */

/* based on settings in tree and node, 
   - it fills it with appropriate callbacks 
   - sets drawing rect info */
void node_update(bNodeTree *ntree, bNode *node)
{
	bNodeSocket *nsock;
	float dy= node->locy;
	
	/* input connectors */
	for(nsock= node->inputs.last; nsock; nsock= nsock->prev) {
		nsock->locx= node->locx;
		nsock->locy= dy + NODE_DYS;
		dy+= NODE_DY;
	}
	
	/* spacer */
	dy+= NODE_DYS;
	
	/* preview rect? */
	node->prv.xmin= node->locx + NODE_DYS;
	node->prv.xmax= node->locx + node->width- NODE_DYS;
	node->prv.ymin= dy;
	node->prv.ymax= dy+=node->prv_h;
	
	/* spacer */
	dy+= NODE_DYS;
	
	/* output connectors */
	for(nsock= node->outputs.last; nsock; nsock= nsock->prev) {
		nsock->locx= node->locx + node->width;
		nsock->locy= dy + NODE_DYS;
		dy+= NODE_DY;
	}
	
	/* header */
	dy+= NODE_DY;

	node->tot.xmin= node->locx;
	node->tot.xmax= node->locx + node->width;
	node->tot.ymin= node->locy;
	node->tot.ymax= dy;
}

/* checks mouse position, and returns found node/socket */
/* type is SOCK_IN and/or SOCK_OUT */
static int find_indicated_socket(SpaceNode *snode, bNode **nodep, bNodeSocket **sockp, int type, int in_out)
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
		if(in_out & SOCK_IN) {
			for(sock= node->inputs.first; sock; sock= sock->next) {
				if(type==-1 || type==sock->type) {
					if(BLI_in_rctf(&rect, sock->locx, sock->locy)) {
						*nodep= node;
						*sockp= sock;
						return 1;
					}
				}
			}
		}
		if(in_out & SOCK_OUT) {
			for(sock= node->outputs.first; sock; sock= sock->next) {
				if(type==-1 || type==sock->type) {
					if(BLI_in_rctf(&rect, sock->locx, sock->locy)) {
						*nodep= node;
						*sockp= sock;
						return 1;
					}
				}
			}
		}
	}
	return 0;
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

static int node_socket_hilights(SpaceNode *snode, int type, int in_out)
{
	bNode *node;
	bNodeSocket *sock, *tsock, *socksel= NULL;
	float mx, my;
	short mval[2], redraw= 0;
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
	
	/* deselect socks */
	for(node= snode->nodetree->nodes.first; node; node= node->next) {
		for(sock= node->inputs.first; sock; sock= sock->next) {
			if(sock->flag & SELECT) {
				sock->flag &= ~SELECT;
				redraw++;
				socksel= sock;
			}
		}
		for(sock= node->outputs.first; sock; sock= sock->next) {
			if(sock->flag & SELECT) {
				sock->flag &= ~SELECT;
				redraw++;
				socksel= sock;
			}
		}
	}
	
	if(find_indicated_socket(snode, &node, &tsock, type, in_out)) {
		tsock->flag |= SELECT;
		if(redraw==1 && tsock==socksel) redraw= 0;
		else redraw= 1;
	}
	
	return redraw;
}

void node_border_select(SpaceNode *snode)
{
	bNode *node;
	rcti rect;
	rctf rectf;
	short val, mval[2];
	
	if ( (val = get_border(&rect, 3)) ) {
		
		mval[0]= rect.xmin;
		mval[1]= rect.ymin;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
		
		for(node= snode->nodetree->nodes.first; node; node= node->next) {
			if(BLI_isect_rctf(&rectf, &node->tot, NULL)) {
				if(val==LEFTMOUSE)
					node->flag |= SELECT;
				else
					node->flag &= ~SELECT;
			}
		}
		allqueue(REDRAWNODE, 1);
		BIF_undo_push("Border select nodes");
	}		
}

/* ****************** Add *********************** */

/* keep adding nodes outside of space context? to kernel maybe? */

bNode *add_test_node(bNodeTree *ntree, float locx, float locy)
{
	bNode *node= nodeAddNode(ntree, "TestNode");
	static int tot= 0;
	
	sprintf(node->name, "Testnode%d", tot++);
	
	node->locx= locx;
	node->locy= locy;
	node->width= 80.0f;
	node->drawfunc= node_basis_draw;
	
	/* add fake sockets */
	nodeAddSocket(node, SOCK_RGBA, SOCK_IN, 1, "Col");
	nodeAddSocket(node, SOCK_RGBA, SOCK_IN, 1, "Spec");
	nodeAddSocket(node, SOCK_RGBA, SOCK_OUT, 0xFFF, "Diffuse");
	
	/* always end with calculating size etc */
	node_update(ntree, node);
	
	return node;
}

static int value_drawfunc(SpaceNode *snode, bNode *node)
{
	
	node_basis_draw(snode, node);
	
	if(snode->block) {
		uiBut *bt;
		
		bt= uiDefButF(snode->block, NUM, B_NOP, "", 
					  node->prv.xmin, node->prv.ymin, node->prv.xmax-node->prv.xmin, node->prv.ymax-node->prv.ymin, 
					  node->vec, 0.0f, 1.0f, 100, 2, "");

	}
	
	return 1;
}

static int hsv_drawfunc(SpaceNode *snode, bNode *node)
{
	
	node_basis_draw(snode, node);
	
	if(snode->block) {
		uiBut *bt;
		uiBlockSetEmboss(snode->block, UI_EMBOSSP);
		
		bt= uiDefButF(snode->block, HSVCUBE, B_NOP, "", 
					node->prv.xmin, node->prv.ymin, node->prv.xmax-node->prv.xmin, 10.0f, 
					node->vec, 0.0f, 1.0f, 3, 0, "");
		bt= uiDefButF(snode->block, HSVCUBE, B_NOP, "", 
					node->prv.xmin, node->prv.ymin+14.0f, node->prv.xmax-node->prv.xmin, node->prv.ymax-node->prv.ymin-14.0f, 
					node->vec, 0.0f, 1.0f, 2, 0, "");
		
		uiDefButF(snode->block, COL, B_NOP, "",		
					node->prv.xmin, node->prv.ymax+10.0f, node->prv.xmax-node->prv.xmin, 15.0f, 
					node->vec, 0.0, 0.0, -1, 0, "");

	}
	
	return 1;
}


bNode *add_value_node(bNodeTree *ntree, float locx, float locy)
{
	bNode *node= nodeAddNode(ntree, "Value");
	
	node->locx= locx;
	node->locy= locy;
	node->width= 80.0f;
	node->prv_h= 20.0f;
	node->drawfunc= value_drawfunc;
	
	/* add sockets */
	nodeAddSocket(node, SOCK_VALUE, SOCK_OUT, 0xFFF, "");
	
	/* always end with calculating size etc */
	node_update(ntree, node);
	
	return node;
}

bNode *add_hsv_node(bNodeTree *ntree, float locx, float locy)
{
	bNode *node= nodeAddNode(ntree, "RGB");
	
	node->locx= locx;
	node->locy= locy;
	node->width= 100.0f;
	node->prv_h= 100.0f;
	node->vec[3]= 1.0f;		/* alpha init */
	node->drawfunc= hsv_drawfunc;	
	
	/* add sockets */
	nodeAddSocket(node, SOCK_RGBA, SOCK_OUT, 0xFFF, "");
	
	/* always end with calculating size etc */
	node_update(ntree, node);
	
	return node;
}


/* editor context */
static void node_add_menu(SpaceNode *snode)
{
	float locx, locy;
	short event, mval[2];
	
	event= pupmenu("Add Node%t|Testnode%x1|Value %x2|Color %x3");
	if(event<1) return;
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &locx, &locy);
	
	node_deselectall(snode, 0);
	
	if(event==1)
		add_test_node(snode->nodetree, locx, locy);
	else if(event==2)
		add_value_node(snode->nodetree, locx, locy);
	else if(event==3)
		add_hsv_node(snode->nodetree, locx, locy);

	
	allqueue(REDRAWNODE, 0);
	BIF_undo_push("Add Node");
}

void node_adduplicate(SpaceNode *snode)
{
	bNode *node, *nnode;
	bNodeLink *link, *nlink;
	bNodeSocket *sock;
	int a;
	
	/* backwards, we add to list end */
	for(node= snode->nodetree->nodes.last; node; node= node->prev) {
		node->new= NULL;
		if(node->flag & SELECT) {
			nnode= nodeCopyNode(snode->nodetree, node);
			node->flag &= ~SELECT;
			nnode->flag |= SELECT;
			node->new= nnode;
		}
	}
	
	/* check for copying links */
	for(link= snode->nodetree->links.first; link; link= link->next) {
		if(link->fromnode->new && link->tonode->new) {
			nlink= nodeAddLink(snode->nodetree, link->fromnode->new, NULL, link->tonode->new, NULL);
			/* sockets were copied in order */
			for(a=0, sock= link->fromnode->outputs.first; sock; sock= sock->next, a++) {
				if(sock==link->fromsock)
					break;
			}
			nlink->fromsock= BLI_findlink(&link->fromnode->new->outputs, a);
			
			for(a=0, sock= link->tonode->inputs.first; sock; sock= sock->next, a++) {
				if(sock==link->tosock)
					break;
			}
			nlink->tosock= BLI_findlink(&link->tonode->new->inputs, a);
		}
	}
	
	transform_nodes(snode, "Duplicate");
}

/* loop that adds a nodelink, called by function below  */
/* type = starting socket */
static int node_draw_link_drag(SpaceNode *snode, bNode *node, bNodeSocket *sock, int in_out)
{
	bNode *tnode;
	bNodeSocket *tsock;
	bNodeLink *link= NULL;
	short mval[2], mvalo[2];
	
	/* we make a temporal link */
	if(in_out==SOCK_OUT)
		link= nodeAddLink(snode->nodetree, node, sock, NULL, NULL);
	else
		link= nodeAddLink(snode->nodetree, NULL, NULL, node, sock);
	
	getmouseco_areawin(mvalo);
	while (get_mbut() & L_MOUSE) {
		
		getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1]) {

			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			if(in_out==SOCK_OUT) {
				if(find_indicated_socket(snode, &tnode, &tsock, sock->type, SOCK_IN)) {
					if(nodeFindLink(snode->nodetree, sock, tsock)==NULL) {
						if(nodeCountSocketLinks(snode->nodetree, tsock) < tsock->limit) {
							if(tnode!=node) {
								link->tonode= tnode;
								link->tosock= tsock;
							}
						}
					}
				}
				else {
					link->tonode= NULL;
					link->tosock= NULL;
				}
			}
			else {
				if(find_indicated_socket(snode, &tnode, &tsock, sock->type, SOCK_OUT)) {
					if(nodeFindLink(snode->nodetree, sock, tsock)==NULL) {
						if(nodeCountSocketLinks(snode->nodetree, tsock) < tsock->limit) {
							if(tnode!=node) {
								link->fromnode= tnode;
								link->fromsock= tsock;
							}
						}
					}
				}
				else {
					link->fromnode= NULL;
					link->fromsock= NULL;
				}
			}
			/* hilight target sockets only */
			node_socket_hilights(snode, sock->type, in_out==SOCK_OUT?SOCK_IN:SOCK_OUT);
			
			force_draw(0);
		}
		else BIF_wait_for_statechange();		
	}
	
	if(link->tonode==NULL || link->fromnode==NULL) {
		BLI_remlink(&snode->nodetree->links, link);
		MEM_freeN(link);
	}
	
	nodeSolveOrder(snode->nodetree);
	
	allqueue(REDRAWNODE, 0);
	
	return 1;
}

static int node_draw_link(SpaceNode *snode)
{
	bNode *node;
	bNodeLink *link;
	bNodeSocket *sock;
	
	/* output indicated? */
	if(find_indicated_socket(snode, &node, &sock, -1, SOCK_OUT)) {
		if(nodeCountSocketLinks(snode->nodetree, sock)<sock->limit)
			return node_draw_link_drag(snode, node, sock, SOCK_OUT);
		else {
			/* find if we break a link */
			for(link= snode->nodetree->links.first; link; link= link->next) {
				if(link->fromsock==sock)
					break;
			}
			if(link) {
				node= link->tonode;
				sock= link->tosock;
				BLI_remlink(&snode->nodetree->links, link);
				MEM_freeN(link);
				return node_draw_link_drag(snode, node, sock, SOCK_IN);
			}
		}
	}
	/* or an input? */
	else if(find_indicated_socket(snode, &node, &sock, -1, SOCK_IN)) {
		if(nodeCountSocketLinks(snode->nodetree, sock)<sock->limit)
			return node_draw_link_drag(snode, node, sock, SOCK_IN);
		else {
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
		}
	}
	
	return 0;
}

static void node_delete(SpaceNode *snode)
{
	bNode *node, *next;
	
	for(node= snode->nodetree->nodes.first; node; node= next) {
		next= node->next;
		if(node->flag & SELECT)
			nodeFreeNode(snode->nodetree, node);
	}
	
	BIF_undo_push("Delete nodes");
	allqueue(REDRAWNODE, 0);
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
			
		case MOUSEY:
			doredraw= node_socket_hilights(snode, -1, SOCK_IN|SOCK_OUT);
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
		case BKEY:
			if(G.qual==0)
				node_border_select(snode);
			break;
		case DKEY:
			if(G.qual==LR_SHIFTKEY)
				node_adduplicate(snode);
			break;
		case CKEY:	/* sort again, showing cyclics */
			nodeSolveOrder(snode->nodetree);
			doredraw= 1;
			break;
		case GKEY:
			transform_nodes(snode, "Translate Node");
			break;
		case DELKEY:
		case XKEY:
			if( okee("Erase selected")==0 ) break;
			node_delete(snode);
			break;
		}
	}

	if(doredraw)
		scrarea_queue_winredraw(sa);
}


