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
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"

#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_interface_icons.h"
#include "BIF_language.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_screen.h"

#include "BSE_drawipo.h"
#include "BSE_node.h"
#include "BSE_view.h"

#include "BMF_Api.h"

#include "blendef.h"
#include "butspace.h"
#include "interface.h"	/* urm...  for rasterpos_safe, roundbox */
#include "mydevice.h"

#include "MEM_guardedalloc.h"

static void snode_drawstring(SpaceNode *snode, char *str, int okwidth)
{
	char drawstr[NODE_MAXSTR];
	int width;
	
	if(str[0]==0 || okwidth<4) return;
	
	BLI_strncpy(drawstr, str, NODE_MAXSTR);
	width= snode->aspect*BIF_GetStringWidth(snode->curfont, drawstr, 0);

	if(width > okwidth) {
		int len= strlen(drawstr)-1;
		
		while(width > okwidth && len>=0) {
			drawstr[len]= 0;
			
			width= snode->aspect*BIF_GetStringWidth(snode->curfont, drawstr, 0);
			len--;
		}
		if(len==0) return;
	}
	BIF_DrawString(snode->curfont, drawstr, 0);

}

/* **************  Socket callbacks *********** */

/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *socket_value_menu(void *sock_v)
{
	bNodeSocket *sock= sock_v;
	uiBlock *block;
	char name[NODE_MAXSTR];
	
	/* don't add new block to a listbase if caller is LABEL button */
	block= uiNewBlock(NULL, "socket menu", UI_EMBOSS, UI_HELV, curarea->win);
	
	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",			-4, -4, 188, 28, NULL, 0, 0, 0, 0, "");
	
	BLI_strncpy(name, sock->name, NODE_MAXSTR-1);
	strcat(name, " ");
	uiDefButF(block, NUMSLI, 0, name,	 0,0,180,20, sock->ns.vec, 0.0, 1.0, 10, 0, "");
	
	uiBlockSetDirection(block, UI_TOP);
	
	return block;
}

/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *socket_vector_menu(void *sock_v)
{
	bNodeSocket *sock= sock_v;
	uiBlock *block;
	
	/* don't add new block to a listbase if caller is LABEL button */
	block= uiNewBlock(NULL, "socket menu", UI_EMBOSS, UI_HELV, curarea->win);
	
	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",			-4, -4, 188, 68, NULL, 0, 0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, 0, "X ",	 0,40,180,20, sock->ns.vec, -1.0, 1.0, 10, 0, "");
	uiDefButF(block, NUMSLI, 0, "Y ",	 0,20,180,20, sock->ns.vec+1, -1.0, 1.0, 10, 0, "");
	uiDefButF(block, NUMSLI, 0, "Z ",	 0,0,180,20, sock->ns.vec+2, -1.0, 1.0, 10, 0, "");
	
	uiBlockSetDirection(block, UI_TOP);
	
	return block;
}

static uiBlock *socket_color_menu(void *sock_v)
{
	bNodeSocket *sock= sock_v;
	uiBlock *block;
	
	/* don't add new block to a listbase if caller is LABEL button */
	block= uiNewBlock(NULL, "socket menu", UI_EMBOSS, UI_HELV, curarea->win);
	
	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",			-4, -4, 188, 68, NULL, 0, 0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, 0, "R ",	 0,40,180,20, sock->ns.vec, 0.0, 1.0, 10, 0, "");
	uiDefButF(block, NUMSLI, 0, "G ",	 0,20,180,20, sock->ns.vec+1, 0.0, 1.0, 10, 0, "");
	uiDefButF(block, NUMSLI, 0, "B ",	 0,0,180,20, sock->ns.vec+2, 0.0, 1.0, 10, 0, "");
	
	uiBlockSetDirection(block, UI_TOP);
	
	return block;
}

static void node_ID_title_cb(void *node_v, void *unused_v)
{
	bNode *node= node_v;

	if(node->id)
		BLI_strncpy(node->name, node->id->name+2, 21);
}

/* ****************** BUTTON CALLBACKS FOR SHADER NODES ***************** */

static int node_shader_buts_material(uiBlock *block, bNode *node, rctf *butr)
{
	if(block) {
		uiBut *bt;
		float dx= (butr->xmax-butr->xmin)/3.0f;
		
		uiBlockBeginAlign(block);
		bt= uiDefIDPoinBut(block, test_matpoin_but, ID_MA, B_NODE_EXEC, "",
					   butr->xmin, butr->ymin+19.0f, butr->xmax-butr->xmin, 19.0f, 
					   &node->id,  ""); 
		uiButSetFunc(bt, node_ID_title_cb, node, NULL);
		
		uiDefButBitS(block, TOG, SH_NODE_MAT_DIFF, B_NODE_EXEC, "Diff",
					 butr->xmin, butr->ymin, dx, 19.0f, 
					 &node->custom1, 0, 0, 0, 0, "Material Node outputs Diffuse");
		uiDefButBitS(block, TOG, SH_NODE_MAT_SPEC, B_NODE_EXEC, "Spec",
					 butr->xmin+dx, butr->ymin, dx, 19.0f, 
					 &node->custom1, 0, 0, 0, 0, "Material Node outputs Specular");
		uiDefButBitS(block, TOG, SH_NODE_MAT_NEG, B_NODE_EXEC, "Neg Normal",
					 butr->xmin+2.0f*dx, butr->ymin, dx, 19.0f,
					 &node->custom1, 0, 0, 0, 0, "Material Node uses inverted Normal");
		uiBlockEndAlign(block);
		
	}	
	return 38;
}

static int node_shader_buts_texture(uiBlock *block, bNode *node, rctf *butr)
{
	if(block) {
		uiBut *bt;
		
		bt= uiDefIDPoinBut(block, test_texpoin_but, ID_TE, B_NODE_EXEC, "",
						   butr->xmin, butr->ymin, butr->xmax-butr->xmin, 19.0f, 
						   &node->id,  ""); 
		uiButSetFunc(bt, node_ID_title_cb, node, NULL);
		
	}	
	return 19;
}

static int node_shader_buts_value(uiBlock *block, bNode *node, rctf *butr)
{
	if(block) {
		bNodeSocket *sock= node->outputs.first;		/* first socket stores value */
		
		uiDefButF(block, NUM, B_NODE_EXEC, "", 
					  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20.0f, 
					  sock->ns.vec, 0.0f, 1.0f, 10, 2, "");
		
	}
	return 20;
}

static int node_shader_buts_rgb(uiBlock *block, bNode *node, rctf *butr)
{
	if(block) {
		bNodeSocket *sock= node->outputs.first;		/* first socket stores value */

		/* enforce square box drawing */
		uiBlockSetEmboss(block, UI_EMBOSSP);
		
		uiDefButF(block, HSVCUBE, B_NODE_EXEC, "", 
					  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 12.0f, 
					  sock->ns.vec, 0.0f, 1.0f, 3, 0, "");
		uiDefButF(block, HSVCUBE, B_NODE_EXEC, "", 
					  butr->xmin, butr->ymin+15.0f, butr->xmax-butr->xmin, butr->ymax-butr->ymin -15.0f -15.0f, 
					  sock->ns.vec, 0.0f, 1.0f, 2, 0, "");
		uiDefButF(block, COL, B_NOP, "",		
					  butr->xmin, butr->ymax-12.0f, butr->xmax-butr->xmin, 12.0f, 
				      sock->ns.vec, 0.0, 0.0, -1, 0, "");
					  /* the -1 above prevents col button to popup a color picker */
		
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	return 30 + (int)(node->width-NODE_DY);
}

static void node_but_title_cb(void *node_v, void *but_v)
{
	bNode *node= node_v;
	uiBut *bt= but_v;
	BLI_strncpy(node->name, bt->drawstr, NODE_MAXSTR);
}

static int node_shader_buts_mix_rgb(uiBlock *block, bNode *node, rctf *butr)
{
	if(block) {
		uiBut *bt;
		bNodeSocket *sock= node->inputs.first;		/* first socket stores "fac" */
		
		uiBlockBeginAlign(block);
		/* blend type */
		bt=uiDefButS(block, MENU, B_NODE_EXEC, "Mix %x0|Add %x1|Subtract %x3|Multiply %x2|Screen %x4|Divide %x5|Difference %x6|Darken %x7|Lighten %x8",
						butr->xmin, butr->ymin+20.0f, butr->xmax-butr->xmin, 20.0f, 
						&node->custom1, 0, 0, 0, 0, "");
		uiButSetFunc(bt, node_but_title_cb, node, bt);
		/* value */
		uiDefButF(block, NUM, B_NODE_EXEC, "", 
					  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20.0f, 
					  sock->ns.vec, 0.0f, 1.0f, 10, 2, "");
		uiBlockEndAlign(block);
	}
	return 40;
}

static int node_shader_buts_valtorgb(uiBlock *block, bNode *node, rctf *butr)
{
	if(block && (node->flag & NODE_OPTIONS)) {
		if(node->storage) {
			draw_colorband_buts_small(block, node->storage, butr, B_NODE_EXEC);
		}
	}
	return 40;
}

/* only once called */
static void node_shader_set_butfunc(bNodeType *ntype)
{
	switch(ntype->type) {
		case SH_NODE_MATERIAL:
			ntype->butfunc= node_shader_buts_material;
			break;
		case SH_NODE_TEXTURE:
			ntype->butfunc= node_shader_buts_texture;
			break;
		case SH_NODE_VALUE:
			ntype->butfunc= node_shader_buts_value;
			break;
		case SH_NODE_RGB:
			ntype->butfunc= node_shader_buts_rgb;
			break;
		case SH_NODE_MIX_RGB:
			ntype->butfunc= node_shader_buts_mix_rgb;
			break;
		case SH_NODE_VALTORGB:
			ntype->butfunc= node_shader_buts_valtorgb;
			break;
		default:
			ntype->butfunc= NULL;
	}
}

/* ******* init draw callbacks for all tree types, only called in usiblender.c, once ************* */

void init_node_butfuncs(void)
{
	bNodeType **typedefs;
	
	/* shader nodes */
	typedefs= node_all_shaders;		/* BKE_node.h */
	while( *typedefs) {
		node_shader_set_butfunc(*typedefs);
		typedefs++;
	}
}

/* ************** Generic drawing ************** */

static void draw_nodespace_grid(SpaceNode *snode)
{
	float start, step= 25.0f;

	/* window is 'pixel size', like buttons */
	
	BIF_ThemeColorShade(TH_BACK, -10);
	
	start= snode->v2d.cur.xmin -fmod(snode->v2d.cur.xmin, step);
	
	glBegin(GL_LINES);
	for(; start<snode->v2d.cur.xmax; start+=step) {
		glVertex2f(start, snode->v2d.cur.ymin);
		glVertex2f(start, snode->v2d.cur.ymax);
	}

	start= snode->v2d.cur.ymin -fmod(snode->v2d.cur.ymin, step);
	for(; start<snode->v2d.cur.ymax; start+=step) {
		glVertex2f(snode->v2d.cur.xmin, start);
		glVertex2f(snode->v2d.cur.xmax, start);
	}
	glEnd();
}


static void nodeshadow(rctf *rct, float radius, int select)
{
	float rad;
	float a;
	char alpha= 2;
	
	glEnable(GL_BLEND);
	
	if(radius > (rct->ymax-rct->ymin-10.0f)/2.0f)
		rad= (rct->ymax-rct->ymin-10.0f)/2.0f;
	else
		rad= radius;
	
	if(select) a= 10.0f; else a= 7.0f;
	for(; a>0.0f; a-=1.0f) {
		/* alpha ranges from 2 to 20 or so */
		glColor4ub(0, 0, 0, alpha);
		alpha+= 2;
		
		gl_round_box(GL_POLYGON, rct->xmin - a, rct->ymin - a, rct->xmax + a, rct->ymax-10.0f + a, rad+a);
	}
	
	/* outline emphasis */
	glEnable( GL_LINE_SMOOTH );
	glColor4ub(0, 0, 0, 100);
	gl_round_box(GL_LINE_LOOP, rct->xmin-0.5f, rct->ymin-0.5f, rct->xmax+0.5f, rct->ymax+0.5f, radius);
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
		if(type==-1)
			glColor3ub(0, 0, 0);
		else if(type==SOCK_VALUE)
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

/* not a callback */
static void node_draw_preview(bNodePreview *preview, rctf *prv)
{
	float scale= (prv->xmax-prv->xmin)/((float)preview->xsize);
	
	glPixelZoom(scale, scale);
	glEnable(GL_BLEND);
	
	glaDrawPixelsTex(prv->xmin, prv->ymin, preview->xsize, preview->ysize, GL_FLOAT, preview->rect);
	
	glDisable(GL_BLEND);
	glPixelZoom(1.0f, 1.0f);
	
}

/* based on settings in node, sets drawing rect info */
static void node_update(bNode *node)
{
	bNodeSocket *nsock;
	
	if(node->flag & NODE_HIDDEN) {
		float rad, drad;
		
		node->totr.xmin= node->locx;
		node->totr.xmax= node->locx + 3*HIDDEN_RAD + node->miniwidth;
		node->totr.ymax= node->locy + (HIDDEN_RAD - 0.5f*NODE_DY);
		node->totr.ymin= node->totr.ymax - 2*HIDDEN_RAD;
		
		/* output connectors */
		rad=drad= M_PI/(1.0f + (float)BLI_countlist(&node->outputs));
		
		for(nsock= node->outputs.first; nsock; nsock= nsock->next, rad+= drad) {
			nsock->locx= node->totr.xmax - HIDDEN_RAD + sin(rad)*HIDDEN_RAD;
			nsock->locy= node->totr.ymin + HIDDEN_RAD + cos(rad)*HIDDEN_RAD;
		}
		
		/* input connectors */
		rad=drad= - M_PI/(1.0f + (float)BLI_countlist(&node->inputs));
		
		for(nsock= node->inputs.first; nsock; nsock= nsock->next, rad+= drad) {
			nsock->locx= node->totr.xmin + HIDDEN_RAD + sin(rad)*HIDDEN_RAD;
			nsock->locy= node->totr.ymin + HIDDEN_RAD + cos(rad)*HIDDEN_RAD;
		}
	}
	else {
		float dy= node->locy;
		
		/* header */
		dy-= NODE_DY;
		
		/* output connectors */
		for(nsock= node->outputs.first; nsock; nsock= nsock->next) {
			nsock->locx= node->locx + node->width;
			nsock->locy= dy - NODE_DYS;
			dy-= NODE_DY;
		}
		
		node->prvr.xmin= node->butr.xmin= node->locx + NODE_DYS;
		node->prvr.xmax= node->butr.xmax= node->locx + node->width- NODE_DYS;
		
		/* preview rect? */
		if(node->flag & NODE_PREVIEW) {
			dy-= NODE_DYS/2;
			node->prvr.ymax= dy;
			node->prvr.ymin= dy-(node->width-NODE_DY);
			dy= node->prvr.ymin - NODE_DYS/2;
		}
		
		/* buttons rect? */
		if((node->flag & NODE_OPTIONS) && node->typeinfo->butfunc) {
			dy-= NODE_DYS/2;
			node->butr.ymax= dy;
			node->butr.ymin= dy - (float)node->typeinfo->butfunc(NULL, node, NULL);
			dy= node->butr.ymin - NODE_DYS/2;
		}
		
		/* input connectors */
		for(nsock= node->inputs.first; nsock; nsock= nsock->next) {
			nsock->locx= node->locx;
			nsock->locy= dy - NODE_DYS;
			dy-= NODE_DY;
		}
		
		node->totr.xmin= node->locx;
		node->totr.xmax= node->locx + node->width;
		node->totr.ymax= node->locy;
		node->totr.ymin= dy;
	}
}

static void node_scaling_widget(int color_id, float aspect, float xmin, float ymin, float xmax, float ymax)
{
	float dx;
	float dy;
	
	dx= 0.5f*(xmax-xmin);
	dy= 0.5f*(ymax-ymin);
	
	BIF_ThemeColorShade(color_id, +30);	
	fdrawline(xmin, ymin, xmax, ymax);
	fdrawline(xmin+dx, ymin, xmax, ymax-dy);
	
	BIF_ThemeColorShade(color_id, -10);
	fdrawline(xmin, ymin+aspect, xmax, ymax+aspect);
	fdrawline(xmin+dx, ymin+aspect, xmax, ymax-dy+aspect);
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
	if(node->typeinfo->nclass==NODE_CLASS_GENERATOR)
		return TH_NODE_GENERATOR;
	if(node->typeinfo->nclass==NODE_CLASS_OPERATOR)
		return TH_NODE_OPERATOR;
	return TH_NODE;
}

static void node_basis_draw(SpaceNode *snode, bNode *node)
{
	bNodeSocket *sock;
	rctf *rct= &node->totr;
	float slen, iconofs;
	int ofs, color_id= node_get_colorid(node);
	
	uiSetRoundBox(15-4);
	nodeshadow(rct, BASIS_RAD, node->flag & SELECT);
	
	/* header */
	BIF_ThemeColorShade(color_id, 0);
	uiSetRoundBox(3);
	uiRoundBox(rct->xmin, rct->ymax-NODE_DY, rct->xmax, rct->ymax, BASIS_RAD);
	
	/* show/hide icons */
	iconofs= rct->xmax;
	
	if(node->typeinfo->flag & NODE_PREVIEW) {
		int icon_id;
		
		if(node->flag & (NODE_ACTIVE_ID|NODE_DO_OUTPUT))
			icon_id= ICON_MATERIAL;
		else
			icon_id= ICON_MATERIAL_DEHLT;
		iconofs-= 18.0f;
		glEnable(GL_BLEND);
		BIF_icon_set_aspect(icon_id, snode->aspect);
		BIF_icon_draw_blended(iconofs, rct->ymax-NODE_DY+2, icon_id, 0, -50);
		glDisable(GL_BLEND);
	}
	if(node->typeinfo->flag & NODE_OPTIONS) {
		iconofs-= 18.0f;
		glEnable(GL_BLEND);
		BIF_icon_set_aspect(ICON_BUTS, snode->aspect);
		BIF_icon_draw_blended(iconofs, rct->ymax-NODE_DY+2, ICON_BUTS, 0, -50);
		glDisable(GL_BLEND);
	}
	
	/* title */
	if(node->flag & SELECT) 
		BIF_ThemeColor(TH_TEXT_HI);
	else
		BIF_ThemeColorBlendShade(TH_TEXT, color_id, 0.4, 10);
	
	/* open/close entirely? */
	ui_draw_tria_icon(rct->xmin+8.0f, rct->ymax-NODE_DY+4.0f, snode->aspect, 'v');

	if(node->flag & SELECT) 
		BIF_ThemeColor(TH_TEXT_HI);
	else
		BIF_ThemeColor(TH_TEXT);
	
	ui_rasterpos_safe(rct->xmin+19.0f, rct->ymax-NODE_DY+5.0f, snode->aspect);
	snode_drawstring(snode, node->name, (int)(iconofs - rct->xmin-18.0f));
					 
	/* body */
	BIF_ThemeColorShade(color_id, 20);	
	uiSetRoundBox(8);
	uiRoundBox(rct->xmin, rct->ymin, rct->xmax, rct->ymax-NODE_DY, BASIS_RAD);
	
	/* scaling indicator */
	node_scaling_widget(color_id, snode->aspect, rct->xmax-BASIS_RAD*snode->aspect, rct->ymin, rct->xmax, rct->ymin+BASIS_RAD*snode->aspect);

	/* outline active emphasis */
	if(node->flag & NODE_ACTIVE) {
		glEnable(GL_BLEND);
		glColor4ub(200, 200, 200, 140);
		uiSetRoundBox(15-4);
		gl_round_box(GL_LINE_LOOP, rct->xmin, rct->ymin, rct->xmax, rct->ymax, BASIS_RAD);
		glDisable(GL_BLEND);
	}
	
	/* socket inputs, label buttons */
	for(sock= node->inputs.first; sock; sock= sock->next) {
		socket_circle_draw(sock->locx, sock->locy, NODE_SOCKSIZE, sock->type, sock->flag & SELECT);
		
		if(sock->type==SOCK_VALUE) {
			uiBut *bt= uiDefBut(snode->block, LABEL, B_NODE_EXEC, sock->name, sock->locx+5.0f, sock->locy-10.0f, node->width-NODE_DY, NODE_DY, sock, 0, 0, 0, 0, "");
			bt->block_func= socket_value_menu;
		}
		else if(sock->type==SOCK_VECTOR) {
			uiBut *bt= uiDefBut(snode->block, LABEL, B_NODE_EXEC, sock->name, sock->locx+5.0f, sock->locy-10.0f, node->width-NODE_DY, NODE_DY, sock, 0, 0, 0, 0, "");
			bt->block_func= socket_vector_menu;
		}
		else if(sock->type==SOCK_RGBA) {
			uiBut *bt= uiDefBut(snode->block, LABEL, B_NODE_EXEC, sock->name, sock->locx+5.0f, sock->locy-10.0f, node->width-NODE_DY, NODE_DY, sock, 0, 0, 0, 0, "");
			bt->block_func= socket_color_menu;
		}
		else {
			BIF_ThemeColor(TH_TEXT);
			ui_rasterpos_safe(sock->locx+8.0f, sock->locy-5.0f, snode->aspect);
			BIF_DrawString(snode->curfont, sock->name, 0);
		}
	}
	
	/* socket outputs */
	for(sock= node->outputs.first; sock; sock= sock->next) {
		socket_circle_draw(sock->locx, sock->locy, NODE_SOCKSIZE, sock->type, sock->flag & SELECT);
		
		BIF_ThemeColor(TH_TEXT);
		ofs= 0;
		slen= snode->aspect*BIF_GetStringWidth(snode->curfont, sock->name, 0);
		while(slen > node->width) {
			ofs++;
			slen= snode->aspect*BIF_GetStringWidth(snode->curfont, sock->name+ofs, 0);
		}
		ui_rasterpos_safe(sock->locx-8.0f-slen, sock->locy-5.0f, snode->aspect);
		BIF_DrawString(snode->curfont, sock->name+ofs, 0);
	}
	
	/* preview */
	if(node->flag & NODE_PREVIEW)
		if(node->preview)
			node_draw_preview(node->preview, &node->prvr);
		
	/* buttons */
	if(node->flag & NODE_OPTIONS)
		if(node->typeinfo->butfunc) 
			node->typeinfo->butfunc(snode->block, node, &node->butr);

}

void node_hidden_draw(SpaceNode *snode, bNode *node)
{
	bNodeSocket *sock;
	rctf *rct= &node->totr;
	float dx, centy= 0.5f*(rct->ymax+rct->ymin);
	int color_id= node_get_colorid(node);
	
	/* shadow */
	uiSetRoundBox(15);
	nodeshadow(rct, HIDDEN_RAD, node->flag & SELECT);

	/* body */
	BIF_ThemeColorShade(color_id, 20);	
	uiRoundBox(rct->xmin, rct->ymin, rct->xmax, rct->ymax, HIDDEN_RAD);
	
	/* outline active emphasis */
	if(node->flag & NODE_ACTIVE) {
		glEnable(GL_BLEND);
		glColor4ub(200, 200, 200, 140);
		gl_round_box(GL_LINE_LOOP, rct->xmin, rct->ymin, rct->xmax, rct->ymax, HIDDEN_RAD);
		glDisable(GL_BLEND);
	}
	
	/* title */
	if(node->flag & SELECT) 
		BIF_ThemeColor(TH_TEXT_HI);
	else
		BIF_ThemeColorBlendShade(TH_TEXT, color_id, 0.4, 10);
	
	/* open/close entirely? */
	ui_draw_tria_icon(rct->xmin+9.0f, centy-6.0f, snode->aspect, 'h');	
	
	if(node->flag & SELECT) 
		BIF_ThemeColor(TH_TEXT_HI);
	else
		BIF_ThemeColor(TH_TEXT);
	
	ui_rasterpos_safe(rct->xmin+21.0f, centy-4.0f, snode->aspect);
	snode_drawstring(snode, node->name, (int)(rct->xmax - rct->xmin-18.0f -12.0f));
	
	/* scale widget thing */
	BIF_ThemeColorShade(color_id, -10);	
	dx= 10.0f;
	fdrawline(rct->xmax-dx, centy-4.0f, rct->xmax-dx, centy+4.0f);
	fdrawline(rct->xmax-dx-3.0f*snode->aspect, centy-4.0f, rct->xmax-dx-3.0f*snode->aspect, centy+4.0f);
	
	BIF_ThemeColorShade(color_id, +30);
	dx-= snode->aspect;
	fdrawline(rct->xmax-dx, centy-4.0f, rct->xmax-dx, centy+4.0f);
	fdrawline(rct->xmax-dx-3.0f*snode->aspect, centy-4.0f, rct->xmax-dx-3.0f*snode->aspect, centy+4.0f);
	
	/* icon */
	//	if(node->id) {
	//		glEnable(GL_BLEND);
	//		BIF_icon_set_aspect(node->id->icon_id, snode->aspect);
	//		BIF_icon_draw(rct->xmin+HIDDEN_RAD, -1.0f+rct->ymin+HIDDEN_RAD/2, node->id->icon_id);
	//		glDisable(GL_BLEND);
	//	}
	
	
	/* sockets */
	for(sock= node->inputs.first; sock; sock= sock->next) {
		socket_circle_draw(sock->locx, sock->locy, NODE_SOCKSIZE, sock->type, sock->flag & SELECT);
	}
	
	for(sock= node->outputs.first; sock; sock= sock->next) {
		socket_circle_draw(sock->locx, sock->locy, NODE_SOCKSIZE, sock->type, sock->flag & SELECT);
	}
}

void node_draw_link(SpaceNode *snode, bNodeLink *link)
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
	
	dist= 0.5f*ABS(vec[0][0] - vec[3][0]);
	
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
	
	snode_set_context(snode);
	
	if(snode->nodetree) {
		bNode *node;
		bNodeLink *link;
		
		/* for now, we set drawing coordinates on each redraw */
		for(node= snode->nodetree->nodes.first; node; node= node->next)
			node_update(node);

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
				if(node->flag & NODE_HIDDEN)
					node_hidden_draw(snode, node);
				else
					node_basis_draw(snode, node);
			}
		}
		uiDrawBlock(snode->block);
		
		/* selected */
		snode->block= uiNewBlock(&sa->uiblocks, "node buttons2", UI_EMBOSS, UI_HELV, sa->win);
		uiBlockSetFlag(snode->block, UI_BLOCK_NO_HILITE);
		
		for(node= snode->nodetree->nodes.first; node; node= node->next) {
			if(node->flag & SELECT) {
				if(node->flag & NODE_HIDDEN)
					node_hidden_draw(snode, node);
				else
					node_basis_draw(snode, node);
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
	
	/* in the end, this is a delayed previewrender test, to allow buttons to be first */
	if(snode->flag & SNODE_DO_PREVIEW) {
		addafterqueue(sa->win, RENDERPREVIEW, 1);
		snode->flag &= ~SNODE_DO_PREVIEW;
	}
}
