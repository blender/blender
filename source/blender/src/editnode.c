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
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_material.h"
#include "BKE_utildefines.h"

#include "BIF_editview.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_previewrender.h"
#include "BIF_resources.h"
#include "BIF_renderwin.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#include "BSE_drawipo.h"
#include "BSE_edit.h"
#include "BSE_filesel.h"
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
//					int test= node->lasty;
					
					ri.cury = node->lasty;
					ri.rect = NULL;
					ri.pr_rectx = PREVIEW_RENDERSIZE;
					ri.pr_recty = PREVIEW_RENDERSIZE;
					
					BIF_previewrender(snode->id, &ri, NULL, PR_DO_RENDER);	/* sends redraw event */
					if(ri.rect) MEM_freeN(ri.rect);
					
					if(ri.cury<PREVIEW_RENDERSIZE-2)
						addafterqueue(sa->win, RENDERPREVIEW, 1);
//					if(test!=node->lasty)
//						printf("node rendered to %d\n", node->lasty);

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
	else if(snode->treetype==NTREE_COMPOSIT) {
		ntreeCompositExecTree(snode->nodetree, &G.scene->r, 1);	/* 1 is do_previews */
		allqueue(REDRAWNODE, 1);
		allqueue(REDRAWIMAGE, 1);
		if(G.scene->r.scemode & R_DOCOMP) {
			BIF_redraw_render_rect();	/* seems to screwup display? */
			mywinset(curarea->win);
		}
	}
}

static void shader_node_event(SpaceNode *snode, short event)
{
	switch(event) {
		case B_NODE_EXEC:
			snode_handle_recalc(snode);
			break;
		case B_REDR:
			allqueue(REDRAWNODE, 1);
			break;
	}
}

static void load_node_image(char *str)	/* called from fileselect */
{
	SpaceNode *snode= curarea->spacedata.first;
	bNode *node= nodeGetActive(snode->nodetree);
	Image *ima= NULL;
	
	ima= add_image(str);
	if(ima) {
		if(node->id)
			node->id->us--;
		
		node->id= &ima->id;
		ima->id.us++;

		free_image_buffers(ima);	/* force read again */
		ima->ok= 1;
		
		addqueue(curarea->win, RENDERPREVIEW, 1);
		allqueue(REDRAWNODE, 0);
	}
}

static void composit_node_event(SpaceNode *snode, short event)
{
	
	switch(event) {
		case B_NODE_EXEC:
			snode_handle_recalc(snode);
			break;
		case B_REDR:
			allqueue(REDRAWNODE, 1);
			break;
		case B_NODE_LOADIMAGE:
		{
			bNode *node= nodeGetActive(snode->nodetree);
			char name[FILE_MAXDIR+FILE_MAXFILE];
			
			if(node->id)
				strcpy(name, ((Image *)node->id)->name);
			else strcpy(name, U.textudir);
			
			activate_fileselect(FILE_SPECIAL, "SELECT IMAGE", name, load_node_image);
		}
	}
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
	
	out= nodeAddNodeType(ma->nodetree, SH_NODE_OUTPUT, NULL);
	out->locx= 300.0f; out->locy= 300.0f;
	
	in= nodeAddNodeType(ma->nodetree, SH_NODE_MATERIAL, NULL);
	in->locx= 10.0f; in->locy= 300.0f;
	nodeSetActive(ma->nodetree, in);
	
	/* only a link from color to color */
	fromsock= in->outputs.first;
	tosock= out->inputs.first;
	nodeAddLink(ma->nodetree, in, fromsock, out, tosock);
	
	ntreeSolveOrder(ma->nodetree);	/* needed for pointers */
}

/* assumes nothing being done in ntree yet, sets the default in/out node */
/* called from shading buttons or header */
void node_composit_default(Scene *sce)
{
	bNode *in, *out;
	bNodeSocket *fromsock, *tosock;
	
	/* but lets check it anyway */
	if(sce->nodetree) {
		printf("error in composit initialize\n");
		return;
	}
	
	sce->nodetree= ntreeAddTree(NTREE_COMPOSIT);
	
	out= nodeAddNodeType(sce->nodetree, CMP_NODE_VIEWER, NULL);
	out->locx= 300.0f; out->locy= 300.0f;
	
	in= nodeAddNodeType(sce->nodetree, CMP_NODE_R_RESULT, NULL);
	in->locx= 10.0f; in->locy= 300.0f;
	nodeSetActive(sce->nodetree, in);
	
	/* only a link from color to color */
	fromsock= in->outputs.first;
	tosock= out->inputs.first;
	nodeAddLink(sce->nodetree, in, fromsock, out, tosock);
	
	ntreeSolveOrder(sce->nodetree);	/* needed for pointers */
	
	out->id= find_id("IM", "Compositor");
	if(out->id==NULL) {
		Image *ima= alloc_libblock(&G.main->image, ID_IM, "Compositor");
		strcpy(ima->name, "Compositor");
		ima->ok= 1;
		ima->xrep= ima->yrep= 1;
		out->id= &ima->id;
	}
}

/* Here we set the active tree(s), even called for each redraw now, so keep it fast :) */
void snode_set_context(SpaceNode *snode)
{
	Object *ob= OBACT;
	bNode *node= NULL;
	
	snode->nodetree= NULL;
	snode->id= snode->from= NULL;
	
	if(snode->treetype==NTREE_SHADER) {
		/* need active object, or we allow pinning... */
		if(ob) {
			Material *ma= give_current_material(ob, ob->actcol);
			if(ma) {
				snode->from= material_from(ob, ob->actcol);
				snode->id= &ma->id;
				snode->nodetree= ma->nodetree;
			}
		}
	}
	else if(snode->treetype==NTREE_COMPOSIT) {
		snode->from= NULL;
		snode->id= &G.scene->id;
		snode->nodetree= G.scene->nodetree;
	}
	
	/* find editable group */
	if(snode->nodetree)
		for(node= snode->nodetree->nodes.first; node; node= node->next)
			if(node->flag & NODE_GROUP_EDIT)
				break;
	
	if(node && node->id)
		snode->edittree= (bNodeTree *)node->id;
	else
		snode->edittree= snode->nodetree;
}

static void node_set_active(SpaceNode *snode, bNode *node)
{
	
	nodeSetActive(snode->edittree, node);
	
	if(node->type!=NODE_GROUP) {
		
		/* tree specific activate calls */
		if(snode->treetype==NTREE_SHADER) {
			
			/* when we select a material, active texture is cleared, for buttons */
			if(node->id && GS(node->id->name)==ID_MA)
				nodeClearActiveID(snode->edittree, ID_TE);
			if(node->id)
				BIF_preview_changed(-1);	/* temp hack to force texture preview to update */
			
			allqueue(REDRAWBUTSSHADING, 1);
		}
		else if(snode->treetype==NTREE_COMPOSIT) {
			/* make active viewer, currently only 1 supported... */
			if(node->type==CMP_NODE_VIEWER) {
				bNode *tnode;
				int was_output= node->flag & NODE_DO_OUTPUT;

				for(tnode= snode->edittree->nodes.first; tnode; tnode= tnode->next)
					if(tnode->type==CMP_NODE_VIEWER)
						tnode->flag &= ~NODE_DO_OUTPUT;
				
				node->flag |= NODE_DO_OUTPUT;
				if(was_output==0) snode_handle_recalc(snode);
				
				/* add node doesnt link this yet... */
				if(node->id==NULL) {
					node->id= find_id("IM", "Compositor");
					if(node->id==NULL) {
						Image *ima= alloc_libblock(&G.main->image, ID_IM, "Compositor");
						strcpy(ima->name, "Compositor");
						ima->ok= 1;
						ima->xrep= ima->yrep= 1;
						node->id= &ima->id;
					}
					else 
						node->id->us++;
				}
			}
		}
	}
}

static bNode *snode_get_editgroup(SpaceNode *snode)
{
	bNode *gnode;
	
	/* get the groupnode */
	for(gnode= snode->nodetree->nodes.first; gnode; gnode= gnode->next)
		if(gnode->flag & NODE_GROUP_EDIT)
			break;
	return gnode;
}

static void snode_make_group_editable(SpaceNode *snode, bNode *gnode)
{
	bNode *node;
	
	/* make sure nothing has group editing on */
	for(node= snode->nodetree->nodes.first; node; node= node->next)
		node->flag &= ~NODE_GROUP_EDIT;
	
	if(gnode==NULL) {
		/* with NULL argument we do a toggle */
		if(snode->edittree==snode->nodetree)
			gnode= nodeGetActive(snode->nodetree);
	}
	
	if(gnode && gnode->type==NODE_GROUP && gnode->id) {
		gnode->flag |= NODE_GROUP_EDIT;
		snode->edittree= (bNodeTree *)gnode->id;
		
		/* deselect all other nodes, so we can also do grabbing of entire subtree */
		for(node= snode->nodetree->nodes.first; node; node= node->next)
			node->flag &= ~SELECT;
		gnode->flag |= SELECT;
		
	}
	else 
		snode->edittree= snode->nodetree;
	
	/* finally send out events for new active node */
	if(snode->treetype==NTREE_SHADER) {
		allqueue(REDRAWBUTSSHADING, 0);
		
		BIF_preview_changed(-1);	/* temp hack to force texture preview to update */
	}
	
	allqueue(REDRAWNODE, 0);
}

static void node_ungroup(SpaceNode *snode)
{
	bNode *gnode;
	
	gnode= nodeGetActive(snode->edittree);
	if(gnode->type!=NODE_GROUP)
		error("Not a group");
	else {
		if(nodeGroupUnGroup(snode->edittree, gnode)) {
			
			ntreeSolveOrder(snode->edittree);
			BIF_undo_push("Deselect all nodes");
			allqueue(REDRAWNODE, 0);
		}
		else
			error("Can't ungroup");
	}
}

/* when links in groups change, inputs/outputs change, nodes added/deleted... */
static void snode_verify_groups(SpaceNode *snode)
{
	bNode *gnode;
	
	gnode= snode_get_editgroup(snode);
	
	/* does all materials */
	if(gnode)
		nodeVerifyGroup((bNodeTree *)gnode->id);
	
}

/* ************************** Node generic ************** */

/* allows to walk the list in order of visibility */
static bNode *next_node(bNodeTree *ntree)
{
	static bNode *current=NULL, *last= NULL;
	
	if(ntree) {
		/* set current to the first selected node */
		for(current= ntree->nodes.last; current; current= current->prev)
			if(current->flag & NODE_SELECT)
				break;
		
		/* set last to the first unselected node */
		for(last= ntree->nodes.last; last; last= last->prev)
			if((last->flag & NODE_SELECT)==0)
				break;
		
		if(current==NULL)
			current= last;
		
		return NULL;
	}
	/* no nodes, or we are ready */
	if(current==NULL)
		return NULL;
	
	/* now we walk the list backwards, but we always return current */
	if(current->flag & NODE_SELECT) {
		bNode *node= current;
		
		/* find previous selected */
		current= current->prev;
		while(current && (current->flag & NODE_SELECT)==0)
			current= current->prev;
		
		/* find first unselected */
		if(current==NULL)
			current= last;
		
		return node;
	}
	else {
		bNode *node= current;
		
		/* find previous unselected */
		current= current->prev;
		while(current && (current->flag & NODE_SELECT))
			current= current->prev;
		
		return node;
	}
	
	return NULL;
}

/* is rct in visible part of node? */
static bNode *visible_node(SpaceNode *snode, rctf *rct)
{
	bNode *tnode;
	
	for(next_node(snode->edittree); (tnode=next_node(NULL));) {
		if(BLI_isect_rctf(&tnode->totr, rct, NULL))
			break;
	}
	return tnode;
}

static void snode_home(ScrArea *sa, SpaceNode *snode)
{
	bNode *node;
	int first= 1;
	
	snode->v2d.cur.xmin= snode->v2d.cur.ymin= 0.0f;
	snode->v2d.cur.xmax= sa->winx;
	snode->v2d.cur.xmax= sa->winy;
	
	for(node= snode->edittree->nodes.first; node; node= node->next) {
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
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(in_out & SOCK_IN) {
			for(sock= node->inputs.first; sock; sock= sock->next) {
				if(!(sock->flag & SOCK_HIDDEN)) {
					if(BLI_in_rctf(&rect, sock->locx, sock->locy)) {
						if(node == visible_node(snode, &rect)) {
							*nodep= node;
							*sockp= sock;
							return 1;
						}
					}
				}
			}
		}
		if(in_out & SOCK_OUT) {
			for(sock= node->outputs.first; sock; sock= sock->next) {
				if(!(sock->flag & SOCK_HIDDEN)) {
					if(BLI_in_rctf(&rect, sock->locx, sock->locy)) {
						if(node == visible_node(snode, &rect)) {
							*nodep= node;
							*sockp= sock;
							return 1;
						}
					}
				}
			}
		}
	}
	return 0;
}

/* ********************* transform ****************** */

/* releases on event, only intern (for extern see below) */
/* we need argument ntree to allow operations on edittree or nodetree */
static void transform_nodes(bNodeTree *ntree, char mode, char *undostr)
{
	bNode *node;
	float mxstart, mystart, mx, my, *oldlocs, *ol;
	int cont=1, tot=0, cancel=0, firsttime=1;
	short mval[2], mvalo[2];
	
	/* count total */
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->flag & SELECT) tot++;
	
	if(tot==0) return;
	
	/* store oldlocs */
	ol= oldlocs= MEM_mallocN(sizeof(float)*2*tot, "oldlocs transform");
	for(node= ntree->nodes.first; node; node= node->next) {
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
			
			for(ol= oldlocs, node= ntree->nodes.first; node; node= node->next) {
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
		for(ol= oldlocs, node= ntree->nodes.first; node; node= node->next) {
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
	SpaceNode *snode= curarea->spacedata.first;
	
	transform_nodes(snode->edittree, 'g', "Translate node");
}


/* releases on event, only 1 node */
static void scale_node(SpaceNode *snode, bNode *node)
{
	float mxstart, mystart, mx, my, oldwidth;
	int cont=1, cancel=0;
	short mval[2], mvalo[2];
	
	/* store old */
	if(node->flag & NODE_HIDDEN)
		oldwidth= node->miniwidth;
	else
		oldwidth= node->width;
		
	getmouseco_areawin(mvalo);
	areamouseco_to_ipoco(G.v2d, mvalo, &mxstart, &mystart);
	
	while(cont) {
		
		getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1]) {
			
			areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			if(node->flag & NODE_HIDDEN) {
				node->miniwidth= oldwidth + mx-mxstart;
				CLAMP(node->miniwidth, 0.0f, 100.0f);
			}
			else {
				node->width= oldwidth + mx-mxstart;
				CLAMP(node->width, node->typeinfo->minwidth, node->typeinfo->maxwidth);
			}
			
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

/* used in buttons to check context, also checks for edited groups */
bNode *editnode_get_active_idnode(bNodeTree *ntree, short id_code)
{
	bNode *node;
	
	/* check for edited group */
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->flag & NODE_GROUP_EDIT)
			break;
	if(node)
		return nodeGetActiveID((bNodeTree *)node->id, id_code);
	else
		return nodeGetActiveID(ntree, id_code);
}

/* used in buttons to check context, also checks for edited groups */
Material *editnode_get_active_material(Material *ma)
{
	if(ma && ma->use_nodes && ma->nodetree) {
		bNode *node= editnode_get_active_idnode(ma->nodetree, ID_MA);
		if(node)
			return (Material *)node->id;
		else
			return NULL;
	}
	return ma;
}

/* used in buttons to check context, also checks for edited groups */
bNode *editnode_get_active(bNodeTree *ntree)
{
	bNode *node;
	
	/* check for edited group */
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->flag & NODE_GROUP_EDIT)
			break;
	if(node)
		return nodeGetActive((bNodeTree *)node->id);
	else
		return nodeGetActive(ntree);
}


/* no undo here! */
void node_deselectall(SpaceNode *snode, int swap)
{
	bNode *node;
	
	if(swap) {
		for(node= snode->edittree->nodes.first; node; node= node->next)
			if(node->flag & SELECT)
				break;
		if(node==NULL) {
			for(node= snode->edittree->nodes.first; node; node= node->next)
				node->flag |= SELECT;
			allqueue(REDRAWNODE, 0);
			return;
		}
		/* else pass on to deselect */
	}
	
	for(node= snode->edittree->nodes.first; node; node= node->next)
		node->flag &= ~SELECT;
	
	allqueue(REDRAWNODE, 0);
}

int node_has_hidden_sockets(bNode *node)
{
	bNodeSocket *sock;
	
	for(sock= node->inputs.first; sock; sock= sock->next)
		if(sock->flag & SOCK_HIDDEN)
			return 1;
	for(sock= node->outputs.first; sock; sock= sock->next)
		if(sock->flag & SOCK_HIDDEN)
			return 1;
	return 0;
}


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
		bNode *gnode= snode_get_editgroup(snode);
		
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

	allqueue(REDRAWNODE, 1);
	snode_verify_groups(snode);
	BIF_undo_push("Hide/Unhide sockets");

}

static int do_header_node(SpaceNode *snode, bNode *node, float mx, float my)
{
	rctf totr= node->totr;
	
	totr.ymin= totr.ymax-20.0f;
	
	totr.xmax= totr.xmin+15.0f;
	if(BLI_in_rctf(&totr, mx, my)) {
		node->flag |= NODE_HIDDEN;
		allqueue(REDRAWNODE, 0);
		return 1;
	}	
	
	totr.xmax= node->totr.xmax;
	totr.xmin= totr.xmax-18.0f;
	if(node->typeinfo->flag & NODE_PREVIEW) {
		if(BLI_in_rctf(&totr, mx, my)) {
			node->flag ^= NODE_PREVIEW;
			allqueue(REDRAWNODE, 0);
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
			allqueue(REDRAWNODE, 0);
			return 1;
		}
		totr.xmin-=18.0f;
	}
	if(node->outputs.first) {
		if(BLI_in_rctf(&totr, mx, my)) {
			node_hide_unhide_sockets(snode, node);
		}
	}
	
	
	totr= node->totr;
	totr.xmin= totr.xmax-10.0f;
	totr.ymax= totr.ymin+10.0f;
	if(BLI_in_rctf(&totr, mx, my)) {
		scale_node(snode, node);
		return 1;
	}
	return 0;
}

static int do_header_hidden_node(SpaceNode *snode, bNode *node, float mx, float my)
{
	rctf totr= node->totr;
	
	totr.xmax= totr.xmin+15.0f;
	if(BLI_in_rctf(&totr, mx, my)) {
		node->flag &= ~NODE_HIDDEN;
		allqueue(REDRAWNODE, 0);
		return 1;
	}	
	
	totr.xmax= node->totr.xmax;
	totr.xmin= node->totr.xmax-15.0f;
	if(BLI_in_rctf(&totr, mx, my)) {
		scale_node(snode, node);
		return 1;
	}
	return 0;
}


/* return 0: nothing done */
static int node_mouse_select(SpaceNode *snode, unsigned short event)
{
	bNode *node;
	float mx, my;
	short mval[2];
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
	
	for(next_node(snode->edittree); (node=next_node(NULL));) {
		
		/* first check for the headers or scaling widget */
		if(node->flag & NODE_HIDDEN) {
			if(do_header_hidden_node(snode, node, mx, my))
				return 1;
		}
		else {
			if(do_header_node(snode, node, mx, my))
				return 1;
		}
		
		/* node body */
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

/* return 0, nothing done */
static int node_mouse_groupheader(SpaceNode *snode)
{
	bNode *gnode;
	float mx, my;
	short mval[2];
	
	gnode= snode_get_editgroup(snode);
	if(gnode==NULL) return 0;
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
	
	/* click in header or outside? */
	if(BLI_in_rctf(&gnode->totr, mx, my)==0) {
		rctf rect= gnode->totr;
		
		rect.ymax += NODE_DY;
		if(BLI_in_rctf(&rect, mx, my)==0)
			snode_make_group_editable(snode, NULL);	/* toggles, so exits editmode */
		else
			transform_nodes(snode->nodetree, 'g', "Move group");
		
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
	
	if(snode->edittree==NULL) return 0;
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
	
	/* deselect socks */
	for(node= snode->edittree->nodes.first; node; node= node->next) {
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
		
		for(node= snode->edittree->nodes.first; node; node= node->next) {
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
bNode *node_add_node(SpaceNode *snode, int type, float locx, float locy)
{
	bNode *node= NULL, *gnode;
	
	node_deselectall(snode, 0);
	
	node= nodeAddNodeType(snode->edittree, type, NULL);
	
	/* generics */
	if(node) {
		node->locx= locx;
		node->locy= locy + 60.0f;		// arbitrary.. so its visible
		node->flag |= SELECT;
		
		gnode= snode_get_editgroup(snode);
		if(gnode) {
			node->locx -= gnode->locx;
			node->locy -= gnode->locy;
		}

		node_set_active(snode, node);
		snode_verify_groups(snode);
	}
	return node;
}

/* hotkey context */
static void node_add_menu(SpaceNode *snode)
{
	float locx, locy;
	short event, mval[2];
	
	if(snode->treetype==NTREE_SHADER) {
		/* shader menu, still hardcoded defines... solve */
		event= pupmenu("Add Node%t|Output%x1|Geometry%x108|Material%x100|Texture%x106|Mapping%x109|Normal%x107|RGB Curves%x111|Vector Curves%x110|Value %x102|Color %x101|Mix Color %x103|ColorRamp %x104|Color to BW %x105");
		if(event<1) return;
	}
	else if(snode->treetype==NTREE_COMPOSIT) {
		/* compo menu, still hardcoded defines... solve */
		event= pupmenu("Add Node%t|Render Result %x221|Composite %x222|Viewer%x201|Image %x220|RGB Curves%x209|AlphaOver %x210|Blur %x211|Filter %x212|Value %x203|Color %x202|Mix %x204|ColorRamp %x205|Color to BW %x206|Map Value %x213|Normal %x207");
		if(event<1) return;
	}
	else return;
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &locx, &locy);
	node_add_node(snode, event, locx, locy);
	
	snode_handle_recalc(snode);
	
	BIF_undo_push("Add Node");
}

void node_adduplicate(SpaceNode *snode)
{
	
	ntreeCopyTree(snode->edittree, 1);	/* 1 == internally selected nodes */
	
	ntreeSolveOrder(snode->edittree);
	snode_verify_groups(snode);
	snode_handle_recalc(snode);

	transform_nodes(snode->edittree, 'g', "Duplicate");
}

static void node_insert_convertor(SpaceNode *snode, bNodeLink *link)
{
	bNode *newnode= NULL;
	
	if(link->fromsock->type==SOCK_RGBA && link->tosock->type==SOCK_VALUE) {
		if(snode->edittree->type==NTREE_SHADER)
			newnode= node_add_node(snode, SH_NODE_RGBTOBW, 0.0f, 0.0f);
		else if(snode->edittree->type==NTREE_COMPOSIT)
			newnode= node_add_node(snode, CMP_NODE_RGBTOBW, 0.0f, 0.0f);
		else
			newnode= NULL;
	}
	else if(link->fromsock->type==SOCK_VALUE && link->tosock->type==SOCK_RGBA) {
		if(snode->edittree->type==NTREE_SHADER)
			newnode= node_add_node(snode, SH_NODE_VALTORGB, 0.0f, 0.0f);
		else if(snode->edittree->type==NTREE_COMPOSIT)
			newnode= node_add_node(snode, CMP_NODE_VALTORGB, 0.0f, 0.0f);
		else
			newnode= NULL;
	}
	
	if(newnode) {
		/* dangerous assumption to use first in/out socks, but thats fine for now */
		newnode->flag |= NODE_HIDDEN;
		newnode->locx= 0.5f*(link->fromsock->locx + link->tosock->locx);
		newnode->locy= 0.5f*(link->fromsock->locy + link->tosock->locy) + HIDDEN_RAD;
		
		nodeAddLink(snode->edittree, newnode, newnode->outputs.first, link->tonode, link->tosock);
		link->tonode= newnode;
		link->tosock= newnode->inputs.first;
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
		link= nodeAddLink(snode->edittree, node, sock, NULL, NULL);
	else
		link= nodeAddLink(snode->edittree, NULL, NULL, node, sock);
	
	getmouseco_areawin(mvalo);
	while (get_mbut() & L_MOUSE) {
		
		getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || firsttime) {
			firsttime= 0;
			
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			if(in_out==SOCK_OUT) {
				if(find_indicated_socket(snode, &tnode, &tsock, SOCK_IN)) {
					if(nodeFindLink(snode->edittree, sock, tsock)==NULL) {
						if(tnode!=node  && link->tonode!=tnode && link->tosock!= tsock) {
							link->tonode= tnode;
							link->tosock= tsock;
							ntreeSolveOrder(snode->edittree);	/* for interactive red line warning */
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
					if(nodeFindLink(snode->edittree, sock, tsock)==NULL) {
						if(nodeCountSocketLinks(snode->edittree, tsock) < tsock->limit) {
							if(tnode!=node && link->fromnode!=tnode && link->fromsock!= tsock) {
								link->fromnode= tnode;
								link->fromsock= tsock;
								ntreeSolveOrder(snode->edittree);	/* for interactive red line warning */
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
		nodeRemLink(snode->edittree, link);
	}
	else {
		bNodeLink *tlink;
		/* we might need to remove a link */
		if(in_out==SOCK_OUT) {
			if(nodeCountSocketLinks(snode->edittree, link->tosock) > tsock->limit) {
				
				for(tlink= snode->edittree->links.first; tlink; tlink= tlink->next) {
					if(link!=tlink && tlink->tosock==link->tosock)
						break;
				}
				if(tlink) {
					/* is there a free input socket with same type? */
					for(tsock= tlink->tonode->inputs.first; tsock; tsock= tsock->next) {
						if(tsock->type==tlink->fromsock->type)
							if(nodeCountSocketLinks(snode->edittree, tsock) < tsock->limit)
								break;
					}
					if(tsock)
						tlink->tosock= tsock;
					else {
						nodeRemLink(snode->edittree, tlink);
					}
				}					
			}
		}
		
		/* and last trick: insert a convertor when types dont match */
		if(link->tosock->type!=link->fromsock->type) {
			node_insert_convertor(snode, link);
			ntreeSolveOrder(snode->edittree);		/* so nice do it twice! well, the sort-order can only handle 1 added link at a time */
		}
	}
	
	ntreeSolveOrder(snode->edittree);
	snode_verify_groups(snode);
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
		if(nodeCountSocketLinks(snode->edittree, sock)<sock->limit)
			return node_add_link_drag(snode, node, sock, SOCK_OUT);
		else {
			/* find if we break a link */
			for(link= snode->edittree->links.first; link; link= link->next) {
				if(link->fromsock==sock)
					break;
			}
			if(link) {
				node= link->tonode;
				sock= link->tosock;
				nodeRemLink(snode->edittree, link);
				return node_add_link_drag(snode, node, sock, SOCK_IN);
			}
		}
	}
	/* or an input? */
	else if(find_indicated_socket(snode, &node, &sock, SOCK_IN)) {
		if(nodeCountSocketLinks(snode->edittree, sock)<sock->limit)
			return node_add_link_drag(snode, node, sock, SOCK_IN);
		else {
			/* find if we break a link */
			for(link= snode->edittree->links.first; link; link= link->next) {
				if(link->tosock==sock)
					break;
			}
			if(link) {
				node= link->fromnode;
				sock= link->fromsock;
				nodeRemLink(snode->edittree, link);
				return node_add_link_drag(snode, node, sock, SOCK_OUT);
			}
		}
	}
	
	return 0;
}

static void node_delete(SpaceNode *snode)
{
	bNode *node, *next;
	
	for(node= snode->edittree->nodes.first; node; node= next) {
		next= node->next;
		if(node->flag & SELECT)
			nodeFreeNode(snode->edittree, node);
	}
	
	snode_verify_groups(snode);
	snode_handle_recalc(snode);
	BIF_undo_push("Delete nodes");
	allqueue(REDRAWNODE, 1);
}

static void node_hide(SpaceNode *snode)
{
	bNode *node;
	int nothidden=0, ishidden=0;
	
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if(node->flag & NODE_HIDDEN)
				ishidden++;
			else
				nothidden++;
		}
	}
	for(node= snode->edittree->nodes.first; node; node= node->next) {
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
	short val, mval[2], mvalo[2];

	/* to make this work more friendly, we first wait for a mouse move */
	getmouseco_areawin(mvalo);
	while (get_mbut() & L_MOUSE) {
		getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1])
			break;
		else BIF_wait_for_statechange();
	}
	if((get_mbut() & L_MOUSE)==0)
		return;
	
	/* now change cursor and draw border */
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
			for(link= snode->edittree->links.first; link; link= link->next) {
				glLoadName(code++);
				node_draw_link(snode, link);
			}
			
			hits= glRenderMode(GL_RENDER);
			glPopName();
			if(hits>0) {
				int a;
				for(a=0; a<hits; a++) {
					bNodeLink *link= BLI_findlink(&snode->edittree->links, buffer[ (4 * a) + 3]);
					if(link)
						link->tonode= NULL;	/* first tag for delete, otherwise indices are wrong */
				}
				for(link= snode->edittree->links.first; link; link= next) {
					next= link->next;
					if(link->tonode==NULL) {
						nodeRemLink(snode->edittree, link);
					}
				}
				ntreeSolveOrder(snode->edittree);
				snode_verify_groups(snode);
				snode_handle_recalc(snode);
			}
			allqueue(REDRAWNODE, 0);
			BIF_undo_push("Erase links");
		}
	}
	
	setcursor_space(SPACE_NODE, CURSOR_STD);
}


/* ********************** */

void node_make_group(SpaceNode *snode)
{
	bNode *gnode;
	
	if(snode->edittree!=snode->nodetree) {
		error("Can not add a new Group in a Group");
		return;
	}
	
	/* for time being... is too complex to handle */
	if(snode->treetype==NTREE_COMPOSIT) {
		for(gnode=snode->nodetree->nodes.first; gnode; gnode= gnode->next) {
			if(gnode->flag & SELECT)
				if(gnode->type==CMP_NODE_R_RESULT)
					break;
		}
		if(gnode) {
			error("Can not add RenderResult in a Group");
			return;
		}
	}
	
	gnode= nodeMakeGroupFromSelected(snode->nodetree);
	if(gnode==NULL) {
		error("Can not make Group");
	}
	else {
		nodeSetActive(snode->nodetree, gnode);
		ntreeSolveOrder(snode->nodetree);
		allqueue(REDRAWNODE, 0);
		BIF_undo_push("Make Node Group");
	}
}

/* ******************** main event loop ****************** */

/* special version to prevent overlapping buttons, has a bit of hack... */
/* yes, check for example composit_node_event(), file window use... */
static int node_uiDoBlocks(SpaceNode *snode, ListBase *lb, short event)
{
	bNode *node;
	rctf rect;
	ListBase listb= *lb;
	void *prev;
	int retval= UI_NOTHING;
	short mval[2];
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &rect.xmin, &rect.ymin);

	/* this happens after filesel usage... */
	if(lb->first==NULL) {
		return UI_NOTHING;
	}
	
	rect.xmin -= 2.0f;
	rect.ymin -= 2.0f;
	rect.xmax = rect.xmin + 4.0f;
	rect.ymax = rect.ymin + 4.0f;
	
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->block) {
			if(node == visible_node(snode, &rect)) {
				
				/* when there's menus, the prev pointer becomes zero! */
				prev= ((struct Link *)node->block)->prev;
				
				lb->first= lb->last= node->block;
				retval= uiDoBlocks(lb, event);
				
				((struct Link *)node->block)->prev= prev;

				break;
			}
		}
	}
	
	*lb= listb;
	
	return retval;
}

void winqreadnodespace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	SpaceNode *snode= spacedata;
	float dx;
	unsigned short event= evt->event;
	short val= evt->val, doredraw=0, fromlib= 0;
	
	if(sa->win==0) return;
	if(snode->nodetree==NULL) return;
	
	if(val) {

		if( node_uiDoBlocks(snode, &sa->uiblocks, event)!=UI_NOTHING ) event= 0;	

		fromlib= (snode->id && snode->id->lib);
		
		switch(event) {
		case LEFTMOUSE:
			if(fromlib) {
				if(node_mouse_groupheader(snode)==0)
					node_mouse_select(snode, event);
			}
			else {
				if(node_add_link(snode)==0)
					if(node_mouse_groupheader(snode)==0)
						if(node_mouse_select(snode, event)==0)
							node_border_link_delete(snode);
			}
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
			/* future: handlerize this! */
			if(snode->treetype==NTREE_SHADER)
				shader_node_event(snode, val);
			else if(snode->treetype==NTREE_COMPOSIT)
				composit_node_event(snode, val);
			break;
			
		case RENDERPREVIEW:
			if(snode->treetype==NTREE_SHADER)
				shader_node_previewrender(sa, snode);
			else if(snode->treetype==NTREE_COMPOSIT)
				snode_handle_recalc(snode);
			break;
			
		case PADPLUSKEY:
			dx= (float)(0.1154*(G.v2d->cur.xmax-G.v2d->cur.xmin));
			G.v2d->cur.xmin+= dx;
			G.v2d->cur.xmax-= dx;
			dx= (float)(0.1154*(G.v2d->cur.ymax-G.v2d->cur.ymin));
			G.v2d->cur.ymin+= dx;
			G.v2d->cur.ymax-= dx;
			test_view2d(G.v2d, sa->winx, sa->winy);
			doredraw= 1;
			break;
		case PADMINUS:
			dx= (float)(0.15*(G.v2d->cur.xmax-G.v2d->cur.xmin));
			G.v2d->cur.xmin-= dx;
			G.v2d->cur.xmax+= dx;
			dx= (float)(0.15*(G.v2d->cur.ymax-G.v2d->cur.ymin));
			G.v2d->cur.ymin-= dx;
			G.v2d->cur.ymax+= dx;
			test_view2d(G.v2d, sa->winx, sa->winy);
			doredraw= 1;
			break;
		case HOMEKEY:
			snode_home(sa, snode);
			doredraw= 1;
			break;
		case TABKEY:
			if(fromlib) fromlib= -1;
			else snode_make_group_editable(snode, NULL);
			break;
			
		case AKEY:
			if(G.qual==LR_SHIFTKEY) {
				if(fromlib) fromlib= -1;
				else node_add_menu(snode);
			}
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
			ntreeSolveOrder(snode->edittree);
			doredraw= 1;
			break;
		case DKEY:
			if(G.qual==LR_SHIFTKEY) {
				if(fromlib) fromlib= -1;
				else node_adduplicate(snode);
			}
			break;
		case GKEY:
			if(fromlib) fromlib= -1;
			else {
				if(G.qual==LR_CTRLKEY) {
					if(okee("Make Group"))
						node_make_group(snode);
				}
				else if(G.qual==LR_ALTKEY) {
					if(okee("Ungroup"))
						node_ungroup(snode);
				}
				else
					transform_nodes(snode->edittree, 'g', "Translate Node");
			}
			break;
		case HKEY:
			node_hide(snode);
			break;
			
		case DELKEY:
		case XKEY:
			if(fromlib) fromlib= -1;
			else node_delete(snode);
			break;
		}
	}

	if(fromlib==-1)
		error("Cannot edit Library Data");
	if(doredraw)
		scrarea_queue_winredraw(sa);
	if(doredraw==2)
		scrarea_queue_headredraw(sa);
}


