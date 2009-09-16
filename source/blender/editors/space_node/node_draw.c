/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 * Contributor(s): Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "DNA_ID.h"
#include "DNA_node_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_action_types.h"
#include "DNA_color_types.h"
#include "DNA_customdata_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_text_types.h"
#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_texture.h"
#include "BKE_text.h"
#include "BKE_utildefines.h"

/* #include "BDR_gpencil.h" XXX */

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_util.h"
#include "ED_types.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "CMP_node.h"
#include "SHD_node.h"

#include "node_intern.h"

// XXX interface.h
extern void ui_dropshadow(rctf *rct, float radius, float aspect, int select);
extern void gl_round_box(int mode, float minx, float miny, float maxx, float maxy, float rad);
extern void ui_draw_tria_icon(float x, float y, float aspect, char dir);


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

/* based on settings in node, sets drawing rect info. each redraw! */
static void node_update(bNode *node)
{
	bNodeSocket *nsock;
	float dy= node->locy;
	
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
	
	node->prvr.xmin= node->butr.xmin= node->locx + NODE_DYS;
	node->prvr.xmax= node->butr.xmax= node->locx + node->width- NODE_DYS;
	
	/* preview rect? */
	if(node->flag & NODE_PREVIEW) {
		/* only recalculate size when there's a preview actually, otherwise we use stored result */
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
	}

	/* XXX ugly hack, typeinfo for group is generated */
	if(node->type == NODE_GROUP)
		; // XXX node->typeinfo->butfunc= node_buts_group;
	
	/* buttons rect? */
	if((node->flag & NODE_OPTIONS) && node->typeinfo->butfunc) {
		dy-= NODE_DYS/2;
		node->butr.ymax= dy;
		node->butr.ymin= dy - (float)node->typeinfo->butfunc(NULL, NULL, node, NULL);
		dy= node->butr.ymin - NODE_DYS/2;
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
static void node_update_group(bNode *gnode)
{
	bNodeTree *ngroup= (bNodeTree *)gnode->id;
	bNode *node;
	bNodeSocket *nsock;
	rctf *rect= &gnode->totr;
	int counter;
	
	/* center them, is a bit of abuse of locx and locy though */
	for(node= ngroup->nodes.first; node; node= node->next) {
		node->locx+= gnode->locx;
		node->locy+= gnode->locy;
		if(node->flag & NODE_HIDDEN)
			node_update_hidden(node);
		else
			node_update(node);
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
	if(counter==1) return;	/* should be prevented? */
	
	rect->xmin-= NODE_DY;
	rect->ymin-= NODE_DY;
	rect->xmax+= NODE_DY;
	rect->ymax+= NODE_DY;
	
	/* output sockets */
	for(nsock= gnode->outputs.first; nsock; nsock= nsock->next) {
		nsock->locx= rect->xmax;
		nsock->locy= nsock->tosock->locy;
	}
	
	/* input sockets */
	for(nsock= gnode->inputs.first; nsock; nsock= nsock->next) {
		nsock->locx= rect->xmin;
		nsock->locy= nsock->tosock->locy;
	}
}

/* note: in cmp_util.c is similar code, for node_compo_pass_on() */
static void node_draw_mute_line(View2D *v2d, SpaceNode *snode, bNode *node)
{
	bNodeSocket *valsock= NULL, *colsock= NULL, *vecsock= NULL;
	bNodeSocket *sock;
	bNodeLink link;
	int a;
	
	memset(&link, 0, sizeof(bNodeLink));
	
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
	UI_ThemeColor(TH_REDALERT);
	glEnable(GL_BLEND);
	glEnable( GL_LINE_SMOOTH );
	
	if(valsock || colsock || vecsock) {
		for(a=0, sock= node->outputs.first; sock; sock= sock->next, a++) {
			if(nodeCountSocketLinks(snode->edittree, sock)) {
				link.tosock= sock;
				
				if(sock->type==SOCK_VALUE && valsock) {
					link.fromsock= valsock;
					node_draw_link_bezier(v2d, snode, &link, TH_WIRE, TH_WIRE, 0);
					valsock= NULL;
				}
				if(sock->type==SOCK_VECTOR && vecsock) {
					link.fromsock= vecsock;
					node_draw_link_bezier(v2d, snode, &link, TH_WIRE, TH_WIRE, 0);
					vecsock= NULL;
				}
				if(sock->type==SOCK_RGBA && colsock) {
					link.fromsock= colsock;
					node_draw_link_bezier(v2d, snode, &link, TH_WIRE, TH_WIRE, 0);
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
static void circle_draw(float x, float y, float size, int type, int col[3])
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
	
	/* choose color based on sock flags */
	if(sock->flag & SELECT) {
		if(sock->flag & SOCK_SEL) {
			col[0]= 240; col[1]= 200; col[2]= 40;}
		else if(sock->type==SOCK_VALUE) {
			col[0]= 200; col[1]= 200; col[2]= 200;}
		else if(sock->type==SOCK_VECTOR) {
			col[0]= 140; col[1]= 140; col[2]= 240;}
		else if(sock->type==SOCK_RGBA) {
			col[0]= 240; col[1]= 240; col[2]= 100;}
		else {
			col[0]= 140; col[1]= 240; col[2]= 140;}
	}
	else if(sock->flag & SOCK_SEL) {
		col[0]= 200; col[1]= 160; col[2]= 0;}
	else {
		if(sock->type==-1) {
			col[0]= 0; col[1]= 0; col[2]= 0;}
		else if(sock->type==SOCK_VALUE) {
			col[0]= 160; col[1]= 160; col[2]= 160;}
		else if(sock->type==SOCK_VECTOR) {
			col[0]= 100; col[1]= 100; col[2]= 200;}
		else if(sock->type==SOCK_RGBA) {
			col[0]= 200; col[1]= 200; col[2]= 40;}
		else { 
			col[0]= 100; col[1]= 200; col[2]= 100;}
	}
	
	circle_draw(sock->locx, sock->locy, size, sock->type, col);
}

static void node_sync_cb(bContext *C, void *snode_v, void *node_v)
{
	SpaceNode *snode= snode_v;
	
	if(snode->treetype==NTREE_SHADER) {
		nodeShaderSynchronizeID(node_v, 1);
		// allqueue(REDRAWBUTSSHADING, 0);
	}
}

/* **************  Socket callbacks *********** */

static void socket_vector_menu_cb(bContext *C, void *node_v, void *ntree_v)
{
	if(node_v && ntree_v) {
		NodeTagChanged(ntree_v, node_v); 
		// addqueue(curarea->win, UI_BUT_EVENT, B_NODE_EXEC); XXX
	}
}

/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *socket_vector_menu(bContext *C, ARegion *ar, void *socket_v)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	ScrArea *sa= CTX_wm_area(C);
	bNode *node;
	bNodeSocket *sock= socket_v;
	bNodeStack *ns= &sock->ns;
	uiBlock *block;
	uiBut *bt;
	
	/* a bit ugly... retrieve the node the socket comes from */
	for(node= snode->nodetree->nodes.first; node; node= node->next) {
		bNodeSocket *sockt;
		for(sockt= node->inputs.first; sockt; sockt= sockt->next)
			if(sockt==sock)
				break;
		if(sockt)
			break;
	}
	
	block= uiBeginBlock(C, ar, "socket menu", UI_EMBOSS);

	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",			-4, -4, 188, 68, NULL, 0, 0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	bt= uiDefButF(block, NUMSLI, 0, "X ",	 0,40,180,20, ns->vec, ns->min, ns->max, 10, 0, "");
	uiButSetFunc(bt, socket_vector_menu_cb, node, snode->nodetree);
	bt= uiDefButF(block, NUMSLI, 0, "Y ",	 0,20,180,20, ns->vec+1, ns->min, ns->max, 10, 0, "");
	uiButSetFunc(bt, socket_vector_menu_cb, node, snode->nodetree);
	bt= uiDefButF(block, NUMSLI, 0, "Z ",	 0,0,180,20, ns->vec+2, ns->min, ns->max, 10, 0, "");
	uiButSetFunc(bt, socket_vector_menu_cb, node, snode->nodetree);
	
	uiBlockSetDirection(block, UI_TOP);
	uiEndBlock(C, block);
	
	ED_area_tag_redraw(sa);
	
	return block;
}

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
	
#ifdef __APPLE__
//	if(is_a_really_crappy_nvidia_card()) {	XXX
//		float zoomx= curarea->winx/(float)(G.v2d->cur.xmax-G.v2d->cur.xmin);
//		float zoomy= curarea->winy/(float)(G.v2d->cur.ymax-G.v2d->cur.ymin);
//		glPixelZoom(zoomx*xscale, zoomy*yscale);
//	}
//	else
#endif
		glPixelZoom(xscale, yscale);

	glEnable(GL_BLEND);
	glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );	/* premul graphics */
	
	glColor4f(1.0, 1.0, 1.0, 1.0);
	glaDrawPixelsTex(prv->xmin, prv->ymin, preview->xsize, preview->ysize, GL_FLOAT, preview->rect);
	
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glDisable(GL_BLEND);
	glPixelZoom(1.0f, 1.0f);

	UI_ThemeColorShadeAlpha(TH_BACK, -15, +100);
	fdrawbox(prv->xmin, prv->ymin, prv->xmax, prv->ymax);
	
}

static void do_node_internal_buttons(bContext *C, void *node_v, int event)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	
	if(event==B_NODE_EXEC) {
		if(snode->treetype==NTREE_SHADER) {
			WM_event_add_notifier(C, NC_MATERIAL|ND_SHADING, snode->id);
		}
		else if(snode->treetype==NTREE_COMPOSIT) {
			bNode *node= node_v;
			
			NodeTagChanged(snode->edittree, node);
			/* don't use NodeTagIDChanged, it gives far too many recomposites for image, scene layers, ... */
				
			/* not the best implementation of the world... but we need it to work now :) */
			if(node->type==CMP_NODE_R_LAYERS && node->custom2) {
				/* add event for this window (after render curarea can be changed) */
				//addqueue(curarea->win, UI_BUT_EVENT, B_NODE_TREE_EXEC);
				
				//composite_node_render(snode, node);
				//snode_handle_recalc(snode);
				
				/* add another event, a render can go fullscreen and open new window */
				//addqueue(curarea->win, UI_BUT_EVENT, B_NODE_TREE_EXEC);
			}
			else {
				node= node_tree_get_editgroup(snode->nodetree);
				if(node)
					NodeTagIDChanged(snode->nodetree, node->id);
			}
			WM_event_add_notifier(C, NC_SCENE|ND_NODES, CTX_data_scene(C));
		}			
		else if(snode->treetype==NTREE_TEXTURE) {
			WM_event_add_notifier(C, NC_TEXTURE|ND_NODES, snode->id);
		}
	}
	
}

static void node_draw_basis(const bContext *C, ARegion *ar, SpaceNode *snode, bNode *node)
{
	bNodeSocket *sock;
	uiBlock *block;
	uiBut *bt;
	rctf *rct= &node->totr;
	float /*slen,*/ iconofs;
	int /*ofs,*/ color_id= node_get_colorid(node);
	char showname[128]; /* 128 used below */
	View2D *v2d = &ar->v2d;
	char str[32];
	
	/* make unique block name, also used for handling blocks in editnode.c */
	sprintf(str, "node buttons %p", node);
	block= uiBeginBlock(C, ar, str, UI_EMBOSS);
	uiBlockSetHandleFunc(block, do_node_internal_buttons, node);
	
	uiSetRoundBox(15-4);
	ui_dropshadow(rct, BASIS_RAD, snode->aspect, node->flag & SELECT);
	
	/* header */
	if(color_id==TH_NODE)
		UI_ThemeColorShade(color_id, -20);
	else
		UI_ThemeColor(color_id);
		
	uiSetRoundBox(3);
	uiRoundBox(rct->xmin, rct->ymax-NODE_DY, rct->xmax, rct->ymax, BASIS_RAD);
	
	/* show/hide icons, note this sequence is copied in editnode.c */
	iconofs= rct->xmax;
	
	if(node->typeinfo->flag & NODE_PREVIEW) {
		int icon_id;
		
		if(node->flag & (NODE_ACTIVE_ID|NODE_DO_OUTPUT))
			icon_id= ICON_MATERIAL;
		else
			icon_id= ICON_MATERIAL_DATA;
		iconofs-= 18.0f;
		glEnable(GL_BLEND);
		UI_icon_draw_aspect_blended(iconofs, rct->ymax-NODE_DY+2, icon_id, snode->aspect, -60);
		glDisable(GL_BLEND);
	}
	if(node->type == NODE_GROUP) {
		
		iconofs-= 18.0f;
		glEnable(GL_BLEND);
		if(node->id->lib) {
			glPixelTransferf(GL_GREEN_SCALE, 0.7f);
			glPixelTransferf(GL_BLUE_SCALE, 0.3f);
			UI_icon_draw_aspect(iconofs, rct->ymax-NODE_DY+2, ICON_NODE, snode->aspect);
			glPixelTransferf(GL_GREEN_SCALE, 1.0f);
			glPixelTransferf(GL_BLUE_SCALE, 1.0f);
		}
		else {
			UI_icon_draw_aspect_blended(iconofs, rct->ymax-NODE_DY+2, ICON_NODE, snode->aspect, -60);
		}
		glDisable(GL_BLEND);
	}
	if(node->typeinfo->flag & NODE_OPTIONS) {
		iconofs-= 18.0f;
		glEnable(GL_BLEND);
		UI_icon_draw_aspect_blended(iconofs, rct->ymax-NODE_DY+2, ICON_BUTS, snode->aspect, -60);
		glDisable(GL_BLEND);
	}
	{	/* always hide/reveal unused sockets */ 
		int shade;

		iconofs-= 18.0f;
		// XXX re-enable
		/*if(node_has_hidden_sockets(node))
			shade= -40;
		else*/
			shade= -90;
		glEnable(GL_BLEND);
		UI_icon_draw_aspect_blended(iconofs, rct->ymax-NODE_DY+2, ICON_PLUS, snode->aspect, shade);
		glDisable(GL_BLEND);
	}
	
	/* title */
	if(node->flag & SELECT) 
		UI_ThemeColor(TH_TEXT_HI);
	else
		UI_ThemeColorBlendShade(TH_TEXT, color_id, 0.4f, 10);
	
	/* open/close entirely? */
	ui_draw_tria_icon(rct->xmin+8.0f, rct->ymax-NODE_DY+4.0f, snode->aspect, 'v');
	
	if(node->flag & SELECT) 
		UI_ThemeColor(TH_TEXT_HI);
	else
		UI_ThemeColor(TH_TEXT);
	
	if(node->flag & NODE_MUTED)
		sprintf(showname, "[%s]", node->name);
	else if(node->username[0])
		sprintf(showname, "(%s) %s", node->username, node->name);
	else
		BLI_strncpy(showname, node->name, 128);
	
	uiDefBut(block, LABEL, 0, showname, (short)(rct->xmin+15), (short)(rct->ymax-NODE_DY), 
			 (int)(iconofs - rct->xmin-18.0f), NODE_DY,  NULL, 0, 0, 0, 0, "");

	/* body */
	UI_ThemeColor4(TH_NODE);
	glEnable(GL_BLEND);
	uiSetRoundBox(8);
	uiRoundBox(rct->xmin, rct->ymin, rct->xmax, rct->ymax-NODE_DY, BASIS_RAD);
	glDisable(GL_BLEND);

	/* scaling indicator */
	node_scaling_widget(TH_NODE, snode->aspect, rct->xmax-BASIS_RAD*snode->aspect, rct->ymin, rct->xmax, rct->ymin+BASIS_RAD*snode->aspect);

	/* outline active emphasis */
	if(node->flag & NODE_ACTIVE) {
		glEnable(GL_BLEND);
		glColor4ub(200, 200, 200, 140);
		uiSetRoundBox(15-4);
		gl_round_box(GL_LINE_LOOP, rct->xmin, rct->ymin, rct->xmax, rct->ymax, BASIS_RAD);
		glDisable(GL_BLEND);
	}
	
	/* disable lines */
	if(node->flag & NODE_MUTED)
		node_draw_mute_line(v2d, snode, node);

	
	/* hurmf... another candidate for callback, have to see how this works first */
	if(node->id && block && snode->treetype==NTREE_SHADER)
		nodeShaderSynchronizeID(node, 0);
	
	/* socket inputs, buttons */
	for(sock= node->inputs.first; sock; sock= sock->next) {
		if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
			socket_circle_draw(sock, NODE_SOCKSIZE);
			
			if(block && sock->link==NULL) {
				float *butpoin= sock->ns.vec;
				
				if(sock->type==SOCK_VALUE) {
					bt= uiDefButF(block, NUM, B_NODE_EXEC, sock->name, 
						  (short)sock->locx+NODE_DYS, (short)(sock->locy)-9, (short)node->width-NODE_DY, 17, 
						  butpoin, sock->ns.min, sock->ns.max, 10, 2, "");
					uiButSetFunc(bt, node_sync_cb, snode, node);
				}
				else if(sock->type==SOCK_VECTOR) {
					uiDefBlockBut(block, socket_vector_menu, sock, sock->name, 
						  (short)sock->locx+NODE_DYS, (short)sock->locy-9, (short)node->width-NODE_DY, 17, 
						  "");
				}
				else if(block && sock->type==SOCK_RGBA) {
					short labelw= (short)node->width-NODE_DY-40, width;
					
					if(labelw>0) width= 40; else width= (short)node->width-NODE_DY;
					
					bt= uiDefButF(block, COL, B_NODE_EXEC, "", 
						(short)(sock->locx+NODE_DYS), (short)sock->locy-8, width, 15, 
						   butpoin, 0, 0, 0, 0, "");
					uiButSetFunc(bt, node_sync_cb, snode, node);
					
					if(labelw>0) uiDefBut(block, LABEL, 0, sock->name, 
										   (short)(sock->locx+NODE_DYS) + 40, (short)sock->locy-8, labelw, 15, 
										   NULL, 0, 0, 0, 0, "");
				}
			}
			else {
				
				uiDefBut(block, LABEL, 0, sock->name, (short)(sock->locx+3.0f), (short)(sock->locy-9.0f), 
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
			
			uiDefBut(block, LABEL, 0, sock->name+ofs, (short)(sock->locx-15.0f-slen), (short)(sock->locy-9.0f), 
					 (short)(node->width-NODE_DY), NODE_DY,  NULL, 0, 0, 0, 0, "");
		}
	}
	
	/* preview */
	if(node->flag & NODE_PREVIEW)
		if(node->preview && node->preview->rect)
			node_draw_preview(node->preview, &node->prvr);
		
	/* buttons */
	if(node->flag & NODE_OPTIONS) {
		if(block) {
			if(node->typeinfo->butfunc) {
				node->typeinfo->butfunc(block, snode->nodetree, node, &node->butr);
			}
		}
	}
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}

static void node_draw_hidden(const bContext *C, ARegion *ar, SpaceNode *snode, bNode *node)
{
	uiBlock *block;
	bNodeSocket *sock;
	rctf *rct= &node->totr;
	float dx, centy= 0.5f*(rct->ymax+rct->ymin);
	float hiddenrad= 0.5f*(rct->ymax-rct->ymin);
	int color_id= node_get_colorid(node);
	char str[32], showname[128];	/* 128 is used below */
	
	/* make unique block name, also used for handling blocks in editnode.c */
	sprintf(str, "node buttons %p", node);
	block= uiBeginBlock(C, ar, str, UI_EMBOSS);
	uiBlockSetHandleFunc(block, do_node_internal_buttons, node);
	
	/* shadow */
	uiSetRoundBox(15);
	ui_dropshadow(rct, hiddenrad, snode->aspect, node->flag & SELECT);

	/* body */
	UI_ThemeColor(color_id);	
	uiRoundBox(rct->xmin, rct->ymin, rct->xmax, rct->ymax, hiddenrad);
	
	/* outline active emphasis */
	if(node->flag & NODE_ACTIVE) {
		glEnable(GL_BLEND);
		glColor4ub(200, 200, 200, 140);
		gl_round_box(GL_LINE_LOOP, rct->xmin, rct->ymin, rct->xmax, rct->ymax, hiddenrad);
		glDisable(GL_BLEND);
	}
	
	/* title */
	if(node->flag & SELECT) 
		UI_ThemeColor(TH_TEXT_HI);
	else
		UI_ThemeColorBlendShade(TH_TEXT, color_id, 0.4f, 10);
	
	/* open entirely icon */
	ui_draw_tria_icon(rct->xmin+9.0f, centy-6.0f, snode->aspect, 'h');	
	
	/* disable lines */
	if(node->flag & NODE_MUTED)
		node_draw_mute_line(&ar->v2d, snode, node);	
	
	if(node->flag & SELECT) 
		UI_ThemeColor(TH_TEXT_HI);
	else
		UI_ThemeColor(TH_TEXT);
	
	if(node->miniwidth>0.0f) {

		if(node->flag & NODE_MUTED)
			sprintf(showname, "[%s]", node->name);
		else if(node->username[0])
			sprintf(showname, "(%s)%s", node->username, node->name);
		else
			BLI_strncpy(showname, node->name, 128);

		uiDefBut(block, LABEL, 0, showname, (short)(rct->xmin+15), (short)(centy-10), 
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
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);

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
				node_draw_basis(C, ar, snode, node);
		}
	}
	
	/* selected */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if(node->flag & NODE_GROUP_EDIT);
			else if(node->flag & NODE_HIDDEN)
				node_draw_hidden(C, ar, snode, node);
			else
				node_draw_basis(C, ar, snode, node);
		}
	}	
}

/* fake links from groupnode to internal nodes */
static void node_draw_group_links(View2D *v2d, SpaceNode *snode, bNode *gnode)
{
	bNodeLink fakelink;
	bNodeSocket *sock;
	
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	
	fakelink.tonode= fakelink.fromnode= gnode;
	
	for(sock= gnode->inputs.first; sock; sock= sock->next) {
		if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
			if(sock->tosock) {
				fakelink.fromsock= sock;
				fakelink.tosock= sock->tosock;
				node_draw_link(v2d, snode, &fakelink);
			}
		}
	}
	
	for(sock= gnode->outputs.first; sock; sock= sock->next) {
		if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
			if(sock->tosock) {
				fakelink.tosock= sock;
				fakelink.fromsock= sock->tosock;
				node_draw_link(v2d, snode, &fakelink);
			}
		}
	}
	
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

/* groups are, on creation, centered around 0,0 */
static void node_draw_group(const bContext *C, ARegion *ar, SpaceNode *snode, bNode *gnode)
{
	bNodeTree *ngroup= (bNodeTree *)gnode->id;
	bNodeSocket *sock;
	rctf rect= gnode->totr;
	char showname[128];
	
	/* backdrop header */
	glEnable(GL_BLEND);
	uiSetRoundBox(3);
	UI_ThemeColorShadeAlpha(TH_NODE_GROUP, 0, -70);
	gl_round_box(GL_POLYGON, rect.xmin, rect.ymax, rect.xmax, rect.ymax+NODE_DY, BASIS_RAD);
	
	/* backdrop body */
	UI_ThemeColorShadeAlpha(TH_BACK, -8, -70);
	uiSetRoundBox(12);
	gl_round_box(GL_POLYGON, rect.xmin, rect.ymin, rect.xmax, rect.ymax, BASIS_RAD);
	
	/* selection outline */
	uiSetRoundBox(15);
	glColor4ub(200, 200, 200, 140);
	glEnable( GL_LINE_SMOOTH );
	gl_round_box(GL_LINE_LOOP, rect.xmin, rect.ymin, rect.xmax, rect.ymax+NODE_DY, BASIS_RAD);
	glDisable( GL_LINE_SMOOTH );
	glDisable(GL_BLEND);
	
	/* backdrop title */
	UI_ThemeColor(TH_TEXT_HI);

	if(gnode->username[0]) {
		strcpy(showname,"(");
		strcat(showname, gnode->username);
		strcat(showname,") ");
		strcat(showname, ngroup->id.name+2);
	}
	else
		strcpy(showname, ngroup->id.name+2);

	UI_DrawString(rect.xmin+8.0f, rect.ymax+5.0f, showname);
	
	/* links from groupsockets to the internal nodes */
	node_draw_group_links(&ar->v2d, snode, gnode);
	
	/* group sockets */
	for(sock= gnode->inputs.first; sock; sock= sock->next)
		if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL)))
			socket_circle_draw(sock, NODE_SOCKSIZE);
	for(sock= gnode->outputs.first; sock; sock= sock->next)
		if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL)))
			socket_circle_draw(sock, NODE_SOCKSIZE);

	/* and finally the whole tree */
	node_draw_nodetree(C, ar, snode, ngroup);
}

void drawnodespace(const bContext *C, ARegion *ar, View2D *v2d)
{
	float col[3];
	View2DScrollers *scrollers;
	SpaceNode *snode= CTX_wm_space_node(C);
	
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0);
	glClear(GL_COLOR_BUFFER_BIT);

	UI_view2d_view_ortho(C, v2d);
	
	//uiFreeBlocksWin(&sa->uiblocks, sa->win);

	/* only set once */
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable(GL_MAP1_VERTEX_3);

	/* aspect+font, set each time */
	snode->aspect= (v2d->cur.xmax - v2d->cur.xmin)/((float)ar->winx);
	// XXX snode->curfont= uiSetCurFont_ext(snode->aspect);

	UI_view2d_constant_grid_draw(C, v2d);
	/* backdrop */
	draw_nodespace_back_pix(ar, snode);
	
	/* nodes */
	snode_set_context(snode, CTX_data_scene(C));
	
	if(snode->nodetree) {
		bNode *node;
		
		/* for now, we set drawing coordinates on each redraw */
		for(node= snode->nodetree->nodes.first; node; node= node->next) {
			if(node->flag & NODE_GROUP_EDIT)
				node_update_group(node);
			else if(node->flag & NODE_HIDDEN)
				node_update_hidden(node);
			else
				node_update(node);
		}

		node_draw_nodetree(C, ar, snode, snode->nodetree);
			
		/* active group */
		for(node= snode->nodetree->nodes.first; node; node= node->next) {
			if(node->flag & NODE_GROUP_EDIT)
				node_draw_group(C, ar, snode, node);
		}
	}
	
	/* draw grease-pencil ('canvas' strokes) */
	/*if ((snode->flag & SNODE_DISPGP) && (snode->nodetree))
		draw_gpencil_2dview(sa, 1);*/
	
	/* restore viewport (not needed yet) */
	/*mywinset(sa->win);*/

	/* ortho at pixel level curarea */
	/*myortho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);*/
	
	/* draw grease-pencil (screen strokes) */
	/*if ((snode->flag & SNODE_DISPGP) && (snode->nodetree))
		draw_gpencil_2dview(sa, 0);*/

	//draw_area_emboss(sa);
	
	/* it is important to end a view in a transform compatible with buttons */
	/*bwin_scalematrix(sa->win, snode->blockscale, snode->blockscale, snode->blockscale);
	nodes_blockhandlers(sa);*/
	
	//curarea->win_swap= WIN_BACK_OK;
	
	/* in the end, this is a delayed previewrender test, to allow buttons to be first */
	/*if(snode->flag & SNODE_DO_PREVIEW) {
		addafterqueue(sa->win, RENDERPREVIEW, 1);
		snode->flag &= ~SNODE_DO_PREVIEW;
	}*/
	
	
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers= UI_view2d_scrollers_calc(C, v2d, 10, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}
