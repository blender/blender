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
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_world_types.h"

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

#include "NOD_composite.h"
#include "NOD_shader.h"

#include "intern/node_util.h"

#include "node_intern.h"

/* width of socket columns in group display */
#define NODE_GROUP_FRAME		120

// XXX interface.h
extern void ui_dropshadow(rctf *rct, float radius, float aspect, int select);

/* XXX update functions for node editor are a mess, needs a clear concept */
void ED_node_tree_update(SpaceNode *snode, Scene *scene)
{
	snode_set_context(snode, scene);
	
	if(snode->nodetree && snode->nodetree->id.us==0)
		snode->nodetree->id.us= 1;
}

void ED_node_changed_update(ID *id, bNode *node)
{
	bNodeTree *nodetree, *edittree;
	int treetype;

	node_tree_from_ID(id, &nodetree, &edittree, &treetype);

	if(treetype==NTREE_SHADER) {
		DAG_id_tag_update(id, 0);

		if(GS(id->name) == ID_MA)
			WM_main_add_notifier(NC_MATERIAL|ND_SHADING_DRAW, id);
		else if(GS(id->name) == ID_LA)
			WM_main_add_notifier(NC_LAMP|ND_LIGHTING_DRAW, id);
		else if(GS(id->name) == ID_WO)
			WM_main_add_notifier(NC_WORLD|ND_WORLD_DRAW, id);
	}
	else if(treetype==NTREE_COMPOSIT) {
		if(node)
			nodeUpdate(edittree, node);
		/* don't use NodeTagIDChanged, it gives far too many recomposites for image, scene layers, ... */
			
		node= node_tree_get_editgroup(nodetree);
		if(node)
			nodeUpdateID(nodetree, node->id);

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

typedef struct NodeUpdateCalldata {
	bNodeTree *ntree;
	bNode *node;
} NodeUpdateCalldata;
static void node_generic_update_cb(void *calldata, ID *owner_id, bNodeTree *ntree)
{
	NodeUpdateCalldata *cd= (NodeUpdateCalldata*)calldata;
	/* check if nodetree uses the group stored in calldata */
	if (has_nodetree(ntree, cd->ntree))
		ED_node_changed_update(owner_id, cd->node);
}
void ED_node_generic_update(Main *bmain, bNodeTree *ntree, bNode *node)
{
	bNodeTreeType *tti= ntreeGetType(ntree->type);
	NodeUpdateCalldata cd;
	cd.ntree = ntree;
	cd.node = node;
	/* look through all datablocks, to support groups */
	tti->foreach_nodetree(bmain, &cd, node_generic_update_cb);
	
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
	
	/* add node uiBlocks in drawing order - prevents events going to overlapping nodes */
	
	for(node= ntree->nodes.first; node; node=node->next) {
			/* ui block */
			sprintf(str, "node buttons %p", (void *)node);
			node->block= uiBeginBlock(C, CTX_wm_region(C), str, UI_EMBOSS);
			uiBlockSetHandleFunc(node->block, do_node_internal_buttons, node);
			
			/* this cancels events for background nodes */
			uiBlockSetFlag(node->block, UI_BLOCK_CLIP_EVENTS);
	}
}

/* based on settings in node, sets drawing rect info. each redraw! */
static void node_update_basis(const bContext *C, bNodeTree *ntree, bNode *node)
{
	uiLayout *layout;
	PointerRNA ptr;
	bNodeSocket *nsock;
	float locx, locy;
	float dy;
	int buty;
	
	/* get "global" coords */
	nodeSpaceCoords(node, &locx, &locy);
	dy= locy;
	
	/* header */
	dy-= NODE_DY;
	
	/* little bit space in top */
	if(node->outputs.first)
		dy-= NODE_DYS/2;

	/* output sockets */
	for(nsock= node->outputs.first; nsock; nsock= nsock->next) {
		if(!nodeSocketIsHidden(nsock)) {
			nsock->locx= locx + node->width;
			nsock->locy= dy - NODE_DYS;
			dy-= NODE_DY;
		}
	}

	node->prvr.xmin= locx + NODE_DYS;
	node->prvr.xmax= locx + node->width- NODE_DYS;

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
							  locx+NODE_DYS, dy, node->butr.xmax, NODE_DY, UI_GetStyle());
		
		node->typeinfo->uifunc(layout, (bContext *)C, &ptr);
		
		uiBlockEndAlign(node->block);
		uiBlockLayoutResolve(node->block, NULL, &buty);
		
		dy= buty - NODE_DYS/2;
	}

	/* input sockets */
	for(nsock= node->inputs.first; nsock; nsock= nsock->next) {
		if(!nodeSocketIsHidden(nsock)) {
			nsock->locx= locx;
			nsock->locy= dy - NODE_DYS;
			dy-= NODE_DY;
		}
	}
	
	/* little bit space in end */
	if(node->inputs.first || (node->flag & (NODE_OPTIONS|NODE_PREVIEW))==0 )
		dy-= NODE_DYS/2;

	node->totr.xmin= locx;
	node->totr.xmax= locx + node->width;
	node->totr.ymax= locy;
	node->totr.ymin= MIN2(dy, locy-2*NODE_DY);
	
	/* Set the block bounds to clip mouse events from underlying nodes.
	 * Add a margin for sockets on each side.
	 */
	uiExplicitBoundsBlock(node->block,
						  node->totr.xmin - NODE_SOCKSIZE,
						  node->totr.ymin,
						  node->totr.xmax + NODE_SOCKSIZE,
						  node->totr.ymax);
}

/* based on settings in node, sets drawing rect info. each redraw! */
static void node_update_hidden(bNode *node)
{
	bNodeSocket *nsock;
	float locx, locy;
	float rad, drad, hiddenrad= HIDDEN_RAD;
	int totin=0, totout=0, tot;
	
	/* get "global" coords */
	nodeSpaceCoords(node, &locx, &locy);

	/* calculate minimal radius */
	for(nsock= node->inputs.first; nsock; nsock= nsock->next)
		if(!nodeSocketIsHidden(nsock))
			totin++;
	for(nsock= node->outputs.first; nsock; nsock= nsock->next)
		if(!nodeSocketIsHidden(nsock))
			totout++;
	
	tot= MAX2(totin, totout);
	if(tot>4) {
		hiddenrad += 5.0f*(float)(tot-4);
	}
	
	node->totr.xmin= locx;
	node->totr.xmax= locx + 3*hiddenrad + node->miniwidth;
	node->totr.ymax= locy + (hiddenrad - 0.5f*NODE_DY);
	node->totr.ymin= node->totr.ymax - 2*hiddenrad;
	
	/* output sockets */
	rad=drad= (float)M_PI/(1.0f + (float)totout);
	
	for(nsock= node->outputs.first; nsock; nsock= nsock->next) {
		if(!nodeSocketIsHidden(nsock)) {
			nsock->locx= node->totr.xmax - hiddenrad + (float)sin(rad)*hiddenrad;
			nsock->locy= node->totr.ymin + hiddenrad + (float)cos(rad)*hiddenrad;
			rad+= drad;
		}
	}
	
	/* input sockets */
	rad=drad= - (float)M_PI/(1.0f + (float)totin);
	
	for(nsock= node->inputs.first; nsock; nsock= nsock->next) {
		if(!nodeSocketIsHidden(nsock)) {
			nsock->locx= node->totr.xmin + hiddenrad + (float)sin(rad)*hiddenrad;
			nsock->locy= node->totr.ymin + hiddenrad + (float)cos(rad)*hiddenrad;
			rad+= drad;
		}
	}

	/* Set the block bounds to clip mouse events from underlying nodes.
	 * Add a margin for sockets on each side.
	 */
	uiExplicitBoundsBlock(node->block,
						  node->totr.xmin - NODE_SOCKSIZE,
						  node->totr.ymin,
						  node->totr.xmax + NODE_SOCKSIZE,
						  node->totr.ymax);
}

void node_update_default(const bContext *C, bNodeTree *ntree, bNode *node)
{
	if(node->flag & NODE_HIDDEN)
		node_update_hidden(node);
	else
		node_update_basis(C, ntree, node);
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

/* note: in cmp_util.c is similar code, for node_compo_pass_on()
 *       the same goes for shader and texture nodes. */
/* note: in node_edit.c is similar code, for untangle node */
static void node_draw_mute_line(View2D *v2d, SpaceNode *snode, bNode *node)
{
	ListBase links;
	LinkInOutsMuteNode *lnk;
	bNodeLink link= {NULL};
	int i;

	if(node->typeinfo->mutelinksfunc == NULL)
		return;

	/* Get default muting links (as bNodeSocket pointers). */
	links = node->typeinfo->mutelinksfunc(snode->edittree, node, NULL, NULL, NULL, NULL);

	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);

	link.fromnode = link.tonode = node;
	for(lnk = links.first; lnk; lnk = lnk->next) {
		for(i = 0; i < lnk->num_outs; i++) {
			link.fromsock = (bNodeSocket*)(lnk->in);
			link.tosock   = (bNodeSocket*)(lnk->outs)+i;
			node_draw_link_bezier(v2d, snode, &link, TH_REDALERT, 0, TH_WIRE, 0, TH_WIRE);
		}
		/* If num_outs > 1, lnk->outs was an allocated table of pointers... */
		if(i > 1)
			MEM_freeN(lnk->outs);
	}

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);

	BLI_freelistN(&links);
}

/* this might have some more generic use */
static void node_circle_draw(float x, float y, float size, char *col)
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

void node_socket_circle_draw(bNodeTree *UNUSED(ntree), bNodeSocket *sock, float size)
{
	bNodeSocketType *stype = ntreeGetSocketType(sock->type);
	node_circle_draw(sock->locx, sock->locy, size, stype->ui_color);
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

/* common handle function for operator buttons that need to select the node first */
static void node_toggle_button_cb(struct bContext *C, void *node_argv, void *op_argv)
{
	bNode *node = (bNode*)node_argv;
	const char *opname = (const char *)op_argv;
	
	/* select & activate only the button's node */
	node_select_single(C, node);
	
	WM_operator_name_call(C, opname, WM_OP_INVOKE_DEFAULT, NULL);
}

static void node_draw_basis(const bContext *C, ARegion *ar, SpaceNode *snode, bNodeTree *ntree, bNode *node)
{
	bNodeSocket *sock;
	rctf *rct= &node->totr;
	float iconofs;
	/* float socket_size= NODE_SOCKSIZE*U.dpi/72; */ /* UNUSED */
	float iconbutw= 0.8f*UI_UNIT_X;
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
	
	uiSetRoundBox(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_LEFT);
	ui_dropshadow(rct, BASIS_RAD, snode->aspect, node->flag & SELECT);
	
	/* header */
	if(color_id==TH_NODE)
		UI_ThemeColorShade(color_id, -20);
	else
		UI_ThemeColor(color_id);
	
	if(node->flag & NODE_MUTED)
		UI_ThemeColorBlend(color_id, TH_REDALERT, 0.5f);

	uiSetRoundBox(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT);
	uiRoundBox(rct->xmin, rct->ymax-NODE_DY, rct->xmax, rct->ymax, BASIS_RAD);
	
	/* show/hide icons */
	iconofs= rct->xmax - 7.0f;
	
	/* preview */
	if(node->typeinfo->flag & NODE_PREVIEW) {
		uiBut *but;
		iconofs-=iconbutw;
		uiBlockSetEmboss(node->block, UI_EMBOSSN);
		but = uiDefIconBut(node->block, TOGBUT, B_REDR, ICON_MATERIAL,
						   iconofs, rct->ymax-NODE_DY, iconbutw, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
		uiButSetFunc(but, node_toggle_button_cb, node, (void*)"NODE_OT_preview_toggle");
		/* XXX this does not work when node is activated and the operator called right afterwards,
		 * since active ID is not updated yet (needs to process the notifier).
		 * This can only work as visual indicator!
		 */
//		if (!(node->flag & (NODE_ACTIVE_ID|NODE_DO_OUTPUT)))
//			uiButSetFlag(but, UI_BUT_DISABLED);
		uiBlockSetEmboss(node->block, UI_EMBOSS);
	}
	/* group edit */
	if(node->type == NODE_GROUP) {
		uiBut *but;
		iconofs-=iconbutw;
		uiBlockSetEmboss(node->block, UI_EMBOSSN);
		but = uiDefIconBut(node->block, TOGBUT, B_REDR, ICON_NODETREE,
						   iconofs, rct->ymax-NODE_DY, iconbutw, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
		uiButSetFunc(but, node_toggle_button_cb, node, (void*)"NODE_OT_group_edit");
		uiBlockSetEmboss(node->block, UI_EMBOSS);
	}
	
	/* title */
	if(node->flag & SELECT) 
		UI_ThemeColor(TH_TEXT_HI);
	else
		UI_ThemeColorBlendShade(TH_TEXT, color_id, 0.4f, 10);
	
	/* open/close entirely? */
	{
		uiBut *but;
		int but_size = UI_UNIT_X *0.6f;
		/* XXX button uses a custom triangle draw below, so make it invisible without icon */
		uiBlockSetEmboss(node->block, UI_EMBOSSN);
		but = uiDefBut(node->block, TOGBUT, B_REDR, "",
					   rct->xmin+10.0f-but_size/2, rct->ymax-NODE_DY/2.0f-but_size/2, but_size, but_size, NULL, 0, 0, 0, 0, "");
		uiButSetFunc(but, node_toggle_button_cb, node, (void*)"NODE_OT_hide_toggle");
		uiBlockSetEmboss(node->block, UI_EMBOSS);
		
		/* custom draw function for this button */
		UI_DrawTriIcon(rct->xmin+10.0f, rct->ymax-NODE_DY/2.0f, 'v');
	}
	
	/* this isn't doing anything for the label, so commenting out
	if(node->flag & SELECT) 
		UI_ThemeColor(TH_TEXT_HI);
	else
		UI_ThemeColor(TH_TEXT); */
	
	BLI_strncpy(showname, nodeLabel(node), sizeof(showname));
	
	//if(node->flag & NODE_MUTED)
	//	sprintf(showname, "[%s]", showname);
	
	uiDefBut(node->block, LABEL, 0, showname, (short)(rct->xmin+15), (short)(rct->ymax-NODE_DY), 
			 (int)(iconofs - rct->xmin-18.0f), NODE_DY,  NULL, 0, 0, 0, 0, "");

	/* body */
	UI_ThemeColor4(TH_NODE);
	glEnable(GL_BLEND);
	uiSetRoundBox(UI_CNR_BOTTOM_LEFT);
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
			uiSetRoundBox(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_LEFT); // round all corners except lower right
			uiDrawBox(GL_LINE_LOOP, rct->xmin, rct->ymin, rct->xmax, rct->ymax, BASIS_RAD);
			
		glDisable( GL_LINE_SMOOTH );
		glDisable(GL_BLEND);
	}
	
	/* disable lines */
	if(node->flag & NODE_MUTED)
		node_draw_mute_line(v2d, snode, node);

	
	/* socket inputs, buttons */
	for(sock= node->inputs.first; sock; sock= sock->next) {
		bNodeSocketType *stype= ntreeGetSocketType(sock->type);
		
		if(nodeSocketIsHidden(sock))
			continue;
		
		node_socket_circle_draw(ntree, sock, NODE_SOCKSIZE);
		
		if (sock->link || (sock->flag & SOCK_HIDE_VALUE)) {
			uiDefBut(node->block, LABEL, 0, sock->name, sock->locx+NODE_DYS, sock->locy-NODE_DYS, node->width-NODE_DY, NODE_DY,
					 NULL, 0, 0, 0, 0, "");
		}
		else {
			if (stype->buttonfunc)
				stype->buttonfunc(C, node->block, ntree, node, sock, sock->name, sock->locx+NODE_DYS, sock->locy-NODE_DYS, node->width-NODE_DY);
		}
	}
	
	/* socket outputs */
	for(sock= node->outputs.first; sock; sock= sock->next) {
		PointerRNA sockptr;
		float slen;
		int ofs;
		
		RNA_pointer_create((ID*)ntree, &RNA_NodeSocket, sock, &sockptr);
		
		if(nodeSocketIsHidden(sock))
			continue;
		
		node_socket_circle_draw(ntree, sock, NODE_SOCKSIZE);
		
		ofs= 0;
		UI_ThemeColor(TH_TEXT);
		slen= snode->aspect*UI_GetStringWidth(sock->name);
		while(slen > node->width) {
			ofs++;
			slen= snode->aspect*UI_GetStringWidth(sock->name+ofs);
		}
		uiDefBut(node->block, LABEL, 0, sock->name+ofs, (short)(sock->locx-15.0f-slen), (short)(sock->locy-9.0f), 
				 (short)(node->width-NODE_DY), NODE_DY,  NULL, 0, 0, 0, 0, "");
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
	float socket_size= NODE_SOCKSIZE*U.dpi/72;
	int color_id= node_get_colorid(node);
	char showname[128];	/* 128 is used below */
	
	/* shadow */
	uiSetRoundBox(UI_CNR_ALL);
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
	{
		uiBut *but;
		int but_size = UI_UNIT_X *0.6f;
		/* XXX button uses a custom triangle draw below, so make it invisible without icon */
		uiBlockSetEmboss(node->block, UI_EMBOSSN);
		but = uiDefBut(node->block, TOGBUT, B_REDR, "",
					   rct->xmin+10.0f-but_size/2, centy-but_size/2, but_size, but_size, NULL, 0, 0, 0, 0, "");
		uiButSetFunc(but, node_toggle_button_cb, node, (void*)"NODE_OT_hide_toggle");
		uiBlockSetEmboss(node->block, UI_EMBOSS);
		
		/* custom draw function for this button */
		UI_DrawTriIcon(rct->xmin+10.0f, centy, 'h');
	}
	
	/* disable lines */
	if(node->flag & NODE_MUTED)
		node_draw_mute_line(&ar->v2d, snode, node);	
	
	if(node->flag & SELECT) 
		UI_ThemeColor(TH_TEXT_HI);
	else
		UI_ThemeColor(TH_TEXT);
	
	if(node->miniwidth>0.0f) {
		BLI_strncpy(showname, nodeLabel(node), sizeof(showname));
		
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
		if(!nodeSocketIsHidden(sock))
			node_socket_circle_draw(snode->nodetree, sock, socket_size);
	}
	
	for(sock= node->outputs.first; sock; sock= sock->next) {
		if(!nodeSocketIsHidden(sock))
			node_socket_circle_draw(snode->nodetree, sock, socket_size);
	}
	
	uiEndBlock(C, node->block);
	uiDrawBlock(C, node->block);
	node->block= NULL;
}

void node_draw_default(const bContext *C, ARegion *ar, SpaceNode *snode, bNodeTree *ntree, bNode *node)
{
	if(node->flag & NODE_HIDDEN)
		node_draw_hidden(C, ar, snode, node);
	else
		node_draw_basis(C, ar, snode, ntree, node);
}

static void node_update(const bContext *C, bNodeTree *ntree, bNode *node)
{
	if (node->typeinfo->drawupdatefunc)
		node->typeinfo->drawupdatefunc(C, ntree, node);
}

void node_update_nodetree(const bContext *C, bNodeTree *ntree, float offsetx, float offsety)
{
	bNode *node;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		/* XXX little hack */
		node->locx += offsetx;
		node->locy += offsety;
		
		node_update(C, ntree, node);
		
		node->locx -= offsetx;
		node->locy -= offsety;
	}
}

static void node_draw(const bContext *C, ARegion *ar, SpaceNode *snode, bNodeTree *ntree, bNode *node)
{
	if (node->typeinfo->drawfunc)
		node->typeinfo->drawfunc(C, ar, snode, ntree, node);
}

void node_draw_nodetree(const bContext *C, ARegion *ar, SpaceNode *snode, bNodeTree *ntree)
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
	
	/* draw nodes, last nodes in front */
	for(a=0, node= ntree->nodes.first; node; node=node->next, a++) {
		node->nr= a;		/* index of node in list, used for exec event code */
		node_draw(C, ar, snode, ntree, node);
	}
}

void drawnodespace(const bContext *C, ARegion *ar, View2D *v2d)
{
	View2DScrollers *scrollers;
	SpaceNode *snode= CTX_wm_space_node(C);
	Scene *scene= CTX_data_scene(C);
	int color_manage = scene->r.color_mgt_flag & R_COLOR_MANAGEMENT;
	bNodeLinkDrag *nldrag;
	
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
		
		node_uiblocks_init(C, snode->nodetree);
		
		/* uiBlocks must be initialized in drawing order for correct event clipping.
		 * Node group internal blocks added after the main group block.
		 */
		for(node= snode->nodetree->nodes.first; node; node= node->next) {
			if(node->flag & NODE_GROUP_EDIT)
				node_uiblocks_init(C, (bNodeTree *)node->id);
		}
		
		node_update_nodetree(C, snode->nodetree, 0.0f, 0.0f);
		node_draw_nodetree(C, ar, snode, snode->nodetree);
		
		#if 0
		/* active group */
		for(node= snode->nodetree->nodes.first; node; node= node->next) {
			if(node->flag & NODE_GROUP_EDIT)
				node_draw_group(C, ar, snode, snode->nodetree, node);
		}
		#endif
	}
	
	/* temporary links */
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	for(nldrag= snode->linkdrag.first; nldrag; nldrag= nldrag->next)
		node_draw_link(&ar->v2d, snode, nldrag->link);
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
	
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
