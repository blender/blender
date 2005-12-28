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

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BIF_editview.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_previewrender.h"
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
#include "butspace.h"
#include "PIL_time.h"
#include "mydevice.h"

/* currently called from BIF_preview_changed */
void snode_tag_dirty(SpaceNode *snode)
{
	bNode *node;
	
	if(snode->treetype==NTREE_SHADER) {
		if(snode->nodetree) {
			for(node= snode->nodetree->nodes.first; node; node= node->next) {
				if(node->type==SH_NODE_OUTPUT)
					node->lasty= 0;
			}
			snode->flag |= SNODE_DO_PREVIEW;	/* this adds an afterqueue on a redraw, to allow button previews to work first */
		}
	}
	allqueue(REDRAWNODE, 1);
}

static void shader_node_previewrender(ScrArea *sa, SpaceNode *snode)
{
	bNode *node;
	
	if(snode->id==NULL) return;
	if( ((Material *)snode->id )->use_nodes==0 ) return;

	for(node= snode->nodetree->nodes.first; node; node= node->next) {
		if(node->type==SH_NODE_OUTPUT) {
			if(node->flag & NODE_DO_OUTPUT) {
				if(node->lasty<PREVIEW_RENDERSIZE-2) {
					RenderInfo ri;	
					int test= node->lasty;
					
					ri.cury = node->lasty;
					ri.rect = NULL;
					ri.pr_rectx = PREVIEW_RENDERSIZE;
					ri.pr_recty = PREVIEW_RENDERSIZE;
					
					BIF_previewrender(snode->id, &ri, NULL, PR_DO_RENDER);	/* sends redraw event */
					if(ri.rect) MEM_freeN(ri.rect);
					
					if(node->lasty<PREVIEW_RENDERSIZE-2)
						addafterqueue(sa->win, RENDERPREVIEW, 1);
//					if(test!=node->lasty)
//						printf("node rendered y %d to %d\n", test, node->lasty);

					break;
				}
			}
		}
	}
}


static void snode_handle_recalc(SpaceNode *snode)
{
	if(snode->treetype==NTREE_SHADER) {
		BIF_preview_changed(ID_MA);	 /* signals buttons windows and node editors */
	}
	else
		allqueue(REDRAWNODE, 1);
}

/* assumes nothing being done in ntree yet, sets the default in/out node */
/* called from shading buttons or header */
void node_shader_default(Material *ma)
{
	bNode *in, *out;
	bNodeSocket *fromsock, *tosock;
	
	/* but lets check it anyway */
	if(ma->nodetree) {
		printf("error in shader initialize\n");
		return;
	}
	
	ma->nodetree= ntreeAddTree(NTREE_SHADER);
	
	in= nodeAddNodeType(ma->nodetree, SH_NODE_INPUT);
	in->locx= 10.0f; in->locy= 200.0f;
	out= nodeAddNodeType(ma->nodetree, SH_NODE_OUTPUT);
	out->locx= 200.0f; out->locy= 200.0f;
	
	/* only a link from color to color */
	fromsock= in->outputs.first;
	tosock= out->inputs.first;
	nodeAddLink(ma->nodetree, in, fromsock, out, tosock);
	
	ntreeSolveOrder(ma->nodetree);	/* needed for pointers */
}

/* even called for each redraw now, so keep it fast :) */
void snode_set_context(SpaceNode *snode)
{
	Object *ob= OBACT;
	
	snode->nodetree= NULL;
	snode->id= snode->from= NULL;
	
	if(snode->treetype==NTREE_SHADER) {
		/* need active object, or we allow pinning... */
		if(ob) {
			Material *ma= give_current_material(ob, ob->actcol);
			if(ma) {
				snode->from= &ob->id;
				snode->id= &ma->id;
				snode->nodetree= ma->nodetree;
			}
		}
	}
}

/* ************************** Node generic ************** */

static void snode_home(ScrArea *sa, SpaceNode *snode)
{
	bNode *node;
	int first= 1;
	
	snode->v2d.cur.xmin= snode->v2d.cur.ymin= 0.0f;
	snode->v2d.cur.xmax= sa->winx;
	snode->v2d.cur.xmax= sa->winy;
	
	for(node= snode->nodetree->nodes.first; node; node= node->next) {
		if(first) {
			first= 0;
			snode->v2d.cur= node->totr;
		}
		else {
			BLI_union_rctf(&snode->v2d.cur, &node->totr);
		}
	}
	snode->v2d.tot= snode->v2d.cur;
	test_view2d(G.v2d, sa->winx, sa->winy);
	
}

/* checks mouse position, and returns found node/socket */
/* type is SOCK_IN and/or SOCK_OUT */
static int find_indicated_socket(SpaceNode *snode, bNode **nodep, bNodeSocket **sockp, int in_out)
{
	bNode *node;
	bNodeSocket *sock;
	rctf rect;
	short mval[2];
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &rect.xmin, &rect.ymin);
	
	rect.xmin -= NODE_SOCKSIZE+3;
	rect.ymin -= NODE_SOCKSIZE+3;
	rect.xmax = rect.xmin + 2*NODE_SOCKSIZE+6;
	rect.ymax = rect.ymin + 2*NODE_SOCKSIZE+6;
	
	/* check if we click in a socket */
	for(node= snode->nodetree->nodes.first; node; node= node->next) {
		if(in_out & SOCK_IN) {
			for(sock= node->inputs.first; sock; sock= sock->next) {
				if(BLI_in_rctf(&rect, sock->locx, sock->locy)) {
					*nodep= node;
					*sockp= sock;
					return 1;
				}
			}
		}
		if(in_out & SOCK_OUT) {
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

/* ********************* transform ****************** */

/* releases on event, only intern (for extern see below) */
static void transform_nodes(SpaceNode *snode, char mode, char *undostr)
{
	bNode *node;
	float mxstart, mystart, mx, my, *oldlocs, *ol;
	int cont=1, tot=0, cancel=0, firsttime=1;
	short mval[2], mvalo[2];
	
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
//			char str[64];

			firsttime= 0;
			
			areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			for(ol= oldlocs, node= snode->nodetree->nodes.first; node; node= node->next) {
				if(node->flag & SELECT) {
					node->locx= ol[0] + mx-mxstart;
					node->locy= ol[1] + my-mystart;
					ol+= 2;
				}
			}
			
//			sprintf(str, "X: %.1f Y: %.1f", mx-mxstart, my-mystart);
//			headerprint(str);
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
	transform_nodes(curarea->spacedata.first, 'g', "Translate node");
}


/* releases on event, only 1 node */
static void scale_node(SpaceNode *snode, bNode *node)
{
	float mxstart, mystart, mx, my, oldwidth;
	int cont=1, cancel=0;
	short mval[2], mvalo[2];
	
	/* store old */
	oldwidth= node->width;
	
	getmouseco_areawin(mvalo);
	areamouseco_to_ipoco(G.v2d, mvalo, &mxstart, &mystart);
	
	while(cont) {
		
		getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1]) {
			
			areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			node->width= oldwidth + mx-mxstart;
			CLAMP(node->width, node->typeinfo->minwidth, node->typeinfo->maxwidth);
			
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
			}
		}
		
	}
	
	if(cancel) {
		node->width= oldwidth;
	}
	else
		BIF_undo_push("Scale Node");
	
	allqueue(REDRAWNODE, 1);
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

/* two active flags, ID nodes have special flag for buttons display */
static void node_set_active(SpaceNode *snode, bNode *node)
{
	bNode *tnode;
	
	/* make sure only one node is active, and only one per ID type */
	for(tnode= snode->nodetree->nodes.first; tnode; tnode= tnode->next) {
		tnode->flag &= ~NODE_ACTIVE;
		
		/* activate input/output will de-active all node-id types */
		if(node->typeinfo->nclass==NODE_CLASS_INPUT || node->typeinfo->nclass==NODE_CLASS_OUTPUT)
			tnode->flag &= ~NODE_ACTIVE_ID;
		
		if(node->id && tnode->id) {
			if(GS(node->id->name) == GS(tnode->id->name))
				tnode->flag &= ~NODE_ACTIVE_ID;
		}
	}
	
	node->flag |= NODE_ACTIVE;
	if(node->id)
		node->flag |= NODE_ACTIVE_ID;
	
	/* tree specific activate calls */
	if(snode->treetype==NTREE_SHADER) {
		allqueue(REDRAWBUTSSHADING, 1);
	}
}

/* return 0: nothing done */
static int node_mouse_select(SpaceNode *snode, unsigned short event)
{
	bNode *node;
	float mx, my;
	short mval[2];
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
	
	/* first check for the headers or scaling widget */
	for(node= snode->nodetree->nodes.first; node; node= node->next) {
		if((node->flag & NODE_HIDDEN)==0) {
			rctf totr= node->totr;
			totr.ymin= totr.ymax-20.0f;
			totr.xmin= totr.xmax-18.0f;
			
			if(node->typeinfo->flag & NODE_PREVIEW) {
				if(BLI_in_rctf(&totr, mx, my)) {
					node->flag ^= NODE_PREVIEW;
					allqueue(REDRAWNODE, 0);
					return 1;
				}
				totr.xmin-=18.0f;
			}
			if(node->typeinfo->flag & NODE_OPTIONS) {
				if(BLI_in_rctf(&totr, mx, my)) {
					node->flag ^= NODE_OPTIONS;
					allqueue(REDRAWNODE, 0);
					return 1;
				}
			}
			
			totr= node->totr;
			totr.xmin= totr.xmax-10.0f;
			totr.ymax= totr.ymin+10.0f;
			if(BLI_in_rctf(&totr, mx, my)) {
				scale_node(snode, node);
				return 1;
			}
		}
	}
	
	for(node= snode->nodetree->nodes.first; node; node= node->next) {
		if(BLI_in_rctf(&node->totr, mx, my))
			break;
	}
	if(node) {
		if((G.qual & LR_SHIFTKEY)==0)
			node_deselectall(snode, 0);
		
		if(G.qual & LR_SHIFTKEY) {
			if(node->flag & SELECT)
				node->flag &= ~SELECT;
			else
				node->flag |= SELECT;
		}
		else 
			node->flag |= SELECT;
		
		node_set_active(snode, node);
		
		/* not so nice (no event), but function below delays redraw otherwise */
		force_draw(0);
		
		std_rmouse_transform(node_transform_ext);	/* does undo push for select */
		
		return 1;
	}
	return 0;
}

static int node_socket_hilights(SpaceNode *snode, int in_out)
{
	bNode *node;
	bNodeSocket *sock, *tsock, *socksel= NULL;
	float mx, my;
	short mval[2], redraw= 0;
	
	if(snode->nodetree==NULL) return 0;
	
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
	
	if(find_indicated_socket(snode, &node, &tsock, in_out)) {
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
			if(BLI_isect_rctf(&rectf, &node->totr, NULL)) {
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

/* can be called from menus too, but they should do own undopush and redraws */
bNode *node_add_shadernode(SpaceNode *snode, int type, float locx, float locy)
{
	bNode *node= NULL;
	
	node_deselectall(snode, 0);
	
	node= nodeAddNodeType(snode->nodetree, type);
	
	/* generics */
	if(node) {
		node->locx= locx;
		node->locy= locy;
		node->flag |= SELECT;
		
		/* custom storage, will become handlerized.. */
		if(node->type==SH_NODE_VALTORGB)
			node->storage= add_colorband(1);
		else if(node->type==SH_NODE_MATERIAL)
			node->custom1= SH_NODE_MAT_DIFF|SH_NODE_MAT_SPEC;
		
		node_set_active(snode, node);
	}
	return node;
}

/* hotkey context */
static void node_add_menu(SpaceNode *snode)
{
	float locx, locy;
	short event, mval[2];
	
	/* shader menu, still hardcoded defines... solve */
	event= pupmenu("Add Node%t|Input%x0|Output%x1|Material%x100|Texture%x106|Value %x102|Color %x101|Mix Color %x103|ColorRamp %x104|Color to BW %x105");
	if(event<1) return;
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &locx, &locy);
	node_add_shadernode(snode, event, locx, locy+40.0f);
	
	snode_handle_recalc(snode);
	
	BIF_undo_push("Add Node");
}

void node_adduplicate(SpaceNode *snode)
{
	
	ntreeCopyTree(snode->nodetree, 1);	/* 1 == internally selected nodes */
	ntreeSolveOrder(snode->nodetree);
	snode_handle_recalc(snode);

	transform_nodes(snode, 'g', "Duplicate");
}

static void node_insert_convertor(SpaceNode *snode, bNodeLink *link)
{
	bNode *newnode= NULL;
	
	if(snode->nodetree->type==NTREE_SHADER) {
		if(link->fromsock->type==SOCK_RGBA && link->tosock->type==SOCK_VALUE) {
			newnode= node_add_shadernode(snode, SH_NODE_RGBTOBW, 0.0f, 0.0f);
		}
		else if(link->fromsock->type==SOCK_VALUE && link->tosock->type==SOCK_RGBA) {
			newnode= node_add_shadernode(snode, SH_NODE_VALTORGB, 0.0f, 0.0f);
		}
		
		if(newnode) {
			/* dangerous assumption to use first in/out socks, but thats fine for now */
			newnode->flag |= NODE_HIDDEN;
			newnode->locx= 0.5f*(link->fromsock->locx + link->tosock->locx);
			newnode->locy= 0.5f*(link->fromsock->locy + link->tosock->locy) + HIDDEN_RAD;
			
			nodeAddLink(snode->nodetree, newnode, newnode->outputs.first, link->tonode, link->tosock);
			link->tonode= newnode;
			link->tosock= newnode->inputs.first;
		}
	}
}


/* loop that adds a nodelink, called by function below  */
/* in_out = starting socket */
static int node_add_link_drag(SpaceNode *snode, bNode *node, bNodeSocket *sock, int in_out)
{
	bNode *tnode;
	bNodeSocket *tsock;
	bNodeLink *link= NULL;
	short mval[2], mvalo[2], firsttime=1;	/* firsttime reconnects a link broken by caller */
	
	/* we make a temporal link */
	if(in_out==SOCK_OUT)
		link= nodeAddLink(snode->nodetree, node, sock, NULL, NULL);
	else
		link= nodeAddLink(snode->nodetree, NULL, NULL, node, sock);
	
	getmouseco_areawin(mvalo);
	while (get_mbut() & L_MOUSE) {
		
		getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || firsttime) {
			firsttime= 0;
			
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			if(in_out==SOCK_OUT) {
				if(find_indicated_socket(snode, &tnode, &tsock, SOCK_IN)) {
					if(nodeFindLink(snode->nodetree, sock, tsock)==NULL) {
							if(tnode!=node  && link->tonode!=tnode && link->tosock!= tsock) {
								link->tonode= tnode;
								link->tosock= tsock;
								ntreeSolveOrder(snode->nodetree);	/* for interactive red line warning */
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
						if(nodeCountSocketLinks(snode->nodetree, tsock) < tsock->limit) {
							if(tnode!=node && link->fromnode!=tnode && link->fromsock!= tsock) {
								link->fromnode= tnode;
								link->fromsock= tsock;
								ntreeSolveOrder(snode->nodetree);	/* for interactive red line warning */
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
			node_socket_hilights(snode, in_out==SOCK_OUT?SOCK_IN:SOCK_OUT);
			
			force_draw(0);
		}
		else BIF_wait_for_statechange();		
	}
	
	if(link->tonode==NULL || link->fromnode==NULL) {
		nodeRemLink(snode->nodetree, link);
	}
	else {
		bNodeLink *tlink;
		/* we might need to remove a link */
		if(in_out==SOCK_OUT) {
			if(nodeCountSocketLinks(snode->nodetree, link->tosock) > tsock->limit) {
				
				for(tlink= snode->nodetree->links.first; tlink; tlink= tlink->next) {
					if(link!=tlink && tlink->tosock==link->tosock)
						break;
				}
				if(tlink) {
					/* is there a free input socket with same type? */
					for(tsock= tlink->tonode->inputs.first; tsock; tsock= tsock->next) {
						if(tsock->type==sock->type)
							if(nodeCountSocketLinks(snode->nodetree, tsock) < tsock->limit)
								break;
					}
					if(tsock)
						tlink->tosock= tsock;
					else {
						nodeRemLink(snode->nodetree, tlink);
					}
				}					
			}
		}
		
		/* and last trick: insert a convertor when types dont match */
		if(link->tosock->type!=link->fromsock->type) {
			node_insert_convertor(snode, link);
			ntreeSolveOrder(snode->nodetree);		/* so nice do it twice! well, the sort-order can only handle 1 added link at a time */
		}
	}
	
	ntreeSolveOrder(snode->nodetree);
	snode_handle_recalc(snode);
	allqueue(REDRAWNODE, 0);
	BIF_undo_push("Add link");

	return 1;
}

/* return 1 when socket clicked */
static int node_add_link(SpaceNode *snode)
{
	bNode *node;
	bNodeLink *link;
	bNodeSocket *sock;
	
	/* output indicated? */
	if(find_indicated_socket(snode, &node, &sock, SOCK_OUT)) {
		if(nodeCountSocketLinks(snode->nodetree, sock)<sock->limit)
			return node_add_link_drag(snode, node, sock, SOCK_OUT);
		else {
			/* find if we break a link */
			for(link= snode->nodetree->links.first; link; link= link->next) {
				if(link->fromsock==sock)
					break;
			}
			if(link) {
				node= link->tonode;
				sock= link->tosock;
				nodeRemLink(snode->nodetree, link);
				return node_add_link_drag(snode, node, sock, SOCK_IN);
			}
		}
	}
	/* or an input? */
	else if(find_indicated_socket(snode, &node, &sock, SOCK_IN)) {
		if(nodeCountSocketLinks(snode->nodetree, sock)<sock->limit)
			return node_add_link_drag(snode, node, sock, SOCK_IN);
		else {
			/* find if we break a link */
			for(link= snode->nodetree->links.first; link; link= link->next) {
				if(link->tosock==sock)
					break;
			}
			if(link) {
				node= link->fromnode;
				sock= link->fromsock;
				nodeRemLink(snode->nodetree, link);
				return node_add_link_drag(snode, node, sock, SOCK_OUT);
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
	
	snode_handle_recalc(snode);
	BIF_undo_push("Delete nodes");
	allqueue(REDRAWNODE, 1);
}

static void node_hide(SpaceNode *snode)
{
	bNode *node;
	int nothidden=0, ishidden=0;
	
	for(node= snode->nodetree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if(node->flag & NODE_HIDDEN)
				ishidden++;
			else
				nothidden++;
		}
	}
	for(node= snode->nodetree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if( (ishidden && nothidden) || ishidden==0)
				node->flag |= NODE_HIDDEN;
			else 
				node->flag &= ~NODE_HIDDEN;
		}
	}
	BIF_undo_push("Hide nodes");
	allqueue(REDRAWNODE, 1);
}
			

static void node_border_link_delete(SpaceNode *snode)
{
	rcti rect;
	short val, mval[2];

	setcursor_space(SPACE_NODE, CURSOR_VPAINT);
	
	if ( (val = get_border(&rect, 2)) ) {
		if(rect.xmin<rect.xmax && rect.ymin<rect.ymax) {
			//#define NODE_MAXPICKBUF	256
			bNodeLink *link, *next;
			GLuint buffer[256];
			rctf rectf;
			int code=0, hits;
			
			mval[0]= rect.xmin;
			mval[1]= rect.ymin;
			areamouseco_to_ipoco(&snode->v2d, mval, &rectf.xmin, &rectf.ymin);
			mval[0]= rect.xmax;
			mval[1]= rect.ymax;
			areamouseco_to_ipoco(&snode->v2d, mval, &rectf.xmax, &rectf.ymax);
			
			myortho2(rectf.xmin, rectf.xmax, rectf.ymin, rectf.ymax);
			
			glSelectBuffer(256, buffer); 
			glRenderMode(GL_SELECT);
			glInitNames();
			glPushName(-1);
			
			/* draw links */
			for(link= snode->nodetree->links.first; link; link= link->next) {
				glLoadName(code++);
				node_draw_link(snode, link);
			}
			
			hits= glRenderMode(GL_RENDER);
			glPopName();
			if(hits>0) {
				int a;
				for(a=0; a<hits; a++) {
					bNodeLink *link= BLI_findlink(&snode->nodetree->links, buffer[ (4 * a) + 3]);
					if(link)
						link->tonode= NULL;	/* first tag for delete, otherwise indices are wrong */
				}
				for(link= snode->nodetree->links.first; link; link= next) {
					next= link->next;
					if(link->tonode==NULL) {
						nodeRemLink(snode->nodetree, link);
					}
				}
				ntreeSolveOrder(snode->nodetree);
				snode_handle_recalc(snode);
			}
			allqueue(REDRAWNODE, 0);
			BIF_undo_push("Erase links");
		}
	}
	
	setcursor_space(SPACE_NODE, CURSOR_STD);

}
/* ********************** */

static void convert_nodes(SpaceNode *snode)
{
	bNode *node, *bnode, *prevnode;
	bNodeSocket *fromsock, *tosock;
	Material *mat= (Material *)snode->id;
	MaterialLayer *ml;
	float locx= 200;
	
	if(GS(mat->id.name)!=ID_MA) return;
	
	prevnode= snode->nodetree->nodes.first;
	
	for(ml= mat->layers.first; ml; ml= ml->next) {
		if(ml->mat) {
			node= nodeAddNodeType(snode->nodetree, SH_NODE_MATERIAL);
			node->id= (ID *)ml->mat;
			node->locx= locx; locx+= 100;
			node->locy= 300;
			
			bnode= nodeAddNodeType(snode->nodetree, SH_NODE_MIX_RGB);
			bnode->custom1= ml->blendmethod;
			bnode->locx= locx; locx+= 100;
			bnode->locy= 200;
			
			fromsock= bnode->inputs.first;
			fromsock->ns.vec[0]= ml->blendfac;
			
			if(prevnode) {
				fromsock= prevnode->outputs.first;
				tosock= bnode->inputs.last;
				nodeAddLink(snode->nodetree, prevnode, fromsock, bnode, tosock);
			}
			
			fromsock= node->outputs.first;
			tosock= bnode->inputs.first; tosock= tosock->next;
			nodeAddLink(snode->nodetree, node, fromsock, bnode, tosock);
			
			prevnode= bnode;
			
		}
	}
	
	ntreeSolveOrder(snode->nodetree);
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
	if(snode->nodetree==NULL) return;
	
	if(val) {
		
		if( uiDoBlocks(&sa->uiblocks, event)!=UI_NOTHING ) event= 0;

		switch(event) {
		case LEFTMOUSE:
			if(node_add_link(snode)==0)
				if(node_mouse_select(snode, event)==0)
					node_border_link_delete(snode);
			break;
			
		case RIGHTMOUSE: 
			node_mouse_select(snode, event);

			break;
		case MIDDLEMOUSE:
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			view2dmove(event);	/* in drawipo.c */
			break;
			
		case MOUSEY:
			doredraw= node_socket_hilights(snode, SOCK_IN|SOCK_OUT);
			break;
		
		case UI_BUT_EVENT:
			if(val==B_NODE_EXEC) {
				snode_handle_recalc(snode);	/* sets redraw events too */
			}
			break;
			
		case RENDERPREVIEW:
			shader_node_previewrender(sa, snode);
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
			snode_home(sa, snode);
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
		case CKEY:	/* sort again, showing cyclics */
			if(G.qual==LR_ALTKEY)
				convert_nodes(snode);			/* temporal for layers */
			ntreeSolveOrder(snode->nodetree);
			doredraw= 1;
			break;
		case DKEY:
			if(G.qual==LR_SHIFTKEY)
				node_adduplicate(snode);
			break;
		case GKEY:
			transform_nodes(snode, 'g', "Translate Node");
			break;
		case HKEY:
			node_hide(snode);
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
	if(doredraw==2)
		scrarea_queue_headredraw(sa);
}


