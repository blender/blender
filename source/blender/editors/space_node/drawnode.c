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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): David Millan Escriva, Juho Vepsäläinen, Bob Holcomb, Thomas Dinges
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "DNA_node_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"

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


#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "node_intern.h"


/* ****************** BUTTON CALLBACKS FOR ALL TREES ***************** */

void node_buts_group(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiTemplateIDBrowse(layout, C, ptr, "nodetree", NULL, NULL, "");
}

static void node_buts_value(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	PointerRNA sockptr;
	PropertyRNA *prop;
	
	/* first socket stores value */
	prop = RNA_struct_find_property(ptr, "outputs");
	RNA_property_collection_lookup_int(ptr, prop, 0, &sockptr);
	
	uiItemR(layout, "", 0, &sockptr, "default_value", 0);
}

static void node_buts_rgb(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	PointerRNA sockptr;
	PropertyRNA *prop;
	
	/* first socket stores value */
	prop = RNA_struct_find_property(ptr, "outputs");
	RNA_property_collection_lookup_int(ptr, prop, 0, &sockptr);
	
	col = uiLayoutColumn(layout, 0);
	uiTemplateColorWheel(col, &sockptr, "default_value", 1, 0);
	uiItemR(col, "", 0, &sockptr, "default_value", 0);
}

static void node_buts_mix_rgb(uiLayout *layout, bContext *C, PointerRNA *ptr)
{	
	uiLayout *row;

	bNodeTree *ntree= (bNodeTree*)ptr->id.data;

	row= uiLayoutRow(layout, 1);
	uiItemR(row, "", 0, ptr, "blend_type", 0);
	if(ntree->type == NTREE_COMPOSIT)
		uiItemR(row, "", ICON_IMAGE_RGB_ALPHA, ptr, "alpha", 0);
}

static void node_buts_time(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *row;
#if 0
	/* XXX no context access here .. */
	bNode *node= ptr->data;
	CurveMapping *cumap= node->storage;
	
	if(cumap) {
		cumap->flag |= CUMA_DRAW_CFRA;
		if(node->custom1<node->custom2)
			cumap->sample[0]= (float)(CFRA - node->custom1)/(float)(node->custom2-node->custom1);
	}
#endif

	uiTemplateCurveMapping(layout, ptr, "curve", 's', 0, 0);

	row= uiLayoutRow(layout, 1);
	uiItemR(row, "Sta", 0, ptr, "start", 0);
	uiItemR(row, "End", 0, ptr, "end", 0);
}

static void node_buts_colorramp(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiTemplateColorRamp(layout, ptr, "color_ramp", 0);
}

static void node_buts_curvevec(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiTemplateCurveMapping(layout, ptr, "mapping", 'v', 0, 0);
}

static float *_sample_col= NULL;	// bad bad, 2.5 will do better?
void node_curvemap_sample(float *col)
{
	_sample_col= col;
}

static void node_buts_curvecol(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node= ptr->data;
	CurveMapping *cumap= node->storage;

	if(_sample_col) {
		cumap->flag |= CUMA_DRAW_SAMPLE;
		VECCOPY(cumap->sample, _sample_col);
	}
	else 
		cumap->flag &= ~CUMA_DRAW_SAMPLE;

	uiTemplateCurveMapping(layout, ptr, "mapping", 'c', 0, 0);
}

static void node_buts_normal(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiBlock *block= uiLayoutAbsoluteBlock(layout);
	bNode *node= ptr->data;
	rctf *butr= &node->butr;
	bNodeSocket *sock= node->outputs.first;		/* first socket stores normal */
	
	uiDefButF(block, BUT_NORMAL, B_NODE_EXEC, "", 
			  (short)butr->xmin, (short)butr->xmin, butr->xmax-butr->xmin, butr->xmax-butr->xmin, 
			  sock->ns.vec, 0.0f, 1.0f, 0, 0, "");
}
#if 0 // not used in 2.5x yet
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
#endif
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

static void node_buts_texture(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node= ptr->data;

	short multi = (
		node->id &&
		((Tex*)node->id)->use_nodes &&
		(node->type != CMP_NODE_TEXTURE) &&
		(node->type != TEX_NODE_TEXTURE)
	);
	
	uiItemR(layout, "", 0, ptr, "texture", 0);
	
	if(multi) {
		/* Number Drawing not optimal here, better have a list*/
		uiItemR(layout, "", 0, ptr, "node_output", 0);
	}
}

static void node_buts_math(uiLayout *layout, bContext *C, PointerRNA *ptr)
{ 
	uiItemR(layout, "", 0, ptr, "operation", 0);
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

static void node_shader_buts_material(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node= ptr->data;
	uiLayout *col;
	
	uiTemplateID(layout, C, ptr, "material", "MATERIAL_OT_new", NULL, NULL);
	
	if(!node->id) return;
	
	col= uiLayoutColumn(layout, 0);
	uiItemR(col, NULL, 0, ptr, "diffuse", 0);
	uiItemR(col, NULL, 0, ptr, "specular", 0);
	uiItemR(col, NULL, 0, ptr, "invert_normal", 0);
}

static void node_shader_buts_mapping(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *row;
	
	uiItemL(layout, "Location:", 0);
	row= uiLayoutRow(layout, 1);
	uiItemR(row, "", 0, ptr, "location", 0);
	
	uiItemL(layout, "Rotation:", 0);
	row= uiLayoutRow(layout, 1);
	uiItemR(row, "", 0, ptr, "rotation", 0);
	
	uiItemL(layout, "Scale:", 0);
	row= uiLayoutRow(layout, 1);
	uiItemR(row, "", 0, ptr, "scale", 0);
	
	row= uiLayoutRow(layout, 1);
	uiItemR(row, "Min", 0, ptr, "clamp_minimum", 0);
	uiItemR(row, "", 0, ptr, "minimum", 0);
	
	row= uiLayoutRow(layout, 1);
	uiItemR(row, "Max", 0, ptr, "clamp_maximum", 0);
	uiItemR(row, "", 0, ptr, "maximum", 0);
	
}

static void node_shader_buts_vect_math(uiLayout *layout, bContext *C, PointerRNA *ptr)
{ 
	uiItemR(layout, "", 0, ptr, "operation", 0);
}

static void node_shader_buts_geometry(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 0);
	uiItemR(col, "UV", 0, ptr, "uv_layer", 0);
	uiItemR(col, "VCol", 0, ptr, "color_layer", 0);
}

static void node_shader_buts_dynamic(uiLayout *layout, bContext *C, PointerRNA *ptr)
{ 
	uiBlock *block= uiLayoutAbsoluteBlock(layout);
	bNode *node= ptr->data;
	bNodeTree *ntree= ptr->id.data;
	rctf *butr= &node->butr;
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

/* only once called */
static void node_shader_set_butfunc(bNodeType *ntype)
{
	switch(ntype->type) {
		/* case NODE_GROUP:	 note, typeinfo for group is generated... see "XXX ugly hack" */

		case SH_NODE_MATERIAL:
		case SH_NODE_MATERIAL_EXT:
			ntype->uifunc= node_shader_buts_material;
			break;
		case SH_NODE_TEXTURE:
			ntype->uifunc= node_buts_texture;
			break;
		case SH_NODE_NORMAL:
			ntype->uifunc= node_buts_normal;
			break;
		case SH_NODE_CURVE_VEC:
			ntype->uifunc= node_buts_curvevec;
			break;
		case SH_NODE_CURVE_RGB:
			ntype->uifunc= node_buts_curvecol;
			break;
		case SH_NODE_MAPPING:
			ntype->uifunc= node_shader_buts_mapping;
			break;
		case SH_NODE_VALUE:
			ntype->uifunc= node_buts_value;
			break;
		case SH_NODE_RGB:
			ntype->uifunc= node_buts_rgb;
			break;
		case SH_NODE_MIX_RGB:
			ntype->uifunc= node_buts_mix_rgb;
			break;
		case SH_NODE_VALTORGB:
			ntype->uifunc= node_buts_colorramp;
			break;
		case SH_NODE_MATH: 
			ntype->uifunc= node_buts_math;
			break; 
		case SH_NODE_VECT_MATH: 
			ntype->uifunc= node_shader_buts_vect_math;
			break; 
		case SH_NODE_GEOMETRY:
			ntype->uifunc= node_shader_buts_geometry;
			break;
		case NODE_DYNAMIC:
			ntype->uifunc= node_shader_buts_dynamic;
			break;
		default:
			ntype->uifunc= NULL;
	}
}

/* ****************** BUTTON CALLBACKS FOR COMPOSITE NODES ***************** */

static void node_composit_buts_image(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	bNode *node= ptr->data;
	PointerRNA imaptr;
	PropertyRNA *prop;
	
	uiTemplateID(layout, C, ptr, "image", NULL, "IMAGE_OT_open", NULL);
	
	if(!node->id) return;
	
	prop = RNA_struct_find_property(ptr, "image");
	if (!prop || RNA_property_type(prop) != PROP_POINTER) return;
	imaptr= RNA_property_pointer_get(ptr, prop);
	
	col= uiLayoutColumn(layout, 0);
	
	uiItemR(col, NULL, 0, &imaptr, "source", 0);
	
	if (ELEM(RNA_enum_get(&imaptr, "source"), IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
		col= uiLayoutColumn(layout, 1);
		uiItemR(col, NULL, 0, ptr, "frames", 0);
		uiItemR(col, NULL, 0, ptr, "start", 0);
		uiItemR(col, NULL, 0, ptr, "offset", 0);
		uiItemR(col, NULL, 0, ptr, "cyclic", 0);
		uiItemR(col, NULL, 0, ptr, "auto_refresh", UI_ITEM_R_ICON_ONLY);
	}

	col= uiLayoutColumn(layout, 0);
	
	if (RNA_enum_get(&imaptr, "type")== IMA_TYPE_MULTILAYER)
		uiItemR(col, NULL, 0, ptr, "layer", 0);
}

static void node_composit_buts_renderlayers(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node= ptr->data;
	uiLayout *col, *row;
	PointerRNA op_ptr;
	PointerRNA scn_ptr;
	PropertyRNA *prop;
	const char *layer_name;
	char scene_name[19];
	
	uiTemplateID(layout, C, ptr, "scene", NULL, NULL, NULL);
	
	if(!node->id) return;

	col= uiLayoutColumn(layout, 0);
	row = uiLayoutRow(col, 0);
	uiItemR(row, "", 0, ptr, "layer", 0);
	
	prop = RNA_struct_find_property(ptr, "layer");
	if (!(RNA_property_enum_identifier(C, ptr, prop, RNA_property_enum_get(ptr, prop), &layer_name)))
		return;
	
	scn_ptr = RNA_pointer_get(ptr, "scene");
	RNA_string_get(&scn_ptr, "name", scene_name);
	
	WM_operator_properties_create(&op_ptr, "RENDER_OT_render");
	RNA_string_set(&op_ptr, "layer", layer_name);
	RNA_string_set(&op_ptr, "scene", scene_name);
	uiItemFullO(row, "", ICON_RENDER_STILL, "RENDER_OT_render", op_ptr.data, WM_OP_INVOKE_DEFAULT, 0);

}


static void node_composit_buts_blur(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 0);
	
	uiItemR(col, "", 0, ptr, "filter_type", 0);
	if (RNA_enum_get(ptr, "filter_type")!= R_FILTER_FAST_GAUSS) {
		uiItemR(col, NULL, 0, ptr, "bokeh", 0);
		uiItemR(col, NULL, 0, ptr, "gamma", 0);
	}
	
	uiItemR(col, NULL, 0, ptr, "relative", 0);
	col= uiLayoutColumn(layout, 1);
	if (RNA_boolean_get(ptr, "relative")) {
		uiItemR(col, "X", 0, ptr, "factor_x", 0);
		uiItemR(col, "Y", 0, ptr, "factor_y", 0);
	}
	else {
		uiItemR(col, "X", 0, ptr, "sizex", 0);
		uiItemR(col, "Y", 0, ptr, "sizey", 0);
	}
}

static void node_composit_buts_dblur(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	
	uiItemR(layout, NULL, 0, ptr, "iterations", 0);
	uiItemR(layout, NULL, 0, ptr, "wrap", 0);
	
	col= uiLayoutColumn(layout, 1);
	uiItemL(col, "Center:", 0);
	uiItemR(col, "X", 0, ptr, "center_x", 0);
	uiItemR(col, "Y", 0, ptr, "center_y", 0);
	
	uiItemS(layout);
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, NULL, 0, ptr, "distance", 0);
	uiItemR(col, NULL, 0, ptr, "angle", 0);
	
	uiItemS(layout);
	
	uiItemR(layout, NULL, 0, ptr, "spin", 0);
	uiItemR(layout, NULL, 0, ptr, "zoom", 0);
}

static void node_composit_buts_bilateralblur(uiLayout *layout, bContext *C, PointerRNA *ptr)
{	
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, NULL, 0, ptr, "iterations", 0);
	uiItemR(col, NULL, 0, ptr, "sigma_color", 0);
	uiItemR(col, NULL, 0, ptr, "sigma_space", 0);
}

static void node_composit_buts_defocus(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *sub, *col;
	
	col= uiLayoutColumn(layout, 0);
	uiItemL(col, "Bokeh Type:", 0);
	uiItemR(col, "", 0, ptr, "bokeh", 0);
	uiItemR(col, NULL, 0, ptr, "angle", 0);

	uiItemR(layout, NULL, 0, ptr, "gamma_correction", 0);

	col = uiLayoutColumn(layout, 0);
	uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_zbuffer")==1);
	uiItemR(col, NULL, 0, ptr, "f_stop", 0);

	uiItemR(layout, NULL, 0, ptr, "max_blur", 0);
	uiItemR(layout, NULL, 0, ptr, "threshold", 0);

	col = uiLayoutColumn(layout, 0);
	uiItemR(col, NULL, 0, ptr, "preview", 0);
	sub = uiLayoutColumn(col, 0);
	uiLayoutSetActive(sub, RNA_boolean_get(ptr, "preview"));
	uiItemR(sub, NULL, 0, ptr, "samples", 0);
	
	col = uiLayoutColumn(layout, 0);
	uiItemR(col, NULL, 0, ptr, "use_zbuffer", 0);
	sub = uiLayoutColumn(col, 0);
	uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_zbuffer")==0);
	uiItemR(sub, NULL, 0, ptr, "z_scale", 0);
}

/* qdn: glare node */
static void node_composit_buts_glare(uiLayout *layout, bContext *C, PointerRNA *ptr)
{	
	uiItemR(layout, "", 0, ptr, "glare_type", 0);
	uiItemR(layout, "", 0, ptr, "quality", 0);

	if (RNA_enum_get(ptr, "glare_type")!= 1) {
		uiItemR(layout, NULL, 0, ptr, "iterations", 0);
	
		if (RNA_enum_get(ptr, "glare_type")!= 0) 
			uiItemR(layout, NULL, 0, ptr, "color_modulation", UI_ITEM_R_SLIDER);
	}
	
	uiItemR(layout, NULL, 0, ptr, "mix", 0);		
	uiItemR(layout, NULL, 0, ptr, "threshold", 0);

	if (RNA_enum_get(ptr, "glare_type")== 2) {
		uiItemR(layout, NULL, 0, ptr, "streaks", 0);		
		uiItemR(layout, NULL, 0, ptr, "angle_offset", 0);
	}
	if (RNA_enum_get(ptr, "glare_type")== 0 || RNA_enum_get(ptr, "glare_type")== 2) {
		uiItemR(layout, NULL, 0, ptr, "fade", UI_ITEM_R_SLIDER);
		
		if (RNA_enum_get(ptr, "glare_type")== 0) 
			uiItemR(layout, NULL, 0, ptr, "rotate_45", 0);
	}
	if (RNA_enum_get(ptr, "glare_type")== 1) {
		uiItemR(layout, NULL, 0, ptr, "size", 0);
	}
}

static void node_composit_buts_tonemap(uiLayout *layout, bContext *C, PointerRNA *ptr)
{	
	uiLayout *col;

	col = uiLayoutColumn(layout, 0);
	uiItemR(col, "", 0, ptr, "tonemap_type", 0);
	if (RNA_enum_get(ptr, "tonemap_type")== 0) {
		uiItemR(col, NULL, 0, ptr, "key", UI_ITEM_R_SLIDER);
		uiItemR(col, NULL, 0, ptr, "offset", 0);
		uiItemR(col, NULL, 0, ptr, "gamma", 0);
	}
	else {
		uiItemR(col, NULL, 0, ptr, "intensity", 0);
		uiItemR(col, NULL, 0, ptr, "contrast", UI_ITEM_R_SLIDER);
		uiItemR(col, NULL, 0, ptr, "adaptation", UI_ITEM_R_SLIDER);
		uiItemR(col, NULL, 0, ptr, "correction", UI_ITEM_R_SLIDER);
	}
}

static void node_composit_buts_lensdist(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;

	col= uiLayoutColumn(layout, 0);
	uiItemR(col, NULL, 0, ptr, "projector", 0);

	col = uiLayoutColumn(col, 0);
	uiLayoutSetActive(col, RNA_boolean_get(ptr, "projector")==0);
	uiItemR(col, NULL, 0, ptr, "jitter", 0);
	uiItemR(col, NULL, 0, ptr, "fit", 0);
}

static void node_composit_buts_vecblur(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 0);
	uiItemR(col, NULL, 0, ptr, "samples", 0);
	uiItemR(col, "Blur", 0, ptr, "factor", 0);
	
	col= uiLayoutColumn(layout, 1);
	uiItemL(col, "Speed:", 0);
	uiItemR(col, "Min", 0, ptr, "min_speed", 0);
	uiItemR(col, "Max", 0, ptr, "max_speed", 0);

	uiItemR(layout, NULL, 0, ptr, "curved", 0);
}

static void node_composit_buts_filter(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiItemR(layout, "", 0, ptr, "filter_type", 0);
}

static void node_composit_buts_flip(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiItemR(layout, "", 0, ptr, "axis", 0);
}

static void node_composit_buts_crop(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	
	uiItemR(layout, NULL, 0, ptr, "crop_size", 0);
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, "Left", 0, ptr, "x1", 0);
	uiItemR(col, "Right", 0, ptr, "x2", 0);
	uiItemR(col, "Up", 0, ptr, "y1", 0);
	uiItemR(col, "Down", 0, ptr, "y2", 0);
}

static void node_composit_buts_splitviewer(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *row, *col;
	
	col= uiLayoutColumn(layout, 0);
	row= uiLayoutRow(col, 0);
	uiItemR(row, NULL, 0, ptr, "axis", UI_ITEM_R_EXPAND);
	uiItemR(col, NULL, 0, ptr, "factor", 0);
}

static void node_composit_buts_map_value(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *sub, *col;
	
	col =uiLayoutColumn(layout, 1);
	uiItemR(col, NULL, 0, ptr, "offset", 0);
	uiItemR(col, NULL, 0, ptr, "size", 0);
	
	col =uiLayoutColumn(layout, 1);
	uiItemR(col, NULL, 0, ptr, "use_min", 0);
	sub =uiLayoutColumn(col, 0);
	uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_min"));
	uiItemR(sub, "", 0, ptr, "min", 0);
	
	col =uiLayoutColumn(layout, 1);
	uiItemR(col, NULL, 0, ptr, "use_max", 0);
	sub =uiLayoutColumn(col, 0);
	uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_max"));
	uiItemR(sub, "", 0, ptr, "max", 0);
}

static void node_composit_buts_alphaover(uiLayout *layout, bContext *C, PointerRNA *ptr)
{	
	uiLayout *col;
	
	col =uiLayoutColumn(layout, 1);
	uiItemR(col, NULL, 0, ptr, "convert_premul", 0);
	uiItemR(col, NULL, 0, ptr, "premul", 0);
}

static void node_composit_buts_hue_sat(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	
	col =uiLayoutColumn(layout, 0);
	uiItemR(col, NULL, 0, ptr, "hue", UI_ITEM_R_SLIDER);
	uiItemR(col, NULL, 0, ptr, "sat", UI_ITEM_R_SLIDER);
	uiItemR(col, NULL, 0, ptr, "val", UI_ITEM_R_SLIDER);
}

static void node_composit_buts_dilateerode(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiItemR(layout, NULL, 0, ptr, "distance", 0);
}

static void node_composit_buts_diff_matte(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	
	col =uiLayoutColumn(layout, 1);
	uiItemR(col, NULL, 0, ptr, "tolerance", UI_ITEM_R_SLIDER);
	uiItemR(col, NULL, 0, ptr, "falloff", UI_ITEM_R_SLIDER);
}

static void node_composit_buts_distance_matte(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	
	col =uiLayoutColumn(layout, 1);
	uiItemR(col, NULL, 0, ptr, "tolerance", UI_ITEM_R_SLIDER);
	uiItemR(col, NULL, 0, ptr, "falloff", UI_ITEM_R_SLIDER);
}

static void node_composit_buts_color_spill(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *row, *col;
	
   uiItemL(layout, "Despill Channel:", 0);
   row =uiLayoutRow(layout,0);
	uiItemR(row, NULL, 0, ptr, "channel", UI_ITEM_R_EXPAND);

   col= uiLayoutColumn(layout, 0);
   uiItemR(col, NULL, 0, ptr, "algorithm", 0);

   if(RNA_enum_get(ptr, "algorithm")==0) {
	  uiItemL(col, "Limiting Channel:", 0);
	  row=uiLayoutRow(col,0);
	  uiItemR(row, NULL, 0, ptr, "limit_channel", UI_ITEM_R_EXPAND);
   }

   uiItemR(col, NULL, 0, ptr, "ratio", UI_ITEM_R_SLIDER);
   uiItemR(col, NULL, 0, ptr, "unspill", 0);   
   if (RNA_enum_get(ptr, "unspill")== 1) {
	  uiItemR(col, NULL, 0, ptr, "unspill_red", UI_ITEM_R_SLIDER);
	  uiItemR(col, NULL, 0, ptr, "unspill_green", UI_ITEM_R_SLIDER);
	  uiItemR(col, NULL, 0, ptr, "unspill_blue", UI_ITEM_R_SLIDER);
   }
}

static void node_composit_buts_chroma_matte(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 0);
	uiItemR(col, NULL, 0, ptr, "acceptance", 0);
	uiItemR(col, NULL, 0, ptr, "cutoff", 0);
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, NULL, 0, ptr, "lift", UI_ITEM_R_SLIDER);
	uiItemR(col, NULL, 0, ptr, "gain", UI_ITEM_R_SLIDER);
	uiItemR(col, NULL, 0, ptr, "shadow_adjust", UI_ITEM_R_SLIDER);
}

static void node_composit_buts_color_matte(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, NULL, 0, ptr, "h", UI_ITEM_R_SLIDER);
	uiItemR(col, NULL, 0, ptr, "s", UI_ITEM_R_SLIDER);
	uiItemR(col, NULL, 0, ptr, "v", UI_ITEM_R_SLIDER);
}

static void node_composit_buts_channel_matte(uiLayout *layout, bContext *C, PointerRNA *ptr)
{	
	uiLayout *col, *row;

   uiItemL(layout, "Color Space:", 0);
	row= uiLayoutRow(layout, 0);
	uiItemR(row, NULL, 0, ptr, "color_space", UI_ITEM_R_EXPAND);

   col=uiLayoutColumn(layout, 0);  
   uiItemL(col, "Key Channel:", 0);
	row= uiLayoutRow(col, 0);
	uiItemR(row, NULL, 0, ptr, "channel", UI_ITEM_R_EXPAND);

	col =uiLayoutColumn(layout, 0);

   uiItemR(col, NULL, 0, ptr, "algorithm", 0);
   if(RNA_enum_get(ptr, "algorithm")==0) {
	  uiItemL(col, "Limiting Channel:", 0);
	  row=uiLayoutRow(col,0);
	  uiItemR(row, NULL, 0, ptr, "limit_channel", UI_ITEM_R_EXPAND);
   }
   
	uiItemR(col, NULL, 0, ptr, "high", UI_ITEM_R_SLIDER);
	uiItemR(col, NULL, 0, ptr, "low", UI_ITEM_R_SLIDER);
}

static void node_composit_buts_luma_matte(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, NULL, 0, ptr, "high", UI_ITEM_R_SLIDER);
	uiItemR(col, NULL, 0, ptr, "low", UI_ITEM_R_SLIDER);
}

static void node_composit_buts_map_uv(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiItemR(layout, NULL, 0, ptr, "alpha", 0);
}

static void node_composit_buts_id_mask(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiItemR(layout, NULL, 0, ptr, "index", 0);
}

static void node_composit_buts_file_output(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col, *row;

	col= uiLayoutColumn(layout, 0);
	uiItemR(col, "", 0, ptr, "filename", 0);
	uiItemR(col, "", 0, ptr, "image_type", 0);
	
	row= uiLayoutRow(layout, 0);
	if (RNA_enum_get(ptr, "image_type")== R_OPENEXR) {
		uiItemR(row, NULL, 0, ptr, "exr_half", 0);
		uiItemR(row, "", 0, ptr, "exr_codec", 0);
	}
	else if (RNA_enum_get(ptr, "image_type")== R_JPEG90) {
		uiItemR(row, NULL, 0, ptr, "quality", UI_ITEM_R_SLIDER);
	}
	
	row= uiLayoutRow(layout, 1);
	uiItemR(row, "Start", 0, ptr, "start_frame", 0);
	uiItemR(row, "End", 0, ptr, "end_frame", 0);
}

static void node_composit_buts_scale(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiItemR(layout, "", 0, ptr, "space", 0);
}

static void node_composit_buts_rotate(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
   uiItemR(layout, "", 0, ptr, "filter", 0);
}

static void node_composit_buts_invert(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 0);
	uiItemR(col, NULL, 0, ptr, "rgb", 0);
	uiItemR(col, NULL, 0, ptr, "alpha", 0);
}

static void node_composit_buts_premulkey(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiItemR(layout, "", 0, ptr, "mapping", 0);
}

static void node_composit_buts_view_levels(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiItemR(layout, NULL, 0, ptr, "channel", UI_ITEM_R_EXPAND);
}

static void node_composit_buts_colorbalance(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *split, *col, *row;
	
	uiItemR(layout, NULL, 0, ptr, "correction_formula", 0);
	
	if (RNA_enum_get(ptr, "correction_formula")== 0) {
	
		split = uiLayoutSplit(layout, 0, 0);
		col = uiLayoutColumn(split, 0);
		uiTemplateColorWheel(col, ptr, "lift", 1, 1);
		row = uiLayoutRow(col, 0);
		uiItemR(row, NULL, 0, ptr, "lift", 0);
		
		col = uiLayoutColumn(split, 0);
		uiTemplateColorWheel(col, ptr, "gamma", 1, 1);
		row = uiLayoutRow(col, 0);
		uiItemR(row, NULL, 0, ptr, "gamma", 0);
		
		col = uiLayoutColumn(split, 0);
		uiTemplateColorWheel(col, ptr, "gain", 1, 1);
		row = uiLayoutRow(col, 0);
		uiItemR(row, NULL, 0, ptr, "gain", 0);

	} else {
		
		split = uiLayoutSplit(layout, 0, 0);
		col = uiLayoutColumn(split, 0);
		uiTemplateColorWheel(col, ptr, "offset", 1, 1);
		row = uiLayoutRow(col, 0);
		uiItemR(row, NULL, 0, ptr, "offset", 0);
		
		col = uiLayoutColumn(split, 0);
		uiTemplateColorWheel(col, ptr, "power", 1, 1);
		row = uiLayoutRow(col, 0);
		uiItemR(row, NULL, 0, ptr, "power", 0);
		
		col = uiLayoutColumn(split, 0);
		uiTemplateColorWheel(col, ptr, "slope", 1, 1);
		row = uiLayoutRow(col, 0);
		uiItemR(row, NULL, 0, ptr, "slope", 0);
	}

}

static void node_composit_buts_huecorrect(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiTemplateCurveMapping(layout, ptr, "mapping", 'h', 0, 0);
}

/* only once called */
static void node_composit_set_butfunc(bNodeType *ntype)
{
	switch(ntype->type) {
		/* case NODE_GROUP:	 note, typeinfo for group is generated... see "XXX ugly hack" */

		case CMP_NODE_IMAGE:
			ntype->uifunc= node_composit_buts_image;
			break;
		case CMP_NODE_R_LAYERS:
			ntype->uifunc= node_composit_buts_renderlayers;
			break;
		case CMP_NODE_NORMAL:
			ntype->uifunc= node_buts_normal;
			break;
		case CMP_NODE_CURVE_VEC:
			ntype->uifunc= node_buts_curvevec;
			break;
		case CMP_NODE_CURVE_RGB:
			ntype->uifunc= node_buts_curvecol;
			break;
		case CMP_NODE_VALUE:
			ntype->uifunc= node_buts_value;
			break;
		case CMP_NODE_RGB:
			ntype->uifunc= node_buts_rgb;
			break;
		case CMP_NODE_FLIP:
			ntype->uifunc= node_composit_buts_flip;
			break;
		case CMP_NODE_SPLITVIEWER:
			ntype->uifunc= node_composit_buts_splitviewer;
			break;
		case CMP_NODE_MIX_RGB:
			ntype->uifunc= node_buts_mix_rgb;
			break;
		case CMP_NODE_VALTORGB:
			ntype->uifunc= node_buts_colorramp;
			break;
		case CMP_NODE_CROP:
			ntype->uifunc= node_composit_buts_crop;
			break;
		case CMP_NODE_BLUR:
			ntype->uifunc= node_composit_buts_blur;
			break;
		case CMP_NODE_DBLUR:
			ntype->uifunc= node_composit_buts_dblur;
			break;
		case CMP_NODE_BILATERALBLUR:
			ntype->uifunc= node_composit_buts_bilateralblur;
			break;
		case CMP_NODE_DEFOCUS:
			ntype->uifunc = node_composit_buts_defocus;
			break;
		case CMP_NODE_GLARE:
			ntype->uifunc = node_composit_buts_glare;
			break;
		case CMP_NODE_TONEMAP:
			ntype->uifunc = node_composit_buts_tonemap;
			break;
		case CMP_NODE_LENSDIST:
			ntype->uifunc = node_composit_buts_lensdist;
			break;
		case CMP_NODE_VECBLUR:
			ntype->uifunc= node_composit_buts_vecblur;
			break;
		case CMP_NODE_FILTER:
			ntype->uifunc= node_composit_buts_filter;
			break;
		case CMP_NODE_MAP_VALUE:
			ntype->uifunc= node_composit_buts_map_value;
			break;
		case CMP_NODE_TIME:
			ntype->uifunc= node_buts_time;
			break;
		case CMP_NODE_ALPHAOVER:
			ntype->uifunc= node_composit_buts_alphaover;
			break;
		case CMP_NODE_HUE_SAT:
			ntype->uifunc= node_composit_buts_hue_sat;
			break;
		case CMP_NODE_TEXTURE:
			ntype->uifunc= node_buts_texture;
			break;
		case CMP_NODE_DILATEERODE:
			ntype->uifunc= node_composit_buts_dilateerode;
			break;
		case CMP_NODE_OUTPUT_FILE:
			ntype->uifunc= node_composit_buts_file_output;
			break;	
		case CMP_NODE_DIFF_MATTE:
			ntype->uifunc=node_composit_buts_diff_matte;
			break;
		case CMP_NODE_DIST_MATTE:
			ntype->uifunc=node_composit_buts_distance_matte;
			break;
		case CMP_NODE_COLOR_SPILL:
			ntype->uifunc=node_composit_buts_color_spill;
			break;
		case CMP_NODE_CHROMA_MATTE:
			ntype->uifunc=node_composit_buts_chroma_matte;
			break;
		case CMP_NODE_COLOR_MATTE:
			ntype->uifunc=node_composit_buts_color_matte;
			break;
		case CMP_NODE_SCALE:
			ntype->uifunc= node_composit_buts_scale;
			break;
	  case CMP_NODE_ROTATE:
		 ntype->uifunc=node_composit_buts_rotate;
		 break;
		case CMP_NODE_CHANNEL_MATTE:
			ntype->uifunc= node_composit_buts_channel_matte;
			break;
		case CMP_NODE_LUMA_MATTE:
			ntype->uifunc= node_composit_buts_luma_matte;
			break;
		case CMP_NODE_MAP_UV:
			ntype->uifunc= node_composit_buts_map_uv;
			break;
		case CMP_NODE_ID_MASK:
			ntype->uifunc= node_composit_buts_id_mask;
			break;
		case CMP_NODE_MATH:
			ntype->uifunc= node_buts_math;
			break;
		case CMP_NODE_INVERT:
			ntype->uifunc= node_composit_buts_invert;
			break;
		case CMP_NODE_PREMULKEY:
			ntype->uifunc= node_composit_buts_premulkey;
			break;
		case CMP_NODE_VIEW_LEVELS:
			ntype->uifunc=node_composit_buts_view_levels;
			 break;
		case CMP_NODE_COLORBALANCE:
			ntype->uifunc=node_composit_buts_colorbalance;
			 break;
		case CMP_NODE_HUECORRECT:
			ntype->uifunc=node_composit_buts_huecorrect;
			 break;
		default:
			ntype->uifunc= NULL;
	}
}

/* ****************** BUTTON CALLBACKS FOR TEXTURE NODES ***************** */

static void node_texture_buts_bricks(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, "Offset", 0, ptr, "offset", 0);
	uiItemR(col, "Frequency", 0, ptr, "offset_frequency", 0);
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, "Squash", 0, ptr, "squash", 0);
	uiItemR(col, "Frequency", 0, ptr, "squash_frequency", 0);
}

static void node_texture_buts_proc(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	PointerRNA tex_ptr;
	bNode *node= ptr->data;
	ID *id= ptr->id.data;
	Tex *tex = (Tex *)node->storage;
	uiLayout *col, *row;
	
	RNA_pointer_create(id, &RNA_Texture, tex, &tex_ptr);

	col= uiLayoutColumn(layout, 0);

	switch( tex->type ) {
		case TEX_BLEND:
			uiItemR(col, "", 0, &tex_ptr, "progression", 0);
			row= uiLayoutRow(col, 0);
			uiItemR(row, NULL, 0, &tex_ptr, "flip_axis", UI_ITEM_R_EXPAND);
			break;

		case TEX_MARBLE:
			row= uiLayoutRow(col, 0);
			uiItemR(row, NULL, 0, &tex_ptr, "stype", UI_ITEM_R_EXPAND);
			row= uiLayoutRow(col, 0);
			uiItemR(row, NULL, 0, &tex_ptr, "noise_type", UI_ITEM_R_EXPAND);
			row= uiLayoutRow(col, 0);
			uiItemR(row, NULL, 0, &tex_ptr, "noisebasis2", UI_ITEM_R_EXPAND);
			break;

		case TEX_WOOD:
			uiItemR(col, "", 0, &tex_ptr, "noise_basis", 0);
			uiItemR(col, "", 0, &tex_ptr, "stype", 0);
			row= uiLayoutRow(col, 0);
			uiItemR(row, NULL, 0, &tex_ptr, "noisebasis2", UI_ITEM_R_EXPAND);
			row= uiLayoutRow(col, 0);
			uiLayoutSetActive(row, !(RNA_enum_get(&tex_ptr, "stype")==TEX_BAND || RNA_enum_get(&tex_ptr, "stype")==TEX_RING)); 
			uiItemR(row, NULL, 0, &tex_ptr, "noise_type", UI_ITEM_R_EXPAND);
			break;
			
		case TEX_CLOUDS:
			uiItemR(col, "", 0, &tex_ptr, "noise_basis", 0);
			row= uiLayoutRow(col, 0);
			uiItemR(row, NULL, 0, &tex_ptr, "stype", UI_ITEM_R_EXPAND);
			row= uiLayoutRow(col, 0);
			uiItemR(row, NULL, 0, &tex_ptr, "noise_type", UI_ITEM_R_EXPAND);
			uiItemR(col, "Depth", 0, &tex_ptr, "noise_depth", UI_ITEM_R_EXPAND);
			break;
			
		case TEX_DISTNOISE:
			uiItemR(col, "", 0, &tex_ptr, "noise_basis", 0);
			uiItemR(col, "", 0, &tex_ptr, "noise_distortion", 0);
			break;
	}
}

static void node_texture_buts_image(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiTemplateID(layout, C, ptr, "image", NULL, "IMAGE_OT_open", NULL);
}

static void node_texture_buts_output(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiItemR(layout, "", 0, ptr, "output_name", 0);
}

/* only once called */
static void node_texture_set_butfunc(bNodeType *ntype)
{
	if( ntype->type >= TEX_NODE_PROC && ntype->type < TEX_NODE_PROC_MAX ) {
		ntype->uifunc = node_texture_buts_proc;
	}
	else switch(ntype->type) {
		
		case TEX_NODE_MATH:
			ntype->uifunc = node_buts_math;
			break;
		
		case TEX_NODE_MIX_RGB:
			ntype->uifunc = node_buts_mix_rgb;
			break;
			
		case TEX_NODE_VALTORGB:
			ntype->uifunc = node_buts_colorramp;
			break;
			
		case TEX_NODE_CURVE_RGB:
			ntype->uifunc= node_buts_curvecol;
			break;
			
		case TEX_NODE_CURVE_TIME:
			ntype->uifunc = node_buts_time;
			break;
			
		case TEX_NODE_TEXTURE:
			ntype->uifunc = node_buts_texture;
			break;
			
		case TEX_NODE_BRICKS:
			ntype->uifunc = node_texture_buts_bricks;
			break;
			
		case TEX_NODE_IMAGE:
			ntype->uifunc = node_texture_buts_image;
			break;
			
		case TEX_NODE_OUTPUT:
			ntype->uifunc = node_texture_buts_output;
			break;
			
		default:
			ntype->uifunc= NULL;
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

void draw_nodespace_back_pix(ARegion *ar, SpaceNode *snode, int color_manage)
{
	
	if((snode->flag & SNODE_BACKDRAW) && snode->treetype==NTREE_COMPOSIT) {
		Image *ima= BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
		void *lock;
		ImBuf *ibuf= BKE_image_acquire_ibuf(ima, NULL, &lock);
		if(ibuf) {
			float x, y; 
			
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();

			/* somehow the offset has to be calculated inverse */
			
			glaDefine2DArea(&ar->winrct);
			/* ortho at pixel level curarea */
			wmOrtho2(-0.375, ar->winx-0.375, -0.375, ar->winy-0.375);
			
			x = (ar->winx-ibuf->x)/2 + snode->xof;
			y = (ar->winy-ibuf->y)/2 + snode->yof;
			
			if(!ibuf->rect) {
				if(color_manage)
					ibuf->profile = IB_PROFILE_LINEAR_RGB;
				else
					ibuf->profile = IB_PROFILE_NONE;
				IMB_rect_from_float(ibuf);
			}

			if(ibuf->rect)
				glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
			
			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
		}

		BKE_image_release_ibuf(ima, lock);
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


