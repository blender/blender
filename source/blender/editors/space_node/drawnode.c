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
#include "BLI_utildefines.h"

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

#include "CMP_node.h"
#include "SHD_node.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_api.h"

#include "MEM_guardedalloc.h"


#include "RNA_access.h"

#include "ED_node.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "node_intern.h"


/* ****************** BUTTON CALLBACKS FOR ALL TREES ***************** */

static void node_buts_value(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	PointerRNA sockptr;
	PropertyRNA *prop;
	
	/* first socket stores value */
	prop = RNA_struct_find_property(ptr, "outputs");
	RNA_property_collection_lookup_int(ptr, prop, 0, &sockptr);
	
	uiItemR(layout, &sockptr, "default_value", 0, "", ICON_NONE);
}

static void node_buts_rgb(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	PointerRNA sockptr;
	PropertyRNA *prop;
	
	/* first socket stores value */
	prop = RNA_struct_find_property(ptr, "outputs");
	RNA_property_collection_lookup_int(ptr, prop, 0, &sockptr);
	
	col = uiLayoutColumn(layout, 0);
	uiTemplateColorWheel(col, &sockptr, "default_value", 1, 0, 0, 0);
	uiItemR(col, &sockptr, "default_value", 0, "", ICON_NONE);
}

static void node_buts_mix_rgb(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiLayout *row;

	bNodeTree *ntree= (bNodeTree*)ptr->id.data;

	row= uiLayoutRow(layout, 1);
	uiItemR(row, ptr, "blend_type", 0, "", ICON_NONE);
	if(ntree->type == NTREE_COMPOSIT)
		uiItemR(row, ptr, "use_alpha", 0, "", ICON_IMAGE_RGB_ALPHA);
}

static void node_buts_time(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
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
	uiItemR(row, ptr, "frame_start", 0, "Sta", ICON_NONE);
	uiItemR(row, ptr, "frame_end", 0, "End", ICON_NONE);
}

static void node_buts_colorramp(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiTemplateColorRamp(layout, ptr, "color_ramp", 0);
}

static void node_buts_curvevec(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiTemplateCurveMapping(layout, ptr, "mapping", 'v', 0, 0);
}

static float *_sample_col= NULL;	// bad bad, 2.5 will do better?
#if 0
static void node_curvemap_sample(float *col)
{
	_sample_col= col;
}
#endif

static void node_buts_curvecol(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
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

static void node_normal_cb(bContext *C, void *ntree_v, void *node_v)
{
	Main *bmain = CTX_data_main(C);

	ED_node_generic_update(bmain, ntree_v, node_v);
	WM_event_add_notifier(C, NC_NODE|NA_EDITED, ntree_v);
}

static void node_buts_normal(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiBlock *block= uiLayoutAbsoluteBlock(layout);
	bNodeTree *ntree= ptr->id.data;
	bNode *node= ptr->data;
	rctf *butr= &node->butr;
	bNodeSocket *sock= node->outputs.first;		/* first socket stores normal */
	uiBut *bt;
	
	bt= uiDefButF(block, BUT_NORMAL, B_NODE_EXEC, "", 
			  (short)butr->xmin, (short)butr->xmin, butr->xmax-butr->xmin, butr->xmax-butr->xmin, 
			  sock->ns.vec, 0.0f, 1.0f, 0, 0, "");
	uiButSetFunc(bt, node_normal_cb, ntree, node);
}
#if 0 // not used in 2.5x yet
static void node_browse_tex_cb(bContext *C, void *ntree_v, void *node_v)
{
	Main *bmain= CTX_data_main(C);
	bNodeTree *ntree= ntree_v;
	bNode *node= node_v;
	Tex *tex;
	
	if(node->menunr<1) return;
	
	if(node->id) {
		node->id->us--;
		node->id= NULL;
	}
	tex= BLI_findlink(&bmain->tex, node->menunr-1);

	node->id= &tex->id;
	id_us_plus(node->id);
	BLI_strncpy(node->name, node->id->name+2, sizeof(node->name));
	
	nodeSetActive(ntree, node);
	
	if( ntree->type == NTREE_TEXTURE )
		ntreeTexCheckCyclics( ntree );
	
	// allqueue(REDRAWBUTSSHADING, 0);
	// allqueue(REDRAWNODE, 0);
	NodeTagChanged(ntree, node); 
	
	node->menunr= 0;
}
#endif
static void node_dynamic_update_cb(bContext *C, void *UNUSED(ntree_v), void *node_v)
{
	Main *bmain= CTX_data_main(C);
	Material *ma;
	bNode *node= (bNode *)node_v;
	ID *id= node->id;
	int error= 0;

	if (BTST(node->custom1, NODE_DYNAMIC_ERROR)) error= 1;

	/* Users only have to press the "update" button in one pynode
	 * and we also update all others sharing the same script */
	for (ma= bmain->mat.first; ma; ma= ma->id.next) {
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

static void node_buts_texture(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	bNode *node= ptr->data;

	short multi = (
		node->id &&
		((Tex*)node->id)->use_nodes &&
		(node->type != CMP_NODE_TEXTURE) &&
		(node->type != TEX_NODE_TEXTURE)
	);
	
	uiItemR(layout, ptr, "texture", 0, "", ICON_NONE);
	
	if(multi) {
		/* Number Drawing not optimal here, better have a list*/
		uiItemR(layout, ptr, "node_output", 0, "", ICON_NONE);
	}
}

static void node_buts_math(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{ 
	uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
}

/* ****************** BUTTON CALLBACKS FOR SHADER NODES ***************** */

static void node_browse_text_cb(bContext *C, void *ntree_v, void *node_v)
{
	Main *bmain= CTX_data_main(C);
	bNodeTree *ntree= ntree_v;
	bNode *node= node_v;
	ID *oldid;
	
	if(node->menunr<1) return;
	
	if(node->id) {
		node->id->us--;
	}
	oldid= node->id;
	node->id= BLI_findlink(&bmain->text, node->menunr-1);
	id_us_plus(node->id);
	BLI_strncpy(node->name, node->id->name+2, sizeof(node->name));

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
	uiItemR(col, ptr, "use_diffuse", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "use_specular", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "invert_normal", 0, NULL, ICON_NONE);
}

static void node_shader_buts_mapping(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *row;
	
	uiItemL(layout, "Location:", ICON_NONE);
	row= uiLayoutRow(layout, 1);
	uiItemR(row, ptr, "location", 0, "", ICON_NONE);
	
	uiItemL(layout, "Rotation:", ICON_NONE);
	row= uiLayoutRow(layout, 1);
	uiItemR(row, ptr, "rotation", 0, "", ICON_NONE);
	
	uiItemL(layout, "Scale:", ICON_NONE);
	row= uiLayoutRow(layout, 1);
	uiItemR(row, ptr, "scale", 0, "", ICON_NONE);
	
	row= uiLayoutRow(layout, 1);
	uiItemR(row, ptr, "use_min", 0, "Min", ICON_NONE);
	uiItemR(row, ptr, "min", 0, "", ICON_NONE);
	
	row= uiLayoutRow(layout, 1);
	uiItemR(row, ptr, "use_max", 0, "Max", ICON_NONE);
	uiItemR(row, ptr, "max", 0, "", ICON_NONE);
	
}

static void node_shader_buts_vect_math(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{ 
	uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
}

static void node_shader_buts_geometry(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	PointerRNA obptr= CTX_data_pointer_get(C, "active_object");
	uiLayout *col;

	col= uiLayoutColumn(layout, 0);

	if(obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
		PointerRNA dataptr= RNA_pointer_get(&obptr, "data");

		uiItemPointerR(col, ptr, "uv_layer", &dataptr, "uv_textures", "", ICON_NONE);
		uiItemPointerR(col, ptr, "color_layer", &dataptr, "vertex_colors", "", ICON_NONE);
	}
	else {
		uiItemR(col, ptr, "uv_layer", 0, "UV", ICON_NONE);
		uiItemR(col, ptr, "color_layer", 0, "VCol", ICON_NONE);
	}
}

static void node_shader_buts_dynamic(uiLayout *layout, bContext *C, PointerRNA *ptr)
{ 
	Main *bmain= CTX_data_main(C);
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
			const char *strp;
			IDnames_to_pupstring(&strp, NULL, "", &(bmain->text), NULL, NULL);
			node->menunr= 0;
			bt= uiDefButS(block, MENU, B_NODE_EXEC/*+node->nr*/, strp, 
							butr->xmin, dy, 19, 19, 
							&node->menunr, 0, 0, 0, 0, "Browses existing choices");
			uiButSetFunc(bt, node_browse_text_cb, ntree, node);
			xoff=19;
			if(strp) MEM_freeN((void *)strp);
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
	
	uiItemR(col, &imaptr, "source", 0, NULL, ICON_NONE);
	
	if (ELEM(RNA_enum_get(&imaptr, "source"), IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
		col= uiLayoutColumn(layout, 1);
		uiItemR(col, ptr, "frame_duration", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "frame_start", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "frame_offset", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "use_cyclic", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "use_auto_refresh", UI_ITEM_R_ICON_ONLY, NULL, ICON_NONE);
	}

	col= uiLayoutColumn(layout, 0);
	
	if (RNA_enum_get(&imaptr, "type")== IMA_TYPE_MULTILAYER)
		uiItemR(col, ptr, "layer", 0, NULL, ICON_NONE);
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
	uiItemR(row, ptr, "layer", 0, "", ICON_NONE);
	
	prop = RNA_struct_find_property(ptr, "layer");
	if (!(RNA_property_enum_identifier(C, ptr, prop, RNA_property_enum_get(ptr, prop), &layer_name)))
		return;
	
	scn_ptr = RNA_pointer_get(ptr, "scene");
	RNA_string_get(&scn_ptr, "name", scene_name);
	
	WM_operator_properties_create(&op_ptr, "RENDER_OT_render");
	RNA_string_set(&op_ptr, "layer", layer_name);
	RNA_string_set(&op_ptr, "scene", scene_name);
	uiItemFullO(row, "RENDER_OT_render", "", ICON_RENDER_STILL, op_ptr.data, WM_OP_INVOKE_DEFAULT, 0);

}


static void node_composit_buts_blur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col, *row;
	
	col= uiLayoutColumn(layout, 0);
	
	uiItemR(col, ptr, "filter_type", 0, "", ICON_NONE);
	if (RNA_enum_get(ptr, "filter_type")!= R_FILTER_FAST_GAUSS) {
		uiItemR(col, ptr, "use_bokeh", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "use_gamma_correction", 0, NULL, ICON_NONE);
	}
	
	uiItemR(col, ptr, "use_relative", 0, NULL, ICON_NONE);
	
	if (RNA_boolean_get(ptr, "use_relative")) {
		uiItemL(col, "Aspect Correction", 0);
		row= uiLayoutRow(layout, 1);
		uiItemR(row, ptr, "aspect_correction", UI_ITEM_R_EXPAND, NULL, 0);
		
		col= uiLayoutColumn(layout, 1);
		uiItemR(col, ptr, "factor_x", 0, "X", ICON_NONE);
		uiItemR(col, ptr, "factor_y", 0, "Y", ICON_NONE);
	}
	else {
		col= uiLayoutColumn(layout, 1);
		uiItemR(col, ptr, "size_x", 0, "X", ICON_NONE);
		uiItemR(col, ptr, "size_y", 0, "Y", ICON_NONE);
	}
}

static void node_composit_buts_dblur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	uiItemR(layout, ptr, "iterations", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "use_wrap", 0, NULL, ICON_NONE);
	
	col= uiLayoutColumn(layout, 1);
	uiItemL(col, "Center:", ICON_NONE);
	uiItemR(col, ptr, "center_x", 0, "X", ICON_NONE);
	uiItemR(col, ptr, "center_y", 0, "Y", ICON_NONE);
	
	uiItemS(layout);
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, ptr, "distance", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "angle", 0, NULL, ICON_NONE);
	
	uiItemS(layout);
	
	uiItemR(layout, ptr, "spin", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "zoom", 0, NULL, ICON_NONE);
}

static void node_composit_buts_bilateralblur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, ptr, "iterations", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "sigma_color", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "sigma_space", 0, NULL, ICON_NONE);
}

static void node_composit_buts_defocus(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *sub, *col;
	
	col= uiLayoutColumn(layout, 0);
	uiItemL(col, "Bokeh Type:", ICON_NONE);
	uiItemR(col, ptr, "bokeh", 0, "", ICON_NONE);
	uiItemR(col, ptr, "angle", 0, NULL, ICON_NONE);

	uiItemR(layout, ptr, "use_gamma_correction", 0, NULL, ICON_NONE);

	col = uiLayoutColumn(layout, 0);
	uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_zbuffer")==1);
	uiItemR(col, ptr, "f_stop", 0, NULL, ICON_NONE);

	uiItemR(layout, ptr, "blur_max", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "threshold", 0, NULL, ICON_NONE);

	col = uiLayoutColumn(layout, 0);
	uiItemR(col, ptr, "use_preview", 0, NULL, ICON_NONE);
	sub = uiLayoutColumn(col, 0);
	uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_preview"));
	uiItemR(sub, ptr, "samples", 0, NULL, ICON_NONE);
	
	col = uiLayoutColumn(layout, 0);
	uiItemR(col, ptr, "use_zbuffer", 0, NULL, ICON_NONE);
	sub = uiLayoutColumn(col, 0);
	uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_zbuffer")==0);
	uiItemR(sub, ptr, "z_scale", 0, NULL, ICON_NONE);
}

/* qdn: glare node */
static void node_composit_buts_glare(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiItemR(layout, ptr, "glare_type", 0, "", ICON_NONE);
	uiItemR(layout, ptr, "quality", 0, "", ICON_NONE);

	if (RNA_enum_get(ptr, "glare_type")!= 1) {
		uiItemR(layout, ptr, "iterations", 0, NULL, ICON_NONE);
	
		if (RNA_enum_get(ptr, "glare_type")!= 0) 
			uiItemR(layout, ptr, "color_modulation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	}
	
	uiItemR(layout, ptr, "mix", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "threshold", 0, NULL, ICON_NONE);

	if (RNA_enum_get(ptr, "glare_type")== 2) {
		uiItemR(layout, ptr, "streaks", 0, NULL, ICON_NONE);
		uiItemR(layout, ptr, "angle_offset", 0, NULL, ICON_NONE);
	}
	if (RNA_enum_get(ptr, "glare_type")== 0 || RNA_enum_get(ptr, "glare_type")== 2) {
		uiItemR(layout, ptr, "fade", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
		
		if (RNA_enum_get(ptr, "glare_type")== 0) 
			uiItemR(layout, ptr, "use_rotate_45", 0, NULL, ICON_NONE);
	}
	if (RNA_enum_get(ptr, "glare_type")== 1) {
		uiItemR(layout, ptr, "size", 0, NULL, ICON_NONE);
	}
}

static void node_composit_buts_tonemap(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiLayout *col;

	col = uiLayoutColumn(layout, 0);
	uiItemR(col, ptr, "tonemap_type", 0, "", ICON_NONE);
	if (RNA_enum_get(ptr, "tonemap_type")== 0) {
		uiItemR(col, ptr, "key", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
		uiItemR(col, ptr, "offset", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "gamma", 0, NULL, ICON_NONE);
	}
	else {
		uiItemR(col, ptr, "intensity", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "contrast", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
		uiItemR(col, ptr, "adaptation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
		uiItemR(col, ptr, "correction", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	}
}

static void node_composit_buts_lensdist(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;

	col= uiLayoutColumn(layout, 0);
	uiItemR(col, ptr, "use_projector", 0, NULL, ICON_NONE);

	col = uiLayoutColumn(col, 0);
	uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_projector")==0);
	uiItemR(col, ptr, "use_jitter", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "use_fit", 0, NULL, ICON_NONE);
}

static void node_composit_buts_vecblur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 0);
	uiItemR(col, ptr, "samples", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "factor", 0, "Blur", ICON_NONE);
	
	col= uiLayoutColumn(layout, 1);
	uiItemL(col, "Speed:", ICON_NONE);
	uiItemR(col, ptr, "speed_min", 0, "Min", ICON_NONE);
	uiItemR(col, ptr, "speed_max", 0, "Max", ICON_NONE);

	uiItemR(layout, ptr, "use_curved", 0, NULL, ICON_NONE);
}

static void node_composit_buts_filter(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "filter_type", 0, "", ICON_NONE);
}

static void node_composit_buts_flip(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "axis", 0, "", ICON_NONE);
}

static void node_composit_buts_crop(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	uiItemR(layout, ptr, "use_crop_size", 0, NULL, ICON_NONE);
   uiItemR(layout, ptr, "relative", 0, NULL, ICON_NONE);

	col= uiLayoutColumn(layout, 1);
   if (RNA_boolean_get(ptr, "relative")){
      uiItemR(col, ptr, "rel_min_x", 0, "Left", ICON_NONE);
      uiItemR(col, ptr, "rel_max_x", 0, "Right", ICON_NONE);
      uiItemR(col, ptr, "rel_min_y", 0, "Up", ICON_NONE);
      uiItemR(col, ptr, "rel_max_y", 0, "Down", ICON_NONE);
   } else {
      uiItemR(col, ptr, "min_x", 0, "Left", ICON_NONE);
      uiItemR(col, ptr, "max_x", 0, "Right", ICON_NONE);
      uiItemR(col, ptr, "min_y", 0, "Up", ICON_NONE);
      uiItemR(col, ptr, "max_y", 0, "Down", ICON_NONE);
   }
}

static void node_composit_buts_splitviewer(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *row, *col;
	
	col= uiLayoutColumn(layout, 0);
	row= uiLayoutRow(col, 0);
	uiItemR(row, ptr, "axis", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
	uiItemR(col, ptr, "factor", 0, NULL, ICON_NONE);
}

static void node_composit_buts_map_value(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *sub, *col;
	
	col =uiLayoutColumn(layout, 1);
	uiItemR(col, ptr, "offset", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "size", 0, NULL, ICON_NONE);
	
	col =uiLayoutColumn(layout, 1);
	uiItemR(col, ptr, "use_min", 0, NULL, ICON_NONE);
	sub =uiLayoutColumn(col, 0);
	uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_min"));
	uiItemR(sub, ptr, "min", 0, "", ICON_NONE);
	
	col =uiLayoutColumn(layout, 1);
	uiItemR(col, ptr, "use_max", 0, NULL, ICON_NONE);
	sub =uiLayoutColumn(col, 0);
	uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_max"));
	uiItemR(sub, ptr, "max", 0, "", ICON_NONE);
}

static void node_composit_buts_alphaover(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiLayout *col;
	
	col =uiLayoutColumn(layout, 1);
	uiItemR(col, ptr, "use_premultiply", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "premul", 0, NULL, ICON_NONE);
}

static void node_composit_buts_zcombine(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiLayout *col;
	
	col =uiLayoutColumn(layout, 1);
	uiItemR(col, ptr, "use_alpha", 0, NULL, ICON_NONE);
}


static void node_composit_buts_hue_sat(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col =uiLayoutColumn(layout, 0);
	uiItemR(col, ptr, "color_hue", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "color_saturation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "color_value", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_dilateerode(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "distance", 0, NULL, ICON_NONE);
}

static void node_composit_buts_diff_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col =uiLayoutColumn(layout, 1);
	uiItemR(col, ptr, "tolerance", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "falloff", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_distance_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col =uiLayoutColumn(layout, 1);
	uiItemR(col, ptr, "tolerance", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "falloff", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_color_spill(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *row, *col;
	
   uiItemL(layout, "Despill Channel:", ICON_NONE);
   row =uiLayoutRow(layout,0);
	uiItemR(row, ptr, "channel", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

   col= uiLayoutColumn(layout, 0);
   uiItemR(col, ptr, "limit_method", 0, NULL, ICON_NONE);

   if(RNA_enum_get(ptr, "limit_method")==0) {
	  uiItemL(col, "Limiting Channel:", ICON_NONE);
	  row=uiLayoutRow(col,0);
	  uiItemR(row, ptr, "limit_channel", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
   }

   uiItemR(col, ptr, "ratio", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
   uiItemR(col, ptr, "use_unspill", 0, NULL, ICON_NONE);
   if (RNA_enum_get(ptr, "use_unspill")== 1) {
	  uiItemR(col, ptr, "unspill_red", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	  uiItemR(col, ptr, "unspill_green", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	  uiItemR(col, ptr, "unspill_blue", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
   }
}

static void node_composit_buts_chroma_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 0);
	uiItemR(col, ptr, "tolerance", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "threshold", 0, NULL, ICON_NONE);
	
	col= uiLayoutColumn(layout, 1);
   /*uiItemR(col, ptr, "lift", UI_ITEM_R_SLIDER, NULL, ICON_NONE);  Removed for now */
	uiItemR(col, ptr, "gain", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
   /*uiItemR(col, ptr, "shadow_adjust", UI_ITEM_R_SLIDER, NULL, ICON_NONE);  Removed for now*/
}

static void node_composit_buts_color_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, ptr, "color_hue", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "color_saturation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "color_value", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_channel_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiLayout *col, *row;

   uiItemL(layout, "Color Space:", ICON_NONE);
	row= uiLayoutRow(layout, 0);
	uiItemR(row, ptr, "color_space", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

   col=uiLayoutColumn(layout, 0);  
   uiItemL(col, "Key Channel:", ICON_NONE);
	row= uiLayoutRow(col, 0);
	uiItemR(row, ptr, "matte_channel", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

	col =uiLayoutColumn(layout, 0);

   uiItemR(col, ptr, "limit_method", 0, NULL, ICON_NONE);
   if(RNA_enum_get(ptr, "limit_method")==0) {
	  uiItemL(col, "Limiting Channel:", ICON_NONE);
	  row=uiLayoutRow(col,0);
	  uiItemR(row, ptr, "limit_channel", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
   }
   
	uiItemR(col, ptr, "limit_max", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "limit_min", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_luma_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, ptr, "limit_max", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "limit_min", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_map_uv(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "alpha", 0, NULL, ICON_NONE);
}

static void node_composit_buts_id_mask(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "index", 0, NULL, ICON_NONE);
}

static void node_composit_buts_file_output(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col, *row;

	col= uiLayoutColumn(layout, 0);
	uiItemR(col, ptr, "filepath", 0, "", ICON_NONE);
	uiItemR(col, ptr, "image_type", 0, "", ICON_NONE);
	
	row= uiLayoutRow(layout, 0);
	if (RNA_enum_get(ptr, "image_type")== R_OPENEXR) {
		uiItemR(row, ptr, "use_exr_half", 0, NULL, ICON_NONE);
		uiItemR(row, ptr, "exr_codec", 0, "", ICON_NONE);
	}
	else if (RNA_enum_get(ptr, "image_type")== R_JPEG90) {
		uiItemR(row, ptr, "quality", UI_ITEM_R_SLIDER, "Quality", ICON_NONE);
	}
	else if (RNA_enum_get(ptr, "image_type")== R_PNG) {
		uiItemR(row, ptr, "quality", UI_ITEM_R_SLIDER, "Compression", ICON_NONE);
	}
	
	row= uiLayoutRow(layout, 1);
	uiItemR(row, ptr, "frame_start", 0, "Start", ICON_NONE);
	uiItemR(row, ptr, "frame_end", 0, "End", ICON_NONE);
}

static void node_composit_buts_scale(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "space", 0, "", ICON_NONE);
}

static void node_composit_buts_rotate(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
   uiItemR(layout, ptr, "filter_type", 0, "", ICON_NONE);
}

static void node_composit_buts_invert(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 0);
	uiItemR(col, ptr, "invert_rgb", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "invert_alpha", 0, NULL, ICON_NONE);
}

static void node_composit_buts_premulkey(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "mapping", 0, "", ICON_NONE);
}

static void node_composit_buts_view_levels(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "channel", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
}

static void node_composit_buts_colorbalance(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *split, *col, *row;
	
	uiItemR(layout, ptr, "correction_method", 0, NULL, ICON_NONE);
	
	if (RNA_enum_get(ptr, "correction_method")== 0) {
	
		split = uiLayoutSplit(layout, 0, 0);
		col = uiLayoutColumn(split, 0);
		uiTemplateColorWheel(col, ptr, "lift", 1, 1, 0, 1);
		row = uiLayoutRow(col, 0);
		uiItemR(row, ptr, "lift", 0, NULL, ICON_NONE);
		
		col = uiLayoutColumn(split, 0);
		uiTemplateColorWheel(col, ptr, "gamma", 1, 1, 1, 1);
		row = uiLayoutRow(col, 0);
		uiItemR(row, ptr, "gamma", 0, NULL, ICON_NONE);
		
		col = uiLayoutColumn(split, 0);
		uiTemplateColorWheel(col, ptr, "gain", 1, 1, 1, 1);
		row = uiLayoutRow(col, 0);
		uiItemR(row, ptr, "gain", 0, NULL, ICON_NONE);

	} else {
		
		split = uiLayoutSplit(layout, 0, 0);
		col = uiLayoutColumn(split, 0);
		uiTemplateColorWheel(col, ptr, "offset", 1, 1, 0, 1);
		row = uiLayoutRow(col, 0);
		uiItemR(row, ptr, "offset", 0, NULL, ICON_NONE);
		
		col = uiLayoutColumn(split, 0);
		uiTemplateColorWheel(col, ptr, "power", 1, 1, 0, 1);
		row = uiLayoutRow(col, 0);
		uiItemR(row, ptr, "power", 0, NULL, ICON_NONE);
		
		col = uiLayoutColumn(split, 0);
		uiTemplateColorWheel(col, ptr, "slope", 1, 1, 0, 1);
		row = uiLayoutRow(col, 0);
		uiItemR(row, ptr, "slope", 0, NULL, ICON_NONE);
	}

}

static void node_composit_buts_huecorrect(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiTemplateCurveMapping(layout, ptr, "mapping", 'h', 0, 0);
}

static void node_composit_buts_ycc(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{ 
	uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
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
		case CMP_NODE_ZCOMBINE:
			ntype->uifunc=node_composit_buts_zcombine;
			 break;
		case CMP_NODE_COMBYCCA:
		case CMP_NODE_SEPYCCA:
			ntype->uifunc=node_composit_buts_ycc;
			break;
		default:
			ntype->uifunc= NULL;
	}
}

/* ****************** BUTTON CALLBACKS FOR TEXTURE NODES ***************** */

static void node_texture_buts_bricks(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, ptr, "offset", 0, "Offset", ICON_NONE);
	uiItemR(col, ptr, "offset_frequency", 0, "Frequency", ICON_NONE);
	
	col= uiLayoutColumn(layout, 1);
	uiItemR(col, ptr, "squash", 0, "Squash", ICON_NONE);
	uiItemR(col, ptr, "squash_frequency", 0, "Frequency", ICON_NONE);
}

static void node_texture_buts_proc(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
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
			uiItemR(col, &tex_ptr, "progression", 0, "", ICON_NONE);
			row= uiLayoutRow(col, 0);
			uiItemR(row, &tex_ptr, "use_flip_axis", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			break;

		case TEX_MARBLE:
			row= uiLayoutRow(col, 0);
			uiItemR(row, &tex_ptr, "marble_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			row= uiLayoutRow(col, 0);
			uiItemR(row, &tex_ptr, "noise_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			row= uiLayoutRow(col, 0);
			uiItemR(row, &tex_ptr, "noisebasis_2", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			break;

		case TEX_WOOD:
			uiItemR(col, &tex_ptr, "noise_basis", 0, "", ICON_NONE);
			uiItemR(col, &tex_ptr, "wood_type", 0, "", ICON_NONE);
			row= uiLayoutRow(col, 0);
			uiItemR(row, &tex_ptr, "noisebasis_2", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			row= uiLayoutRow(col, 0);
			uiLayoutSetActive(row, !(RNA_enum_get(&tex_ptr, "wood_type")==TEX_BAND || RNA_enum_get(&tex_ptr, "wood_type")==TEX_RING)); 
			uiItemR(row, &tex_ptr, "noise_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			break;
			
		case TEX_CLOUDS:
			uiItemR(col, &tex_ptr, "noise_basis", 0, "", ICON_NONE);
			row= uiLayoutRow(col, 0);
			uiItemR(row, &tex_ptr, "cloud_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			row= uiLayoutRow(col, 0);
			uiItemR(row, &tex_ptr, "noise_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			uiItemR(col, &tex_ptr, "noise_depth", UI_ITEM_R_EXPAND, "Depth", ICON_NONE);
			break;
			
		case TEX_DISTNOISE:
			uiItemR(col, &tex_ptr, "noise_basis", 0, "", ICON_NONE);
			uiItemR(col, &tex_ptr, "noise_distortion", 0, "", ICON_NONE);
			break;
	}
}

static void node_texture_buts_image(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiTemplateID(layout, C, ptr, "image", NULL, "IMAGE_OT_open", NULL);
}

static void node_texture_buts_output(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "filepath", 0, "", ICON_NONE);
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

			/* keep this, saves us from a version patch */
			if(snode->zoom==0.0f) snode->zoom= 1.0f;
			
			/* somehow the offset has to be calculated inverse */
			
			glaDefine2DArea(&ar->winrct);
			/* ortho at pixel level curarea */
			wmOrtho2(-0.375, ar->winx-0.375, -0.375, ar->winy-0.375);
			
			x = (ar->winx-snode->zoom*ibuf->x)/2 + snode->xof;
			y = (ar->winy-snode->zoom*ibuf->y)/2 + snode->yof;
			
			if(!ibuf->rect) {
				if(color_manage)
					ibuf->profile = IB_PROFILE_LINEAR_RGB;
				else
					ibuf->profile = IB_PROFILE_NONE;
				IMB_rect_from_float(ibuf);
			}

			if(ibuf->rect) {
				if (snode->flag & SNODE_SHOW_ALPHA) {
					glPixelZoom(snode->zoom, snode->zoom);
					/* swap bytes, so alpha is most significant one, then just draw it as luminance int */
					if(ENDIAN_ORDER == B_ENDIAN)
						glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
					
					glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_LUMINANCE, GL_UNSIGNED_INT, ibuf->rect);
					
					glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
					glPixelZoom(1.0f, 1.0f);
				} else if (snode->flag & SNODE_USE_ALPHA) {
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					glPixelZoom(snode->zoom, snode->zoom);
					
					glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
					
					glPixelZoom(1.0f, 1.0f);
					glDisable(GL_BLEND);
				} else {
					glPixelZoom(snode->zoom, snode->zoom);
					
					glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
					
					glPixelZoom(1.0f, 1.0f);
				}
			}
			
			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
		}

		BKE_image_release_ibuf(ima, lock);
	}
}

void draw_nodespace_color_info(ARegion *ar, int channels, int x, int y, char *cp, float *fp)
{
	char str[256];
	int ofs;
	
	ofs= sprintf(str, "X: %4d Y: %4d ", x, y);

	if(channels==4) {
		if(cp)
			ofs+= sprintf(str+ofs, "| R: %3d G: %3d B: %3d A: %3d ", cp[0], cp[1], cp[2], cp[3]);
		if (fp)
			ofs+= sprintf(str+ofs, "| R: %.3f G: %.3f B: %.3f A: %.3f ", fp[0], fp[1], fp[2], fp[3]);
	}
	else if(channels==1) {
		if(cp)
			ofs+= sprintf(str+ofs, "| Val: %3d ", cp[0]);
		if (fp)
			ofs+= sprintf(str+ofs, "| Val: %.3f ", fp[0]);
	}
	else if(channels==3) {
		if(cp)
			ofs+= sprintf(str+ofs, "| R: %3d G: %3d B: %3d ", cp[0], cp[1], cp[2]);
		if (fp)
			ofs+= sprintf(str+ofs, "| R: %.3f G: %.3f B: %.3f ", fp[0], fp[1], fp[2]);
	}

	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	glColor4f(.0,.0,.0,.25);
	glRecti(0.0, 0.0, ar->winrct.xmax - ar->winrct.xmin + 1, 20);
	glDisable(GL_BLEND);
	
	glColor3ub(255, 255, 255);
	
	// UI_DrawString(6, 6, str); // works ok but fixed width is nicer.
	BLF_size(blf_mono_font, 11, 72);
	BLF_position(blf_mono_font, 6, 6, 0);
	BLF_draw_ascii(blf_mono_font, str, sizeof(str));
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
void node_draw_link_bezier(View2D *v2d, SpaceNode *snode, bNodeLink *link, int th_col1, int do_shaded, int th_col2, int do_triple, int th_col3 )
{
	float coord_array[LINK_RESOL+1][2];
	
	if(node_link_bezier_points(v2d, snode, link, coord_array, LINK_RESOL)) {
		float dist, spline_step = 0.0f;
		int i;
		
		/* store current linewidth */
		float linew;
		glGetFloatv(GL_LINE_WIDTH, &linew);
		
		/* we can reuse the dist variable here to increment the GL curve eval amount*/
		dist = 1.0f/(float)LINK_RESOL;
		
		glEnable(GL_LINE_SMOOTH);
		
		if(do_triple) {
			UI_ThemeColorShadeAlpha(th_col3, -80, -120);
			glLineWidth(4.0f);
			
			glBegin(GL_LINE_STRIP);
			for(i=0; i<=LINK_RESOL; i++) {
				glVertex2fv(coord_array[i]);
			}
			glEnd();
		}
		
		UI_ThemeColor(th_col1);
		glLineWidth(1.5f);
		
		glBegin(GL_LINE_STRIP);
		for(i=0; i<=LINK_RESOL; i++) {
			if(do_shaded) {
				UI_ThemeColorBlend(th_col1, th_col2, spline_step);
				spline_step += dist;
			}
			glVertex2fv(coord_array[i]);
		}
		glEnd();
		
		glDisable(GL_LINE_SMOOTH);
		
		/* restore previuos linewidth */
		glLineWidth(linew);
	}
}

/* note; this is used for fake links in groups too */
void node_draw_link(View2D *v2d, SpaceNode *snode, bNodeLink *link)
{
	int do_shaded= 0, th_col1= TH_HEADER, th_col2= TH_HEADER;
	int do_triple= 0, th_col3= TH_WIRE;
	
	if(link->fromsock==NULL && link->tosock==NULL)
		return;
	
	/* new connection */
	if(!link->fromsock || !link->tosock) {
		th_col1 = TH_ACTIVE;
		do_triple = 1;
	}
	else {
		/* going to give issues once... */
		if(link->tosock->flag & SOCK_UNAVAIL)
			return;
		if(link->fromsock->flag & SOCK_UNAVAIL)
			return;
		
		/* a bit ugly... but thats how we detect the internal group links */
		if(!link->fromnode || !link->tonode) {
			UI_ThemeColorBlend(TH_BACK, TH_WIRE, 0.5f);
			do_shaded= 0;
		}
		else {
			/* check cyclic */
			if(link->fromnode->level >= link->tonode->level && link->tonode->level!=0xFFF) {
				if(link->fromnode->flag & SELECT)
					th_col1= TH_EDGE_SELECT;
				if(link->tonode->flag & SELECT)
					th_col2= TH_EDGE_SELECT;
				do_shaded= 1;
				do_triple= 1;
			}				
			else {
				th_col1 = TH_REDALERT;
			}
		}
	}
	
	node_draw_link_bezier(v2d, snode, link, th_col1, do_shaded, th_col2, do_triple, th_col3);
}


