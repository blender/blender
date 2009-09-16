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
 * Contributor(s): David Millan Escriva, Juho Vepsäläinen, Bob Holcomb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

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

#include "BKE_context.h"
#include "BKE_curve.h"
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

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "MEM_guardedalloc.h"

#include "ED_node.h"
#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"
#include "UI_interface.h"
#include "UI_resources.h"

#include "RE_pipeline.h"
#include "IMB_imbuf_types.h"

#include "node_intern.h"


/* autocomplete callback for buttons */
static void autocomplete_vcol(bContext *C, char *str, void *arg_v)
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

/* ****************** GENERAL CALLBACKS FOR NODES ***************** */

static void node_ID_title_cb(bContext *C, void *node_v, void *unused_v)
{
	bNode *node= node_v;
	
	if(node->id) {
		test_idbutton(node->id->name+2);	/* library.c, verifies unique name */
		BLI_strncpy(node->name, node->id->name+2, 21);
	}
}


static void node_but_title_cb(bContext *C, void *node_v, void *but_v)
{
	// bNode *node= node_v;
	// XXX uiBut *bt= but_v;
	// XXX BLI_strncpy(node->name, bt->drawstr, NODE_MAXSTR);
	
	// allqueue(REDRAWNODE, 0);
}

#if 0
/* XXX not used yet, make compiler happy :) */
static void node_group_alone_cb(bContext *C, void *node_v, void *unused_v)
{
	bNode *node= node_v;
	
	nodeCopyGroup(node);

	// allqueue(REDRAWNODE, 0);
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
					 (short)butr->xmin, (short)butr->ymin, width, 19, 
					 node->id->name+2, 0.0, 19.0, 0, 0, "NodeTree name");
		uiButSetFunc(bt, node_ID_title_cb, node, NULL);
		
		/* user amount */
		if(node->id->us>1) {
			char str1[32];
			sprintf(str1, "%d", node->id->us);
			bt= uiDefBut(block, BUT, B_NOP, str1, 
						 (short)butr->xmax-19, (short)butr->ymin, 19, 19, 
						 NULL, 0, 0, 0, 0, "Displays number of users.");
			uiButSetFunc(bt, node_group_alone_cb, node, NULL);
		}
		
		uiBlockEndAlign(block);
	}	
	return 19;
}
#endif

static int node_buts_value(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		bNodeSocket *sock= node->outputs.first;		/* first socket stores value */
		
		uiDefButF(block, NUM, B_NODE_EXEC, "", 
				  (short)butr->xmin, (short)butr->ymin, butr->xmax-butr->xmin, 20, 
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
			
			uiDefButF(block, HSVCUBE, B_NODE_EXEC, "", 
					  (short)butr->xmin, (short)butr->ymin, butr->xmax-butr->xmin, 12, 
					  sock->ns.vec, 0.0f, 1.0f, 3, 0, "");
			uiDefButF(block, HSVCUBE, B_NODE_EXEC, "", 
					  (short)butr->xmin, (short)butr->ymin+15, butr->xmax-butr->xmin, butr->ymax-butr->ymin -15 -15, 
					  sock->ns.vec, 0.0f, 1.0f, 2, 0, "");
			uiDefButF(block, COL, B_NOP, "",		
					  (short)butr->xmin, (short)butr->ymax-12, butr->xmax-butr->xmin, 12, 
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
		bt=uiDefButS(block, MENU, B_NODE_EXEC, "Mix %x0|Add %x1|Subtract %x3|Multiply %x2|Screen %x4|Overlay %x9|Divide %x5|Difference %x6|Darken %x7|Lighten %x8|Dodge %x10|Burn %x11|Color %x15|Value %x14|Saturation %x13|Hue %x12|Soft Light %x16|Linear Light %x17", 
					 (short)butr->xmin, (short)butr->ymin, butr->xmax-butr->xmin -(a_but?20:0), 20, 
					 &node->custom1, 0, 0, 0, 0, "");
		uiButSetFunc(bt, node_but_title_cb, node, bt);
		/* Alpha option, composite */
		if(a_but)
			uiDefIconButS(block, TOG, B_NODE_EXEC, ICON_IMAGE_RGB_ALPHA,
				  (short)butr->xmax-20, (short)butr->ymin, 20, 20, 
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

		curvemap_buttons(block, node->storage, 's', B_NODE_EXEC, B_REDR, butr);
		
		if(cumap) {
			//cumap->flag |= CUMA_DRAW_CFRA;
			//if(node->custom1<node->custom2)
			//	cumap->sample[0]= (float)(CFRA - node->custom1)/(float)(node->custom2-node->custom1);
		}

		uiBlockBeginAlign(block);
		uiDefButS(block, NUM, B_NODE_EXEC, "Sta:",
				  (short)butr->xmin, (short)butr->ymin-22, dx, 19, 
				  &node->custom1, 1.0, 20000.0, 0, 0, "Start frame");
		uiDefButS(block, NUM, B_NODE_EXEC, "End:",
				  (short)butr->xmin+dx, (short)butr->ymin-22, dx, 19, 
				  &node->custom2, 1.0, 20000.0, 0, 0, "End frame");
	}
	
	return node->width-NODE_DY;
}

static int node_buts_valtorgb(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		if(node->storage) {
			uiBlockColorbandButtons(block, node->storage, butr, B_NODE_EXEC);
		}
	}
	return 40;
}

static int node_buts_curvevec(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		curvemap_buttons(block, node->storage, 'v', B_NODE_EXEC, B_REDR, butr);
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

		curvemap_buttons(block, node->storage, 'c', B_NODE_EXEC, B_REDR, butr);
	}	
	return (int)(node->width-NODE_DY);
}

static int node_buts_normal(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		bNodeSocket *sock= node->outputs.first;		/* first socket stores normal */
		
		uiDefButF(block, BUT_NORMAL, B_NODE_EXEC, "", 
				  (short)butr->xmin, (short)butr->ymin, butr->xmax-butr->xmin, butr->ymax-butr->ymin, 
				  sock->ns.vec, 0.0f, 1.0f, 0, 0, "");
		
	}	
	return (int)(node->width-NODE_DY);
}

static void node_browse_tex_cb(bContext *C, void *ntree_v, void *node_v)
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
	
	if( ntree->type == NTREE_TEXTURE )
		ntreeTexCheckCyclics( ntree );
	
	// allqueue(REDRAWBUTSSHADING, 0);
	// allqueue(REDRAWNODE, 0);
	NodeTagChanged(ntree, node); 
	
	node->menunr= 0;
}

static void node_dynamic_update_cb(bContext *C, void *ntree_v, void *node_v)
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

	// allqueue(REDRAWBUTSSHADING, 0);
	// allqueue(REDRAWNODE, 0);
	// XXX BIF_preview_changed(ID_MA);
}

static int node_buts_texture(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	short multi = (
		node->id &&
		((Tex*)node->id)->use_nodes &&
		(node->type != CMP_NODE_TEXTURE) &&
		(node->type != TEX_NODE_TEXTURE)
	);
	
	if(block) {
		uiBut *bt;
		char *strp;
		short width = (short)(butr->xmax - butr->xmin);
		
		/* browse button texture */
		uiBlockBeginAlign(block);
		IDnames_to_pupstring(&strp, NULL, "", &(G.main->tex), NULL, NULL);
		node->menunr= 0;
		bt= uiDefButS(block, MENU, B_NODE_EXEC, strp, 
				butr->xmin, butr->ymin+(multi?30:0), 20, 19, 
					  &node->menunr, 0, 0, 0, 0, "Browse texture");
		uiButSetFunc(bt, node_browse_tex_cb, ntree, node);
		if(strp) MEM_freeN(strp);
		
		if(node->id) {
			bt= uiDefBut(block, TEX, B_NOP, "TE:",
					butr->xmin+19, butr->ymin+(multi?30:0), butr->xmax-butr->xmin-19, 19, 
						 node->id->name+2, 0.0, 19.0, 0, 0, "Texture name");
			uiButSetFunc(bt, node_ID_title_cb, node, NULL);
		}
		uiBlockEndAlign(block);
		
		if(multi) {
			char *menustr = ntreeTexOutputMenu(((Tex*)node->id)->nodetree);
			uiDefButS(block, MENU, B_MATPRV, menustr, butr->xmin, butr->ymin, width, 19, &node->custom1, 0, 0, 0, 0, "Which output to use, for multi-output textures");
			free(menustr);
			return 50;
		}
		return 20;
	}	
	else return multi? 50: 20;
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

static void node_browse_text_cb(bContext *C, void *ntree_v, void *node_v)
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

	// allqueue(REDRAWBUTSSHADING, 0);
	// allqueue(REDRAWNODE, 0);

	node->menunr= 0;
}

static void node_mat_alone_cb(bContext *C, void *node_v, void *unused)
{
	bNode *node= node_v;
	
	node->id= (ID *)copy_material((Material *)node->id);
	
	//BIF_undo_push("Single user material");
	// allqueue(REDRAWBUTSSHADING, 0);
	// allqueue(REDRAWNODE, 0);
	// allqueue(REDRAWOOPS, 0);
}

static void node_browse_mat_cb(bContext *C, void *ntree_v, void *node_v)
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

	// allqueue(REDRAWBUTSSHADING, 0);
	// allqueue(REDRAWNODE, 0);
	// XXX BIF_preview_changed(ID_MA);

	node->menunr= 0;
}

static void node_new_mat_cb(bContext *C, void *ntree_v, void *node_v)
{
	bNodeTree *ntree= ntree_v;
	bNode *node= node_v;
	
	node->id= (ID *)add_material("MatNode");
	BLI_strncpy(node->name, node->id->name+2, 21);

	nodeSetActive(ntree, node);

	// allqueue(REDRAWBUTSSHADING, 0);
	// allqueue(REDRAWNODE, 0);
	// XXX BIF_preview_changed(ID_MA);

}

static void node_texmap_cb(bContext *C, void *texmap_v, void *unused_v)
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
		/* XXX
		if(node->id==NULL) uiBlockSetCol(block, TH_REDALERT);
		else if(has_us) uiBlockSetCol(block, TH_BUT_SETTING1);
		else uiBlockSetCol(block, TH_BUT_SETTING2);
		*/
		
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
				uiDefButBitS(block, TOG, SH_NODE_MAT_DIFF, B_NODE_EXEC, "Diff",
							 butr->xmin, butr->ymin, dx, 19, 
							 &node->custom1, 0, 0, 0, 0, "Material Node outputs Diffuse");
				uiDefButBitS(block, TOG, SH_NODE_MAT_SPEC, B_NODE_EXEC, "Spec",
							 butr->xmin+dx, butr->ymin, dx, 19, 
							 &node->custom1, 0, 0, 0, 0, "Material Node outputs Specular");
				uiDefButBitS(block, TOG, SH_NODE_MAT_NEG, B_NODE_EXEC, "Neg Normal",
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
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+dx, dy, 2*dx, 19, texmap->loc, -1000.0f, 1000.0f, 10, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->loc+1, -1000.0f, 1000.0f, 10, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->loc+2, -1000.0f, 1000.0f, 10, 2, "");
		dy-= 19;
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+dx, dy, 2*dx, 19, texmap->rot, -1000.0f, 1000.0f, 1000, 1, "");
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->rot+1, -1000.0f, 1000.0f, 1000, 1, "");
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->rot+2, -1000.0f, 1000.0f, 1000, 1, "");
		dy-= 19;
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+dx, dy, 2*dx, 19, texmap->size, -1000.0f, 1000.0f, 10, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->size+1, -1000.0f, 1000.0f, 10, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->size+2, -1000.0f, 1000.0f, 10, 2, "");
		dy-= 25;
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+dx, dy, 2*dx, 19, texmap->min, -10.0f, 10.0f, 100, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->min+1, -10.0f, 10.0f, 100, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->min+2, -10.0f, 10.0f, 100, 2, "");
		dy-= 19;
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+dx, dy, 2*dx, 19, texmap->max, -10.0f, 10.0f, 10, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->max+1, -10.0f, 10.0f, 10, 2, "");
		uiDefButF(block, NUM, B_NODE_EXEC, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->max+2, -10.0f, 10.0f, 10, 2, "");
		uiBlockEndAlign(block);
		
		/* labels/options */
		
		dy= (short)(butr->ymax-19);
		uiDefBut(block, LABEL, B_NOP, "Loc", butr->xmin, dy, dx, 19, NULL, 0.0f, 0.0f, 0, 0, "");
		dy-= 19;
		uiDefBut(block, LABEL, B_NOP, "Rot", butr->xmin, dy, dx, 19, NULL, 0.0f, 0.0f, 0, 0, "");
		dy-= 19;
		uiDefBut(block, LABEL, B_NOP, "Size", butr->xmin, dy, dx, 19, NULL, 0.0f, 0.0f, 0, 0, "");
		dy-= 25;
		uiDefButBitI(block, TOG, TEXMAP_CLIP_MIN, B_NODE_EXEC, "Min", butr->xmin, dy, dx-4, 19, &texmap->flag, 0.0f, 0.0f, 0, 0, "");
		dy-= 19;
		uiDefButBitI(block, TOG, TEXMAP_CLIP_MAX, B_NODE_EXEC, "Max", butr->xmin, dy, dx-4, 19, &texmap->flag, 0.0f, 0.0f, 0, 0, "");
		
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

		// XXX if(!verify_valid_uv_name(ngeo->uvname))
		// XXX	uiBlockSetCol(block, TH_REDALERT);
		but= uiDefBut(block, TEX, B_NODE_EXEC, "UV:", butr->xmin, butr->ymin+20, butr->xmax-butr->xmin, 20, ngeo->uvname, 0, 31, 0, 0, "Set name of UV layer to use, default is active UV layer");
		// XXX uiButSetCompleteFunc(but, autocomplete_uv, NULL);

		if(!verify_valid_vcol_name(ngeo->colname));
//			uiBlockSetCol(block, TH_REDALERT);
		but= uiDefBut(block, TEX, B_NODE_EXEC, "Col:", butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, ngeo->colname, 0, 31, 0, 0, "Set name of vertex color layer to use, default is active vertex color layer");
		uiButSetCompleteFunc(but, autocomplete_vcol, NULL);
	}

	return 40;
}

static int node_shader_buts_dynamic(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr) 
{ 
	if (block) { 
		uiBut *bt;
		// XXX SpaceNode *snode= curarea->spacedata.first;
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
				// UI_ThemeColor(TH_REDALERT);
				// XXX ui_rasterpos_safe(butr->xmin + xoff, butr->ymin + 5, snode->aspect);
				// XXX snode_drawstring(snode, "Error! Check console...", butr->xmax - butr->xmin);
				;
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



static void node_browse_image_cb(bContext *C, void *ntree_v, void *node_v)
{
	bNodeTree *ntree= ntree_v;
	bNode *node= node_v;
	
	nodeSetActive(ntree, node);
	
	if(node->menunr<1) return;
	if(node->menunr==32767) {	/* code for Load New */
		/// addqueue(curarea->win, UI_BUT_EVENT, B_NODE_LOADIMAGE); XXX
	}
	else {
		if(node->id) node->id->us--;
		node->id= BLI_findlink(&G.main->image, node->menunr-1);
		id_us_plus(node->id);

		BLI_strncpy(node->name, node->id->name+2, 21);

		NodeTagChanged(ntree, node); 
		BKE_image_signal((Image *)node->id, node->storage, IMA_SIGNAL_USER_NEW_IMAGE);
		// addqueue(curarea->win, UI_BUT_EVENT, B_NODE_EXEC); XXX
	}
	node->menunr= 0;
}

static void node_active_cb(bContext *C, void *ntree_v, void *node_v)
{
	nodeSetActive(ntree_v, node_v);
}
static void node_image_type_cb(bContext *C, void *node_v, void *unused)
{
	
	// allqueue(REDRAWNODE, 1);
}

static char *node_image_type_pup(void)
{
	char *str= MEM_mallocN(256, "image type pup");
	int a;
	
	str[0]= 0;
	
	a= sprintf(str, "Image Type %%t|");
	a+= sprintf(str+a, "  Image %%x%d %%i%d|", IMA_SRC_FILE, ICON_IMAGE_DATA);
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

static void image_layer_cb(bContext *C, void *ima_v, void *iuser_v)
{
	Scene *scene= CTX_data_scene(C);
	
	ntreeCompositForceHidden(scene->nodetree, scene);
	BKE_image_multilayer_index(ima_v, iuser_v);
	// allqueue(REDRAWNODE, 0);
}

static int node_composit_buts_image(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	ImageUser *iuser= node->storage;
	
	if(block) {
		uiBut *bt;
		short dy= (short)butr->ymax-19;
		char *strp;
		
		uiBlockBeginAlign(block);
		
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
		}
		else {
			/* name button + type */
			Image *ima= (Image *)node->id;
			short xmin= (short)butr->xmin, xmax= (short)butr->xmax;
			short width= xmax - xmin - 45;
			short icon= ICON_IMAGE_DATA;
			
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
				uiDefButI(block, NUM, B_NODE_EXEC, "Frs:",
						  xmin, dy, width, 19, 
						  &iuser->frames, 1.0, MAXFRAMEF, 0, 0, "Amount of images used in animation");
				uiDefButI(block, NUM, B_NODE_EXEC, "SFra:",
						  xmin+width, dy, width, 19, 
						  &iuser->sfra, 1.0, MAXFRAMEF, 0, 0, "Start frame of animation");
				dy-= 19;
				uiDefButI(block, NUM, B_NODE_EXEC, "Offs:",
						  xmin, dy, width, 19, 
						  &iuser->offset, -MAXFRAMEF, MAXFRAMEF, 0, 0, "Offsets the number of the frame to use in the animation");
				uiDefButS(block, TOG, B_NODE_EXEC, "Cycl",
						  xmin+width, dy, width-20, 19, 
						  &iuser->cycl, 0.0, 0.0, 0, 0, "Make animation go cyclic");
				uiDefIconButBitS(block, TOG, IMA_ANIM_ALWAYS, B_NODE_EXEC, ICON_AUTO,
						  xmax-20, dy, 20, 19, 
						  &iuser->flag, 0.0, 0.0, 0, 0, "Always refresh Image on frame changes");
			}
			if( ima->type==IMA_TYPE_MULTILAYER && ima->rr) {
				RenderLayer *rl= BLI_findlink(&ima->rr->layers, iuser->layer);
				if(rl) {
					width= (xmax-xmin);
					dy-= 19;
					strp= layer_menu(ima->rr);
					bt= uiDefButS(block, MENU, B_NODE_EXEC, strp,
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
			// addqueue(curarea->win, UI_BUT_EVENT, B_NODE_EXEC); XXX
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
static void set_render_layers_title(bContext *C, void *node_v, void *unused)
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
		sce= CTX_data_scene(C);
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

static void node_browse_scene_cb(bContext *C, void *ntree_v, void *node_v)
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
	node->id= &sce->id;
	id_us_plus(node->id);
	
	set_render_layers_title(C, node, NULL);
	nodeSetActive(ntree, node);

	// allqueue(REDRAWBUTSSHADING, 0);
	// allqueue(REDRAWNODE, 0);
	NodeTagChanged(ntree, node); 

	node->menunr= 0;
}


static int node_composit_buts_renderlayers(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block && node->id) {
		Scene *scene= (Scene *)node->id;
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
		strp= scene_layer_menu(node->id?(Scene *)node->id:scene);
		if(node->id)
			bt= uiDefIconTextButS(block, MENU, B_NODE_EXEC, ICON_RENDERLAYERS, strp, 
				  butr->xmin+20, butr->ymin, (butr->xmax-butr->xmin)-40, 19, 
				  &node->custom1, 0, 0, 0, 0, "Choose Render Layer");
		else
			bt= uiDefButS(block, MENU, B_NODE_EXEC, strp, 
				  butr->xmin+20, butr->ymin, (butr->xmax-butr->xmin)-40, 19, 
				  &node->custom1, 0, 0, 0, 0, "Choose Render Layer");
		uiButSetFunc(bt, set_render_layers_title, node, NULL);
		MEM_freeN(strp);
		
		/* re-render */
		/* uses custom2, not the best implementation of the world... but we need it to work now :) */
		bt= uiDefIconButS(block, TOG, B_NODE_EXEC, ICON_SCENE, 
				  butr->xmax-20, butr->ymin, 20, 19, 
				  &node->custom2, 0, 0, 0, 0, "Re-render this Layer");
		
	}
	return 19;
}

static void node_blur_relative_cb(bContext *C, void *node, void *poin2)
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
	// allqueue(REDRAWNODE, 0);
}
static void node_blur_update_sizex_cb(bContext *C, void *node, void *poin2)
{
	bNode *nodev= node;
	NodeBlurData *nbd= nodev->storage;

	nbd->sizex= (int)(nbd->percentx*nbd->image_in_width);
}
static void node_blur_update_sizey_cb(bContext *C, void *node, void *poin2)
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
		uiDefButS(block, MENU, B_NODE_EXEC,str,
				  butr->xmin, dy, dx*2, 19, 
				  &nbd->filtertype, 0, 0, 0, 0, "Set sampling filter for blur");
		dy-=19;
		if (nbd->filtertype != R_FILTER_FAST_GAUSS) { 
			uiDefButC(block, TOG, B_NODE_EXEC, "Bokeh",
					butr->xmin, dy, dx, 19, 
					&nbd->bokeh, 0, 0, 0, 0, "Uses circular filter, warning it's slow!");
			uiDefButC(block, TOG, B_NODE_EXEC, "Gamma",
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
			bt= uiDefButF(block, NUM, B_NODE_EXEC, "X:",
						 butr->xmin, dy, dx, 19, 
						 &nbd->percentx, 0.0f, 1.0f, 0, 0, "");
			uiButSetFunc(bt, node_blur_update_sizex_cb, node, NULL);
			bt= uiDefButF(block, NUM, B_NODE_EXEC, "Y:",
						 butr->xmin+dx, dy, dx, 19, 
						 &nbd->percenty, 0.0f, 1.0f, 0, 0, "");
			uiButSetFunc(bt, node_blur_update_sizey_cb, node, NULL);
		}
		else {
			uiDefButS(block, NUM, B_NODE_EXEC, "X:",
						 butr->xmin, dy, dx, 19, 
						 &nbd->sizex, 0, 256, 0, 0, "");
			uiDefButS(block, NUM, B_NODE_EXEC, "Y:",
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
		uiDefButS(block, NUM, B_NODE_EXEC, "Iterations:",
				butr->xmin, dy, dx, 19,
				&ndbd->iter, 1, 32, 10, 0, "Amount of iterations");
		uiDefButC(block, TOG, B_NODE_EXEC, "Wrap",
				butr->xmin, dy-= 19, dx, 19, 
				&ndbd->wrap, 0, 0, 0, 0, "Wrap blur");
		uiBlockEndAlign(block);

		dy-= 9;

		uiDefBut(block, LABEL, B_NOP, "Center", butr->xmin, dy-= 19, dx, 19, NULL, 0.0f, 0.0f, 0, 0, "");

		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_NODE_EXEC, "X:",
				butr->xmin, dy-= 19, halfdx, 19,
				&ndbd->center_x, 0.0f, 1.0f, 10, 0, "X center in percents");
		uiDefButF(block, NUM, B_NODE_EXEC, "Y:",
				butr->xmin+halfdx, dy, halfdx, 19,
				&ndbd->center_y, 0.0f, 1.0f, 10, 0, "Y center in percents");
		uiBlockEndAlign(block);

		dy-= 9;

		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_NODE_EXEC, "Distance:",
				butr->xmin, dy-= 19, dx, 19,
				&ndbd->distance, -1.0f, 1.0f, 10, 0, "Amount of which the image moves");
		uiDefButF(block, NUM, B_NODE_EXEC, "Angle:",
				butr->xmin, dy-= 19, dx, 19,
				&ndbd->angle, 0.0f, 360.0f, 1000, 0, "Angle in which the image will be moved");
		uiBlockEndAlign(block);

		dy-= 9;

		uiDefButF(block, NUM, B_NODE_EXEC, "Spin:",
				butr->xmin, dy-= 19, dx, 19,
				&ndbd->spin, -360.0f, 360.0f, 1000, 0, "Angle that is used to spin the image");

		dy-= 9;

		uiDefButF(block, NUM, B_NODE_EXEC, "Zoom:",
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
		uiDefButS(block, NUM, B_NODE_EXEC, "Iterations:",
				 butr->xmin, dy, dx, 19, 
				 &nbbd->iter, 1, 128, 0, 0, "Amount of iterations");
		dy-=19;
		uiDefButF(block, NUM, B_NODE_EXEC, "Color Sigma:",
				  butr->xmin, dy, dx, 19, 
				  &nbbd->sigma_color,0.01, 3, 10, 0, "Sigma value used to modify color");
		dy-=19;
		uiDefButF(block, NUM, B_NODE_EXEC, "Space Sigma:",
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
		uiDefButC(block, MENU, B_NODE_EXEC, mstr1,
		          butr->xmin, dy-19, dx, 19,
		          &nqd->bktype, 0, 0, 0, 0, "Bokeh type");
		if (nqd->bktype) { /* for some reason rotating a disk doesn't seem to work... ;) */
			uiDefButC(block, NUM, B_NODE_EXEC, "Rotate:",
			          butr->xmin, dy-38, dx, 19,
			          &nqd->rotation, 0, 90, 0, 0, "Bokeh shape rotation offset in degrees");
		}
		uiDefButC(block, TOG, B_NODE_EXEC, "Gamma Correct",
		          butr->xmin, dy-57, dx, 19,
		          &nqd->gamco, 0, 0, 0, 0, "Enable gamma correction before and after main process");
		if (nqd->no_zbuf==0) {
			// only needed for zbuffer input
			uiDefButF(block, NUM, B_NODE_EXEC, "fStop:",
			          butr->xmin, dy-76, dx, 19,
			          &nqd->fstop, 0.5, 128, 10, 0, "Amount of focal blur, 128=infinity=perfect focus, half the value doubles the blur radius");
		}
		uiDefButF(block, NUM, B_NODE_EXEC, "Maxblur:",
		          butr->xmin, dy-95, dx, 19,
		          &nqd->maxblur, 0, 10000, 1000, 0, "blur limit, maximum CoC radius, 0=no limit");
		uiDefButF(block, NUM, B_NODE_EXEC, "BThreshold:",
		          butr->xmin, dy-114, dx, 19,
		          &nqd->bthresh, 0, 100, 100, 0, "CoC radius threshold, prevents background bleed on in-focus midground, 0=off");
		uiDefButC(block, TOG, B_NODE_EXEC, "Preview",
		          butr->xmin, dy-142, dx, 19,
		          &nqd->preview, 0, 0, 0, 0, "Enable sampling mode, useful for preview when using low samplecounts");
		if (nqd->preview) {
			/* only visible when sampling mode enabled */
			uiDefButS(block, NUM, B_NODE_EXEC, "Samples:",
			          butr->xmin, dy-161, dx, 19,
			          &nqd->samples, 16, 256, 0, 0, "Number of samples (16=grainy, higher=less noise)");
		}
		uiDefButS(block, TOG, B_NODE_EXEC, "No zbuffer",
		          butr->xmin, dy-190, dx, 19,
		          &nqd->no_zbuf, 0, 0, 0, 0, "Enable when using an image as input instead of actual zbuffer (auto enabled if node not image based, eg. time node)");
		if (nqd->no_zbuf) {
			uiDefButF(block, NUM, B_NODE_EXEC, "Zscale:",
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
		uiDefButC(block, MENU, B_NODE_EXEC, mn1,
		          butr->xmin, dy, dx, 19,
		          &ndg->type, 0, 0, 0, 0, "Glow/Flare/Bloom type");
		uiDefButC(block, MENU, B_NODE_EXEC, mn2,
		          butr->xmin, dy-19, dx, 19,
		          &ndg->quality, 0, 0, 0, 0,
		          "Quality speed trade off, if not set to high quality, effect will be applied to low-res copy of source image");
		if (ndg->type != 1) {
			uiDefButC(block, NUM, B_NODE_EXEC, "Iterations:",
			          butr->xmin, dy-38, dx, 19,
			          &ndg->iter, 2, 5, 1, 0,
			          "higher values will generate longer/more streaks/ghosts");
			if (ndg->type != 0)
				uiDefButF(block, NUM, B_NODE_EXEC, "ColMod:",
				          butr->xmin, dy-57, dx, 19,
				          &ndg->colmod, 0, 1, 10, 0,
				          "Amount of Color Modulation, modulates colors of streaks and ghosts for a spectral dispersion effect");
		}
		uiDefButF(block, NUM, B_NODE_EXEC, "Mix:",
		          butr->xmin, dy-76, dx, 19,
		          &ndg->mix, -1, 1, 10, 0,
		          "Mix balance, -1 is original image only, 0 is exact 50/50 mix, 1 is processed image only");
		uiDefButF(block, NUM, B_NODE_EXEC, "Threshold:",
		          butr->xmin, dy-95, dx, 19,
		          &ndg->threshold, 0, 1000, 10, 0,
		          "Brightness threshold, the glarefilter will be applied only to pixels brighter than this value");
		if ((ndg->type == 2) || (ndg->type == 0))
		{
			if (ndg->type == 2) {
				uiDefButC(block, NUM, B_NODE_EXEC, "streaks:",
				          butr->xmin, dy-114, dx, 19,
				          &ndg->angle, 2, 16, 1000, 0,
				          "Total number of streaks");
				uiDefButC(block, NUM, B_NODE_EXEC, "AngOfs:",
				          butr->xmin, dy-133, dx, 19,
				          &ndg->angle_ofs, 0, 180, 1000, 0,
				          "Streak angle rotation offset in degrees");
			}
			uiDefButF(block, NUM, B_NODE_EXEC, "Fade:",
			          butr->xmin, dy-152, dx, 19,
			          &ndg->fade, 0.75, 1, 5, 0,
			          "Streak fade out factor");
		}
		if (ndg->type == 0)
			uiDefButC(block, TOG, B_NODE_EXEC, "Rot45",
			          butr->xmin, dy-114, dx, 19,
			          &ndg->angle, 0, 0, 0, 0,
			          "simple star filter, add 45 degree rotation offset");
		if ((ndg->type == 1) || (ndg->type > 3))	// PBGH and fog glow
			uiDefButC(block, NUM, B_NODE_EXEC, "Size:",
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
		uiDefButI(block, MENU, B_NODE_EXEC, mn,
		          butr->xmin, dy, dx, 19,
		          &ntm->type, 0, 0, 0, 0,
		          "Tone mapping type");
		if (ntm->type == 0) {
			uiDefButF(block, NUM, B_NODE_EXEC, "Key:",
			          butr->xmin, dy-19, dx, 19,
			          &ntm->key, 0, 1, 5, 0,
			          "The value the average luminance is mapped to");
			uiDefButF(block, NUM, B_NODE_EXEC, "Offset:",
			          butr->xmin, dy-38, dx, 19,
			          &ntm->offset, 0.001, 10, 5, 0,
			          "Tonemap offset, normally always 1, but can be used as an extra control to alter the brightness curve");
			uiDefButF(block, NUM, B_NODE_EXEC, "Gamma:",
			          butr->xmin, dy-57, dx, 19,
			          &ntm->gamma, 0.001, 3, 5, 0,
			          "Gamma factor, if not used, set to 1");
		}
		else {
			uiDefButF(block, NUM, B_NODE_EXEC, "Intensity:",
			          butr->xmin, dy-19, dx, 19,
			          &ntm->f, -8, 8, 10, 0, "if less than zero, darkens image, otherwise makes it brighter");
			uiDefButF(block, NUM, B_NODE_EXEC, "Contrast:",
			          butr->xmin, dy-38, dx, 19,
			          &ntm->m, 0, 1, 5, 0, "Set to 0 to use estimate from input image");
			uiDefButF(block, NUM, B_NODE_EXEC, "Adaptation:",
			          butr->xmin, dy-57, dx, 19,
			          &ntm->a, 0, 1, 5, 0, "if 0, global, if 1, based on pixel intensity");
			uiDefButF(block, NUM, B_NODE_EXEC, "ColCorrect:",
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
		uiDefButS(block, TOG, B_NODE_EXEC, "Projector",
		          butr->xmin, dy, dx, 19,
		          &nld->proj, 0, 0, 0, 0,
		          "Enable/disable projector mode, effect is applied in horizontal direction only");
		if (!nld->proj) {
			uiDefButS(block, TOG, B_NODE_EXEC, "Jitter",
			          butr->xmin, dy-19, dx/2, 19,
			          &nld->jit, 0, 0, 0, 0,
			          "Enable/disable jittering, faster, but also noisier");
			uiDefButS(block, TOG, B_NODE_EXEC, "Fit",
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
		PointerRNA ptr;
		short dy= butr->ymin;
		short dx= (butr->xmax-butr->xmin);
		
		RNA_pointer_create((ID *)ntree, &RNA_Node, node, &ptr);
		
		uiBlockBeginAlign(block);	
		uiDefButR(block, NUM, B_NODE_EXEC, NULL, 
			butr->xmin, dy+76, dx, 19, 
			&ptr, "samples", 0, 1, 256, 0, 0, NULL);
		uiDefButR(block, NUM, B_NODE_EXEC, NULL, 
			butr->xmin, dy+57, dx, 19, 
			&ptr, "min_speed", 0, 0, 1024, 0, 0, NULL);
		uiDefButR(block, NUM, B_NODE_EXEC, NULL, 
			butr->xmin, dy+38, dx, 19, 
			&ptr, "max_speed", 0, 0, 1024, 0, 0, NULL);
		uiDefButR(block, NUM, B_NODE_EXEC, "Blur", 
			butr->xmin, dy+19, dx, 19, 
			&ptr, "factor", 0, 0, 2, 10, 2, NULL);
		uiDefButR(block, TOG, B_NODE_EXEC, NULL, 
			butr->xmin, dy, dx, 19, 
			&ptr, "curved", 0, 0, 2, 10, 2, NULL);
		uiBlockEndAlign(block);
	}
	return 95;
}

static int node_composit_buts_filter(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiBut *bt;
		
		/* blend type */
		bt=uiDefButS(block, MENU, B_NODE_EXEC, "Soften %x0|Sharpen %x1|Laplace %x2|Sobel %x3|Prewitt %x4|Kirsch %x5|Shadow %x6",
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
		bt=uiDefButS(block, MENU, B_NODE_EXEC, "Flip X %x0|Flip Y %x1|Flip X & Y %x2",
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
		uiDefButS(block, TOG, B_NODE_EXEC, "Crop Image Size",
				  butr->xmin, dy, dx*2, elementheight, 
				  &node->custom1, 0, 0, 0, 0, "Crop the size of the input image.");

		dy-=elementheight;

		/* x1 */
		uiDefButS(block, NUM, B_NODE_EXEC, "X1:",
					 butr->xmin, dy, dx, elementheight,
					 &ntxy->x1, xymin, xymax, 0, 0, "");
		/* y1 */
		uiDefButS(block, NUM, B_NODE_EXEC, "Y1:",
					 butr->xmin+dx, dy, dx, elementheight,
					 &ntxy->y1, xymin, xymax, 0, 0, "");

		dy-=elementheight;

		/* x2 */
		uiDefButS(block, NUM, B_NODE_EXEC, "X2:",
					 butr->xmin, dy, dx, elementheight,
					 &ntxy->x2, xymin, xymax, 0, 0, "");
		/* y2 */
		uiDefButS(block, NUM, B_NODE_EXEC, "Y2:",
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
		
		uiDefButS(block, ROW, B_NODE_EXEC, "X",
				  butr->xmin, butr->ymin+19, (butr->xmax-butr->xmin)/2, 20, 
				  &node->custom2, 0.0, 0.0, 0, 0, "");
		uiDefButS(block, ROW, B_NODE_EXEC, "Y",
				  butr->xmin+(butr->xmax-butr->xmin)/2, butr->ymin+19, (butr->xmax-butr->xmin)/2, 20, 
				  &node->custom2, 0.0, 1.0, 0, 0, "");
				  
		uiDefButS(block, NUMSLI, B_NODE_EXEC, "Split %: ",
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
		uiDefButF(block, NUM, B_NODE_EXEC, "Offs:", xstart, dy, 2*dx, 19, texmap->loc, -1000.0f, 1000.0f, 10, 2, "");
		dy-= 19;
		uiDefButF(block, NUM, B_NODE_EXEC, "Size:", xstart, dy, 2*dx, 19, texmap->size, -1000.0f, 1000.0f, 10, 3, "");
		dy-= 23;
		uiBlockBeginAlign(block);
		uiDefButBitI(block, TOG, TEXMAP_CLIP_MIN, B_NODE_EXEC, "Min", xstart, dy, dx, 19, &texmap->flag, 0.0f, 0.0f, 0, 0, "");
		uiDefButF(block, NUM, B_NODE_EXEC, "", xstart+dx, dy, dx, 19, texmap->min, -1000.0f, 1000.0f, 10, 2, "");
		dy-= 19;
		uiDefButBitI(block, TOG, TEXMAP_CLIP_MAX, B_NODE_EXEC, "Max", xstart, dy, dx, 19, &texmap->flag, 0.0f, 0.0f, 0, 0, "");
		uiDefButF(block, NUM, B_NODE_EXEC, "", xstart+dx, dy, dx, 19, texmap->max, -1000.0f, 1000.0f, 10, 2, "");
	}
	return 80;
}

static int node_composit_buts_alphaover(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeTwoFloats *ntf= node->storage;
		
		/* alpha type */
		uiDefButS(block, TOG, B_NODE_EXEC, "ConvertPremul",
				  butr->xmin, butr->ymin+19, butr->xmax-butr->xmin, 19, 
				  &node->custom1, 0, 0, 0, 0, "");
		/* mix factor */
		uiDefButF(block, NUM, B_NODE_EXEC, "Premul: ",
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
		uiDefButF(block, NUMSLI, B_NODE_EXEC, "Hue: ",
				  butr->xmin, butr->ymin+40.0f, butr->xmax-butr->xmin, 20, 
				  &nhs->hue, 0.0f, 1.0f, 100, 0, "");
		uiDefButF(block, NUMSLI, B_NODE_EXEC, "Sat: ",
				  butr->xmin, butr->ymin+20.0f, butr->xmax-butr->xmin, 20, 
				  &nhs->sat, 0.0f, 2.0f, 100, 0, "");
		uiDefButF(block, NUMSLI, B_NODE_EXEC, "Val: ",
				  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, 
				  &nhs->val, 0.0f, 2.0f, 100, 0, "");
	}
	return 60;
}

static int node_composit_buts_dilateerode(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiDefButS(block, NUM, B_NODE_EXEC, "Distance:",
				  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, 
				  &node->custom2, -100, 100, 0, 0, "Distance to grow/shrink (number of iterations)");
	}
	return 20;
}

static int node_composit_buts_diff_matte(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeChroma *c= node->storage;
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Tolerance: ", 
			butr->xmin, butr->ymin+20, butr->xmax-butr->xmin, 20,
			&c->t1, 0.0f, 1.0f, 100, 0, "Color differences below this threshold are keyed.");
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Falloff: ", 
			butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20,
			&c->t2, 0.0f, 1.0f, 100, 0, "Color differences below this additional threshold are partially keyed.");
      uiBlockEndAlign(block);
	}
	return 40;
}

static int node_composit_buts_distance_matte(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeChroma *c= node->storage;
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Tolerance: ", 
			butr->xmin, butr->ymin+20, butr->xmax-butr->xmin, 20,
			&c->t1, 0.0f, 1.0f, 100, 0, "Color distances below this threshold are keyed.");
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "Falloff: ", 
			butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20,
			&c->t2, 0.0f, 1.0f, 100, 0, "Color distances below this additional threshold are partially keyed.");
       uiBlockEndAlign(block);
	}
	return 40;
}

static int node_composit_buts_color_spill(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		short dx= (butr->xmax-butr->xmin)/3;

		NodeChroma *c=node->storage;
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_NODE_EXEC, "Enhance: ", 
				butr->xmin, butr->ymin+20.0, butr->xmax-butr->xmin, 20,
				&c->t1, 0.0f, 0.5f, 100, 2, "Adjusts how much selected channel is affected by color spill algorithm");
		uiDefButS(block, ROW, B_NODE_EXEC, "R",
				butr->xmin,butr->ymin,dx,20,
				&node->custom1,1,1, 0, 0, "Red Spill Suppression");
		uiDefButS(block, ROW, B_NODE_EXEC, "G",
				butr->xmin+dx,butr->ymin,dx,20,
				&node->custom1,1,2, 0, 0, "Green Spill Suppression");
		uiDefButS(block, ROW, B_NODE_EXEC, "B",
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

		uiDefButF(block, NUMSLI, B_NODE_EXEC, "Acceptance ",
			butr->xmin, butr->ymin+60, butr->xmax-butr->xmin, 20,
			&c->t1, 1.0f, 80.0f, 100, 0, "Tolerance for colors to be considered a keying color");
		uiDefButF(block, NUMSLI, B_NODE_EXEC, "Cutoff ",
			butr->xmin, butr->ymin+40, butr->xmax-butr->xmin, 20,
			&c->t2, 0.0f, 30.0f, 100, 0, "Colors below this will be considered as exact matches for keying color");

		uiDefButF(block, NUMSLI, B_NODE_EXEC, "Lift ",
			butr->xmin, butr->ymin+20, dx, 20,
			&c->fsize, 0.0f, 1.0f, 100, 0, "Alpha Lift");
		uiDefButF(block, NUMSLI, B_NODE_EXEC, "Gain ",
			butr->xmin+dx, butr->ymin+20, dx, 20,
			&c->fstrength, 0.0f, 1.0f, 100, 0, "Alpha Gain");

		uiDefButF(block, NUMSLI, B_NODE_EXEC, "Shadow Adjust ",
			butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20,
			&c->t3, 0.0f, 1.0f, 100, 0, "Adjusts the brightness of any shadows captured");
		uiBlockEndAlign(block);

		if(c->t2 > c->t1)
			c->t2=c->t1;
	}
	return 80;
}

static int node_composit_buts_color_matte(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		NodeChroma *c= node->storage;
		uiBlockBeginAlign(block);

		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "H: ",
			butr->xmin, butr->ymin+40, butr->xmax-butr->xmin, 20,
			&c->t1, 0.0f, 0.25f, 100, 0, "Hue tolerance for colors to be considered a keying color");
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "S: ",
			butr->xmin, butr->ymin+20, butr->xmax-butr->xmin, 20,
			&c->t2, 0.0f, 1.0f, 100, 0, "Saturation Tolerance for the color");
		uiDefButF(block, NUMSLI, B_NODE_EXEC+node->nr, "V: ",
			butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20,
			&c->t3, 0.0f, 1.0f, 100, 0, "Value Tolerance for the color");

		uiBlockEndAlign(block);
	}
	return 60;
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
		uiDefButS(block, ROW,B_NODE_EXEC,"RGB",
			butr->xmin,butr->ymin+60,sx,20,&node->custom1,1,1, 0, 0, "RGB Color Space");
		uiDefButS(block, ROW,B_NODE_EXEC,"HSV",
			butr->xmin+sx,butr->ymin+60,sx,20,&node->custom1,1,2, 0, 0, "HSV Color Space");
		uiDefButS(block, ROW,B_NODE_EXEC,"YUV",
			butr->xmin+2*sx,butr->ymin+60,sx,20,&node->custom1,1,3, 0, 0, "YUV Color Space");
		uiDefButS(block, ROW,B_NODE_EXEC,"YCC",
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
		uiDefButS(block, ROW, B_NODE_EXEC, c1,
			butr->xmin,butr->ymin+40,cx,20,&node->custom2,1, 1, 0, 0, "Channel 1");
		uiDefButS(block, ROW, B_NODE_EXEC, c2,
			butr->xmin+cx,butr->ymin+40,cx,20,&node->custom2,1, 2, 0, 0, "Channel 2");
		uiDefButS(block, ROW, B_NODE_EXEC, c3,
			butr->xmin+cx+cx,butr->ymin+40,cx,20,&node->custom2, 1, 3, 0, 0, "Channel 3");
	
		/*tolerance sliders */
		uiDefButF(block, NUMSLI, B_NODE_EXEC, "High ", 
			butr->xmin, butr->ymin+20.0, butr->xmax-butr->xmin, 20,
			&c->t1, 0.0f, 1.0f, 100, 0, "Values higher than this setting are 100% opaque");
		uiDefButF(block, NUMSLI, B_NODE_EXEC, "Low ", 
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
		uiDefButF(block, NUMSLI, B_NODE_EXEC, "High ", 
			butr->xmin, butr->ymin+20.0, butr->xmax-butr->xmin, 20,
			&c->t1, 0.0f, 1.0f, 100, 0, "Values higher than this setting are 100% opaque");
		uiDefButF(block, NUMSLI, B_NODE_EXEC, "Low ", 
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
		uiDefButS(block, NUM, B_NODE_EXEC, "Alpha:",
				  butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, 
				  &node->custom1, 0, 100, 0, 0, "Conversion percentage of UV differences to Alpha");
	}
	return 20;
}

static int node_composit_buts_id_mask(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiDefButS(block, NUM, B_NODE_EXEC, "ID:",
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

static void node_set_image_cb(bContext *C, void *ntree_v, void *node_v)
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
		uiDefButI(block, NUM, B_NODE_EXEC, "SFra: ", 
				  x, y, w/2, 20, 
				  &nif->sfra, 1, MAXFRAMEF, 10, 0, "");
		uiDefButI(block, NUM, B_NODE_EXEC, "EFra: ", 
				  x+w/2, y, w/2, 20, 
				  &nif->efra, 1, MAXFRAMEF, 10, 0, "");
		
	}
	return 80;
}

static void node_scale_cb(bContext *C, void *node_v, void *unused_v)
{
	bNode *node= node_v;
	bNodeSocket *nsock;

	/* check the 2 inputs, and set them to reasonable values */
	for(nsock= node->inputs.first; nsock; nsock= nsock->next) {
		if(ELEM(node->custom1, CMP_SCALE_RELATIVE, CMP_SCALE_SCENEPERCENT))
			nsock->ns.vec[0]= 1.0;
		else {
			if(nsock->next==NULL)
				nsock->ns.vec[0]= (float)CTX_data_scene(C)->r.ysch;
			else
				nsock->ns.vec[0]= (float)CTX_data_scene(C)->r.xsch;
		}
	}	
}

static int node_composit_buts_scale(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		uiBut *bt= uiDefButS(block, MENU, B_NODE_EXEC, "Relative %x0|Absolute %x1|Scene Size % %x2|",
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
		uiDefButBitS(block, TOG, CMP_CHAN_RGB, B_NODE_EXEC, "RGB",
					 butr->xmin, butr->ymin, (butr->xmax-butr->xmin)/2, 20, 
					 &node->custom1, 0, 0, 0, 0, "");
		uiDefButBitS(block, TOG, CMP_CHAN_A, B_NODE_EXEC, "A",
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
		bt=uiDefButS(block, MENU, B_NODE_EXEC, "Key to Premul %x0|Premul to Key %x1",
					 butr->xmin, butr->ymin, butr->xmax-butr->xmin, 20, 
					 &node->custom1, 0, 0, 0, 0, "Conversion between premultiplied alpha and key alpha");
	}
	return 20;
}

static int node_composit_buts_view_levels(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		short sx= (butr->xmax-butr->xmin)/5;
	
		/*color space selectors*/
		uiBlockBeginAlign(block);
		uiDefButS(block, ROW,B_NODE_EXEC+node->nr,"C",
			butr->xmin,butr->ymin,sx,20,&node->custom1,1,1, 0, 0, "Combined RGB");
		uiDefButS(block, ROW,B_NODE_EXEC+node->nr,"R",
			butr->xmin+sx,butr->ymin,sx,20,&node->custom1,1,2, 0, 0, "Red Channel");
		uiDefButS(block, ROW,B_NODE_EXEC+node->nr,"G",
			butr->xmin+2*sx,butr->ymin,sx,20,&node->custom1,1,3, 0, 0, "Green Channel");
		uiDefButS(block, ROW,B_NODE_EXEC+node->nr,"B",
			butr->xmin+3*sx,butr->ymin,sx,20,&node->custom1,1,4, 0, 0, "Blue Channel");
      uiDefButS(block, ROW,B_NODE_EXEC+node->nr,"L",
			butr->xmin+4*sx,butr->ymin,sx,20,&node->custom1,1,5, 0, 0, "Luminenc Channel");
		uiBlockEndAlign(block);
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
		case CMP_NODE_DIST_MATTE:
			ntype->butfunc=node_composit_buts_distance_matte;
			break;
		case CMP_NODE_COLOR_SPILL:
			ntype->butfunc=node_composit_buts_color_spill;
			break;
		case CMP_NODE_CHROMA_MATTE:
			ntype->butfunc=node_composit_buts_chroma_matte;
			break;
		case CMP_NODE_COLOR_MATTE:
			ntype->butfunc=node_composit_buts_color_matte;
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
      case CMP_NODE_VIEW_LEVELS:
			ntype->butfunc=node_composit_buts_view_levels;
 			break;
		default:
			ntype->butfunc= NULL;
	}
}

/* ****************** BUTTON CALLBACKS FOR TEXTURE NODES ***************** */

static int node_texture_buts_bricks(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if(block) {
		short w = butr->xmax-butr->xmin;
		short ofw = 32;
		
		uiBlockBeginAlign(block);
		
		/* Offset */
		uiDefButF(
			block, NUM, B_NODE_EXEC, "Offset",
			butr->xmin, butr->ymin+20, w-ofw, 20,
			&node->custom3,
			0, 1, 0.25, 2,
			"Offset amount" );
		uiDefButS(
			block, NUM, B_NODE_EXEC, "",
			butr->xmin+w-ofw, butr->ymin+20, ofw, 20,
			&node->custom1,
			2, 99, 0, 0,
			"Offset every N rows" );
		
		/* Squash */
		uiDefButF(
			block, NUM, B_NODE_EXEC, "Squash",
			butr->xmin, butr->ymin+0, w-ofw, 20,
			&node->custom4,
			0, 99, 0.25, 2,
			"Stretch amount" );
		uiDefButS(
			block, NUM, B_NODE_EXEC, "",
			butr->xmin+w-ofw, butr->ymin+0, ofw, 20,
			&node->custom2,
			2, 99, 0, 0,
			"Stretch every N rows" );
		
		uiBlockEndAlign(block);
	}
	return 40;
}

/* Copied from buttons_shading.c -- needs unifying */
static char* noisebasis_menu()
{
	static char nbmenu[256];
	sprintf(nbmenu, "Noise Basis %%t|Blender Original %%x%d|Original Perlin %%x%d|Improved Perlin %%x%d|Voronoi F1 %%x%d|Voronoi F2 %%x%d|Voronoi F3 %%x%d|Voronoi F4 %%x%d|Voronoi F2-F1 %%x%d|Voronoi Crackle %%x%d|CellNoise %%x%d", TEX_BLENDER, TEX_STDPERLIN, TEX_NEWPERLIN, TEX_VORONOI_F1, TEX_VORONOI_F2, TEX_VORONOI_F3, TEX_VORONOI_F4, TEX_VORONOI_F2F1, TEX_VORONOI_CRACKLE, TEX_CELLNOISE);
	return nbmenu;
}

static int node_texture_buts_proc(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	Tex *tex = (Tex *)node->storage;
	short x,y,w,h;
	
	if( block ) {
		x = butr->xmin;
		y = butr->ymin;
		w = butr->xmax - x;
		h = butr->ymax - y;
	}
	else
		return 0;
	
	switch( tex->type ) {
		case TEX_BLEND:
			if( block ) {
				uiBlockBeginAlign( block );
				uiDefButS( block, MENU, B_NODE_EXEC,
					"Linear %x0|Quad %x1|Ease %x2|Diag %x3|Sphere %x4|Halo %x5|Radial %x6",
					x, y+20, w, 20, &tex->stype, 0, 1, 0, 0, "Blend Type" );
				uiDefButBitS(block, TOG, TEX_FLIPBLEND, B_NODE_EXEC, "Flip XY", x, y, w, 20,
					&tex->flag, 0, 0, 0, 0, "Flips the direction of the progression 90 degrees");
				uiBlockEndAlign( block );
			}
			return 40;
			
			
		case TEX_MARBLE:
			if( block ) {
				uiBlockBeginAlign(block);
			
				uiDefButS(block, ROW, B_NODE_EXEC, "Soft",       0*w/3+x, 40+y, w/3, 18, &tex->stype, 2.0, (float)TEX_SOFT, 0, 0, "Uses soft marble"); 
				uiDefButS(block, ROW, B_NODE_EXEC, "Sharp",      1*w/3+x, 40+y, w/3, 18, &tex->stype, 2.0, (float)TEX_SHARP, 0, 0, "Uses more clearly defined marble"); 
				uiDefButS(block, ROW, B_NODE_EXEC, "Sharper",    2*w/3+x, 40+y, w/3, 18, &tex->stype, 2.0, (float)TEX_SHARPER, 0, 0, "Uses very clearly defined marble"); 
				
				uiDefButS(block, ROW, B_NODE_EXEC, "Soft noise", 0*w/2+x, 20+y, w/2, 19, &tex->noisetype, 12.0, (float)TEX_NOISESOFT, 0, 0, "Generates soft noise");
				uiDefButS(block, ROW, B_NODE_EXEC, "Hard noise", 1*w/2+x, 20+y, w/2, 19, &tex->noisetype, 12.0, (float)TEX_NOISEPERL, 0, 0, "Generates hard noise");
				
				uiDefButS(block, ROW, B_NODE_EXEC, "Sin",        0*w/3+x,  0+y, w/3, 18, &tex->noisebasis2, 8.0, 0.0, 0, 0, "Uses a sine wave to produce bands."); 
				uiDefButS(block, ROW, B_NODE_EXEC, "Saw",        1*w/3+x,  0+y, w/3, 18, &tex->noisebasis2, 8.0, 1.0, 0, 0, "Uses a saw wave to produce bands"); 
				uiDefButS(block, ROW, B_NODE_EXEC, "Tri",        2*w/3+x,  0+y, w/3, 18, &tex->noisebasis2, 8.0, 2.0, 0, 0, "Uses a triangle wave to produce bands"); 
            
				uiBlockEndAlign(block);
			}
			return 60;
			
		case TEX_WOOD:
			if( block ) {
				uiDefButS(block, MENU, B_TEXPRV, noisebasis_menu(), x, y+64, w, 18, &tex->noisebasis, 0,0,0,0, "Sets the noise basis used for turbulence");
				
				uiBlockBeginAlign(block);
				uiDefButS(block, ROW, B_TEXPRV,             "Bands",     x,  40+y, w/2, 18, &tex->stype, 2.0, (float)TEX_BANDNOISE, 0, 0, "Uses standard noise"); 
				uiDefButS(block, ROW, B_TEXPRV,             "Rings", w/2+x,  40+y, w/2, 18, &tex->stype, 2.0, (float)TEX_RINGNOISE, 0, 0, "Lets Noise return RGB value"); 
				
				uiDefButS(block, ROW, B_NODE_EXEC, "Sin", 0*w/3+x,  20+y, w/3, 18, &tex->noisebasis2, 8.0, (float)TEX_SIN, 0, 0, "Uses a sine wave to produce bands."); 
				uiDefButS(block, ROW, B_NODE_EXEC, "Saw", 1*w/3+x,  20+y, w/3, 18, &tex->noisebasis2, 8.0, (float)TEX_SAW, 0, 0, "Uses a saw wave to produce bands"); 
				uiDefButS(block, ROW, B_NODE_EXEC, "Tri", 2*w/3+x,  20+y, w/3, 18, &tex->noisebasis2, 8.0, (float)TEX_TRI, 0, 0, "Uses a triangle wave to produce bands");
				
				uiDefButS(block, ROW, B_NODE_EXEC, "Soft noise", 0*w/2+x, 0+y, w/2, 19, &tex->noisetype, 12.0, (float)TEX_NOISESOFT, 0, 0, "Generates soft noise");
				uiDefButS(block, ROW, B_NODE_EXEC, "Hard noise", 1*w/2+x, 0+y, w/2, 19, &tex->noisetype, 12.0, (float)TEX_NOISEPERL, 0, 0, "Generates hard noise");
				uiBlockEndAlign(block);
			}
			return 80; 
			
		case TEX_CLOUDS:
			if( block ) {
				uiDefButS(block, MENU, B_TEXPRV, noisebasis_menu(), x, y+60, w, 18, &tex->noisebasis, 0,0,0,0, "Sets the noise basis used for turbulence");
				
				uiBlockBeginAlign(block);
				uiDefButS(block, ROW, B_TEXPRV, "B/W",       x, y+38, w/2, 18, &tex->stype, 2.0, (float)TEX_DEFAULT, 0, 0, "Uses standard noise"); 
				uiDefButS(block, ROW, B_TEXPRV, "Color", w/2+x, y+38, w/2, 18, &tex->stype, 2.0, (float)TEX_COLOR, 0, 0, "Lets Noise return RGB value"); 
				uiDefButS(block, ROW, B_TEXPRV, "Soft",      x, y+20, w/2, 18, &tex->noisetype, 12.0, (float)TEX_NOISESOFT, 0, 0, "Generates soft noise");
				uiDefButS(block, ROW, B_TEXPRV, "Hard",  w/2+x, y+20, w/2, 18, &tex->noisetype, 12.0, (float)TEX_NOISEPERL, 0, 0, "Generates hard noise");
				uiBlockEndAlign(block);
				
				uiDefButS(block, NUM, B_TEXPRV, "Depth:", x, y, w, 18, &tex->noisedepth, 0.0, 6.0, 0, 0, "Sets the depth of the cloud calculation");
			}
			return 80;
			
		case TEX_DISTNOISE:
			if( block ) {
				uiBlockBeginAlign(block);
				uiDefButS(block, MENU, B_TEXPRV, noisebasis_menu(), x, y+18, w, 18, &tex->noisebasis2, 0,0,0,0, "Sets the noise basis to distort");
				uiDefButS(block, MENU, B_TEXPRV, noisebasis_menu(), x, y,    w, 18, &tex->noisebasis,  0,0,0,0, "Sets the noise basis which does the distortion");
				uiBlockEndAlign(block);
			}
			return 36;
	}
	return 0;
}

static int node_texture_buts_image(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	char *strp;
	uiBut *bt;
	
	if( block ) {
		uiBlockBeginAlign(block);
		
		/* browse button */
		IMAnames_to_pupstring(&strp, NULL, "LOAD NEW %x32767", &(G.main->image), NULL, NULL);
		node->menunr= 0;
		bt= uiDefButS(block, MENU, B_NOP, strp, 
					  butr->xmin, butr->ymin, 19, 19, 
					  &node->menunr, 0, 0, 0, 0, "Browses existing choices");
		uiButSetFunc(bt, node_browse_image_cb, ntree, node);
		if(strp) MEM_freeN(strp);
		
		/* Add New button */
		if(node->id==NULL) {
			bt= uiDefBut(block, BUT, B_NODE_LOADIMAGE, "Load New",
						 butr->xmin+19, butr->ymin, (short)(butr->xmax-butr->xmin-19.0f), 19, 
						 NULL, 0.0, 0.0, 0, 0, "Add new Image");
			uiButSetFunc(bt, node_active_cb, ntree, node);
		}
		else {
			/* name button */
			short xmin= (short)butr->xmin, xmax= (short)butr->xmax;
			short width= xmax - xmin - 19;
			
			bt= uiDefBut(block, TEX, B_NOP, "IM:",
						 xmin+19, butr->ymin, width, 19, 
						 node->id->name+2, 0.0, 19.0, 0, 0, "Image name");
			uiButSetFunc(bt, node_ID_title_cb, node, NULL);
		}
	}
	return 20;
}

static int node_texture_buts_output(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
	if( block ) {
		uiBut *bt;
		short width;
		char *name = ((TexNodeOutput*)node->storage)->name;
		
		uiBlockBeginAlign(block);
		
		width = (short)(butr->xmax - butr->xmin);
		
		bt = uiDefBut(
			block, TEX, B_NOP,
			"Name:",
			butr->xmin, butr->ymin,
			width, 19, 
			name, 0, 31,
			0, 0, 
			"Name this output"
		);
		
		uiBlockEndAlign(block);
	}
	return 19;
}

/* only once called */
static void node_texture_set_butfunc(bNodeType *ntype)
{
	if( ntype->type >= TEX_NODE_PROC && ntype->type < TEX_NODE_PROC_MAX ) {
		ntype->butfunc = node_texture_buts_proc;
	}
	else switch(ntype->type) {
		
		case TEX_NODE_MATH:
			ntype->butfunc = node_buts_math;
			break;
		
		case TEX_NODE_MIX_RGB:
			ntype->butfunc = node_buts_mix_rgb;
			break;
			
		case TEX_NODE_VALTORGB:
			ntype->butfunc = node_buts_valtorgb;
			break;
			
		case TEX_NODE_CURVE_RGB:
			ntype->butfunc= node_buts_curvecol;
			break;
			
		case TEX_NODE_CURVE_TIME:
			ntype->butfunc = node_buts_time;
			break;
			
		case TEX_NODE_TEXTURE:
			ntype->butfunc = node_buts_texture;
			break;
			
		case TEX_NODE_BRICKS:
			ntype->butfunc = node_texture_buts_bricks;
			break;
			
		case TEX_NODE_IMAGE:
			ntype->butfunc = node_texture_buts_image;
			break;
			
		case TEX_NODE_OUTPUT:
			ntype->butfunc = node_texture_buts_output;
			break;
			
		default:
			ntype->butfunc= NULL;
	}
}

/* ******* init draw callbacks for all tree types, only called in usiblender.c, once ************* */

void ED_init_node_butfuncs(void)
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
	ntype = node_all_textures.first;
	while(ntype) {
		node_texture_set_butfunc(ntype);
		ntype= ntype->next;
	}
}

/* ************** Generic drawing ************** */

#if 0
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

#endif

void draw_nodespace_back_pix(ARegion *ar, SpaceNode *snode)
{
	
	if((snode->flag & SNODE_BACKDRAW) && snode->treetype==NTREE_COMPOSIT) {
		Image *ima= BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
		ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
		if(ibuf) {
			float x, y; 
			
			wmPushMatrix();
			
			/* somehow the offset has to be calculated inverse */
			
			glaDefine2DArea(&ar->winrct);
			/* ortho at pixel level curarea */
			wmOrtho2(-0.375, ar->winx-0.375, -0.375, ar->winy-0.375);
			
			x = (ar->winx-ibuf->x)/2 + snode->xof;
			y = (ar->winy-ibuf->y)/2 + snode->yof;
			
			if(ibuf->rect)
				glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
			else if(ibuf->channels==4)
				glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_FLOAT, ibuf->rect_float);
			
			wmPopMatrix();
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

/* if v2d not NULL, it clips and returns 0 if not visible */
int node_link_bezier_points(View2D *v2d, SpaceNode *snode, bNodeLink *link, float coord_array[][2], int resol)
{
	float dist, vec[4][2];
	
	/* in v0 and v3 we put begin/end points */
	if(link->fromsock) {
		vec[0][0]= link->fromsock->locx;
		vec[0][1]= link->fromsock->locy;
	}
	else {
		if(snode==NULL) return 0;
		vec[0][0]= snode->mx;
		vec[0][1]= snode->my;
	}
	if(link->tosock) {
		vec[3][0]= link->tosock->locx;
		vec[3][1]= link->tosock->locy;
	}
	else {
		if(snode==NULL) return 0;
		vec[3][0]= snode->mx;
		vec[3][1]= snode->my;
	}
	
	dist= 0.5f*ABS(vec[0][0] - vec[3][0]);
	
	/* check direction later, for top sockets */
	vec[1][0]= vec[0][0]+dist;
	vec[1][1]= vec[0][1];
	
	vec[2][0]= vec[3][0]-dist;
	vec[2][1]= vec[3][1];
	
	if(v2d && MIN4(vec[0][0], vec[1][0], vec[2][0], vec[3][0]) > v2d->cur.xmax); /* clipped */	
	else if (v2d && MAX4(vec[0][0], vec[1][0], vec[2][0], vec[3][0]) < v2d->cur.xmin); /* clipped */
	else {
		
		/* always do all three, to prevent data hanging around */
		forward_diff_bezier(vec[0][0], vec[1][0], vec[2][0], vec[3][0], coord_array[0], resol, sizeof(float)*2);
		forward_diff_bezier(vec[0][1], vec[1][1], vec[2][1], vec[3][1], coord_array[0]+1, resol, sizeof(float)*2);
		
		return 1;
	}
	return 0;
}

#define LINK_RESOL	24
void node_draw_link_bezier(View2D *v2d, SpaceNode *snode, bNodeLink *link, int th_col1, int th_col2, int do_shaded)
{
	float coord_array[LINK_RESOL+1][2];
	
	if(node_link_bezier_points(v2d, snode, link, coord_array, LINK_RESOL)) {
		float dist, spline_step = 0.0f;
		int i;
		
		/* we can reuse the dist variable here to increment the GL curve eval amount*/
		dist = 1.0f/(float)LINK_RESOL;
		
		glBegin(GL_LINE_STRIP);
		for(i=0; i<=LINK_RESOL; i++) {
			if(do_shaded) {
				UI_ThemeColorBlend(th_col1, th_col2, spline_step);
				spline_step += dist;
			}				
			glVertex2fv(coord_array[i]);
		}
		glEnd();
	}
}

/* note; this is used for fake links in groups too */
void node_draw_link(View2D *v2d, SpaceNode *snode, bNodeLink *link)
{
	int do_shaded= 1, th_col1= TH_WIRE, th_col2= TH_WIRE;
	
	if(link->fromnode==NULL && link->tonode==NULL)
		return;
	
	if(link->fromnode==NULL || link->tonode==NULL) {
		UI_ThemeColor(TH_WIRE);
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
			UI_ThemeColorBlend(TH_BACK, TH_WIRE, 0.25f);
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
				UI_ThemeColor(TH_REDALERT);
				do_shaded= 0;
			}
		}
	}
	
	node_draw_link_bezier(v2d, snode, link, th_col1, th_col2, do_shaded);
}


