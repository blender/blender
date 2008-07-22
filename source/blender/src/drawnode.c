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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): David Millan Escriva, Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_action_types.h"
#include "DNA_color_types.h"
#include "DNA_customdata_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_ipo_types.h"
#include "DNA_ID.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_text_types.h"
#include "DNA_userdef_types.h"

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

#include "CMP_node.h"
#include "SHD_node.h"

#include "BDR_gpencil.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_drawgpencil.h"
#include "BIF_interface.h"
#include "BIF_interface_icons.h"
#include "BIF_language.h"
#include "BIF_mywindow.h"
#include "BIF_previewrender.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"

#include "BSE_drawipo.h"
#include "BSE_node.h"
#include "BSE_view.h"

#include "BMF_Api.h"

#include "MEM_guardedalloc.h"

#include "RE_pipeline.h"
#include "IMB_imbuf_types.h"

#include "blendef.h"
#include "butspace.h"
#include "interface.h"	/* urm...  for rasterpos_safe, roundbox */
#include "mydevice.h"

extern void autocomplete_uv(char *str, void *arg_v);
extern int verify_valid_uv_name(char *str);

/* autocomplete callback for buttons */
static void autocomplete_vcol(char *str, void *arg_v)
{
	Mesh *me;
	CustomDataLayer *layer;
	AutoComplete *autocpl;
	int a;

	if(str[0]==0)
		return;

	autocpl= autocomplete_begin(str, 32);
		
	/* search if str matches the beginning of name */
	for(me= G.main->mesh.first; me; me=me->id.next)
		for(a=0, layer= me->fdata.layers; a<me->fdata.totlayer; a++, layer++)
			if(layer->type == CD_MCOL)
				autocomplete_do_name(autocpl, layer->name);
	
	autocomplete_end(autocpl, str);
}

static int verify_valid_vcol_name(char *str)
{
	Mesh *me;
	CustomDataLayer *layer;
	int a;
	
	if(str[0]==0)
		return 1;

	/* search if str matches the name */
	for(me= G.main->mesh.first; me; me=me->id.next)
		for(a=0, layer= me->fdata.layers; a<me->fdata.totlayer; a++, layer++)
			if(layer->type == CD_MCOL)
				if(strcmp(layer->name, str)==0)
					return 1;
	
	return 0;
}

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

static void socket_vector_menu_cb(void *node_v, void *ntree_v)
{
	if(node_v && ntree_v) {
		NodeTagChanged(ntree_v, node_v); 
		addqueue(curarea->win, UI_BUT_EVENT, B_NODE_EXEC+((bNode *)node_v)->nr);
	}
}

/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *socket_vector_menu(void *socket_v)
{
	SpaceNode *snode= curarea->spacedata.first;
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
	
	block= uiNewBlock(&curarea->uiblocks, "socket menu", UI_EMBOSS, UI_HELV, curarea->win);

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
	
	allqueue(REDRAWNODE, 0);
	
	return block;
}

static void node_sync_cb(void *snode_v, void *node_v)
{
	SpaceNode *snode= snode_v;
	
	if(snode->treetype==NTREE_SHADER) {
		nodeShaderSynchronizeID(node_v, 1);
		allqueue(REDRAWBUTSSHADING, 0);
	}
}

/* ****************** GENERAL CALLBACKS FOR NODES ***************** */

static void node_ID_title_cb(void *node_v, void *unused_v)
{
	bNode *node= node_v;
	
	if(node->id) {
		test_idbutton(node->id->name+2);	/* library.c, verifies unique name */
		BLI_strncpy(node->name, node->id->name+2, 21);
		
		allqueue(REDRAWBUTSSHADING, 0);
		allqueue(REDRAWNODE, 0);
		allqueue(REDRAWOOPS, 0);
	}
}


static void node_but_title_cb(void *node_v, void *but_v)
{
	bNode *node= node_v;
	uiBut *bt= but_v;
	BLI_strncpy(node->name, bt->drawstr, NODE_MAXSTR);
	
	allqueue(REDRAWNODE, 0);
}

static void node_group_alone_cb(void *node_v, void *unused_v)
{
	bNode *node= node_v;
	
	nodeCopyGroup(node);

	allqueue(REDRAWNODE, 0);
}

/* ****************** BUTTON CALLBACKS FOR ALL TREES ***************** */

static int node_buts_group(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block && node->id) {
		uiBut *bt;
		short width;
		
		uiBlockBeginAlign(block);
		
		/* name button */
		width= (short)(butr->xmax-butr->xmin - (node->id->us>1?19.0f:0.0f));
		bt= uiDefBut(block, TEX, B_NOP, "NT:",
					 butr->xmin, butr->ymin, width, 19, 
					 node->id->name+2, 0.0, 19.0, 0, 0, "NodeTree name");
		uiButSetFunc(bt, node_ID_title_cb, node, NULL);
		
		/* user amount */
		if(node->id->us>1) {
			char str1[32];
			sprintf(str1, "%d", node->id->us);
			bt= uiDefBut(block, BUT, B_NOP, str1, 
						 butr->xmax-19, butr->ymin, 19, 19, 
						 NULL, 0, 0, 0, 0, "Displays number of users.");
			uiButSetFunc(bt, node_group_alone_cb, node, NULL);
		}
		
		uiBlockEndAlign(block);
	}	
	return 19;
}

static int node_buts_value(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		bNodeSocket *sock= node->outputs.first;		/* first socket stores value */
		
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", 
				  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, 
				  sock->ns.vec, sock->ns.min, sock->ns.max, 10, 2, "");
		
	}
	return 20;
}

static int node_buts_rgb(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		bNodeSocket *sock= node->outputs.first;		/* first socket stores value */
		if(sock) {
			/* enforce square box drawing */
			uiBlockSetEmboss(block, UI_EMBOSSP);
			
			uiDefButF(block, HSVCUBE, B_NODE_EXEC+node->nr, "", 
					  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 12, 
					  sock->ns.vec, 0.0f, 1.0f, 3, 0, "");
			uiDefButF(block, HSVCUBE, B_NODE_EXEC+node->nr, "", 
					  butr->xmin, butr->ymin+15, butr->xmax-butr->xmin, butr->ymax-butr->ymin -15 -15, 
					  sock->ns.vec, 0.0f, 1.0f, 2, 0, "");
			uiDefButF(block, COL, B_NOP, "",		
					  butr->xmin, butr->ymax-12, butr->xmax-butr->xmin, 12, 
					  sock->ns.vec, 0.0, 0.0, -1, 0, "");
			/* the -1 above prevents col button to popup a color picker */
			
			uiBlockSetEmboss(block, UI_EMBOSS);
		}
	}
	return 30 + (int)(node->width-NODE_DY);
}

static int node_buts_mix_rgb(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiBut *bt;
		int a_but= (ntree->type==NTREE_COMPOSIT);
		
		/* blend type */
		uiBlockBeginAlign(block);
		bt=uiDefButS(block, MENU, B_NODE_EXEC+node->nr, "Mix %x0|Add %x1|Subtract %x3|Multiply %x2|Screen %x4|Overlay %x9|Divide %x5|Difference %x6|Darken %x7|Lighten %x8|Dodge %x10|Burn %x11|Color %x15|Value %x14|Saturation %x13|Hue %x12",
					 butr->xmin, butr->ymin, butr->xmax-butr->xmin -(a_but?20:0), 20, 
					 &node->custom1, 0, 0, 0, 0, "");
		uiButSetFunc(bt, node_but_title_cb, node, bt);
		/* Alpha option, composite */
		if(a_but)
			uiDefButS(block, TOG, B_NODE_EXEC+node->nr, "A",
				  butr->xmax-20, butr->ymin, 20, 20, 
				  &node->custom2, 0, 0, 0, 0, "Include Alpha of 2nd input in this operation");
	}
	return 20;
}

static int node_buts_time(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		CurveMapping *cumap= node->storage;
		short dx= (short)((butr->xmax-butr->xmin)/2);
		butr->ymin += 26;

		curvemap_buttons(block, node->storage, 's', B_NODE_EXEC+node->nr, B_REDR, butr);
		
		if(cumap) {
			cumap->flag |= CUMA_DRAW_CFRA;
			if(node->custom1<node->custom2)
				cumap->sample[0]= (float)(CFRA - node->custom1)/(float)(node->custom2-node->custom1);
		}

		uiBlockBeginAlign(block);
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "Sta:",
				  butr->xmin, butr->ymin-22, dx, 19, 
				  &node->custom1, 1.0, 20000.0, 0, 0, "Start frame");
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "End:",
				  butr->xmin+dx, butr->ymin-22, dx, 19, 
				  &node->custom2, 1.0, 20000.0, 0, 0, "End frame");
	}
	
	return node->width-NODE_DY;
}

static int node_buts_valtorgb(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		if(node->storage) {
			draw_colorband_buts_small(block, node->storage, butr, B_NODE_EXEC+node->nr);
		}
	}
	return 40;
}

static int node_buts_curvevec(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		curvemap_buttons(block, node->storage, 'v', B_NODE_EXEC+node->nr, B_REDR, butr);
	}	
	return (int)(node->width-NODE_DY);
}

static float *_sample_col= NULL;	// bad bad, 2.5 will do better?
void node_curvemap_sample(float *col)
{
	_sample_col= col;
}

static int node_buts_curvecol(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		CurveMapping *cumap= node->storage;
		if(_sample_col) {
			cumap->flag |= CUMA_DRAW_SAMPLE;
			VECCOPY(cumap->sample, _sample_col);
		}
		else 
			cumap->flag &= ~CUMA_DRAW_SAMPLE;

		curvemap_buttons(block, node->storage, 'c', B_NODE_EXEC+node->nr, B_REDR, butr);
	}	
	return (int)(node->width-NODE_DY);
}

static int node_buts_normal(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		bNodeSocket *sock= node->outputs.first;		/* first socket stores normal */
		
		uiDefButF(block, BUT_NORMAL, B_NODE_EXEC+node->nr, "", 
				  butr->xmin, butr->ymin, butr->xmax-butr->xmin, butr->ymax-butr->ymin, 
				  sock->ns.vec, 0.0f, 1.0f, 0, 0, "");
		
	}	
	return (int)(node->width-NODE_DY);
}

static void node_browse_tex_cb(void *ntree_v, void *node_v)
{
	bNodeTree *ntree= ntree_v;
	bNode *node= node_v;
	Tex *tex;
	
	if(node->menunr<1) return;
	
	if(node->id) {
		node->id->us--;
		node->id= NULL;
	}
	tex= BLI_findlink(&G.main->tex, node->menunr-1);

	node->id= &tex->id;
	id_us_plus(node->id);
	BLI_strncpy(node->name, node->id->name+2, 21);
	
	nodeSetActive(ntree, node);
	
	allqueue(REDRAWBUTSSHADING, 0);
	allqueue(REDRAWNODE, 0);
	NodeTagChanged(ntree, node); 
	
	node->menunr= 0;
}

static void node_dynamic_update_cb(void *ntree_v, void *node_v)
{
	Material *ma;
	bNode *node= (bNode *)node_v;
	ID *id= node->id;
	int error= 0;

	if (BTST(node->custom1, NODE_DYNAMIC_ERROR)) error= 1;

	/* Users only have to press the "update" button in one pynode
	 * and we also update all others sharing the same script */
	for (ma= G.main->mat.first; ma; ma= ma->id.next) {
		if (ma->nodetree) {
			bNode *nd;
			for (nd= ma->nodetree->nodes.first; nd; nd= nd->next) {
				if ((nd->type == NODE_DYNAMIC) && (nd->id == id)) {
					nd->custom1= 0;
					nd->custom1= BSET(nd->custom1, NODE_DYNAMIC_REPARSE);
					nd->menunr= 0;
					if (error)
						nd->custom1= BSET(nd->custom1, NODE_DYNAMIC_ERROR);
				}
			}
		}
	}

	allqueue(REDRAWBUTSSHADING, 0);
	allqueue(REDRAWNODE, 0);
	BIF_preview_changed(ID_MA);
}

static int node_buts_texture(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiBut *bt;
		char *strp;
		
		/* browse button texture */
		uiBlockBeginAlign(block);
		IDnames_to_pupstring(&strp, NULL, "", &(G.main->tex), NULL, NULL);
		node->menunr= 0;
		bt= uiDefButS(block, MENU, B_NODE_EXEC+node->nr, strp, 
					  butr->xmin, butr->ymin, 20, 19, 
					  &node->menunr, 0, 0, 0, 0, "Browse texture");
		uiButSetFunc(bt, node_browse_tex_cb, ntree, node);
		if(strp) MEM_freeN(strp);
		
		if(node->id) {
			bt= uiDefBut(block, TEX, B_NOP, "TE:",
						 butr->xmin+19, butr->ymin, butr->xmax-butr->xmin-19, 19, 
						 node->id->name+2, 0.0, 19.0, 0, 0, "Texture name");
			uiButSetFunc(bt, node_ID_title_cb, node, NULL);
		}
		
	}	
	return 19;
}

static int node_buts_math(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr) 
{ 
	if(block) { 
		uiBut *bt; 

		bt=uiDefButS(block, MENU, B_NODE_EXEC,  "Add %x0|Subtract %x1|Multiply %x2|Divide %x3|Sine %x4|Cosine %x5|Tangent %x6|Arcsine %x7|Arccosine %x8|Arctangent %x9|Power %x10|Logarithm %x11|Minimum %x12|Maximum %x13|Round %x14|Less Than %x15|Greater Than %x16", butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, &node->custom1, 0, 0, 0, 0, ""); 
		uiButSetFunc(bt, node_but_title_cb, node, bt); 
	} 
	return 20; 
}


/* ****************** BUTTON CALLBACKS FOR SHADER NODES ***************** */

static void node_browse_text_cb(void *ntree_v, void *node_v)
{
	bNodeTree *ntree= ntree_v;
	bNode *node= node_v;
	ID *oldid;
	
	if(node->menunr<1) return;
	
	if(node->id) {
		node->id->us--;
	}
	oldid= node->id;
	node->id= BLI_findlink(&G.main->text, node->menunr-1);
	id_us_plus(node->id);
	BLI_strncpy(node->name, node->id->name+2, 21); /* huh? why 21? */

	node->custom1= BSET(node->custom1, NODE_DYNAMIC_NEW);
	
	nodeSetActive(ntree, node);

	allqueue(REDRAWBUTSSHADING, 0);
	allqueue(REDRAWNODE, 0);

	node->menunr= 0;
}

static void node_mat_alone_cb(void *node_v, void *unused)
{
	bNode *node= node_v;
	
	node->id= (ID *)copy_material((Material *)node->id);
	
	BIF_undo_push("Single user material");
	allqueue(REDRAWBUTSSHADING, 0);
	allqueue(REDRAWNODE, 0);
	allqueue(REDRAWOOPS, 0);
}

static void node_browse_mat_cb(void *ntree_v, void *node_v)
{
	bNodeTree *ntree= ntree_v;
	bNode *node= node_v;
	
	if(node->menunr<1) return;
	
	if(node->menunr==32767) {	/* code for Add New */
		if(node->id) {
			/* make copy, but make sure it doesnt have the node tag nor nodes */
			Material *ma= (Material *)node->id;
			ma->id.us--;
			ma= copy_material(ma);
			ma->use_nodes= 0;
			if(ma->nodetree) {
				ntreeFreeTree(ma->nodetree);
				MEM_freeN(ma->nodetree);
			}
			ma->nodetree= NULL;
			node->id= (ID *)ma;
		}
		else node->id= (ID *)add_material("MatNode");
	}
	else {
		if(node->id) node->id->us--;
		node->id= BLI_findlink(&G.main->mat, node->menunr-1);
		id_us_plus(node->id);
	}
	BLI_strncpy(node->name, node->id->name+2, 21);
	
	nodeSetActive(ntree, node);

	allqueue(REDRAWBUTSSHADING, 0);
	allqueue(REDRAWNODE, 0);
	BIF_preview_changed(ID_MA);

	node->menunr= 0;
}

static void node_new_mat_cb(void *ntree_v, void *node_v)
{
	bNodeTree *ntree= ntree_v;
	bNode *node= node_v;
	
	node->id= (ID *)add_material("MatNode");
	BLI_strncpy(node->name, node->id->name+2, 21);

	nodeSetActive(ntree, node);

	allqueue(REDRAWBUTSSHADING, 0);
	allqueue(REDRAWNODE, 0);
	BIF_preview_changed(ID_MA);

}

static void node_texmap_cb(void *texmap_v, void *unused_v)
{
	init_mapping(texmap_v);
}

static int node_shader_buts_material(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiBut *bt;
		short dx= (short)((butr->xmax-butr->xmin)/3.0f), has_us= (node->id && node->id->us>1);
		short dy= (short)butr->ymin;
		char *strp;
		
		/* WATCH IT: we use this callback in material buttons, but then only want first row */
		if(butr->ymax-butr->ymin > 21.0f) dy+= 19;
		
		uiBlockBeginAlign(block);
		if(node->id==NULL) uiBlockSetCol(block, TH_REDALERT);
		else if(has_us) uiBlockSetCol(block, TH_BUT_SETTING1);
		else uiBlockSetCol(block, TH_BUT_SETTING2);
		
		/* browse button */
		IDnames_to_pupstring(&strp, NULL, "ADD NEW %x32767", &(G.main->mat), NULL, NULL);
		node->menunr= 0;
		bt= uiDefButS(block, MENU, B_NOP, strp, 
				  butr->xmin, dy, 19, 19, 
				  &node->menunr, 0, 0, 0, 0, "Browses existing choices or adds NEW");
		uiButSetFunc(bt, node_browse_mat_cb, ntree, node);
		if(strp) MEM_freeN(strp);
		
		/* Add New button */
		if(node->id==NULL) {
			bt= uiDefBut(block, BUT, B_NOP, "Add New",
						 butr->xmin+19, dy, (short)(butr->xmax-butr->xmin-19.0f), 19, 
						 NULL, 0.0, 0.0, 0, 0, "Add new Material");
			uiButSetFunc(bt, node_new_mat_cb, ntree, node);
			uiBlockSetCol(block, TH_AUTO);
		}
		else {
			/* name button */
			short width= (short)(butr->xmax-butr->xmin-19.0f - (has_us?19.0f:0.0f));
			bt= uiDefBut(block, TEX, B_NOP, "MA:",
						  butr->xmin+19, dy, width, 19, 
						  node->id->name+2, 0.0, 19.0, 0, 0, "Material name");
			uiButSetFunc(bt, node_ID_title_cb, node, NULL);
			
			/* user amount */
			if(has_us) {
				char str1[32];
				sprintf(str1, "%d", node->id->us);
				bt= uiDefBut(block, BUT, B_NOP, str1, 
							  butr->xmax-19, dy, 19, 19, 
							  NULL, 0, 0, 0, 0, "Displays number of users. Click to make a single-user copy.");
				uiButSetFunc(bt, node_mat_alone_cb, node, NULL);
			}
			
			/* WATCH IT: we use this callback in material buttons, but then only want first row */
			if(butr->ymax-butr->ymin > 21.0f) {
				/* node options */
				uiBlockSetCol(block, TH_AUTO);
				uiDefButBitS(block, TOG, SH_NODE_MAT_DIFF, B_NODE_EXEC+node->nr, "Diff",
							 butr->xmin, butr->ymin, dx, 19, 
							 &node->custom1, 0, 0, 0, 0, "Material Node outputs Diffuse");
				uiDefButBitS(block, TOG, SH_NODE_MAT_SPEC, B_NODE_EXEC+node->nr, "Spec",
							 butr->xmin+dx, butr->ymin, dx, 19, 
							 &node->custom1, 0, 0, 0, 0, "Material Node outputs Specular");
				uiDefButBitS(block, TOG, SH_NODE_MAT_NEG, B_NODE_EXEC+node->nr, "Neg Normal",
							 butr->xmax-dx, butr->ymin, dx, 19,
							 &node->custom1, 0, 0, 0, 0, "Material Node uses inverted Normal");
			}
		}
		uiBlockEndAlign(block);
	}	
	return 38;
}

static int node_shader_buts_mapping(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		TexMapping *texmap= node->storage;
		short dx= (short)((butr->xmax-butr->xmin)/7.0f);
		short dy= (short)(butr->ymax-19);
		
		uiBlockSetFunc(block, node_texmap_cb, texmap, NULL);	/* all buttons get this */
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+dx, dy, 2*dx, 19, texmap->loc, -1000.0f, 1000.0f, 10, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->loc+1, -1000.0f, 1000.0f, 10, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->loc+2, -1000.0f, 1000.0f, 10, 2, "");
		dy-= 19;
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+dx, dy, 2*dx, 19, texmap->rot, -1000.0f, 1000.0f, 1000, 1, "");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->rot+1, -1000.0f, 1000.0f, 1000, 1, "");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->rot+2, -1000.0f, 1000.0f, 1000, 1, "");
		dy-= 19;
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+dx, dy, 2*dx, 19, texmap->size, -1000.0f, 1000.0f, 10, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->size+1, -1000.0f, 1000.0f, 10, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->size+2, -1000.0f, 1000.0f, 10, 2, "");
		dy-= 25;
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+dx, dy, 2*dx, 19, texmap->min, -10.0f, 10.0f, 100, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->min+1, -10.0f, 10.0f, 100, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->min+2, -10.0f, 10.0f, 100, 2, "");
		dy-= 19;
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+dx, dy, 2*dx, 19, texmap->max, -10.0f, 10.0f, 10, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->max+1, -10.0f, 10.0f, 10, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->max+2, -10.0f, 10.0f, 10, 2, "");
		uiBlockEndAlign(block);
		
		/* labels/options */
		
		dy= (short)(butr->ymax-19);
		uiDefBut(block, LABEL, B_NOP, "Loc", butr->xmin, dy, dx, 19, NULL, 0.0f, 0.0f, 0, 0, "");
		dy-= 19;
		uiDefBut(block, LABEL, B_NOP, "Rot", butr->xmin, dy, dx, 19, NULL, 0.0f, 0.0f, 0, 0, "");
		dy-= 19;
		uiDefBut(block, LABEL, B_NOP, "Size", butr->xmin, dy, dx, 19, NULL, 0.0f, 0.0f, 0, 0, "");
		dy-= 25;
		uiDefButBitI(block, TOG, TEXMAP_CLIP_MIN, B_NODE_EXEC+node->nr, "Min", butr->xmin, dy, dx-4, 19, &texmap->flag, 0.0f, 0.0f, 0, 0, "");
		dy-= 19;
		uiDefButBitI(block, TOG, TEXMAP_CLIP_MAX, B_NODE_EXEC+node->nr, "Max", butr->xmin, dy, dx-4, 19, &texmap->flag, 0.0f, 0.0f, 0, 0, "");
		
	}	
	return 5*19 + 6;
}

static int node_shader_buts_vect_math(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr) 
{ 
	if(block) { 
		uiBut *bt; 
	
		bt=uiDefButS(block, MENU, B_NODE_EXEC,  "Add %x0|Subtract %x1|Average %x2|Dot Product %x3 |Cross Product %x4|Normalize %x5", butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, &node->custom1, 0, 0, 0, 0, ""); 
		uiButSetFunc(bt, node_but_title_cb, node, bt); 
	} 
	return 20; 
}

static int node_shader_buts_geometry(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiBut *but;
		NodeGeometry *ngeo= (NodeGeometry*)node->storage;

		if(!verify_valid_uv_name(ngeo->uvname))
			uiBlockSetCol(block, TH_REDALERT);
		but= uiDefBut(block, TEX, B_NODE_EXEC+node->nr, "UV:", butr->xmin, butr->ymin+20, butr->xmax-butr->xmin, 20, ngeo->uvname, 0, 31, 0, 0, "Set name of UV layer to use, default is active UV layer");
		uiButSetCompleteFunc(but, autocomplete_uv, NULL);
		uiBlockSetCol(block, TH_AUTO);

		if(!verify_valid_vcol_name(ngeo->colname))
			uiBlockSetCol(block, TH_REDALERT);
		but= uiDefBut(block, TEX, B_NODE_EXEC+node->nr, "Col:", butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, ngeo->colname, 0, 31, 0, 0, "Set name of vertex color layer to use, default is active vertex color layer");
		uiButSetCompleteFunc(but, autocomplete_vcol, NULL);
		uiBlockSetCol(block, TH_AUTO);
	}

	return 40;
}

static int node_shader_buts_dynamic(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr) 
{ 
	if (block) { 
		uiBut *bt;
		SpaceNode *snode= curarea->spacedata.first;
		short dy= (short)butr->ymin;
		int xoff=0;

		/* B_NODE_EXEC is handled in butspace.c do_node_buts */
		if(!node->id) {
				char *strp;
				IDnames_to_pupstring(&strp, NULL, "", &(G.main->text), NULL, NULL);
				node->menunr= 0;
				bt= uiDefButS(block, MENU, B_NODE_EXEC/*+node->nr*/, strp, 
								butr->xmin, dy, 19, 19, 
								&node->menunr, 0, 0, 0, 0, "Browses existing choices");
				uiButSetFunc(bt, node_browse_text_cb, ntree, node);
				xoff=19;
				if(strp) MEM_freeN(strp);	
		}
		else {
			bt = uiDefBut(block, BUT, B_NOP, "Update",
					butr->xmin+xoff, butr->ymin+20, 50, 19,
					&node->menunr, 0.0, 19.0, 0, 0, "Refresh this node (and all others that use the same script)");
			uiButSetFunc(bt, node_dynamic_update_cb, ntree, node);

			if (BTST(node->custom1, NODE_DYNAMIC_ERROR)) {
				BIF_ThemeColor(TH_REDALERT);
				ui_rasterpos_safe(butr->xmin + xoff, butr->ymin + 5, snode->aspect);
				snode_drawstring(snode, "Error! Check console...", butr->xmax - butr->xmin);
			}
		}
	}
	return 20+19; 
}

/* only once called */
static void node_shader_set_butfunc(bNodeType *ntype)
{
	switch(ntype->type) {
		/* case NODE_GROUP:	 note, typeinfo for group is generated... see "XXX ugly hack" */

		case SH_NODE_MATERIAL:
		case SH_NODE_MATERIAL_EXT:
			ntype->butfunc= node_shader_buts_material;
			break;
		case SH_NODE_TEXTURE:
			ntype->butfunc= node_buts_texture;
			break;
		case SH_NODE_NORMAL:
			ntype->butfunc= node_buts_normal;
			break;
		case SH_NODE_CURVE_VEC:
			ntype->butfunc= node_buts_curvevec;
			break;
		case SH_NODE_CURVE_RGB:
			ntype->butfunc= node_buts_curvecol;
			break;
		case SH_NODE_MAPPING:
			ntype->butfunc= node_shader_buts_mapping;
			break;
		case SH_NODE_VALUE:
			ntype->butfunc= node_buts_value;
			break;
		case SH_NODE_RGB:
			ntype->butfunc= node_buts_rgb;
			break;
		case SH_NODE_MIX_RGB:
			ntype->butfunc= node_buts_mix_rgb;
			break;
		case SH_NODE_VALTORGB:
			ntype->butfunc= node_buts_valtorgb;
			break;
		case SH_NODE_MATH: 
			ntype->butfunc= node_buts_math;
			break; 
		case SH_NODE_VECT_MATH: 
			ntype->butfunc= node_shader_buts_vect_math;
			break; 
		case SH_NODE_GEOMETRY:
			ntype->butfunc= node_shader_buts_geometry;
			break;
		case NODE_DYNAMIC:
			ntype->butfunc= node_shader_buts_dynamic;
			break;
		default:
			ntype->butfunc= NULL;
	}
}

/* ****************** BUTTON CALLBACKS FOR COMPOSITE NODES ***************** */



static void node_browse_image_cb(void *ntree_v, void *node_v)
{
	bNodeTree *ntree= ntree_v;
	bNode *node= node_v;
	
	nodeSetActive(ntree, node);
	
	if(node->menunr<1) return;
	if(node->menunr==32767) {	/* code for Load New */
		addqueue(curarea->win, UI_BUT_EVENT, B_NODE_LOADIMAGE);
	}
	else {
		if(node->id) node->id->us--;
		node->id= BLI_findlink(&G.main->image, node->menunr-1);
		id_us_plus(node->id);

		BLI_strncpy(node->name, node->id->name+2, 21);

		NodeTagChanged(ntree, node); 
		BKE_image_signal((Image *)node->id, node->storage, IMA_SIGNAL_USER_NEW_IMAGE);
		addqueue(curarea->win, UI_BUT_EVENT, B_NODE_EXEC+node->nr);
	}
	node->menunr= 0;
}

static void node_active_cb(void *ntree_v, void *node_v)
{
	nodeSetActive(ntree_v, node_v);
}
static void node_image_type_cb(void *node_v, void *unused)
{
	
	allqueue(REDRAWNODE, 1);
}

static char *node_image_type_pup(void)
{
	char *str= MEM_mallocN(256, "image type pup");
	int a;
	
	str[0]= 0;
	
	a= sprintf(str, "Image Type %%t|");
	a+= sprintf(str+a, "  Image %%x%d %%i%d|", IMA_SRC_FILE, ICON_IMAGE_DEHLT);
	a+= sprintf(str+a, "  Movie %%x%d %%i%d|", IMA_SRC_MOVIE, ICON_SEQUENCE);
	a+= sprintf(str+a, "  Sequence %%x%d %%i%d|", IMA_SRC_SEQUENCE, ICON_IMAGE_COL);
	a+= sprintf(str+a, "  Generated %%x%d %%i%d", IMA_SRC_GENERATED, ICON_BLANK1);
	
	return str;
}

/* copy from buttons_shading.c */
static char *layer_menu(RenderResult *rr)
{
	RenderLayer *rl;
	int len= 40 + 40*BLI_countlist(&rr->layers);
	short a, nr;
	char *str= MEM_callocN(len, "menu layers");
	
	strcpy(str, "Layer %t");
	a= strlen(str);
	for(nr=0, rl= rr->layers.first; rl; rl= rl->next, nr++) {
		a+= sprintf(str+a, "|%s %%x%d", rl->name, nr);
	}
	
	return str;
}

static void image_layer_cb(void *ima_v, void *iuser_v)
{
	
	ntreeCompositForceHidden(G.scene->nodetree);
	BKE_image_multilayer_index(ima_v, iuser_v);
	allqueue(REDRAWNODE, 0);
}

static int node_composit_buts_image(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	ImageUser *iuser= node->storage;
	
	if(block) {
		uiBut *bt;
		short dy= (short)butr->ymax-19;
		char *strp;
		
		uiBlockBeginAlign(block);
		uiBlockSetCol(block, TH_BUT_SETTING2);
		
		/* browse button */
		IMAnames_to_pupstring(&strp, NULL, "LOAD NEW %x32767", &(G.main->image), NULL, NULL);
		node->menunr= 0;
		bt= uiDefButS(block, MENU, B_NOP, strp, 
					  butr->xmin, dy, 19, 19, 
					  &node->menunr, 0, 0, 0, 0, "Browses existing choices");
		uiButSetFunc(bt, node_browse_image_cb, ntree, node);
		if(strp) MEM_freeN(strp);
		
		/* Add New button */
		if(node->id==NULL) {
			bt= uiDefBut(block, BUT, B_NODE_LOADIMAGE, "Load New",
						 butr->xmin+19, dy, (short)(butr->xmax-butr->xmin-19.0f), 19, 
						 NULL, 0.0, 0.0, 0, 0, "Add new Image");
			uiButSetFunc(bt, node_active_cb, ntree, node);
			uiBlockSetCol(block, TH_AUTO);
		}
		else {
			/* name button + type */
			Image *ima= (Image *)node->id;
			short xmin= (short)butr->xmin, xmax= (short)butr->xmax;
			short width= xmax - xmin - 45;
			short icon= ICON_IMAGE_DEHLT;
			
			if(ima->source==IMA_SRC_MOVIE) icon= ICON_SEQUENCE;
			else if(ima->source==IMA_SRC_SEQUENCE) icon= ICON_IMAGE_COL;
			else if(ima->source==IMA_SRC_GENERATED) icon= ICON_BLANK1;
			
			bt= uiDefBut(block, TEX, B_NOP, "IM:",
						 xmin+19, dy, width, 19, 
						 node->id->name+2, 0.0, 19.0, 0, 0, "Image name");
			uiButSetFunc(bt, node_ID_title_cb, node, NULL);
			
			/* buffer type option */
			strp= node_image_type_pup();
			bt= uiDefIconTextButS(block, MENU, B_NOP, icon, strp,
						 xmax-26, dy, 26, 19, 
						 &ima->source, 0.0, 19.0, 0, 0, "Image type");
			uiButSetFunc(bt, node_image_type_cb, node, ima);
			MEM_freeN(strp);
			
			if( ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE) ) {
				width= (xmax-xmin)/2;
				
				dy-= 19;
				uiDefButI(block, NUM, B_NODE_EXEC+node->nr, "Frs:",
						  xmin, dy, width, 19, 
						  &iuser->frames, 1.0, MAXFRAMEF, 0, 0, "Amount of images used in animation");
				uiDefButI(block, NUM, B_NODE_EXEC+node->nr, "SFra:",
						  xmin+width, dy, width, 19, 
						  &iuser->sfra, 1.0, MAXFRAMEF, 0, 0, "Start frame of animation");
				dy-= 19;
				uiDefButI(block, NUM, B_NODE_EXEC+node->nr, "Offs:",
						  xmin, dy, width, 19, 
						  &iuser->offset, -MAXFRAMEF, MAXFRAMEF, 0, 0, "Offsets the number of the frame to use in the animation");
				uiDefButS(block, TOG, B_NODE_EXEC+node->nr, "Cycl",
						  xmin+width, dy, width-20, 19, 
						  &iuser->cycl, 0.0, 0.0, 0, 0, "Make animation go cyclic");
				uiDefIconButBitS(block, TOG, IMA_ANIM_ALWAYS, B_NODE_EXEC+node->nr, ICON_AUTO,
						  xmax-20, dy, 20, 19, 
						  &iuser->flag, 0.0, 0.0, 0, 0, "Always refresh Image on frame changes");
			}
			if( ima->type==IMA_TYPE_MULTILAYER && ima->rr) {
				RenderLayer *rl= BLI_findlink(&ima->rr->layers, iuser->layer);
				if(rl) {
					width= (xmax-xmin);
					dy-= 19;
					strp= layer_menu(ima->rr);
					bt= uiDefButS(block, MENU, B_NODE_EXEC+node->nr, strp,
							  xmin, dy, width, 19, 
							  &iuser->layer, 0.0, 10000.0, 0, 0, "Layer");
					uiButSetFunc(bt, image_layer_cb, ima->rr, node->storage);
					MEM_freeN(strp);
				}
			}
		}
		
	}	
	if(node->id) {
		Image *ima= (Image *)node->id;
		int retval= 19;
		
		/* for each draw we test for anim refresh event */
		if(iuser->flag & IMA_ANIM_REFRESHED) {
			iuser->flag &= ~IMA_ANIM_REFRESHED;
			addqueue(curarea->win, UI_BUT_EVENT, B_NODE_EXEC+node->nr);
		}
		
		if( ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE) )
			retval+= 38;
		if( ima->type==IMA_TYPE_MULTILAYER)
			retval+= 19;
		return retval;
	}
	else
		return 19;
}

/* if we use render layers from other scene, we make a nice title */
static void set_render_layers_title(void *node_v, void *unused)
{
	bNode *node= node_v;
	Scene *sce;
	SceneRenderLayer *srl;
	char str[64];
	
	if(node->id) {
		BLI_strncpy(str, node->id->name+2, 21);
		strcat(str, "|");
		sce= (Scene *)node->id;
	}
	else {
		str[0]= 0;
		sce= G.scene;
	}
	srl= BLI_findlink(&sce->r.layers, node->custom1);
	if(srl==NULL) {
		node->custom1= 0;
		srl= sce->r.layers.first;
	}
	
	strcat(str, srl->name);
	BLI_strncpy(node->name, str, 32);
}

static char *scene_layer_menu(Scene *sce)
{
	SceneRenderLayer *srl;
	int len= 40 + 40*BLI_countlist(&sce->r.layers);
	short a, nr;
	char *str= MEM_callocN(len, "menu layers");
	
	strcpy(str, "Active Layer %t");
	a= strlen(str);
	for(nr=0, srl= sce->r.layers.first; srl; srl= srl->next, nr++) {
		a+= sprintf(str+a, "|%s %%x%d", srl->name, nr);
	}
	
	return str;
}

static void node_browse_scene_cb(void *ntree_v, void *node_v)
{
	bNodeTree *ntree= ntree_v;
	bNode *node= node_v;
	Scene *sce;
	
	if(node->menunr<1) return;
	
	if(node->id) {
		node->id->us--;
		node->id= NULL;
	}
	sce= BLI_findlink(&G.main->scene, node->menunr-1);
	if(sce!=G.scene) {
		node->id= &sce->id;
		id_us_plus(node->id);
	}
	
	set_render_layers_title(node, NULL);
	nodeSetActive(ntree, node);

	allqueue(REDRAWBUTSSHADING, 0);
	allqueue(REDRAWNODE, 0);
	NodeTagChanged(ntree, node); 

	node->menunr= 0;
}


static int node_composit_buts_renderlayers(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiBut *bt;
		char *strp;
		
		/* browse button scene */
		uiBlockBeginAlign(block);
		IDnames_to_pupstring(&strp, NULL, "", &(G.main->scene), NULL, NULL);
		node->menunr= 0;
		bt= uiDefButS(block, MENU, B_NOP, strp, 
					  butr->xmin, butr->ymin, 20, 19, 
					  &node->menunr, 0, 0, 0, 0, "Browse Scene to use RenderLayer from");
		uiButSetFunc(bt, node_browse_scene_cb, ntree, node);
		if(strp) MEM_freeN(strp);
		
		/* browse button layer */
		strp= scene_layer_menu(node->id?(Scene *)node->id:G.scene);
		if(node->id)
			bt= uiDefIconTextButS(block, MENU, B_NODE_EXEC+node->nr, ICON_SCENE_DEHLT, strp, 
				  butr->xmin+20, butr->ymin, (butr->xmax-butr->xmin)-40, 19, 
				  &node->custom1, 0, 0, 0, 0, "Choose Render Layer");
		else
			bt= uiDefButS(block, MENU, B_NODE_EXEC+node->nr, strp, 
				  butr->xmin+20, butr->ymin, (butr->xmax-butr->xmin)-40, 19, 
				  &node->custom1, 0, 0, 0, 0, "Choose Render Layer");
		uiButSetFunc(bt, set_render_layers_title, node, NULL);
		MEM_freeN(strp);
		
		/* re-render */
		/* uses custom2, not the best implementation of the world... but we need it to work now :) */
		bt= uiDefIconButS(block, TOG, B_NODE_EXEC+node->nr, ICON_SCENE, 
				  butr->xmax-20, butr->ymin, 20, 19, 
				  &node->custom2, 0, 0, 0, 0, "Re-render this Layer");
		
	}
	return 19;
}

static void node_blur_relative_cb(void *node, void *poin2)
{
	bNode *nodev= node;
	NodeBlurData *nbd= nodev->storage;
	if(nbd->image_in_width != 0){
		if(nbd->relative){ /* convert absolute values to relative */
			nbd->percentx= (float)(nbd->sizex)/nbd->image_in_width;
			nbd->percenty= (float)(nbd->sizey)/nbd->image_in_height;
		}else{ /* convert relative values to absolute */
			nbd->sizex= (int)(nbd->percentx*nbd->image_in_width);
			nbd->sizey= (int)(nbd->percenty*nbd->image_in_height);
		}
	}
	allqueue(REDRAWNODE, 0);
}
static void node_blur_update_sizex_cb(void *node, void *poin2)
{
	bNode *nodev= node;
	NodeBlurData *nbd= nodev->storage;

	nbd->sizex= (int)(nbd->percentx*nbd->image_in_width);
}
static void node_blur_update_sizey_cb(void *node, void *poin2)
{
	bNode *nodev= node;
	NodeBlurData *nbd= nodev->storage;

	nbd->sizey= (int)(nbd->percenty*nbd->image_in_height);
}
static int node_composit_buts_blur(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeBlurData *nbd= node->storage;
		uiBut *bt;
		short dy= butr->ymin+58;
		short dx= (butr->xmax-butr->xmin)/2;
		char str[256];
		
		uiBlockBeginAlign(block);
		sprintf(str, "Filter Type%%t|Flat %%x%d|Tent %%x%d|Quad %%x%d|Cubic %%x%d|Gauss %%x%d|Fast Gauss%%x%d|CatRom %%x%d|Mitch %%x%d", R_FILTER_BOX, R_FILTER_TENT, R_FILTER_QUAD, R_FILTER_CUBIC, R_FILTER_GAUSS, R_FILTER_FAST_GAUSS, R_FILTER_CATROM, R_FILTER_MITCH);
		uiDefButS(block, MENU, B_NODE_EXEC+node->nr,str,
				  butr->xmin, dy, dx*2, 19, 
				  &nbd->filtertype, 0, 0, 0, 0, "Set sampling filter for blur");
		dy-=19;
		if (nbd->filtertype != R_FILTER_FAST_GAUSS) { 
			uiDefButC(block, TOG, B_NODE_EXEC+node->nr, "Bokeh",
					butr->xmin, dy, dx, 19, 
					&nbd->bokeh, 0, 0, 0, 0, "Uses circular filter, warning it's slow!");
			uiDefButC(block, TOG, B_NODE_EXEC+node->nr, "Gamma",
					butr->xmin+dx, dy, dx, 19, 
					&nbd->gamma, 0, 0, 0, 0, "Applies filter on gamma corrected values");
		} else {
			uiBlockEndAlign(block);
			uiBlockBeginAlign(block);
		}
		dy-=19;
		bt= uiDefButS(block, TOG, B_NOP, "Relative",
				  butr->xmin, dy, dx*2, 19,
				  &nbd->relative, 0, 0, 0, 0, "Use relative (percent) values to define blur radius");
		uiButSetFunc(bt, node_blur_relative_cb, node, NULL);

		dy-=19;
		if(nbd->relative) {
			bt= uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "X:",
						 butr->xmin, dy, dx, 19, 
						 &nbd->percentx, 0.0f, 1.0f, 0, 0, "");
			uiButSetFunc(bt, node_blur_update_sizex_cb, node, NULL);
			bt= uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Y:",
						 butr->xmin+dx, dy, dx, 19, 
						 &nbd->percenty, 0.0f, 1.0f, 0, 0, "");
			uiButSetFunc(bt, node_blur_update_sizey_cb, node, NULL);
		}
		else {
			uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "X:",
						 butr->xmin, dy, dx, 19, 
						 &nbd->sizex, 0, 256, 0, 0, "");
			uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "Y:",
						 butr->xmin+dx, dy, dx, 19, 
						 &nbd->sizey, 0, 256, 0, 0, "");
		}
		uiBlockEndAlign(block);
	}
	return 77;
}

static int node_composit_buts_dblur(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeDBlurData *ndbd = node->storage;
		short dy = butr->ymin + 171;
		short dx = butr->xmax - butr->xmin;
		short halfdx= (short)dx/2;

		uiBlockBeginAlign(block);
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "Iterations:",
				butr->xmin, dy, dx, 19,
				&ndbd->iter, 1, 32, 10, 0, "Amount of iterations");
		uiDefButC(block, TOG, B_NODE_EXEC+node->nr, "Wrap",
				butr->xmin, dy-= 19, dx, 19, 
				&ndbd->wrap, 0, 0, 0, 0, "Wrap blur");
		uiBlockEndAlign(block);

		dy-= 9;

		uiDefBut(block, LABEL, B_NOP, "Center", butr->xmin, dy-= 19, dx, 19, NULL, 0.0f, 0.0f, 0, 0, "");

		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "X:",
				butr->xmin, dy-= 19, halfdx, 19,
				&ndbd->center_x, 0.0f, 1.0f, 10, 0, "X center in percents");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Y:",
				butr->xmin+halfdx, dy, halfdx, 19,
				&ndbd->center_y, 0.0f, 1.0f, 10, 0, "Y center in percents");
		uiBlockEndAlign(block);

		dy-= 9;

		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Distance:",
				butr->xmin, dy-= 19, dx, 19,
				&ndbd->distance, -1.0f, 1.0f, 10, 0, "Amount of which the image moves");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Angle:",
				butr->xmin, dy-= 19, dx, 19,
				&ndbd->angle, 0.0f, 360.0f, 1000, 0, "Angle in which the image will be moved");
		uiBlockEndAlign(block);

		dy-= 9;

		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Spin:",
				butr->xmin, dy-= 19, dx, 19,
				&ndbd->spin, -360.0f, 360.0f, 1000, 0, "Angle that is used to spin the image");

		dy-= 9;

		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Zoom:",
				butr->xmin, dy-= 19, dx, 19,
				&ndbd->zoom, 0.0f, 100.0f, 100, 0, "Amount of which the image is zoomed");

	}
	return 190;
}

static int node_composit_buts_bilateralblur(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeBilateralBlurData *nbbd= node->storage;
		short dy= butr->ymin+38;
		short dx= (butr->xmax-butr->xmin);
		
		uiBlockBeginAlign(block);
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "Iterations:",
				 butr->xmin, dy, dx, 19, 
				 &nbbd->iter, 1, 128, 0, 0, "Amount of iterations");
		dy-=19;
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Color Sigma:",
				  butr->xmin, dy, dx, 19, 
				  &nbbd->sigma_color,0.01, 3, 10, 0, "Sigma value used to modify color");
		dy-=19;
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Space Sigma:",
				  butr->xmin, dy, dx, 19, 
				  &nbbd->sigma_space ,0.01, 30, 10, 0, "Sigma value used to modify space");
		
	}
	return 57;
}

/* qdn: defocus node */
static int node_composit_buts_defocus(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeDefocus *nqd = node->storage;
		short dy = butr->ymin + 209;
		short dx = butr->xmax - butr->xmin; 
		char* mstr1 = "Bokeh Type%t|Octagon %x8|Heptagon %x7|Hexagon %x6|Pentagon %x5|Square %x4|Triangle %x3|Disk %x0";

		uiDefBut(block, LABEL, B_NOP, "Bokeh Type", butr->xmin, dy, dx, 19, NULL, 0, 0, 0, 0, "");
		uiDefButC(block, MENU, B_NODE_EXEC+node->nr, mstr1,
		          butr->xmin, dy-19, dx, 19,
		          &nqd->bktype, 0, 0, 0, 0, "Bokeh type");
		if (nqd->bktype) { /* for some reason rotating a disk doesn't seem to work... ;) */
			uiDefButC(block, NUM, B_NODE_EXEC+node->nr, "Rotate:",
			          butr->xmin, dy-38, dx, 19,
			          &nqd->rotation, 0, 90, 0, 0, "Bokeh shape rotation offset in degrees");
		}
		uiDefButC(block, TOG, B_NODE_EXEC+node->nr, "Gamma Correct",
		          butr->xmin, dy-57, dx, 19,
		          &nqd->gamco, 0, 0, 0, 0, "Enable gamma correction before and after main process");
		if (nqd->no_zbuf==0) {
			// only needed for zbuffer input
			uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "fStop:",
			          butr->xmin, dy-76, dx, 19,
			          &nqd->fstop, 0.5, 128, 10, 0, "Amount of focal blur, 128=infinity=perfect focus, half the value doubles the blur radius");
		}
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Maxblur:",
		          butr->xmin, dy-95, dx, 19,
		          &nqd->maxblur, 0, 10000, 1000, 0, "blur limit, maximum CoC radius, 0=no limit");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "BThreshold:",
		          butr->xmin, dy-114, dx, 19,
		          &nqd->bthresh, 0, 100, 100, 0, "CoC radius threshold, prevents background bleed on in-focus midground, 0=off");
		uiDefButC(block, TOG, B_NODE_EXEC+node->nr, "Preview",
		          butr->xmin, dy-142, dx, 19,
		          &nqd->preview, 0, 0, 0, 0, "Enable sampling mode, useful for preview when using low samplecounts");
		if (nqd->preview) {
			/* only visible when sampling mode enabled */
			uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "Samples:",
			          butr->xmin, dy-161, dx, 19,
			          &nqd->samples, 16, 256, 0, 0, "Number of samples (16=grainy, higher=less noise)");
		}
		uiDefButS(block, TOG, B_NODE_EXEC+node->nr, "No zbuffer",
		          butr->xmin, dy-190, dx, 19,
		          &nqd->no_zbuf, 0, 0, 0, 0, "Enable when using an image as input instead of actual zbuffer (auto enabled if node not image based, eg. time node)");
		if (nqd->no_zbuf) {
			uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Zscale:",
		            butr->xmin, dy-209, dx, 19,
		            &nqd->scale, 0, 1000, 100, 0, "Scales the Z input when not using a zbuffer, controls maximum blur designated by the color white or input value 1");
		}
	}
	return 228;
}


/* qdn: glare node */
static int node_composit_buts_glare(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeGlare *ndg = node->storage;
		short dy = butr->ymin + 152, dx = butr->xmax - butr->xmin; 
		char* mn1 = "Type%t|Ghosts%x3|Streaks%x2|Fog Glow%x1|Simple Star%x0";
		char* mn2 = "Quality/Speed%t|High/Slow%x0|Medium/Medium%x1|Low/Fast%x2";
		uiDefButC(block, MENU, B_NODE_EXEC+node->nr, mn1,
		          butr->xmin, dy, dx, 19,
		          &ndg->type, 0, 0, 0, 0, "Glow/Flare/Bloom type");
		uiDefButC(block, MENU, B_NODE_EXEC+node->nr, mn2,
		          butr->xmin, dy-19, dx, 19,
		          &ndg->quality, 0, 0, 0, 0,
		          "Quality speed trade off, if not set to high quality, effect will be applied to low-res copy of source image");
		if (ndg->type != 1) {
			uiDefButC(block, NUM, B_NODE_EXEC+node->nr, "Iterations:",
			          butr->xmin, dy-38, dx, 19,
			          &ndg->iter, 2, 5, 1, 0,
			          "higher values will generate longer/more streaks/ghosts");
			if (ndg->type != 0)
				uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "ColMod:",
				          butr->xmin, dy-57, dx, 19,
				          &ndg->colmod, 0, 1, 10, 0,
				          "Amount of Color Modulation, modulates colors of streaks and ghosts for a spectral dispersion effect");
		}
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Mix:",
		          butr->xmin, dy-76, dx, 19,
		          &ndg->mix, -1, 1, 10, 0,
		          "Mix balance, -1 is original image only, 0 is exact 50/50 mix, 1 is processed image only");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Threshold:",
		          butr->xmin, dy-95, dx, 19,
		          &ndg->threshold, 0, 1000, 10, 0,
		          "Brightness threshold, the glarefilter will be applied only to pixels brighter than this value");
		if ((ndg->type == 2) || (ndg->type == 0))
		{
			if (ndg->type == 2) {
				uiDefButC(block, NUM, B_NODE_EXEC+node->nr, "streaks:",
				          butr->xmin, dy-114, dx, 19,
				          &ndg->angle, 2, 16, 1000, 0,
				          "Total number of streaks");
				uiDefButC(block, NUM, B_NODE_EXEC+node->nr, "AngOfs:",
				          butr->xmin, dy-133, dx, 19,
				          &ndg->angle_ofs, 0, 180, 1000, 0,
				          "Streak angle rotation offset in degrees");
			}
			uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Fade:",
			          butr->xmin, dy-152, dx, 19,
			          &ndg->fade, 0.75, 1, 5, 0,
			          "Streak fade out factor");
		}
		if (ndg->type == 0)
			uiDefButC(block, TOG, B_NODE_EXEC+node->nr, "Rot45",
			          butr->xmin, dy-114, dx, 19,
			          &ndg->angle, 0, 0, 0, 0,
			          "simple star filter, add 45 degree rotation offset");
		if ((ndg->type == 1) || (ndg->type > 3))	// PBGH and fog glow
			uiDefButC(block, NUM, B_NODE_EXEC+node->nr, "Size:",
			          butr->xmin, dy-114, dx, 19,
			          &ndg->size, 6, 9, 1000, 0,
			          "glow/glare size (not actual size, relative to initial size of bright area of pixels)");
	}
	return 171;
}

/* qdn: tonemap node */
static int node_composit_buts_tonemap(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeTonemap *ntm = node->storage;
		short dy = butr->ymin + 76, dx = butr->xmax - butr->xmin; 
		char* mn = "Type%t|R/D Photoreceptor%x1|Rh Simple%x0";
		
		uiBlockBeginAlign(block);
		uiDefButI(block, MENU, B_NODE_EXEC+node->nr, mn,
		          butr->xmin, dy, dx, 19,
		          &ntm->type, 0, 0, 0, 0,
		          "Tone mapping type");
		if (ntm->type == 0) {
			uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Key:",
			          butr->xmin, dy-19, dx, 19,
			          &ntm->key, 0, 1, 5, 0,
			          "The value the average luminance is mapped to");
			uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Offset:",
			          butr->xmin, dy-38, dx, 19,
			          &ntm->offset, 0.001, 10, 5, 0,
			          "Tonemap offset, normally always 1, but can be used as an extra control to alter the brightness curve");
			uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Gamma:",
			          butr->xmin, dy-57, dx, 19,
			          &ntm->gamma, 0.001, 3, 5, 0,
			          "Gamma factor, if not used, set to 1");
		}
		else {
			uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Intensity:",
			          butr->xmin, dy-19, dx, 19,
			          &ntm->f, -8, 8, 10, 0, "if less than zero, darkens image, otherwise makes it brighter");
			uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Contrast:",
			          butr->xmin, dy-38, dx, 19,
			          &ntm->m, 0, 1, 5, 0, "Set to 0 to use estimate from input image");
			uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Adaptation:",
			          butr->xmin, dy-57, dx, 19,
			          &ntm->a, 0, 1, 5, 0, "if 0, global, if 1, based on pixel intensity");
			uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "ColCorrect:",
			          butr->xmin, dy-76, dx, 19,
			          &ntm->c, 0, 1, 5, 0, "color correction, if 0, same for all channels, if 1, each independent");
		}
		uiBlockEndAlign(block);
	}
	return 95;
}

/* qdn: lens distortion node */
static int node_composit_buts_lensdist(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeLensDist *nld = node->storage;
		short dy = butr->ymin + 19, dx = butr->xmax - butr->xmin; 
		uiBlockBeginAlign(block);
		uiDefButS(block, TOG, B_NODE_EXEC+node->nr, "Projector",
		          butr->xmin, dy, dx, 19,
		          &nld->proj, 0, 0, 0, 0,
		          "Enable/disable projector mode, effect is applied in horizontal direction only");
		if (!nld->proj) {
			uiDefButS(block, TOG, B_NODE_EXEC+node->nr, "Jitter",
			          butr->xmin, dy-19, dx/2, 19,
			          &nld->jit, 0, 0, 0, 0,
			          "Enable/disable jittering, faster, but also noisier");
			uiDefButS(block, TOG, B_NODE_EXEC+node->nr, "Fit",
			          butr->xmin+dx/2, dy-19, dx/2, 19,
			          &nld->fit, 0, 0, 0, 0,
			          "For positive distortion factor only, scale image such that black areas are not visible");
		}
		uiBlockEndAlign(block);
	}
	return 38;
}


static int node_composit_buts_vecblur(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeBlurData *nbd= node->storage;
		short dy= butr->ymin;
		short dx= (butr->xmax-butr->xmin);
		
		uiBlockBeginAlign(block);
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "Samples:",
				 butr->xmin, dy+76, dx, 19, 
				 &nbd->samples, 1, 256, 0, 0, "Amount of samples");
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "MinSpeed:",
				  butr->xmin, dy+57, dx, 19, 
				  &nbd->minspeed, 0, 1024, 0, 0, "Minimum speed for a pixel to be blurred, used to separate background from foreground");
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "MaxSpeed:",
				  butr->xmin, dy+38, dx, 19, 
				  &nbd->maxspeed, 0, 1024, 0, 0, "If not zero, maximum speed in pixels");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "BlurFac:",
				  butr->xmin, dy+19, dx, 19, 
				  &nbd->fac, 0.0f, 2.0f, 10, 2, "Scaling factor for motion vectors, actually 'shutter speed' in frames");
		uiDefButS(block, TOG, B_NODE_EXEC+node->nr, "Curved",
				  butr->xmin, dy, dx, 19, 
				  &nbd->curved, 0.0f, 2.0f, 10, 2, "Interpolate between frames in a bezier curve, rather than linearly");
		uiBlockEndAlign(block);
	}
	return 95;
}

static int node_composit_buts_filter(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiBut *bt;
		
		/* blend type */
		bt=uiDefButS(block, MENU, B_NODE_EXEC+node->nr, "Soften %x0|Sharpen %x1|Laplace %x2|Sobel %x3|Prewitt %x4|Kirsch %x5|Shadow %x6",
					 butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, 
					 &node->custom1, 0, 0, 0, 0, "");
		uiButSetFunc(bt, node_but_title_cb, node, bt);
	}
	return 20;
}

static int node_composit_buts_flip(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr) 
{
	if(block) {
		uiBut *bt;
		
		/* flip x\y */
		bt=uiDefButS(block, MENU, B_NODE_EXEC+node->nr, "Flip X %x0|Flip Y %x1|Flip X & Y %x2",
					 butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, 
					 &node->custom1, 0, 0, 0, 0, "");
		uiButSetFunc(bt, node_but_title_cb, node, bt);
	}
	return 20;	
}

static int node_composit_buts_crop(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeTwoXYs *ntxy= node->storage;
		char elementheight = 19;
		short dx= (butr->xmax-butr->xmin)/2;
		short dy= butr->ymax - elementheight;
		short xymin= 0, xymax= 10000;

		uiBlockBeginAlign(block);

		/* crop image size toggle */
		uiDefButS(block, TOG, B_NODE_EXEC+node->nr, "Crop Image Size",
				  butr->xmin, dy, dx*2, elementheight, 
				  &node->custom1, 0, 0, 0, 0, "Crop the size of the input image.");

		dy-=elementheight;

		/* x1 */
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "X1:",
					 butr->xmin, dy, dx, elementheight,
					 &ntxy->x1, xymin, xymax, 0, 0, "");
		/* y1 */
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "Y1:",
					 butr->xmin+dx, dy, dx, elementheight,
					 &ntxy->y1, xymin, xymax, 0, 0, "");

		dy-=elementheight;

		/* x2 */
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "X2:",
					 butr->xmin, dy, dx, elementheight,
					 &ntxy->x2, xymin, xymax, 0, 0, "");
		/* y2 */
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "Y2:",
					 butr->xmin+dx, dy, dx, elementheight,
					 &ntxy->y2, xymin, xymax, 0, 0, "");

		uiBlockEndAlign(block);
	}
	return 60;
}

static int node_composit_buts_splitviewer(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {	
		uiBlockBeginAlign(block);
		
		uiDefButS(block, ROW, B_NODE_EXEC+node->nr, "X",
				  butr->xmin, butr->ymin+19, (butr->xmax-butr->xmin)/2, 20, 
				  &node->custom2, 0.0, 0.0, 0, 0, "");
		uiDefButS(block, ROW, B_NODE_EXEC+node->nr, "Y",
				  butr->xmin+(butr->xmax-butr->xmin)/2, butr->ymin+19, (butr->xmax-butr->xmin)/2, 20, 
				  &node->custom2, 0.0, 1.0, 0, 0, "");
				  
		uiDefButS(block, NUMSLI, B_NODE_EXEC+node->nr, "Split %: ",
				butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, &node->custom1, 0, 100, 10, 0, "");
	}
	return 40;
}

static int node_composit_buts_map_value(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		TexMapping *texmap= node->storage;
		short xstart= (short)butr->xmin;
		short dy= (short)(butr->ymax-19.0f);
		short dx= (short)(butr->xmax-butr->xmin)/2;
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Offs:", xstart, dy, 2*dx, 19, texmap->loc, -1000.0f, 1000.0f, 10, 2, "");
		dy-= 19;
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Size:", xstart, dy, 2*dx, 19, texmap->size, -1000.0f, 1000.0f, 10, 3, "");
		dy-= 23;
		uiBlockBeginAlign(block);
		uiDefButBitI(block, TOG, TEXMAP_CLIP_MIN, B_NODE_EXEC+node->nr, "Min", xstart, dy, dx, 19, &texmap->flag, 0.0f, 0.0f, 0, 0, "");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", xstart+dx, dy, dx, 19, texmap->min, -1000.0f, 1000.0f, 10, 2, "");
		dy-= 19;
		uiDefButBitI(block, TOG, TEXMAP_CLIP_MAX, B_NODE_EXEC+node->nr, "Max", xstart, dy, dx, 19, &texmap->flag, 0.0f, 0.0f, 0, 0, "");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", xstart+dx, dy, dx, 19, texmap->max, -1000.0f, 1000.0f, 10, 2, "");
	}
	return 80;
}

static int node_composit_buts_alphaover(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeTwoFloats *ntf= node->storage;
		
		/* alpha type */
		uiDefButS(block, TOG, B_NODE_EXEC+node->nr, "ConvertPremul",
				  butr->xmin, butr->ymin+19, butr->xmax-butr->xmin, 19, 
				  &node->custom1, 0, 0, 0, 0, "");
		/* mix factor */
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Premul: ",
				  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 19, 
				  &ntf->x, 0.0f, 1.0f, 100, 0, "");
	}
	return 38;
}

static int node_composit_buts_hue_sat(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeHueSat *nhs= node->storage;
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Hue: ",
				  butr->xmin, butr->ymin+40.0f, butr->xmax-butr->xmin, 20, 
				  &nhs->hue, 0.0f, 1.0f, 100, 0, "");
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Sat: ",
				  butr->xmin, butr->ymin+20.0f, butr->xmax-butr->xmin, 20, 
				  &nhs->sat, 0.0f, 2.0f, 100, 0, "");
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Val: ",
				  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, 
				  &nhs->val, 0.0f, 2.0f, 100, 0, "");
	}
	return 60;
}

static int node_composit_buts_dilateerode(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "Distance:",
				  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, 
				  &node->custom2, -100, 100, 0, 0, "Distance to grow/shrink (number of iterations)");
	}
	return 20;
}

static int node_composit_buts_diff_matte(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		short sx= (butr->xmax-butr->xmin)/4;
		short dx= (butr->xmax-butr->xmin)/3;
		NodeChroma *c= node->storage;
		
		uiBlockBeginAlign(block);
		/*color space selectors*/
		uiDefButS(block, ROW,B_NODE_EXEC+node->nr,"RGB",
							butr->xmin,butr->ymin+60,sx,20,
							&node->custom1,1,1, 0, 0, "RGB Color Space");
		uiDefButS(block, ROW,B_NODE_EXEC+node->nr,"HSV",
							butr->xmin+sx,butr->ymin+60,sx,20,
							&node->custom1,1,2, 0, 0, "HSV Color Space");
		uiDefButS(block, ROW,B_NODE_EXEC+node->nr,"YUV",
							butr->xmin+2*sx,butr->ymin+60,sx,20,
							&node->custom1,1,3, 0, 0, "YUV Color Space");
					uiDefButS(block, ROW,B_NODE_EXEC+node->nr,"YCC",
							butr->xmin+3*sx,butr->ymin+60,sx,20,
							&node->custom1,1,4, 0, 0, "YCbCr Color Space");
		/*channel tolorences*/
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, " ",
							butr->xmin, butr->ymin+40, dx, 20,
							&c->t1, 0.0f, 1.0f, 100, 0, "Channel 1 Tolerance");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, " ",
							butr->xmin+dx, butr->ymin+40, dx, 20,
							&c->t2, 0.0f, 1.0f, 100, 0, "Channel 2 Tolorence");
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, " ",
							butr->xmin+2*dx, butr->ymin+40, dx, 20,
							&c->t3, 0.0f, 1.0f, 100, 0, "Channel 3 Tolorence");
		/*falloff parameters*/
		/*
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Falloff Size ",
			butr->xmin, butr->ymin+20, butr->xmax-butr->xmin, 20,
			&c->fsize, 0.0f, 1.0f, 100, 0, "");
		*/
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Falloff: ",
			butr->xmin, butr->ymin+20, butr->xmax-butr->xmin, 20,
			&c->fstrength, 0.0f, 1.0f, 100, 0, "");
	}
	return 80;
}

static int node_composit_buts_color_spill(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		short dx= (butr->xmax-butr->xmin)/3;

		NodeChroma *c=node->storage;
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Enhance: ", 
				butr->xmin, butr->ymin+20.0, butr->xmax-butr->xmin, 20,
				&c->t1, 0.0f, 0.5f, 100, 2, "Adjusts how much selected channel is affected by color spill algorithm");
		uiDefButS(block, ROW, B_NODE_EXEC+node->nr, "R",
				butr->xmin,butr->ymin,dx,20,
				&node->custom1,1,1, 0, 0, "Red Spill Suppression");
		uiDefButS(block, ROW, B_NODE_EXEC+node->nr, "G",
				butr->xmin+dx,butr->ymin,dx,20,
				&node->custom1,1,2, 0, 0, "Green Spill Suppression");
		uiDefButS(block, ROW, B_NODE_EXEC+node->nr, "B",
				butr->xmin+2*dx,butr->ymin,dx,20,
				&node->custom1, 1, 3, 0, 0, "Blue Spill Suppression");
		uiBlockEndAlign(block);
	}
	return 60;
}

static int node_composit_buts_chroma_matte(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		short dx=(butr->xmax-butr->xmin)/2;
		NodeChroma *c= node->storage;
		uiBlockBeginAlign(block);

		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Acceptance ",
			butr->xmin, butr->ymin+60, butr->xmax-butr->xmin, 20,
			&c->t1, 1.0f, 80.0f, 100, 0, "Tolerance for colors to be considered a keying color");
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Cutoff ",
			butr->xmin, butr->ymin+40, butr->xmax-butr->xmin, 20,
			&c->t2, 0.0f, 30.0f, 100, 0, "Colors below this will be considered as exact matches for keying color");

		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Lift ",
			butr->xmin, butr->ymin+20, dx, 20,
			&c->fsize, 0.0f, 1.0f, 100, 0, "Alpha Lift");
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Gain ",
			butr->xmin+dx, butr->ymin+20, dx, 20,
			&c->fstrength, 0.0f, 1.0f, 100, 0, "Alpha Gain");

		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Shadow Adjust ",
			butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20,
			&c->t3, 0.0f, 1.0f, 100, 0, "Adjusts the brightness of any shadows captured");

		if(c->t2 > c->t1)
			c->t2=c->t1;
	}
	return 80;
}

static int node_composit_buts_channel_matte(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		short sx= (butr->xmax-butr->xmin)/4;
		short cx= (butr->xmax-butr->xmin)/3;
		NodeChroma *c=node->storage;
		char *c1, *c2, *c3;
	
		/*color space selectors*/
		uiBlockBeginAlign(block);
		uiDefButS(block, ROW,B_NODE_EXEC+node->nr,"RGB",
			butr->xmin,butr->ymin+60,sx,20,&node->custom1,1,1, 0, 0, "RGB Color Space");
		uiDefButS(block, ROW,B_NODE_EXEC+node->nr,"HSV",
			butr->xmin+sx,butr->ymin+60,sx,20,&node->custom1,1,2, 0, 0, "HSV Color Space");
		uiDefButS(block, ROW,B_NODE_EXEC+node->nr,"YUV",
			butr->xmin+2*sx,butr->ymin+60,sx,20,&node->custom1,1,3, 0, 0, "YUV Color Space");
		uiDefButS(block, ROW,B_NODE_EXEC+node->nr,"YCC",
			butr->xmin+3*sx,butr->ymin+60,sx,20,&node->custom1,1,4, 0, 0, "YCbCr Color Space");
	
		if (node->custom1==1) {
			c1="R"; c2="G"; c3="B";
		}
		else if(node->custom1==2){
			c1="H"; c2="S"; c3="V";
		}
		else if(node->custom1==3){
			c1="Y"; c2="U"; c3="V";
		}
		else { // if(node->custom1==4){
			c1="Y"; c2="Cb"; c3="Cr";
		}
	
		/*channel selector */
		uiDefButS(block, ROW, B_NODE_EXEC+node->nr, c1,
			butr->xmin,butr->ymin+40,cx,20,&node->custom2,1, 1, 0, 0, "Channel 1");
		uiDefButS(block, ROW, B_NODE_EXEC+node->nr, c2,
			butr->xmin+cx,butr->ymin+40,cx,20,&node->custom2,1, 2, 0, 0, "Channel 2");
		uiDefButS(block, ROW, B_NODE_EXEC+node->nr, c3,
			butr->xmin+cx+cx,butr->ymin+40,cx,20,&node->custom2, 1, 3, 0, 0, "Channel 3");
	
		/*tolerance sliders */
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "High ", 
			butr->xmin, butr->ymin+20.0, butr->xmax-butr->xmin, 20,
			&c->t1, 0.0f, 1.0f, 100, 0, "Values higher than this setting are 100% opaque");
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Low ", 
			butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20,
			&c->t2, 0.0f, 1.0f, 100, 0, "Values lower than this setting are 100% keyed");
		uiBlockEndAlign(block);
	
		/*keep t2 (low) less than t1 (high) */
		if(c->t2 > c->t1) {
			c->t2=c->t1;
		}
	}
	return 80;
}

static int node_composit_buts_luma_matte(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeChroma *c=node->storage;
	
		/*tolerance sliders */
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "High ", 
			butr->xmin, butr->ymin+20.0, butr->xmax-butr->xmin, 20,
			&c->t1, 0.0f, 1.0f, 100, 0, "Values higher than this setting are 100% opaque");
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Low ", 
			butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20,
			&c->t2, 0.0f, 1.0f, 100, 0, "Values lower than this setting are 100% keyed");
		uiBlockEndAlign(block);
	
		/*keep t2 (low) less than t1 (high) */
		if(c->t2 > c->t1) {
			c->t2=c->t1;
		}
	}
	return 40;
}

static int node_composit_buts_map_uv(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "Alpha:",
				  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, 
				  &node->custom1, 0, 100, 0, 0, "Conversion percentage of UV differences to Alpha");
	}
	return 20;
}

static int node_composit_buts_id_mask(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "ID:",
				  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, 
				  &node->custom1, 0, 10000, 0, 0, "Pass Index number to convert to Alpha");
	}
	return 20;
}

/* allocate sufficient! */
static void node_imagetype_string(char *str)
{
	str += sprintf(str, "Save Image as: %%t|");
	str += sprintf(str, "Targa %%x%d|", R_TARGA);
	str += sprintf(str, "Targa Raw %%x%d|", R_RAWTGA);
	str += sprintf(str, "PNG %%x%d|", R_PNG);
	str += sprintf(str, "BMP %%x%d|", R_BMP);
	str += sprintf(str, "Jpeg %%x%d|", R_JPEG90);
	str += sprintf(str, "Iris %%x%d|", R_IRIS);
	str += sprintf(str, "Radiance HDR %%x%d|", R_RADHDR);
	str += sprintf(str, "Cineon %%x%d|", R_CINEON);
	str += sprintf(str, "DPX %%x%d|", R_DPX);
	str += sprintf(str, "OpenEXR %%x%d", R_OPENEXR);
}

static void node_set_image_cb(void *ntree_v, void *node_v)
{
	bNodeTree *ntree= ntree_v;
	bNode *node= node_v;
	
	nodeSetActive(ntree, node);
}

static int node_composit_buts_file_output(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeImageFile *nif= node->storage;
		uiBut *bt;
		short x= (short)butr->xmin;
		short y= (short)butr->ymin;
		short w= (short)butr->xmax-butr->xmin;
		char str[320];
		
		node_imagetype_string(str);
		
		uiBlockBeginAlign(block);
		
		bt = uiDefIconBut(block, BUT, B_NODE_SETIMAGE, ICON_FILESEL,
				  x, y+60, 20, 20,
				  0, 0, 0, 0, 0, "Open Fileselect to get Backbuf image");
		uiButSetFunc(bt, node_set_image_cb, ntree, node);
		
		uiDefBut(block, TEX, B_NOP, "",
				  20+x, y+60, w-20, 20, 
				  nif->name, 0.0f, 240.0f, 0, 0, "");
		
		uiDefButS(block, MENU, B_NOP, str,
				  x, y+40, w, 20, 
				  &nif->imtype, 0.0f, 1.0f, 0, 0, "");
		
		if(nif->imtype==R_OPENEXR) {
			uiDefButBitS(block, TOG, R_OPENEXR_HALF, B_REDR, "Half",	
						x, y+20, w/2, 20, 
						&nif->subimtype, 0, 0, 0, 0, "");

			uiDefButS(block, MENU,B_NOP, "Codec %t|None %x0|Pxr24 (lossy) %x1|ZIP (lossless) %x2|PIZ (lossless) %x3|RLE (lossless) %x4",  
						x+w/2, y+20, w/2, 20, 
						&nif->codec, 0, 0, 0, 0, "");
		}
		else {
			uiDefButS(block, NUM, B_NOP, "Quality: ",
				  x, y+20, w, 20, 
				  &nif->quality, 10.0f, 100.0f, 10, 0, "");
		}
		
		/* start frame, end frame */
		uiDefButI(block, NUM, B_NODE_EXEC+node->nr, "SFra: ", 
				  x, y, w/2, 20, 
				  &nif->sfra, 1, MAXFRAMEF, 10, 0, "");
		uiDefButI(block, NUM, B_NODE_EXEC+node->nr, "EFra: ", 
				  x+w/2, y, w/2, 20, 
				  &nif->efra, 1, MAXFRAMEF, 10, 0, "");
		
	}
	return 80;
}

static void node_scale_cb(void *node_v, void *unused_v)
{
	bNode *node= node_v;
	bNodeSocket *nsock;

	/* check the 2 inputs, and set them to reasonable values */
	for(nsock= node->inputs.first; nsock; nsock= nsock->next) {
		if(ELEM(node->custom1, CMP_SCALE_RELATIVE, CMP_SCALE_SCENEPERCENT))
			nsock->ns.vec[0]= 1.0;
		else {
			if(nsock->next==NULL)
				nsock->ns.vec[0]= (float)G.scene->r.ysch;
			else
				nsock->ns.vec[0]= (float)G.scene->r.xsch;
		}
	}	
}

static int node_composit_buts_scale(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiBut *bt= uiDefButS(block, MENU, B_NODE_EXEC+node->nr, "Relative %x0|Absolute %x1|Scene Size % %x2|",
				  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, 
				  &node->custom1, 0, 0, 0, 0, "Scale new image to absolute pixel size, size relative to the incoming image, or using the 'percent' size of the scene");
		uiButSetFunc(bt, node_scale_cb, node, NULL);
	}
	return 20;
}

static int node_composit_buts_invert(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr) 
{
	if(block) {
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, CMP_CHAN_RGB, B_NODE_EXEC+node->nr, "RGB",
					 butr->xmin, butr->ymin, (butr->xmax-butr->xmin)/2, 20, 
					 &node->custom1, 0, 0, 0, 0, "");
		uiDefButBitS(block, TOG, CMP_CHAN_A, B_NODE_EXEC+node->nr, "A",
					 butr->xmin+(butr->xmax-butr->xmin)/2, butr->ymin, (butr->xmax-butr->xmin)/2, 20, 
					 &node->custom1, 0, 0, 0, 0, "");
		uiBlockEndAlign(block);
	}
	return 20;	
}

static int node_composit_buts_premulkey(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiBut *bt;
		
		/* blend type */
		bt=uiDefButS(block, MENU, B_NODE_EXEC+node->nr, "Key to Premul %x0|Premul to Key %x1",
					 butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, 
					 &node->custom1, 0, 0, 0, 0, "Conversion between premultiplied alpha and key alpha");
	}
	return 20;
}

/* only once called */
static void node_composit_set_butfunc(bNodeType *ntype)
{
	switch(ntype->type) {
		/* case NODE_GROUP:	 note, typeinfo for group is generated... see "XXX ugly hack" */

		case CMP_NODE_IMAGE:
			ntype->butfunc= node_composit_buts_image;
			break;
		case CMP_NODE_R_LAYERS:
			ntype->butfunc= node_composit_buts_renderlayers;
			break;
		case CMP_NODE_NORMAL:
			ntype->butfunc= node_buts_normal;
			break;
		case CMP_NODE_CURVE_VEC:
			ntype->butfunc= node_buts_curvevec;
			break;
		case CMP_NODE_CURVE_RGB:
			ntype->butfunc= node_buts_curvecol;
			break;
		case CMP_NODE_VALUE:
			ntype->butfunc= node_buts_value;
			break;
		case CMP_NODE_RGB:
			ntype->butfunc= node_buts_rgb;
			break;
		case CMP_NODE_FLIP:
			ntype->butfunc= node_composit_buts_flip;
			break;
		case CMP_NODE_SPLITVIEWER:
			ntype->butfunc= node_composit_buts_splitviewer;
			break;
		case CMP_NODE_MIX_RGB:
			ntype->butfunc= node_buts_mix_rgb;
			break;
		case CMP_NODE_VALTORGB:
			ntype->butfunc= node_buts_valtorgb;
			break;
		case CMP_NODE_CROP:
			ntype->butfunc= node_composit_buts_crop;
			break;
		case CMP_NODE_BLUR:
			ntype->butfunc= node_composit_buts_blur;
			break;
		case CMP_NODE_DBLUR:
			ntype->butfunc= node_composit_buts_dblur;
			break;
		case CMP_NODE_BILATERALBLUR:
			ntype->butfunc= node_composit_buts_bilateralblur;
			break;
		/* qdn: defocus node */
		case CMP_NODE_DEFOCUS:
			ntype->butfunc = node_composit_buts_defocus;
			break;
		/* qdn: glare node */
		case CMP_NODE_GLARE:
			ntype->butfunc = node_composit_buts_glare;
			break;
		/* qdn: tonemap node */
		case CMP_NODE_TONEMAP:
			ntype->butfunc = node_composit_buts_tonemap;
			break;
		/* qdn: lens distortion node */
		case CMP_NODE_LENSDIST:
			ntype->butfunc = node_composit_buts_lensdist;
			break;
		case CMP_NODE_VECBLUR:
			ntype->butfunc= node_composit_buts_vecblur;
			break;
		case CMP_NODE_FILTER:
			ntype->butfunc= node_composit_buts_filter;
			break;
		case CMP_NODE_MAP_VALUE:
			ntype->butfunc= node_composit_buts_map_value;
			break;
		case CMP_NODE_TIME:
			ntype->butfunc= node_buts_time;
			break;
		case CMP_NODE_ALPHAOVER:
			ntype->butfunc= node_composit_buts_alphaover;
			break;
		case CMP_NODE_HUE_SAT:
			ntype->butfunc= node_composit_buts_hue_sat;
			break;
		case CMP_NODE_TEXTURE:
			ntype->butfunc= node_buts_texture;
			break;
		case CMP_NODE_DILATEERODE:
			ntype->butfunc= node_composit_buts_dilateerode;
			break;
		case CMP_NODE_OUTPUT_FILE:
			ntype->butfunc= node_composit_buts_file_output;
			break;
	
		case CMP_NODE_DIFF_MATTE:
			ntype->butfunc=node_composit_buts_diff_matte;
			break;
		case CMP_NODE_COLOR_SPILL:
			ntype->butfunc=node_composit_buts_color_spill;
			break;
		case CMP_NODE_CHROMA:
			ntype->butfunc=node_composit_buts_chroma_matte;
			break;
		case CMP_NODE_SCALE:
			ntype->butfunc= node_composit_buts_scale;
			break;
		case CMP_NODE_CHANNEL_MATTE:
			ntype->butfunc= node_composit_buts_channel_matte;
			break;
		case CMP_NODE_LUMA_MATTE:
			ntype->butfunc= node_composit_buts_luma_matte;
			break;
		case CMP_NODE_MAP_UV:
			ntype->butfunc= node_composit_buts_map_uv;
			break;
		case CMP_NODE_ID_MASK:
			ntype->butfunc= node_composit_buts_id_mask;
			break;
		case CMP_NODE_MATH:
			ntype->butfunc= node_buts_math;
			break;
		case CMP_NODE_INVERT:
			ntype->butfunc= node_composit_buts_invert;
			break;
		case CMP_NODE_PREMULKEY:
			ntype->butfunc= node_composit_buts_premulkey;
			break;
		default:
			ntype->butfunc= NULL;
	}
}


/* ******* init draw callbacks for all tree types, only called in usiblender.c, once ************* */

void init_node_butfuncs(void)
{
	bNodeType *ntype;
	
	/* shader nodes */
	ntype= node_all_shaders.first;
	while(ntype) {
		node_shader_set_butfunc(ntype);
		ntype= ntype->next;
	}
	/* composit nodes */
	ntype= node_all_composit.first;
	while(ntype) {
		node_composit_set_butfunc(ntype);
		ntype= ntype->next;
	}
}

/* ************** Generic drawing ************** */

void node_rename_but(char *s)
{
	uiBlock *block;
	ListBase listb={0, 0};
	int dy, x1, y1, sizex=80, sizey=30;
	short pivot[2], mval[2], ret=0;
	
	getmouseco_sc(mval);

	pivot[0]= CLAMPIS(mval[0], (sizex+10), G.curscreen->sizex-30);
	pivot[1]= CLAMPIS(mval[1], (sizey/2)+10, G.curscreen->sizey-(sizey/2)-10);
	
	if (pivot[0]!=mval[0] || pivot[1]!=mval[1])
		warp_pointer(pivot[0], pivot[1]);

	mywinset(G.curscreen->mainwin);
	
	x1= pivot[0]-sizex+10;
	y1= pivot[1]-sizey/2;
	dy= sizey/2;
	
	block= uiNewBlock(&listb, "button", UI_EMBOSS, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT|UI_BLOCK_ENTER_OK);
	
	/* buttons have 0 as return event, to prevent menu to close on hotkeys */
	uiBlockBeginAlign(block);
	
	uiDefBut(block, TEX, B_NOP, "Name: ", (short)(x1),(short)(y1+dy), 150, 19, s, 0.0, 19.0, 0, 0, "Node user name");
	
	uiBlockEndAlign(block);

	uiDefBut(block, BUT, 32767, "OK", (short)(x1+150), (short)(y1+dy), 29, 19, NULL, 0, 0, 0, 0, "");

	uiBoundsBlock(block, 2);

	ret= uiDoBlocks(&listb, 0, 0);
}


static void draw_nodespace_grid(SpaceNode *snode)
{
	float start, step= 25.0f;

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
	
	/* X and Y axis */
	BIF_ThemeColorShade(TH_BACK, -18);
	glVertex2f(0.0f, snode->v2d.cur.ymin);
	glVertex2f(0.0f, snode->v2d.cur.ymax);
	glVertex2f(snode->v2d.cur.xmin, 0.0f);
	glVertex2f(snode->v2d.cur.xmax, 0.0f);
	
	glEnd();
}

static void draw_nodespace_back_pix(ScrArea *sa, SpaceNode *snode)
{
	
	draw_nodespace_grid(snode);
	
	if((snode->flag & SNODE_BACKDRAW) && snode->treetype==NTREE_COMPOSIT) {
		Image *ima= BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
		ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
		if(ibuf) {
			int x, y; 
			/* somehow the offset has to be calculated inverse */
			
			glaDefine2DArea(&sa->winrct);
			/* ortho at pixel level curarea */
			myortho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);
			
			x = (sa->winx-ibuf->x)/2 + snode->xof;
			y = (sa->winy-ibuf->y)/2 + snode->yof;
			
			if(ibuf->rect)
				glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
			else if(ibuf->channels==4)
				glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_FLOAT, ibuf->rect_float);
			
			/* sort this out, this should not be needed */
			myortho2(snode->v2d.cur.xmin, snode->v2d.cur.xmax, snode->v2d.cur.ymin, snode->v2d.cur.ymax);
			bwin_clear_viewmat(sa->win);	/* clear buttons view */
			glLoadIdentity();
		}
	}
}

#if 0
/* note: needs to be userpref or opengl profile option */
static void draw_nodespace_back_tex(ScrArea *sa, SpaceNode *snode)
{

	draw_nodespace_grid(snode);
	
	if(snode->flag & SNODE_BACKDRAW) {
		Image *ima= BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
		ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
		if(ibuf) {
			int x, y;
			float zoom = 1.0;

			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			
			glaDefine2DArea(&sa->winrct);

			if(ibuf->x > sa->winx || ibuf->y > sa->winy) {
				float zoomx, zoomy;
				zoomx= (float)sa->winx/ibuf->x;
				zoomy= (float)sa->winy/ibuf->y;
				zoom = MIN2(zoomx, zoomy);
			}
			
			x = (sa->winx-zoom*ibuf->x)/2 + snode->xof;
			y = (sa->winy-zoom*ibuf->y)/2 + snode->yof;

			glPixelZoom(zoom, zoom);

			glColor4f(1.0, 1.0, 1.0, 1.0);
			if(ibuf->rect)
				glaDrawPixelsTex(x, y, ibuf->x, ibuf->y, GL_UNSIGNED_BYTE, ibuf->rect);
			else if(ibuf->channels==4)
				glaDrawPixelsTex(x, y, ibuf->x, ibuf->y, GL_FLOAT, ibuf->rect_float);

			glPixelZoom(1.0, 1.0);

			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
		}
	}
}
#endif

/* nice AA filled circle */
/* this might have some more generic use */
static void circle_draw(float x, float y, float size, int type, int col[3])
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

/* not a callback */
static void node_draw_preview(bNodePreview *preview, rctf *prv)
{
	float xscale= (prv->xmax-prv->xmin)/((float)preview->xsize);
	float yscale= (prv->ymax-prv->ymin)/((float)preview->ysize);
	float tile= (prv->xmax - prv->xmin) / 10.0;
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
	glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );	/* premul graphics */
	
	glColor4f(1.0, 1.0, 1.0, 1.0);
	glaDrawPixelsTex(prv->xmin, prv->ymin, preview->xsize, preview->ysize, GL_FLOAT, preview->rect);
	
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glDisable(GL_BLEND);
	glPixelZoom(1.0f, 1.0f);

	BIF_ThemeColorShadeAlpha(TH_BACK, -15, +100);
	fdrawbox(prv->xmin, prv->ymin, prv->xmax, prv->ymax);
	
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
		hiddenrad += 5.0*(float)(tot-4);
	}
	
	node->totr.xmin= node->locx;
	node->totr.xmax= node->locx + 3*hiddenrad + node->miniwidth;
	node->totr.ymax= node->locy + (hiddenrad - 0.5f*NODE_DY);
	node->totr.ymin= node->totr.ymax - 2*hiddenrad;
	
	/* output sockets */
	rad=drad= M_PI/(1.0f + (float)totout);
	
	for(nsock= node->outputs.first; nsock; nsock= nsock->next) {
		if(!(nsock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
			nsock->locx= node->totr.xmax - hiddenrad + sin(rad)*hiddenrad;
			nsock->locy= node->totr.ymin + hiddenrad + cos(rad)*hiddenrad;
			rad+= drad;
		}
	}
	
	/* input sockets */
	rad=drad= - M_PI/(1.0f + (float)totin);
	
	for(nsock= node->inputs.first; nsock; nsock= nsock->next) {
		if(!(nsock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
			nsock->locx= node->totr.xmin + hiddenrad + sin(rad)*hiddenrad;
			nsock->locy= node->totr.ymin + hiddenrad + cos(rad)*hiddenrad;
			rad+= drad;
		}
	}
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
				
				node->prvr.xmin+= 0.5*dx;
				node->prvr.xmax-= 0.5*dx;
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
		node->typeinfo->butfunc= node_buts_group;
	
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
	if(node->typeinfo->nclass==NODE_CLASS_CONVERTOR)
		return TH_NODE_CONVERTOR;
	if(ELEM3(node->typeinfo->nclass, NODE_CLASS_OP_COLOR, NODE_CLASS_OP_VECTOR, NODE_CLASS_OP_FILTER))
		return TH_NODE_OPERATOR;
	if(node->typeinfo->nclass==NODE_CLASS_GROUP)
		return TH_NODE_GROUP;
	return TH_NODE;
}

static void node_draw_link_bezier(float vec[][3], int th_col1, int th_col2, int do_shaded)
{
	float dist;
	
	dist= 0.5f*ABS(vec[0][0] - vec[3][0]);
	
	/* check direction later, for top sockets */
	vec[1][0]= vec[0][0]+dist;
	vec[1][1]= vec[0][1];
	
	vec[2][0]= vec[3][0]-dist;
	vec[2][1]= vec[3][1];
	
	if( MIN4(vec[0][0], vec[1][0], vec[2][0], vec[3][0]) > G.v2d->cur.xmax); /* clipped */	
	else if ( MAX4(vec[0][0], vec[1][0], vec[2][0], vec[3][0]) < G.v2d->cur.xmin); /* clipped */
	else {
		float curve_res = 24, spline_step = 0.0f;
		
		/* we can reuse the dist variable here to increment the GL curve eval amount*/
		dist = 1.0f/curve_res;
		
		glMap1f(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 4, vec[0]);
		glBegin(GL_LINE_STRIP);
		while (spline_step < 1.000001f) {
			if(do_shaded)
				BIF_ThemeColorBlend(th_col1, th_col2, spline_step);
			glEvalCoord1f(spline_step);
			spline_step += dist;
		}
		glEnd();
	}
	
}

/* note; this is used for fake links in groups too */
void node_draw_link(SpaceNode *snode, bNodeLink *link)
{
	float vec[4][3];
	float mx=0.0f, my=0.0f;
	int do_shaded= 1, th_col1= TH_WIRE, th_col2= TH_WIRE;
	
	if(link->fromnode==NULL && link->tonode==NULL)
		return;
	
	/* this is dragging link */
	if(link->fromnode==NULL || link->tonode==NULL) {
		short mval[2];
		getmouseco_areawin(mval);
		areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
		BIF_ThemeColor(TH_WIRE);
		do_shaded= 0;
	}
	else {
		/* going to give issues once... */
		if(link->tosock->flag & SOCK_UNAVAIL)
			return;
		if(link->fromsock->flag & SOCK_UNAVAIL)
			return;
		
		/* a bit ugly... but thats how we detect the internal group links */
		if(link->fromnode==link->tonode) {
			BIF_ThemeColorBlend(TH_BACK, TH_WIRE, 0.25f);
			do_shaded= 0;
		}
		else {
			/* check cyclic */
			if(link->fromnode->level >= link->tonode->level && link->tonode->level!=0xFFF) {
				if(link->fromnode->flag & SELECT)
					th_col1= TH_EDGE_SELECT;
				if(link->tonode->flag & SELECT)
					th_col2= TH_EDGE_SELECT;
			}				
			else {
				BIF_ThemeColor(TH_REDALERT);
				do_shaded= 0;
			}
		}
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
	
	node_draw_link_bezier(vec, th_col1, th_col2, do_shaded);
}


/* note: in cmp_util.c is similar code, for node_compo_pass_on() */
static void node_draw_mute_line(SpaceNode *snode, bNode *node)
{
	bNodeSocket *valsock= NULL, *colsock= NULL, *vecsock= NULL;
	bNodeSocket *sock;
	float vec[4][3];
	int a;
	
	vec[0][2]= vec[1][2]= vec[2][2]= vec[3][2]= 0.0; /* only 2d spline, set the Z to 0*/
	
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
	BIF_ThemeColor(TH_REDALERT);
	glEnable(GL_BLEND);
	glEnable( GL_LINE_SMOOTH );
	
	if(valsock || colsock || vecsock) {
		for(a=0, sock= node->outputs.first; sock; sock= sock->next, a++) {
			if(nodeCountSocketLinks(snode->edittree, sock)) {
				vec[3][0]= sock->locx;
				vec[3][1]= sock->locy;
				
				if(sock->type==SOCK_VALUE && valsock) {
					vec[0][0]= valsock->locx;
					vec[0][1]= valsock->locy;
					node_draw_link_bezier(vec, TH_WIRE, TH_WIRE, 0);
					valsock= NULL;
				}
				if(sock->type==SOCK_VECTOR && vecsock) {
					vec[0][0]= vecsock->locx;
					vec[0][1]= vecsock->locy;
					node_draw_link_bezier(vec, TH_WIRE, TH_WIRE, 0);
					vecsock= NULL;
				}
				if(sock->type==SOCK_RGBA && colsock) {
					vec[0][0]= colsock->locx;
					vec[0][1]= colsock->locy;
					node_draw_link_bezier(vec, TH_WIRE, TH_WIRE, 0);
					colsock= NULL;
				}
			}
		}
	}
	glDisable(GL_BLEND);
	glDisable( GL_LINE_SMOOTH );
}


static void node_draw_basis(ScrArea *sa, SpaceNode *snode, bNode *node)
{
	bNodeSocket *sock;
	uiBlock *block= NULL;
	uiBut *bt;
	rctf *rct= &node->totr;
	float slen, iconofs;
	int ofs, color_id= node_get_colorid(node);
	char showname[128]; /* 128 used below */
	
	uiSetRoundBox(15-4);
	ui_dropshadow(rct, BASIS_RAD, snode->aspect, node->flag & SELECT);
	
	/* header */
	if(color_id==TH_NODE)
		BIF_ThemeColorShade(color_id, -20);
	else
		BIF_ThemeColor(color_id);
		
	uiSetRoundBox(3);
	uiRoundBox(rct->xmin, rct->ymax-NODE_DY, rct->xmax, rct->ymax, BASIS_RAD);
	
	/* show/hide icons, note this sequence is copied in editnode.c */
	iconofs= rct->xmax;
	
	if(node->typeinfo->flag & NODE_PREVIEW) {
		int icon_id;
		
		if(node->flag & (NODE_ACTIVE_ID|NODE_DO_OUTPUT))
			icon_id= ICON_MATERIAL;
		else
			icon_id= ICON_MATERIAL_DEHLT;
		iconofs-= 18.0f;
		glEnable(GL_BLEND);
		BIF_icon_draw_aspect_blended(iconofs, rct->ymax-NODE_DY+2, icon_id, snode->aspect, -60);
		glDisable(GL_BLEND);
	}
	if(node->type == NODE_GROUP) {
		
		iconofs-= 18.0f;
		glEnable(GL_BLEND);
		if(node->id->lib) {
			glPixelTransferf(GL_GREEN_SCALE, 0.7f);
			glPixelTransferf(GL_BLUE_SCALE, 0.3f);
			BIF_icon_draw_aspect(iconofs, rct->ymax-NODE_DY+2, ICON_NODE, snode->aspect);
			glPixelTransferf(GL_GREEN_SCALE, 1.0f);
			glPixelTransferf(GL_BLUE_SCALE, 1.0f);
		}
		else {
			BIF_icon_draw_aspect_blended(iconofs, rct->ymax-NODE_DY+2, ICON_NODE, snode->aspect, -60);
		}
		glDisable(GL_BLEND);
	}
	if(node->typeinfo->flag & NODE_OPTIONS) {
		iconofs-= 18.0f;
		glEnable(GL_BLEND);
		BIF_icon_draw_aspect_blended(iconofs, rct->ymax-NODE_DY+2, ICON_BUTS, snode->aspect, -60);
		glDisable(GL_BLEND);
	}
	{	/* always hide/reveil unused sockets */ 
		int shade;

		iconofs-= 18.0f;
		if(node_has_hidden_sockets(node))
			shade= -40;
		else
			shade= -90;
		glEnable(GL_BLEND);
		BIF_icon_draw_aspect_blended(iconofs, rct->ymax-NODE_DY+2, ICON_PLUS, snode->aspect, shade);
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
	
	if(node->flag & NODE_MUTED)
		sprintf(showname, "[%s]", node->name);
	else if(node->username[0])
		sprintf(showname, "(%s) %s", node->username, node->name);
	else
		BLI_strncpy(showname, node->name, 128);

	snode_drawstring(snode, showname, (int)(iconofs - rct->xmin-18.0f));

	/* body */
	BIF_ThemeColor4(TH_NODE);
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
		node_draw_mute_line(snode, node);

	/* we make buttons for input sockets, if... */
	if(node->flag & NODE_OPTIONS) {
		if(node->inputs.first || node->typeinfo->butfunc) {
			char str[32];
			
			/* make unique block name, also used for handling blocks in editnode.c */
			sprintf(str, "node buttons %p", node);
			
			block= uiNewBlock(&sa->uiblocks, str, UI_EMBOSS, UI_HELV, sa->win);
			uiBlockSetFlag(block, UI_BLOCK_NO_HILITE);
			if(snode->id)
				uiSetButLock(snode->id->lib!=NULL, ERROR_LIBDATA_MESSAGE);
		}
	}
	
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
					bt= uiDefButF(block, NUM, B_NODE_EXEC+node->nr, sock->name, 
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
					short labelw= node->width-NODE_DY-40, width;
					
					if(labelw>0) width= 40; else width= node->width-NODE_DY;
					
					bt= uiDefButF(block, COL, B_NODE_EXEC+node->nr, "", 
						(short)(sock->locx+NODE_DYS), (short)sock->locy-8, width, 15, 
						   butpoin, 0, 0, 0, 0, "");
					uiButSetFunc(bt, node_sync_cb, snode, node);
					
					if(labelw>0) uiDefBut(block, LABEL, 0, sock->name, 
										   (short)(sock->locx+NODE_DYS) + 40, (short)sock->locy-8, labelw, 15, 
										   NULL, 0, 0, 0, 0, "");
				}
			}
			else {
				BIF_ThemeColor(TH_TEXT);
				ui_rasterpos_safe(sock->locx+8.0f, sock->locy-5.0f, snode->aspect);
				BIF_DrawString(snode->curfont, sock->name, 0);
			}
		}
	}
	
	/* socket outputs */
	for(sock= node->outputs.first; sock; sock= sock->next) {
		if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
			socket_circle_draw(sock, NODE_SOCKSIZE);
			
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
			uiDrawBlock(block);
		}
	}
	
}

static void node_draw_hidden(SpaceNode *snode, bNode *node)
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
	BIF_ThemeColor(color_id);	
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
		BIF_ThemeColor(TH_TEXT_HI);
	else
		BIF_ThemeColorBlendShade(TH_TEXT, color_id, 0.4, 10);
	
	/* open entirely icon */
	ui_draw_tria_icon(rct->xmin+9.0f, centy-6.0f, snode->aspect, 'h');	
	
	/* disable lines */
	if(node->flag & NODE_MUTED)
		node_draw_mute_line(snode, node);	
	
	if(node->flag & SELECT) 
		BIF_ThemeColor(TH_TEXT_HI);
	else
		BIF_ThemeColor(TH_TEXT);
	
	if(node->miniwidth>0.0f) {
		ui_rasterpos_safe(rct->xmin+21.0f, centy-4.0f, snode->aspect);

		if(node->flag & NODE_MUTED)
			sprintf(showname, "[%s]", node->name);
		else if(node->username[0])
			sprintf(showname, "(%s)%s", node->username, node->name);
		else
			BLI_strncpy(showname, node->name, 128);

		snode_drawstring(snode, showname, (int)(rct->xmax - rct->xmin-18.0f -12.0f));
	}	

	/* scale widget thing */
	BIF_ThemeColorShade(color_id, -10);	
	dx= 10.0f;
	fdrawline(rct->xmax-dx, centy-4.0f, rct->xmax-dx, centy+4.0f);
	fdrawline(rct->xmax-dx-3.0f*snode->aspect, centy-4.0f, rct->xmax-dx-3.0f*snode->aspect, centy+4.0f);
	
	BIF_ThemeColorShade(color_id, +30);
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
}

static void node_draw_nodetree(ScrArea *sa, SpaceNode *snode, bNodeTree *ntree)
{
	bNode *node;
	bNodeLink *link;
	int a;
	
	if(ntree==NULL) return;		/* groups... */
	
	/* node lines */
	glEnable(GL_BLEND);
	glEnable( GL_LINE_SMOOTH );
	for(link= ntree->links.first; link; link= link->next)
		node_draw_link(snode, link);
	glDisable(GL_BLEND);
	glDisable( GL_LINE_SMOOTH );
	
	/* not selected first */
	for(a=0, node= ntree->nodes.first; node; node= node->next, a++) {
		node->nr= a;		/* index of node in list, used for exec event code */
		if(!(node->flag & SELECT)) {
			if(node->flag & NODE_GROUP_EDIT);
			else if(node->flag & NODE_HIDDEN)
				node_draw_hidden(snode, node);
			else
				node_draw_basis(sa, snode, node);
		}
	}
	
	/* selected */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if(node->flag & NODE_GROUP_EDIT);
			else if(node->flag & NODE_HIDDEN)
				node_draw_hidden(snode, node);
			else
				node_draw_basis(sa, snode, node);
		}
	}	
}

/* fake links from groupnode to internal nodes */
static void node_draw_group_links(SpaceNode *snode, bNode *gnode)
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
				node_draw_link(snode, &fakelink);
			}
		}
	}
	
	for(sock= gnode->outputs.first; sock; sock= sock->next) {
		if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
			if(sock->tosock) {
				fakelink.tosock= sock;
				fakelink.fromsock= sock->tosock;
				node_draw_link(snode, &fakelink);
			}
		}
	}
	
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

/* groups are, on creation, centered around 0,0 */
static void node_draw_group(ScrArea *sa, SpaceNode *snode, bNode *gnode)
{
	bNodeTree *ngroup= (bNodeTree *)gnode->id;
	bNodeSocket *sock;
	rctf rect= gnode->totr;
	char showname[128];
	
	/* backdrop header */
	glEnable(GL_BLEND);
	uiSetRoundBox(3);
	BIF_ThemeColorShadeAlpha(TH_NODE_GROUP, 0, -70);
	gl_round_box(GL_POLYGON, rect.xmin, rect.ymax, rect.xmax, rect.ymax+NODE_DY, BASIS_RAD);
	
	/* backdrop body */
	BIF_ThemeColorShadeAlpha(TH_BACK, -8, -70);
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
	BIF_ThemeColor(TH_TEXT_HI);
	ui_rasterpos_safe(rect.xmin+8.0f, rect.ymax+5.0f, snode->aspect);

	if(gnode->username[0]) {
		strcpy(showname,"(");
		strcat(showname, gnode->username);
		strcat(showname,") ");
		strcat(showname, ngroup->id.name+2);
	}
	else
		strcpy(showname, ngroup->id.name+2);

	BIF_DrawString(snode->curfont, showname, 0);
	
	/* links from groupsockets to the internal nodes */
	node_draw_group_links(snode, gnode);
	
	/* group sockets */
	for(sock= gnode->inputs.first; sock; sock= sock->next)
		if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL)))
			socket_circle_draw(sock, NODE_SOCKSIZE);
	for(sock= gnode->outputs.first; sock; sock= sock->next)
		if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL)))
			socket_circle_draw(sock, NODE_SOCKSIZE);

	/* and finally the whole tree */
	node_draw_nodetree(sa, snode, ngroup);
}



static void nodes_panel_gpencil(short cntrl)	// NODES_HANDLER_GREASEPENCIL
{
	uiBlock *block;
	SpaceNode *snode;
	
	snode= curarea->spacedata.first;

	block= uiNewBlock(&curarea->uiblocks, "nodes_panel_gpencil", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE  | cntrl);
	uiSetPanelHandler(NODES_HANDLER_GREASEPENCIL);  // for close and esc
	if (uiNewPanel(curarea, block, "Grease Pencil", "SpaceNode", 100, 30, 318, 204)==0) return;
	
	/* we can only really draw stuff if there are nodes (otherwise no events are handled */
	if (snode->nodetree == NULL)
		return;
	
	/* allocate memory for gpd if drawing enabled (this must be done first or else we crash) */
	if (snode->flag & SNODE_DISPGP) {
		if (snode->gpd == NULL)
			gpencil_data_setactive(curarea, gpencil_data_addnew());
	}
	
	if (snode->flag & SNODE_DISPGP) {
		bGPdata *gpd= snode->gpd;
		short newheight;
		
		/* this is a variable height panel, newpanel doesnt force new size on existing panels */
		/* so first we make it default height */
		uiNewPanelHeight(block, 204);
		
		/* draw button for showing gpencil settings and drawings */
		uiDefButBitS(block, TOG, SNODE_DISPGP, B_REDR, "Use Grease Pencil", 10, 225, 150, 20, &snode->flag, 0, 0, 0, 0, "Display freehand annotations overlay over this Node Editor");
		
		/* extend the panel if the contents won't fit */
		newheight= draw_gpencil_panel(block, gpd, curarea); 
		uiNewPanelHeight(block, newheight);
	}
	else {
		uiDefButBitS(block, TOG, SNODE_DISPGP, B_REDR, "Use Grease Pencil", 10, 225, 150, 20, &snode->flag, 0, 0, 0, 0, "Display freehand annotations overlay over this Node Editor");
		uiDefBut(block, LABEL, 1, " ",	160, 180, 150, 20, NULL, 0.0, 0.0, 0, 0, "");
	}
}

static void nodes_blockhandlers(ScrArea *sa)
{
	SpaceNode *snode= sa->spacedata.first;
	short a;
	
	for(a=0; a<SPACE_MAXHANDLER; a+=2) {
		/* clear action value for event */
		switch(snode->blockhandler[a]) {
			case NODES_HANDLER_GREASEPENCIL:
				nodes_panel_gpencil(snode->blockhandler[a+1]);
				break;
		}
	}
	uiDrawBlocksPanels(sa, 0);
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
	
	/* always free, blocks here have no unique identifier (1 block per node) */
	uiFreeBlocksWin(&sa->uiblocks, sa->win);

	/* only set once */
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable(GL_MAP1_VERTEX_3);

	/* aspect+font, set each time */
	snode->aspect= (snode->v2d.cur.xmax - snode->v2d.cur.xmin)/((float)sa->winx);
	snode->curfont= uiSetCurFont_ext(snode->aspect);

	/* backdrop */
	draw_nodespace_back_pix(sa, snode);
	
	/* nodes */
	snode_set_context(snode);
	
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

		node_draw_nodetree(sa, snode, snode->nodetree);
			
		/* active group */
		for(node= snode->nodetree->nodes.first; node; node= node->next) {
			if(node->flag & NODE_GROUP_EDIT)
				node_draw_group(sa, snode, node);
		}
	}
	
	/* draw grease-pencil ('canvas' strokes) */
	if ((snode->flag & SNODE_DISPGP) && (snode->nodetree))
		draw_gpencil_2dview(sa, 1);
	
	/* restore viewport (not needed yet) */
	mywinset(sa->win);

	/* ortho at pixel level curarea */
	myortho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);
	
	/* draw grease-pencil (screen strokes) */
	if ((snode->flag & SNODE_DISPGP) && (snode->nodetree))
		draw_gpencil_2dview(sa, 0);

	draw_area_emboss(sa);
	
	/* it is important to end a view in a transform compatible with buttons */
	bwin_scalematrix(sa->win, snode->blockscale, snode->blockscale, snode->blockscale);
	nodes_blockhandlers(sa);
	
	curarea->win_swap= WIN_BACK_OK;
	
	/* in the end, this is a delayed previewrender test, to allow buttons to be first */
	if(snode->flag & SNODE_DO_PREVIEW) {
		addafterqueue(sa->win, RENDERPREVIEW, 1);
		snode->flag &= ~SNODE_DO_PREVIEW;
	}
}
