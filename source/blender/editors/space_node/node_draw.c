/*
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
 * The Original Code is: all of this file.
 * Contributor(s): Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/node_draw.c
 *  \ingroup spnode
 */


#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_node.h"
#include "ED_gpencil.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "CMP_node.h"
#include "SHD_node.h"

#include "node_intern.h"

/* width of socket columns in group display */
#define NODE_GROUP_FRAME		120

// XXX interface.h
extern void ui_dropshadow(rctf *rct, float radius, float aspect, int select);

void ED_node_changed_update(ID *id, bNode *node)
{
	bNodeTree *nodetree, *edittree;
	int treetype;

	node_tree_from_ID(id, &nodetree, &edittree, &treetype);

	if(treetype==NTREE_SHADER) {
		DAG_id_tag_update(id, 0);
		WM_main_add_notifier(NC_MATERIAL|ND_SHADING_DRAW, id);
	}
	else if(treetype==NTREE_COMPOSIT) {
		NodeTagChanged(edittree, node);
		/* don't use NodeTagIDChanged, it gives far too many recomposites for image, scene layers, ... */
			
		node= node_tree_get_editgroup(nodetree);
		if(node)
			NodeTagIDChanged(nodetree, node->id);

		WM_main_add_notifier(NC_SCENE|ND_NODES, id);
	}			
	else if(treetype==NTREE_TEXTURE) {
		DAG_id_tag_update(id, 0);
		WM_main_add_notifier(NC_TEXTURE|ND_NODES, id);
	}
}

static int has_nodetree(bNodeTree *ntree, bNodeTree *lookup)
{
	bNode *node;
	
	if(ntree == lookup)
		return 1;
	
	for(node=ntree->nodes.first; node; node=node->next)
		if(node->type == NODE_GROUP && node->id)
			if(has_nodetree((bNodeTree*)node->id, lookup))
				return 1;
	
	return 0;
}

void ED_node_generic_update(Main *bmain, bNodeTree *ntree, bNode *node)
{
	Material *ma;
	Tex *tex;
	Scene *sce;
	
	/* look through all datablocks, to support groups */
	for(ma=bmain->mat.first; ma; ma=ma->id.next)
		if(ma->nodetree && ma->use_nodes && has_nodetree(ma->nodetree, ntree))
			ED_node_changed_update(&ma->id, node);
	
	for(tex=bmain->tex.first; tex; tex=tex->id.next)
		if(tex->nodetree && tex->use_nodes && has_nodetree(tex->nodetree, ntree))
			ED_node_changed_update(&tex->id, node);
	
	for(sce=bmain->scene.first; sce; sce=sce->id.next)
		if(sce->nodetree && sce->use_nodes && has_nodetree(sce->nodetree, ntree))
			ED_node_changed_update(&sce->id, node);
	
	if(ntree->type == NTREE_TEXTURE)
		ntreeTexCheckCyclics(ntree);
}

static void do_node_internal_buttons(bContext *C, void *node_v, int event)
{
	if(event==B_NODE_EXEC) {
		SpaceNode *snode= CTX_wm_space_node(C);
		if(snode && snode->id)
			ED_node_changed_update(snode->id, node_v);
	}
}


static void node_scaling_widget(int color_id, float aspect, float xmin, float ymin, float xmax, float ymax)
{
	float dx;
	float dy;
	
	dx= 0.5f*(xmax-xmin);
	dy= 0.5f*(ymax-ymin);
	
	UI_ThemeColorShade(color_id, +30);	
	fdrawline(xmin, ymin, xmax, ymax);
	fdrawline(xmin+dx, ymin, xmax, ymax-dy);
	
	UI_ThemeColorShade(color_id, -10);
	fdrawline(xmin, ymin+aspect, xmax, ymax+aspect);
	fdrawline(xmin+dx, ymin+aspect, xmax, ymax-dy+aspect);
}

static void node_uiblocks_init(const bContext *C, bNodeTree *ntree)
{
	bNode *node;
	char str[32];
	
	/* add node uiBlocks in reverse order - prevents events going to overlapping nodes */
	
	/* process selected nodes first so they're at the start of the uiblocks list */
	for(node= ntree->nodes.last; node; node= node->prev) {
		
		if (node->flag & NODE_SELECT) {
			/* ui block */
			sprintf(str, "node buttons %p", (void *)node);
			node->block= uiBeginBlock(C, CTX_wm_region(C), str, UI_EMBOSS);
			uiBlockSetHandleFunc(node->block, do_node_internal_buttons, node);
		}
	}
	
	/* then the rest */
	for(node= ntree->nodes.last; node; node= node->prev) {
		
		if (!(node->flag & (NODE_GROUP_EDIT|NODE_SELECT))) {
			/* ui block */
			sprintf(str, "node buttons %p", (void *)node);
			node->block= uiBeginBlock(C, CTX_wm_region(C), str, UI_EMBOSS);
			uiBlockSetHandleFunc(node->block, do_node_internal_buttons, node);
		}
	}
}

/* based on settings in node, sets drawing rect info. each redraw! */
static void node_update(const bContext *C, bNodeTree *ntree, bNode *node)
{
	uiLayout *layout;
	PointerRNA ptr;
	bNodeSocket *nsock;
	float dy= node->locy;
	int buty;
	
	/* header */
	dy-= NODE_DY;
	
	/* little bit space in top */
	if(node->outputs.first)
		dy-= NODE_DYS/2;

	/* output sockets */
	for(nsock= node->outputs.first; nsock; nsock= nsock->next) {
		if(!(nsock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
			nsock->locx= node->locx + node->width;
			nsock->locy= dy - NODE_DYS;
			dy-= NODE_DY;
		}
	}

	node->prvr.xmin= node->locx + NODE_DYS;
	node->prvr.xmax= node->locx + node->width- NODE_DYS;

	/* preview rect? */
	if(node->flag & NODE_PREVIEW) {
		/* only recalculate size when there's a preview actually, otherwise we use stored result */
		BLI_lock_thread(LOCK_PREVIEW);

		if(node->preview && node->preview->rect) {
			float aspect= 1.0f;
			
			if(node->preview && node->preview->xsize && node->preview->ysize) 
				aspect= (float)node->preview->ysize/(float)node->preview->xsize;
			
			dy-= NODE_DYS/2;
			node->prvr.ymax= dy;
			
			if(aspect <= 1.0f)
				node->prvr.ymin= dy - aspect*(node->width-NODE_DY);
			else {
				float dx= (node->width - NODE_DYS) - (node->width- NODE_DYS)/aspect;	/* width correction of image */
				
				node->prvr.ymin= dy - (node->width-NODE_DY);
				
				node->prvr.xmin+= 0.5f*dx;
				node->prvr.xmax-= 0.5f*dx;
			}

			dy= node->prvr.ymin - NODE_DYS/2;

			/* make sure that maximums are bigger or equal to minimums */
			if(node->prvr.xmax < node->prvr.xmin) SWAP(float, node->prvr.xmax, node->prvr.xmin);
			if(node->prvr.ymax < node->prvr.ymin) SWAP(float, node->prvr.ymax, node->prvr.ymin);
		}
		else {
			float oldh= node->prvr.ymax - node->prvr.ymin;
			if(oldh==0.0f)
				oldh= 0.6f*node->width-NODE_DY;
			dy-= NODE_DYS/2;
			node->prvr.ymax= dy;
			node->prvr.ymin= dy - oldh;
			dy= node->prvr.ymin - NODE_DYS/2;
		}

		BLI_unlock_thread(LOCK_PREVIEW);
	}

	/* buttons rect? */
	if((node->flag & NODE_OPTIONS) && node->typeinfo->uifunc) {
		dy-= NODE_DYS/2;

		/* set this for uifunc() that don't use layout engine yet */
		node->butr.xmin= 0;
		node->butr.xmax= node->width - 2*NODE_DYS;
		node->butr.ymin= 0;
		node->butr.ymax= 0;

		RNA_pointer_create(&ntree->id, &RNA_Node, node, &ptr);

		layout= uiBlockLayout(node->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL,
			node->locx+NODE_DYS, dy, node->butr.xmax, 20, U.uistyles.first);

		node->typeinfo->uifunc(layout, (bContext *)C, &ptr);
		uiBlockEndAlign(node->block);
		uiBlockLayoutResolve(node->block, NULL, &buty);

		dy= buty - NODE_DYS/2;
	}

	/* input sockets */
	for(nsock= node->inputs.first; nsock; nsock= nsock->next) {
		if(!(nsock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
			nsock->locx= node->locx;
			nsock->locy= dy - NODE_DYS;
			dy-= NODE_DY;
		}
	}
	
	/* little bit space in end */
	if(node->inputs.first || (node->flag & (NODE_OPTIONS|NODE_PREVIEW))==0 )
		dy-= NODE_DYS/2;

	node->totr.xmin= node->locx;
	node->totr.xmax= node->locx + node->width;
	node->totr.ymax= node->locy;
	node->totr.ymin= dy;
}

/* based on settings in node, sets drawing rect info. each redraw! */
static void node_update_hidden(bNode *node)
{
	bNodeSocket *nsock;
	float rad, drad, hiddenrad= HIDDEN_RAD;
	int totin=0, totout=0, tot;
	
	/* calculate minimal radius */
	for(nsock= node->inputs.first; nsock; nsock= nsock->next)
		if(!(nsock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL)))
			totin++;
	for(nsock= node->outputs.first; nsock; nsock= nsock->next)
		if(!(nsock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL)))
			totout++;
	
	tot= MAX2(totin, totout);
	if(tot>4) {
		hiddenrad += 5.0f*(float)(tot-4);
	}
	
	node->totr.xmin= node->locx;
	node->totr.xmax= node->locx + 3*hiddenrad + node->miniwidth;
	node->totr.ymax= node->locy + (hiddenrad - 0.5f*NODE_DY);
	node->totr.ymin= node->totr.ymax - 2*hiddenrad;
	
	/* output sockets */
	rad=drad= (float)M_PI/(1.0f + (float)totout);
	
	for(nsock= node->outputs.first; nsock; nsock= nsock->next) {
		if(!(nsock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
			nsock->locx= node->totr.xmax - hiddenrad + (float)sin(rad)*hiddenrad;
			nsock->locy= node->totr.ymin + hiddenrad + (float)cos(rad)*hiddenrad;
			rad+= drad;
		}
	}
	
	/* input sockets */
	rad=drad= - (float)M_PI/(1.0f + (float)totin);
	
	for(nsock= node->inputs.first; nsock; nsock= nsock->next) {
		if(!(nsock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
			nsock->locx= node->totr.xmin + hiddenrad + (float)sin(rad)*hiddenrad;
			nsock->locy= node->totr.ymin + hiddenrad + (float)cos(rad)*hiddenrad;
			rad+= drad;
		}
	}
}

static int node_get_colorid(bNode *node)
{
	if(node->typeinfo->nclass==NODE_CLASS_INPUT)
		return TH_NODE_IN_OUT;
	if(node->typeinfo->nclass==NODE_CLASS_OUTPUT) {
		if(node->flag & NODE_DO_OUTPUT)
			return TH_NODE_IN_OUT;
		else
			return TH_NODE;
	}
	if(node->typeinfo->nclass==NODE_CLASS_CONVERTOR)
		return TH_NODE_CONVERTOR;
	if(ELEM3(node->typeinfo->nclass, NODE_CLASS_OP_COLOR, NODE_CLASS_OP_VECTOR, NODE_CLASS_OP_FILTER))
		return TH_NODE_OPERATOR;
	if(node->typeinfo->nclass==NODE_CLASS_GROUP)
		return TH_NODE_GROUP;
	return TH_NODE;
}

/* based on settings in node, sets drawing rect info. each redraw! */
/* note: this assumes only 1 group at a time is drawn (linked data) */
/* in node->totr the entire boundbox for the group is stored */
static void node_update_group(const bContext *C, bNodeTree *ntree, bNode *gnode)
{
	bNodeTree *ngroup= (bNodeTree *)gnode->id;
	bNode *node;
	bNodeSocket *sock, *gsock;
	rctf *rect= &gnode->totr;
	int counter;
	int dy;
	
	rect->xmin = rect->xmax = gnode->locx;
	rect->ymin = rect->ymax = gnode->locy;
	
	/* center them, is a bit of abuse of locx and locy though */
	for(node= ngroup->nodes.first; node; node= node->next) {
		node->locx+= gnode->locx;
		node->locy+= gnode->locy;
		
		if(node->flag & NODE_HIDDEN)
			node_update_hidden(node);
		else
			node_update(C, ntree, node);
		node->locx-= gnode->locx;
		node->locy-= gnode->locy;
	}
	counter= 1;
	for(node= ngroup->nodes.first; node; node= node->next) {
		if(counter) {
			*rect= node->totr;
			counter= 0;
		}
		else
			BLI_union_rctf(rect, &node->totr);
	}
	
	/* add some room for links to group sockets */
	rect->xmin -= 4*NODE_DY;
	rect->xmax += 4*NODE_DY;
	rect->ymin-= NODE_DY;
	rect->ymax+= NODE_DY;
	
	/* input sockets */
	dy = 0.5f*(rect->ymin+rect->ymax) + NODE_DY*(BLI_countlist(&gnode->inputs)-1);
	for(gsock=ngroup->inputs.first, sock=gnode->inputs.first; gsock; gsock=gsock->next, sock=sock->next) {
		gsock->locx = rect->xmin;
		sock->locx = rect->xmin - NODE_GROUP_FRAME;
		sock->locy = gsock->locy = dy;
		
		/* prevent long socket lists from growing out of the group box */
		if (dy-3*NODE_DYS < rect->ymin)
			rect->ymin = dy-3*NODE_DYS;
		if (dy+3*NODE_DYS > rect->ymax)
			rect->ymax = dy+3*NODE_DYS;
		
		dy -= 2*NODE_DY;
	}
	
	/* output sockets */
	dy = 0.5f*(rect->ymin+rect->ymax) + NODE_DY*(BLI_countlist(&gnode->outputs)-1);
	for(gsock=ngroup->outputs.first, sock=gnode->outputs.first; gsock; gsock=gsock->next, sock=sock->next) {
		gsock->locx = rect->xmax;
		sock->locx = rect->xmax + NODE_GROUP_FRAME;
		sock->locy = gsock->locy = dy - NODE_DYS;
		
		/* prevent long socket lists from growing out of the group box */
		if (dy-3*NODE_DYS < rect->ymin)
			rect->ymin = dy-3*NODE_DYS;
		if (dy+3*NODE_DYS > rect->ymax)
			rect->ymax = dy+3*NODE_DYS;
		
		dy -= 2*NODE_DY;
	}
}

/* note: in cmp_util.c is similar code, for node_compo_pass_on() */
static void node_draw_mute_line(View2D *v2d, SpaceNode *snode, bNode *node)
{
	bNodeSocket *valsock= NULL, *colsock= NULL, *vecsock= NULL;
	bNodeSocket *sock;
	bNodeLink link= {0};
	int a;
	
	/* connect the first value buffer in with first value out */
	/* connect the first RGBA buffer in with first RGBA out */
	
	/* test the inputs */
	for(a=0, sock= node->inputs.first; sock; sock= sock->next, a++) {
		if(nodeCountSocketLinks(snode->edittree, sock)) {
			if(sock->type==SOCK_VALUE && valsock==NULL) valsock= sock;
			if(sock->type==SOCK_VECTOR && vecsock==NULL) vecsock= sock;
			if(sock->type==SOCK_RGBA && colsock==NULL) colsock= sock;
		}
	}
	
	/* outputs, draw lines */
	glEnable(GL_BLEND);
	glEnable( GL_LINE_SMOOTH );
	
	if(valsock || colsock || vecsock) {
		for(a=0, sock= node->outputs.first; sock; sock= sock->next, a++) {
			if(nodeCountSocketLinks(snode->edittree, sock)) {
				link.tosock= sock;
				
				if(sock->type==SOCK_VALUE && valsock) {
					link.fromsock= valsock;
					node_draw_link_bezier(v2d, snode, &link, TH_REDALERT, 0, TH_WIRE, 0, TH_WIRE);
					valsock= NULL;
				}
				if(sock->type==SOCK_VECTOR && vecsock) {
					link.fromsock= vecsock;
					node_draw_link_bezier(v2d, snode, &link, TH_REDALERT, 0, TH_WIRE, 0, TH_WIRE);
					vecsock= NULL;
				}
				if(sock->type==SOCK_RGBA && colsock) {
					link.fromsock= colsock;
					node_draw_link_bezier(v2d, snode, &link, TH_REDALERT, 0, TH_WIRE, 0, TH_WIRE);
					colsock= NULL;
				}
			}
		}
	}
	glDisable(GL_BLEND);
	glDisable( GL_LINE_SMOOTH );
}

/* nice AA filled circle */
/* this might have some more generic use */
static void circle_draw(float x, float y, float size, int col[3])
{
	/* 16 values of sin function */
	static float si[16] = {
		0.00000000f, 0.39435585f,0.72479278f,0.93775213f,
		0.99871650f,0.89780453f,0.65137248f,0.29936312f,
		-0.10116832f,-0.48530196f,-0.79077573f,-0.96807711f,
		-0.98846832f,-0.84864425f,-0.57126821f,-0.20129852f
	};
	/* 16 values of cos function */
	static float co[16] ={
		1.00000000f,0.91895781f,0.68896691f,0.34730525f,
		-0.05064916f,-0.44039415f,-0.75875812f,-0.95413925f,
		-0.99486932f,-0.87434661f,-0.61210598f,-0.25065253f,
		0.15142777f,0.52896401f,0.82076344f,0.97952994f,
	};
	int a;
	
	glColor3ub(col[0], col[1], col[2]);
	
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

static void socket_circle_draw(bNodeSocket *sock, float size)
{
	int col[3];
	
	if(sock->type==-1) {
		col[0]= 0; col[1]= 0; col[2]= 0;
	}
	else if(sock->type==SOCK_VALUE) {
		col[0]= 160; col[1]= 160; col[2]= 160;
	}
	else if(sock->type==SOCK_VECTOR) {
		col[0]= 100; col[1]= 100; col[2]= 200;
	}
	else if(sock->type==SOCK_RGBA) {
		col[0]= 200; col[1]= 200; col[2]= 40;
	}
	else { 
		col[0]= 100; col[1]= 200; col[2]= 100;
	}

	circle_draw(sock->locx, sock->locy, size, col);
}

static void node_sync_cb(bContext *UNUSED(C), void *snode_v, void *node_v)
{
	SpaceNode *snode= snode_v;
	
	if(snode->treetype==NTREE_SHADER) {
		nodeShaderSynchronizeID(node_v, 1);
		// allqueue(REDRAWBUTSSHADING, 0);
	}
}

/* **************  Socket callbacks *********** */

/* not a callback */
static void node_draw_preview(bNodePreview *preview, rctf *prv)
{
	float xscale= (prv->xmax-prv->xmin)/((float)preview->xsize);
	float yscale= (prv->ymax-prv->ymin)/((float)preview->ysize);
	float tile= (prv->xmax - prv->xmin) / 10.0f;
	float x, y;
	
	/* draw checkerboard backdrop to show alpha */
	glColor3ub(120, 120, 120);
	glRectf(prv->xmin, prv->ymin, prv->xmax, prv->ymax);
	glColor3ub(160, 160, 160);
	
	for(y=prv->ymin; y<prv->ymax; y+=tile*2) {
		for(x=prv->xmin; x<prv->xmax; x+=tile*2) {
			float tilex= tile, tiley= tile;

			if(x+tile > prv->xmax)
				tilex= prv->xmax-x;
			if(y+tile > prv->ymax)
				tiley= prv->ymax-y;

			glRectf(x, y, x + tilex, y + tiley);
		}
	}
	for(y=prv->ymin+tile; y<prv->ymax; y+=tile*2) {
		for(x=prv->xmin+tile; x<prv->xmax; x+=tile*2) {
			float tilex= tile, tiley= tile;

			if(x+tile > prv->xmax)
				tilex= prv->xmax-x;
			if(y+tile > prv->ymax)
				tiley= prv->ymax-y;

			glRectf(x, y, x + tilex, y + tiley);
		}
	}
	
	glPixelZoom(xscale, yscale);

	glEnable(GL_BLEND);
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );	/* premul graphics */
	
	glColor4f(1.0, 1.0, 1.0, 1.0);
	glaDrawPixelsTex(prv->xmin, prv->ymin, preview->xsize, preview->ysize, GL_UNSIGNED_BYTE, preview->rect);
	
	glDisable(GL_BLEND);
	glPixelZoom(1.0f, 1.0f);

	UI_ThemeColorShadeAlpha(TH_BACK, -15, +100);
	fdrawbox(prv->xmin, prv->ymin, prv->xmax, prv->ymax);
	
}

typedef struct SocketVectorMenuArgs {
	PointerRNA ptr;
	int x, y, width;
	uiButHandleFunc cb;
	void *arg1, *arg2;
} SocketVectorMenuArgs;

/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *socket_vector_menu(bContext *C, ARegion *ar, void *args_v)
{
	SocketVectorMenuArgs *args= (SocketVectorMenuArgs*)args_v;
	uiBlock *block;
	uiLayout *layout;
	
	block= uiBeginBlock(C, ar, "socket menu", UI_EMBOSS);
	uiBlockSetFlag(block, UI_BLOCK_KEEP_OPEN);
	
	layout= uiLayoutColumn(uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, args->x, args->y+2, args->width, 20, U.uistyles.first), 0);
	
	uiItemR(layout, &args->ptr, "default_value", UI_ITEM_R_EXPAND, "", ICON_NONE);
	
	return block;
}

static void node_draw_socket_button(bNodeTree *ntree, bNodeSocket *sock, const char *name,
									uiBlock *block, int x, int y, int width,
									uiButHandleFunc cb, void *arg1, void *arg2)
{
	uiBut *bt= NULL;
	PointerRNA ptr;
	int labelw;
	SocketVectorMenuArgs *args;
	
	RNA_pointer_create(&ntree->id, &RNA_NodeSocket, sock, &ptr);
	
	switch (sock->type) {
	case SOCK_VALUE:
		bt=uiDefButR(block, NUM, B_NODE_EXEC, name,
					 x, y+1, width, 17, 
					 &ptr, "default_value", 0, sock->ns.min, sock->ns.max, -1, -1, NULL);
		if (cb)
			uiButSetFunc(bt, cb, arg1, arg2);
		break;
		
	case SOCK_VECTOR:
		args= MEM_callocN(sizeof(SocketVectorMenuArgs), "SocketVectorMenuArgs");
	
		args->ptr = ptr;
		args->x = x;
		args->y = y;
		args->width = width;
		args->cb = cb;
		args->arg1 = arg1;
		args->arg2 = arg2;
		
		uiDefBlockButN(block, socket_vector_menu, args, name, 
					   x, y+1, width, 17, 
					   "");
		break;
		
	case SOCK_RGBA:
		labelw= width - 40;
		
		bt=uiDefButR(block, COL, B_NODE_EXEC, "",
					 x, y+2, (labelw>0 ? 40 : width), 15, 
					 &ptr, "default_value", 0, sock->ns.min, sock->ns.max, -1, -1, NULL);
		if (cb)
			uiButSetFunc(bt, cb, arg1, arg2);
		
		if (name[0]!='\0' && labelw>0)
			uiDefBut(block, LABEL, 0, name, 
					 x + 40, y+2, labelw, 15, 
					 NULL, 0, 0, 0, 0, "");
		break;
	}
}

static void node_draw_basis(const bContext *C, ARegion *ar, SpaceNode *snode, bNodeTree *ntree, bNode *node)
{
	bNodeSocket *sock;
	rctf *rct= &node->totr;
	float iconofs;
	int color_id= node_get_colorid(node);
	char showname[128]; /* 128 used below */
	View2D *v2d = &ar->v2d;
	
	/* hurmf... another candidate for callback, have to see how this works first */
	if(node->id && node->block && snode->treetype==NTREE_SHADER)
		nodeShaderSynchronizeID(node, 0);
	
	/* skip if out of view */
	if (node->totr.xmax < ar->v2d.cur.xmin || node->totr.xmin > ar->v2d.cur.xmax ||
			node->totr.ymax < ar->v2d.cur.ymin || node->totr.ymin > ar->v2d.cur.ymax) {
		
		uiEndBlock(C, node->block);
		node->block= NULL;
		return;
	}
	
	uiSetRoundBox(15-4);
	ui_dropshadow(rct, BASIS_RAD, snode->aspect, node->flag & SELECT);
	
	/* header */
	if(color_id==TH_NODE)
		UI_ThemeColorShade(color_id, -20);
	else
		UI_ThemeColor(color_id);
	
	if(node->flag & NODE_MUTED)
	   UI_ThemeColorBlend(color_id, TH_REDALERT, 0.5f);
		
	uiSetRoundBox(3);
	uiRoundBox(rct->xmin, rct->ymax-NODE_DY, rct->xmax, rct->ymax, BASIS_RAD);
	
	/* show/hide icons, note this sequence is copied in do_header_node() node_state.c */
	iconofs= rct->xmax - 7.0f;
	
	if(node->typeinfo->flag & NODE_PREVIEW) {
		int icon_id;
		
		if(node->flag & (NODE_ACTIVE_ID|NODE_DO_OUTPUT))
			icon_id= ICON_MATERIAL;
		else
			icon_id= ICON_MATERIAL_DATA;
		iconofs-=15.0f;
		uiDefIconBut(node->block, LABEL, B_REDR, icon_id, iconofs, rct->ymax-NODE_DY,
					 UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 1.0, 0.5, "");
	}
	if(node->type == NODE_GROUP) {
		
		iconofs-=15.0f;
		uiDefIconBut(node->block, LABEL, B_REDR, ICON_NODETREE, iconofs, rct->ymax-NODE_DY,
					 UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 1.0, 0.5, "");
	}
	if(node->typeinfo->flag & NODE_OPTIONS) {
		iconofs-=15.0f;
		uiDefIconBut(node->block, LABEL, B_REDR, ICON_BUTS, iconofs, rct->ymax-NODE_DY,
					 UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 1.0, 0.5, "");
	}
	{	/* always hide/reveal unused sockets */ 
		int shade;

		iconofs-=15.0f;
		// XXX re-enable
		/*if(node_has_hidden_sockets(node))
			shade= -40;
		else*/
			shade= -90;
		uiDefIconBut(node->block, LABEL, B_REDR, ICON_PLUS, iconofs, rct->ymax-NODE_DY,
						  UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 1.0, 0.5, "");
	}
	
	/* title */
	if(node->flag & SELECT) 
		UI_ThemeColor(TH_TEXT_HI);
	else
		UI_ThemeColorBlendShade(TH_TEXT, color_id, 0.4f, 10);
	
	/* open/close entirely? */
	UI_DrawTriIcon(rct->xmin+10.0f, rct->ymax-NODE_DY/2.0f, 'v');
	
	/* this isn't doing anything for the label, so commenting out
	if(node->flag & SELECT) 
		UI_ThemeColor(TH_TEXT_HI);
	else
		UI_ThemeColor(TH_TEXT); */
	
	if (node->typeinfo->labelfunc)
		BLI_strncpy(showname, node->typeinfo->labelfunc(node), sizeof(showname));
	else
		BLI_strncpy(showname, node->typeinfo->name, sizeof(showname));

	//if(node->flag & NODE_MUTED)
	//	sprintf(showname, "[%s]", showname);
	
	uiDefBut(node->block, LABEL, 0, showname, (short)(rct->xmin+15), (short)(rct->ymax-NODE_DY), 
			 (int)(iconofs - rct->xmin-18.0f), NODE_DY,  NULL, 0, 0, 0, 0, "");

	/* body */
	UI_ThemeColor4(TH_NODE);
	glEnable(GL_BLEND);
	uiSetRoundBox(8);
	uiRoundBox(rct->xmin, rct->ymin, rct->xmax, rct->ymax-NODE_DY, BASIS_RAD);
	glDisable(GL_BLEND);

	/* scaling indicator */
	node_scaling_widget(TH_NODE, snode->aspect, rct->xmax-BASIS_RAD*snode->aspect, rct->ymin, rct->xmax, rct->ymin+BASIS_RAD*snode->aspect);

	/* outline active and selected emphasis */
	if( node->flag & (NODE_ACTIVE|SELECT) ) {
		glEnable(GL_BLEND);
		glEnable( GL_LINE_SMOOTH );
			/* using different shades of TH_TEXT_HI for the empasis, like triangle */
			if( node->flag & NODE_ACTIVE ) 
				UI_ThemeColorShadeAlpha(TH_TEXT_HI, 0, -40);
			else
				UI_ThemeColorShadeAlpha(TH_TEXT_HI, -20, -120);
			uiSetRoundBox(15-4); // round all corners except lower right
			uiDrawBox(GL_LINE_LOOP, rct->xmin, rct->ymin, rct->xmax, rct->ymax, BASIS_RAD);
			
		glDisable( GL_LINE_SMOOTH );
		glDisable(GL_BLEND);
	}
	
	/* disable lines */
	if(node->flag & NODE_MUTED)
		node_draw_mute_line(v2d, snode, node);

	
	/* socket inputs, buttons */
	for(sock= node->inputs.first; sock; sock= sock->next) {
		if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
			socket_circle_draw(sock, NODE_SOCKSIZE);
			
			if(node->block && sock->link==NULL) {
				node_draw_socket_button(ntree, sock, sock->name, node->block, sock->locx+NODE_DYS, sock->locy-NODE_DYS, node->width-NODE_DY, node_sync_cb, snode, node);
			}
			else {
				uiDefBut(node->block, LABEL, 0, sock->name, (short)(sock->locx+7), (short)(sock->locy-9.0f), 
						 (short)(node->width-NODE_DY), NODE_DY,  NULL, 0, 0, 0, 0, "");
			}
		}
	}
	
	/* socket outputs */
	for(sock= node->outputs.first; sock; sock= sock->next) {
		if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
			float slen;
			int ofs= 0;
			
			socket_circle_draw(sock, NODE_SOCKSIZE);
			
			UI_ThemeColor(TH_TEXT);
			slen= snode->aspect*UI_GetStringWidth(sock->name);
			while(slen > node->width) {
				ofs++;
				slen= snode->aspect*UI_GetStringWidth(sock->name+ofs);
			}
			
			uiDefBut(node->block, LABEL, 0, sock->name+ofs, (short)(sock->locx-15.0f-slen), (short)(sock->locy-9.0f), 
					 (short)(node->width-NODE_DY), NODE_DY,  NULL, 0, 0, 0, 0, "");
		}
	}
	
	/* preview */
	if(node->flag & NODE_PREVIEW) {
		BLI_lock_thread(LOCK_PREVIEW);
		if(node->preview && node->preview->rect && !BLI_rctf_is_empty(&node->prvr))
			node_draw_preview(node->preview, &node->prvr);
		BLI_unlock_thread(LOCK_PREVIEW);
	}
	
	UI_ThemeClearColor(color_id);
		
	uiEndBlock(C, node->block);
	uiDrawBlock(C, node->block);
	node->block= NULL;
}

static void node_draw_hidden(const bContext *C, ARegion *ar, SpaceNode *snode, bNode *node)
{
	bNodeSocket *sock;
	rctf *rct= &node->totr;
	float dx, centy= 0.5f*(rct->ymax+rct->ymin);
	float hiddenrad= 0.5f*(rct->ymax-rct->ymin);
	int color_id= node_get_colorid(node);
	char showname[128];	/* 128 is used below */
	
	/* shadow */
	uiSetRoundBox(15);
	ui_dropshadow(rct, hiddenrad, snode->aspect, node->flag & SELECT);

	/* body */
	UI_ThemeColor(color_id);
	if(node->flag & NODE_MUTED)
	   UI_ThemeColorBlend(color_id, TH_REDALERT, 0.5f);	
	uiRoundBox(rct->xmin, rct->ymin, rct->xmax, rct->ymax, hiddenrad);
	
	/* outline active and selected emphasis */
	if( node->flag & (NODE_ACTIVE|SELECT) ) {
		glEnable(GL_BLEND);
		glEnable( GL_LINE_SMOOTH );
			/* using different shades of TH_TEXT_HI for the empasis, like triangle */
			if( node->flag & NODE_ACTIVE ) 
				UI_ThemeColorShadeAlpha(TH_TEXT_HI, 0, -40);
			else
				UI_ThemeColorShadeAlpha(TH_TEXT_HI, -20, -120);
			uiDrawBox(GL_LINE_LOOP, rct->xmin, rct->ymin, rct->xmax, rct->ymax, hiddenrad);
		glDisable( GL_LINE_SMOOTH );
		glDisable(GL_BLEND);
	}
	
	/* title */
	if(node->flag & SELECT) 
		UI_ThemeColor(TH_TEXT_HI);
	else
		UI_ThemeColorBlendShade(TH_TEXT, color_id, 0.4f, 10);
	
	/* open entirely icon */
	UI_DrawTriIcon(rct->xmin+10.0f, centy, 'h');	
	
	/* disable lines */
	if(node->flag & NODE_MUTED)
		node_draw_mute_line(&ar->v2d, snode, node);	
	
	if(node->flag & SELECT) 
		UI_ThemeColor(TH_TEXT_HI);
	else
		UI_ThemeColor(TH_TEXT);
	
	if(node->miniwidth>0.0f) {
		if (node->typeinfo->labelfunc)
			BLI_strncpy(showname, node->typeinfo->labelfunc(node), sizeof(showname));
		else
			BLI_strncpy(showname, node->typeinfo->name, sizeof(showname));
		
		//if(node->flag & NODE_MUTED)
		//	sprintf(showname, "[%s]", showname);

		uiDefBut(node->block, LABEL, 0, showname, (short)(rct->xmin+15), (short)(centy-10), 
				 (int)(rct->xmax - rct->xmin-18.0f -12.0f), NODE_DY,  NULL, 0, 0, 0, 0, "");
	}	

	/* scale widget thing */
	UI_ThemeColorShade(color_id, -10);	
	dx= 10.0f;
	fdrawline(rct->xmax-dx, centy-4.0f, rct->xmax-dx, centy+4.0f);
	fdrawline(rct->xmax-dx-3.0f*snode->aspect, centy-4.0f, rct->xmax-dx-3.0f*snode->aspect, centy+4.0f);
	
	UI_ThemeColorShade(color_id, +30);
	dx-= snode->aspect;
	fdrawline(rct->xmax-dx, centy-4.0f, rct->xmax-dx, centy+4.0f);
	fdrawline(rct->xmax-dx-3.0f*snode->aspect, centy-4.0f, rct->xmax-dx-3.0f*snode->aspect, centy+4.0f);
	
	/* sockets */
	for(sock= node->inputs.first; sock; sock= sock->next) {
		if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL)))
			socket_circle_draw(sock, NODE_SOCKSIZE);
	}
	
	for(sock= node->outputs.first; sock; sock= sock->next) {
		if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL)))
			socket_circle_draw(sock, NODE_SOCKSIZE);
	}
	
	uiEndBlock(C, node->block);
	uiDrawBlock(C, node->block);
	node->block= NULL;
}

static void node_draw_nodetree(const bContext *C, ARegion *ar, SpaceNode *snode, bNodeTree *ntree)
{
	bNode *node;
	bNodeLink *link;
	int a;
	
	if(ntree==NULL) return;		/* groups... */
	
	/* node lines */
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	for(link= ntree->links.first; link; link= link->next)
		node_draw_link(&ar->v2d, snode, link);
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
	
	/* not selected first */
	for(a=0, node= ntree->nodes.first; node; node= node->next, a++) {
		node->nr= a;		/* index of node in list, used for exec event code */
		if(!(node->flag & SELECT)) {
			if(node->flag & NODE_GROUP_EDIT);
			else if(node->flag & NODE_HIDDEN)
				node_draw_hidden(C, ar, snode, node);
			else
				node_draw_basis(C, ar, snode, ntree, node);
		}
	}
	
	/* selected */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if(node->flag & NODE_GROUP_EDIT);
			else if(node->flag & NODE_HIDDEN)
				node_draw_hidden(C, ar, snode, node);
			else
				node_draw_basis(C, ar, snode, ntree, node);
		}
	}	
}

static void group_verify_cb(bContext *UNUSED(C), void *UNUSED(snode_v), void *ngroup_v)
{
	bNodeTree *ngroup= (bNodeTree*)ngroup_v;
	
	nodeGroupVerify(ngroup);
}

/* groups are, on creation, centered around 0,0 */
static void node_draw_group(const bContext *C, ARegion *ar, SpaceNode *snode, bNodeTree *ntree, bNode *gnode)
{
	bNodeTree *ngroup= (bNodeTree *)gnode->id;
	bNodeSocket *sock;
	rctf rect= gnode->totr;
	int index;
	uiLayout *layout;
	PointerRNA ptr;
	uiBut *bt;
	
	/* backdrop header */
	glEnable(GL_BLEND);
	uiSetRoundBox(3);
	UI_ThemeColorShadeAlpha(TH_NODE_GROUP, 0, -70);
	uiDrawBox(GL_POLYGON, rect.xmin-NODE_GROUP_FRAME, rect.ymax, rect.xmax+NODE_GROUP_FRAME, rect.ymax+26, BASIS_RAD);
	
	/* backdrop body */
	UI_ThemeColorShadeAlpha(TH_BACK, -8, -70);
	uiSetRoundBox(0);
	uiDrawBox(GL_POLYGON, rect.xmin, rect.ymin, rect.xmax, rect.ymax, BASIS_RAD);

	/* input column */
	UI_ThemeColorShadeAlpha(TH_BACK, 10, -50);
	uiSetRoundBox(8);
	uiDrawBox(GL_POLYGON, rect.xmin-NODE_GROUP_FRAME, rect.ymin, rect.xmin, rect.ymax, BASIS_RAD);

	/* output column */
	UI_ThemeColorShadeAlpha(TH_BACK, 10, -50);
	uiSetRoundBox(4);
	uiDrawBox(GL_POLYGON, rect.xmax, rect.ymin, rect.xmax+NODE_GROUP_FRAME, rect.ymax, BASIS_RAD);

	/* input column separator */
	glColor4ub(200, 200, 200, 140);
	glBegin(GL_LINES);
	glVertex2f(rect.xmin, rect.ymin);
	glVertex2f(rect.xmin, rect.ymax);
	glEnd();

	/* output column separator */
	glColor4ub(200, 200, 200, 140);
	glBegin(GL_LINES);
	glVertex2f(rect.xmax, rect.ymin);
	glVertex2f(rect.xmax, rect.ymax);
	glEnd();

	/* group node outline */
	uiSetRoundBox(15);
	glColor4ub(200, 200, 200, 140);
	glEnable( GL_LINE_SMOOTH );
	uiDrawBox(GL_LINE_LOOP, rect.xmin-NODE_GROUP_FRAME, rect.ymin, rect.xmax+NODE_GROUP_FRAME, rect.ymax+26, BASIS_RAD);
	glDisable( GL_LINE_SMOOTH );
	glDisable(GL_BLEND);
	
	/* backdrop title */
	UI_ThemeColor(TH_TEXT_HI);

	layout = uiBlockLayout(gnode->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, (short)(rect.xmin+15), (short)(rect.ymax+23),
						   MIN2((int)(rect.xmax - rect.xmin-18.0f), 140), 20, U.uistyles.first);
	RNA_pointer_create(&ntree->id, &RNA_Node, gnode, &ptr);
	uiTemplateIDBrowse(layout, (bContext*)C, &ptr, "node_tree", NULL, NULL, NULL);
	uiBlockLayoutResolve(gnode->block, NULL, NULL);

	/* draw the internal tree nodes and links */
	node_draw_nodetree(C, ar, snode, ngroup);

	/* group sockets */
	for(sock=ngroup->inputs.first, index=0; sock; sock=sock->next, ++index) {
		socket_circle_draw(sock, NODE_SOCKSIZE);
		/* small hack to use socket_circle_draw function with offset */
		sock->locx -= NODE_GROUP_FRAME;
		socket_circle_draw(sock, NODE_SOCKSIZE);
		sock->locx += NODE_GROUP_FRAME;

		bt = uiDefBut(gnode->block, TEX, 0, "", 
					  sock->locx-114, sock->locy+1, 72, NODE_DY,
					  sock->name, 0, 31, 0, 0, "");
		uiButSetFunc(bt, group_verify_cb, snode, ngroup);
		
		node_draw_socket_button(ngroup, sock, "", gnode->block,
								sock->locx-114, sock->locy-NODE_DY, 72,
								NULL, NULL, NULL);

		uiBlockSetDirection(gnode->block, UI_TOP);
		uiBlockBeginAlign(gnode->block);
		bt = uiDefIconButO(gnode->block, BUT, "NODE_OT_group_socket_move_up", 0, ICON_TRIA_UP,
						   sock->locx-40, sock->locy, 16, 16, "");
		if (!sock->prev)
			uiButSetFlag(bt, UI_BUT_DISABLED);
		RNA_int_set(uiButGetOperatorPtrRNA(bt), "index", index);
		RNA_enum_set(uiButGetOperatorPtrRNA(bt), "in_out", SOCK_IN);
		bt = uiDefIconButO(gnode->block, BUT, "NODE_OT_group_socket_move_down", 0, ICON_TRIA_DOWN,
						   sock->locx-40, sock->locy-16, 16, 16, "");
		if (!sock->next)
			uiButSetFlag(bt, UI_BUT_DISABLED);
		RNA_int_set(uiButGetOperatorPtrRNA(bt), "index", index);
		RNA_enum_set(uiButGetOperatorPtrRNA(bt), "in_out", SOCK_IN);
		uiBlockEndAlign(gnode->block);
		uiBlockSetDirection(gnode->block, 0);
		
		uiBlockSetEmboss(gnode->block, UI_EMBOSSN);
		bt = uiDefIconButO(gnode->block, BUT, "NODE_OT_group_socket_remove", 0, ICON_X,
						   sock->locx-22, sock->locy-8, 16, 16, "");
		RNA_int_set(uiButGetOperatorPtrRNA(bt), "index", index);
		RNA_enum_set(uiButGetOperatorPtrRNA(bt), "in_out", SOCK_IN);
		uiBlockSetEmboss(gnode->block, UI_EMBOSS);
	}
	
	for(sock=ngroup->outputs.first, index=0; sock; sock=sock->next, ++index) {
		socket_circle_draw(sock, NODE_SOCKSIZE);
		/* small hack to use socket_circle_draw function with offset */
		sock->locx += NODE_GROUP_FRAME;
		socket_circle_draw(sock, NODE_SOCKSIZE);
		sock->locx -= NODE_GROUP_FRAME;
		
		uiBlockSetEmboss(gnode->block, UI_EMBOSSN);
		bt = uiDefIconButO(gnode->block, BUT, "NODE_OT_group_socket_remove", 0, ICON_X,
						   sock->locx+6, sock->locy-8, 16, 16, "");
		RNA_int_set(uiButGetOperatorPtrRNA(bt), "index", index);
		RNA_enum_set(uiButGetOperatorPtrRNA(bt), "in_out", SOCK_OUT);
		uiBlockSetEmboss(gnode->block, UI_EMBOSS);
		
		uiBlockSetDirection(gnode->block, UI_TOP);
		uiBlockBeginAlign(gnode->block);
		bt = uiDefIconButO(gnode->block, BUT, "NODE_OT_group_socket_move_up", 0, ICON_TRIA_UP,
						   sock->locx+24, sock->locy, 16, 16, "");
		if (!sock->prev)
			uiButSetFlag(bt, UI_BUT_DISABLED);
		RNA_int_set(uiButGetOperatorPtrRNA(bt), "index", index);
		RNA_enum_set(uiButGetOperatorPtrRNA(bt), "in_out", SOCK_OUT);
		bt = uiDefIconButO(gnode->block, BUT, "NODE_OT_group_socket_move_down", 0, ICON_TRIA_DOWN,
						   sock->locx+24, sock->locy-16, 16, 16, "");
		if (!sock->next)
			uiButSetFlag(bt, UI_BUT_DISABLED);
		RNA_int_set(uiButGetOperatorPtrRNA(bt), "index", index);
		RNA_enum_set(uiButGetOperatorPtrRNA(bt), "in_out", SOCK_OUT);
		uiBlockEndAlign(gnode->block);
		uiBlockSetDirection(gnode->block, 0);
		
		if (sock->link) {
			bt = uiDefBut(gnode->block, TEX, 0, "", 
						  sock->locx+42, sock->locy-NODE_DYS+1, 72, NODE_DY,
						  sock->name, 0, 31, 0, 0, "");
			uiButSetFunc(bt, group_verify_cb, snode, ngroup);
		}
		else {
			bt = uiDefBut(gnode->block, TEX, 0, "", 
						  sock->locx+42, sock->locy+1, 72, NODE_DY,
						  sock->name, 0, 31, 0, 0, "");
			uiButSetFunc(bt, group_verify_cb, snode, ngroup);
			
			node_draw_socket_button(ngroup, sock, "", gnode->block, sock->locx+42, sock->locy-NODE_DY, 72, NULL, NULL, NULL);
		}
	}
	
	uiEndBlock(C, gnode->block);
	uiDrawBlock(C, gnode->block);
	gnode->block= NULL;
}

void drawnodespace(const bContext *C, ARegion *ar, View2D *v2d)
{
	View2DScrollers *scrollers;
	SpaceNode *snode= CTX_wm_space_node(C);
	Scene *scene= CTX_data_scene(C);
	int color_manage = scene->r.color_mgt_flag & R_COLOR_MANAGEMENT;
	
	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);

	UI_view2d_view_ortho(v2d);
	
	//uiFreeBlocksWin(&sa->uiblocks, sa->win);

	/* only set once */
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable(GL_MAP1_VERTEX_3);

	/* aspect+font, set each time */
	snode->aspect= (v2d->cur.xmax - v2d->cur.xmin)/((float)ar->winx);
	// XXX snode->curfont= uiSetCurFont_ext(snode->aspect);

	UI_view2d_constant_grid_draw(v2d);
	/* backdrop */
	draw_nodespace_back_pix(ar, snode, color_manage);
	
	/* nodes */
	snode_set_context(snode, CTX_data_scene(C));
	
	if(snode->nodetree) {
		bNode *node;
		
		/* init ui blocks for opened node group trees first 
		 * so they're in the correct depth stack order */
		for(node= snode->nodetree->nodes.first; node; node= node->next) {
			if(node->flag & NODE_GROUP_EDIT)
				node_uiblocks_init(C, (bNodeTree *)node->id);
		}

		node_uiblocks_init(C, snode->nodetree);
		
		
		/* for now, we set drawing coordinates on each redraw */
		for(node= snode->nodetree->nodes.first; node; node= node->next) {
			if(node->flag & NODE_GROUP_EDIT)
				node_update_group(C, snode->nodetree, node);
			else if(node->flag & NODE_HIDDEN)
				node_update_hidden(node);
			else
				node_update(C, snode->nodetree, node);
		}

		node_draw_nodetree(C, ar, snode, snode->nodetree);
			
		/* active group */
		for(node= snode->nodetree->nodes.first; node; node= node->next) {
			if(node->flag & NODE_GROUP_EDIT)
				node_draw_group(C, ar, snode, snode->nodetree, node);
		}
	}
	
	/* draw grease-pencil ('canvas' strokes) */
	if (/*(snode->flag & SNODE_DISPGP) &&*/ (snode->nodetree))
		draw_gpencil_view2d((bContext*)C, 1);
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* draw grease-pencil (screen strokes, and also paintbuffer) */
	if (/*(snode->flag & SNODE_DISPGP) && */(snode->nodetree))
		draw_gpencil_view2d((bContext*)C, 0);
	
	/* scrollers */
	scrollers= UI_view2d_scrollers_calc(C, v2d, 10, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}
