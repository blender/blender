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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include <string.h>
#include <stdio.h>

#include "DNA_color_types.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_node_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_packedFile.h"
#include "BKE_paint.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "RE_pipeline.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "ED_gpencil.h"
#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_uvedit.h"
#include "ED_util.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "image_intern.h"

#define B_REDR				1
#define B_IMAGECHANGED		2
#define B_TRANS_IMAGE		3
#define B_CURSOR_IMAGE		4
#define B_NOP				0
#define B_TWINANIM			5
#define B_SIMAGETILE		6
#define B_IDNAME			10
#define B_FACESEL_PAINT_TEST	11
#define B_SIMA_RECORD		12
#define B_SIMA_PLAY			13
#define B_SIMARANGE			14
#define B_SIMACURVES		15

#define B_SIMANOTHING		16
#define B_SIMABRUSHCHANGE	17	
#define B_SIMABRUSHBROWSE	18
#define B_SIMABRUSHLOCAL	19
#define B_SIMABRUSHDELETE	20
#define B_KEEPDATA			21
#define B_SIMABTEXBROWSE	22
#define B_SIMABTEXDELETE	23
#define B_VPCOLSLI			24
#define B_SIMACLONEBROWSE	25
#define B_SIMACLONEDELETE	26

/* XXX */
static int okee() {return 0;}
static int simaFaceDraw_Check() {return 0;}
static int simaUVSel_Check() {return 0;}
static int is_uv_tface_editing_allowed_silent() {return 0;}
/* XXX */

/* proto */
static void image_editvertex_buts(const bContext *C, uiBlock *block);
static void image_editcursor_buts(const bContext *C, View2D *v2d, uiBlock *block);


static void do_image_panel_events(bContext *C, void *arg, int event)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ARegion *ar= CTX_wm_region(C);
	
	switch(event) {
		case B_REDR:
			break;
		case B_SIMACURVES:
			curvemapping_do_ibuf(sima->cumap, ED_space_image_buffer(sima));
			break;
		case B_SIMARANGE:
			curvemapping_set_black_white(sima->cumap, NULL, NULL);
			curvemapping_do_ibuf(sima->cumap, ED_space_image_buffer(sima));
			break;
		case B_TRANS_IMAGE:
			image_editvertex_buts(C, NULL);
			break;
		case B_CURSOR_IMAGE:
			image_editcursor_buts(C, &ar->v2d, NULL);
			break;
	}
	/* all events now */
	WM_event_add_notifier(C, NC_IMAGE, sima->image);
}



static void image_info(Image *ima, ImBuf *ibuf, char *str)
{
	int ofs= 0;
	
	str[0]= 0;
	
	if(ima==NULL) return;
	if(ibuf==NULL) {
		sprintf(str, "Can not get an image");
		return;
	}
	
	if(ima->source==IMA_SRC_MOVIE) {
		ofs= sprintf(str, "Movie ");
		if(ima->anim) 
			ofs+= sprintf(str+ofs, "%d frs", IMB_anim_get_duration(ima->anim));
	}
	else
	 	ofs= sprintf(str, "Image ");
	
	ofs+= sprintf(str+ofs, ": size %d x %d,", ibuf->x, ibuf->y);
	
	if(ibuf->rect_float) {
		if(ibuf->channels!=4) {
			sprintf(str+ofs, "%d float channel(s)", ibuf->channels);
		}
		else if(ibuf->depth==32)
			strcat(str, " RGBA float");
		else
			strcat(str, " RGB float");
	}
	else {
		if(ibuf->depth==32)
			strcat(str, " RGBA byte");
		else
			strcat(str, " RGB byte");
	}
	if(ibuf->zbuf || ibuf->zbuf_float)
		strcat(str, " + Z");
	
}

/* gets active viewer user */
struct ImageUser *ntree_get_active_iuser(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree)
		for(node= ntree->nodes.first; node; node= node->next)
			if( ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) 
				if(node->flag & NODE_DO_OUTPUT)
					return node->storage;
	return NULL;
}


/* ************ panel stuff ************* */

/* this function gets the values for cursor and vertex number buttons */
static void image_transform_but_attr(SpaceImage *sima, int *imx, int *imy, int *step, int *digits) /*, float *xcoord, float *ycoord)*/
{
	ImBuf *ibuf= ED_space_image_buffer(sima);
	if(ibuf) {
		*imx= ibuf->x;
		*imy= ibuf->y;
	}
	
	if (sima->flag & SI_COORDFLOATS) {
		*step= 1;
		*digits= 3;
	}
	else {
		*step= 100;
		*digits= 2;
	}
}


/* is used for both read and write... */
static void image_editvertex_buts(const bContext *C, uiBlock *block)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Object *obedit= CTX_data_edit_object(C);
	static float ocent[2];
	float cent[2]= {0.0, 0.0};
	int imx= 256, imy= 256;
	int nactive= 0, step, digits;
	EditMesh *em;
	EditFace *efa;
	MTFace *tf;
	
	if(obedit==NULL || obedit->type!=OB_MESH) return;
	
	if( is_uv_tface_editing_allowed_silent()==0 ) return;
	
	image_transform_but_attr(sima, &imx, &imy, &step, &digits);
	
	em= BKE_mesh_get_editmesh((Mesh *)obedit->data);
	for (efa= em->faces.first; efa; efa= efa->next) {
		tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if (simaFaceDraw_Check(efa, tf)) {
			
			if (simaUVSel_Check(efa, tf, 0)) {
				cent[0]+= tf->uv[0][0];
				cent[1]+= tf->uv[0][1];
				nactive++;
			}
			if (simaUVSel_Check(efa, tf, 1)) {
				cent[0]+= tf->uv[1][0];
				cent[1]+= tf->uv[1][1];
				nactive++;
			}
			if (simaUVSel_Check(efa, tf, 2)) {
				cent[0]+= tf->uv[2][0];
				cent[1]+= tf->uv[2][1];
				nactive++;
			}
			if (efa->v4 && simaUVSel_Check(efa, tf, 3)) {
				cent[0]+= tf->uv[3][0];
				cent[1]+= tf->uv[3][1];
				nactive++;
			}
		}
	}
		
	if(block) {	// do the buttons
		if (nactive) {
			ocent[0]= cent[0]/nactive;
			ocent[1]= cent[1]/nactive;
			if (sima->flag & SI_COORDFLOATS) {
			} else {
				ocent[0] *= imx;
				ocent[1] *= imy;
			}
			
			//uiBlockBeginAlign(block);
			if(nactive==1) {
				uiDefButF(block, NUM, B_TRANS_IMAGE, "Vertex X:",	10, 10, 145, 19, &ocent[0], -10*imx, 10.0*imx, step, digits, "");
				uiDefButF(block, NUM, B_TRANS_IMAGE, "Vertex Y:",	165, 10, 145, 19, &ocent[1], -10*imy, 10.0*imy, step, digits, "");
			}
			else {
				uiDefButF(block, NUM, B_TRANS_IMAGE, "Median X:",	10, 10, 145, 19, &ocent[0], -10*imx, 10.0*imx, step, digits, "");
				uiDefButF(block, NUM, B_TRANS_IMAGE, "Median Y:",	165, 10, 145, 19, &ocent[1], -10*imy, 10.0*imy, step, digits, "");
			}
			//uiBlockEndAlign(block);
		}
	}
	else {	// apply event
		float delta[2];
		
		cent[0]= cent[0]/nactive;
		cent[1]= cent[1]/nactive;
			
		if (sima->flag & SI_COORDFLOATS) {
			delta[0]= ocent[0]-cent[0];
			delta[1]= ocent[1]-cent[1];
		}
		else {
			delta[0]= ocent[0]/imx - cent[0];
			delta[1]= ocent[1]/imy - cent[1];
		}

		for (efa= em->faces.first; efa; efa= efa->next) {
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (simaFaceDraw_Check(efa, tf)) {
				if (simaUVSel_Check(efa, tf, 0)) {
					tf->uv[0][0]+= delta[0];
					tf->uv[0][1]+= delta[1];
				}
				if (simaUVSel_Check(efa, tf, 1)) {
					tf->uv[1][0]+= delta[0];
					tf->uv[1][1]+= delta[1];
				}
				if (simaUVSel_Check(efa, tf, 2)) {
					tf->uv[2][0]+= delta[0];
					tf->uv[2][1]+= delta[1];
				}
				if (efa->v4 && simaUVSel_Check(efa, tf, 3)) {
					tf->uv[3][0]+= delta[0];
					tf->uv[3][1]+= delta[1];
				}
			}
		}
		
		WM_event_add_notifier(C, NC_IMAGE, sima->image);
	}

	BKE_mesh_end_editmesh(obedit->data, em);
}


/* is used for both read and write... */
static void image_editcursor_buts(const bContext *C, View2D *v2d, uiBlock *block)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	static float ocent[2];
	int imx= 256, imy= 256;
	int step, digits;
	
	if( is_uv_tface_editing_allowed_silent()==0 ) return;
	
	image_transform_but_attr(sima, &imx, &imy, &step, &digits);
		
	if(block) {	// do the buttons
		ocent[0]= v2d->cursor[0];
		ocent[1]= v2d->cursor[1];
		if (sima->flag & SI_COORDFLOATS) {
		} else {
			ocent[0] *= imx;
			ocent[1] *= imy;
		}
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_CURSOR_IMAGE, "Cursor X:",	165, 120, 145, 19, &ocent[0], -10*imx, 10.0*imx, step, digits, "");
		uiDefButF(block, NUM, B_CURSOR_IMAGE, "Cursor Y:",	165, 100, 145, 19, &ocent[1], -10*imy, 10.0*imy, step, digits, "");
		uiBlockEndAlign(block);
	}
	else {	// apply event
		if (sima->flag & SI_COORDFLOATS) {
			v2d->cursor[0]= ocent[0];
			v2d->cursor[1]= ocent[1];
		}
		else {
			v2d->cursor[0]= ocent[0]/imx;
			v2d->cursor[1]= ocent[1]/imy;
		}
		WM_event_add_notifier(C, NC_IMAGE, sima->image);
	}
}

#if 0
static void image_panel_view_properties(const bContext *C, Panel *pa)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ARegion *ar= CTX_wm_region(C);
	Object *obedit= CTX_data_edit_object(C);
	uiBlock *block;

	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_image_panel_events, NULL);
	
	uiDefButBitI(block, TOG, SI_DRAW_TILE, B_REDR, "Repeat Image",	10,160,140,19, &sima->flag, 0, 0, 0, 0, "Repeat/Tile the image display");
	uiDefButBitI(block, TOG, SI_COORDFLOATS, B_REDR, "Normalized Coords",	165,160,145,19, &sima->flag, 0, 0, 0, 0, "Display coords from 0.0 to 1.0 rather then in pixels");
	
	if (sima->image) {
		uiDefBut(block, LABEL, B_NOP, "Image Display:",		10,140,140,19, 0, 0, 0, 0, 0, "");
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_REDR, "AspX:", 10,120,140,19, &sima->image->aspx, 0.1, 5000.0, 100, 0, "X Display Aspect for this image, does not affect renderingm 0 disables.");
		uiDefButF(block, NUM, B_REDR, "AspY:", 10,100,140,19, &sima->image->aspy, 0.1, 5000.0, 100, 0, "X Display Aspect for this image, does not affect rendering 0 disables.");
		uiBlockEndAlign(block);
	}
	
	if (obedit && obedit->type==OB_MESH) {
		Mesh *me= obedit->data;
		EditMesh *em= BKE_mesh_get_editmesh(me);
		
		if(EM_texFaceCheck(em)) {
			uiDefBut(block, LABEL, B_NOP, "Draw Type:",		10, 80,120,19, 0, 0, 0, 0, 0, "");
			uiBlockBeginAlign(block);
			uiDefButC(block,  ROW, B_REDR, "Outline",		10,60,58,19, &sima->dt_uv, 0.0, SI_UVDT_OUTLINE, 0, 0, "Outline Wire UV drawtype");
			uiDefButC(block,  ROW, B_REDR, "Dash",			68, 60,58,19, &sima->dt_uv, 0.0, SI_UVDT_DASH, 0, 0, "Dashed Wire UV drawtype");
			uiDefButC(block,  ROW, B_REDR, "Black",			126, 60,58,19, &sima->dt_uv, 0.0, SI_UVDT_BLACK, 0, 0, "Black Wire UV drawtype");
			uiDefButC(block,  ROW, B_REDR, "White",			184,60,58,19, &sima->dt_uv, 0.0, SI_UVDT_WHITE, 0, 0, "White Wire UV drawtype");
			
			uiBlockEndAlign(block);
			uiDefButBitI(block, TOG, SI_SMOOTH_UV, B_REDR, "Smooth",	250,60,60,19,  &sima->flag, 0, 0, 0, 0, "Display smooth lines in the UV view");
			
			
			uiDefButBitI(block, TOG, ME_DRAWFACES, B_REDR, "Faces",		10,30,60,19,  &me->drawflag, 0, 0, 0, 0, "Displays all faces as shades in the 3d view and UV editor");
			uiDefButBitI(block, TOG, ME_DRAWEDGES, B_REDR, "Edges", 70, 30,60,19, &me->drawflag, 0, 0, 0, 0, "Displays selected edges using hilights in the 3d view and UV editor");
			
			uiDefButBitI(block, TOG, SI_DRAWSHADOW, B_REDR, "Final Shadow", 130, 30,110,19, &sima->flag, 0, 0, 0, 0, "Draw the final result from the objects modifiers");
			uiDefButBitI(block, TOG, SI_DRAW_OTHER, B_REDR, "Other Objs", 230, 30, 80, 19, &sima->flag, 0, 0, 0, 0, "Also draw all 3d view selected mesh objects that use this image");
			
			uiDefButBitI(block, TOG, SI_DRAW_STRETCH, B_REDR, "UV Stretch",	10,0,100,19,  &sima->flag, 0, 0, 0, 0, "Difference between UV's and the 3D coords (blue for low distortion, red is high)");
			if (sima->flag & SI_DRAW_STRETCH) {
				uiBlockBeginAlign(block);
				uiDefButC(block,  ROW, B_REDR, "Area",			120,0,60,19, &sima->dt_uvstretch, 0.0, SI_UVDT_STRETCH_AREA, 0, 0, "Area distortion between UV's and 3D coords");
				uiDefButC(block,  ROW, B_REDR, "Angle",		180,0,60,19, &sima->dt_uvstretch, 0.0, SI_UVDT_STRETCH_ANGLE, 0, 0, "Angle distortion between UV's and 3D coords");
				uiBlockEndAlign(block);
			}
		}

		BKE_mesh_end_editmesh(me, em);
	}
	image_editcursor_buts(C, &ar->v2d, block);
}
#endif

void brush_buttons(const bContext *C, uiBlock *block, short fromsima,
				   int evt_nop, int evt_change,
				   int evt_browse, int evt_local,
				   int evt_del, int evt_keepdata,
				   int evt_texbrowse, int evt_texdel)
{
//	SpaceImage *sima= CTX_wm_space_image(C);
	ToolSettings *settings= CTX_data_tool_settings(C);
	Brush *brush= paint_brush(&settings->imapaint.paint);
	ID *id;
	int yco, xco, butw, but_idx;
//	short *menupoin = &(sima->menunr); // XXX : &(G.buts->menunr);
	short do_project = settings->imapaint.flag & IMAGEPAINT_PROJECT_DISABLE ? 0:1;
	
	yco= 160;
	
	butw = fromsima ? 80 : 106;
	
	uiBlockBeginAlign(block);
	but_idx = 0;
	uiDefButS(block, ROW, evt_change, "Draw",		butw*(but_idx++),yco,butw,19, &settings->imapaint.tool, 7.0, PAINT_TOOL_DRAW, 0, 0, "Draw brush");
	if (fromsima || do_project==0)
		uiDefButS(block, ROW, evt_change, "Soften",	butw*(but_idx++),	yco,butw,19, &settings->imapaint.tool, 7.0, PAINT_TOOL_SOFTEN, 0, 0, "Soften brush");
	uiDefButS(block, ROW, evt_change, "Smear",		butw*(but_idx++),	yco,butw,19, &settings->imapaint.tool, 7.0, PAINT_TOOL_SMEAR, 0, 0, "Smear brush");
	if (fromsima || do_project)
		uiDefButS(block, ROW, evt_change, "Clone",	butw*(but_idx++),	yco,butw,19, &settings->imapaint.tool, 7.0, PAINT_TOOL_CLONE, 0, 0, "Clone brush, use RMB to drag source image");
	
	uiBlockEndAlign(block);
	yco -= 30;
	
	id= (ID*)brush;
	xco= 200; // std_libbuttons(block, 0, yco, 0, NULL, evt_browse, ID_BR, 0, id, NULL, menupoin, 0, evt_local, evt_del, 0, evt_keepdata);
	
	if(brush && !brush->id.lib) {
		
		butw= 320-(xco+10);
		
		uiDefButS(block, MENU, evt_nop, "Mix %x0|Add %x1|Subtract %x2|Multiply %x3|Lighten %x4|Darken %x5|Erase Alpha %x6|Add Alpha %x7", xco+10,yco,butw,19, &brush->blend, 0, 0, 0, 0, "Blending method for applying brushes");
		
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG|BIT, BRUSH_AIRBRUSH, evt_change, "Airbrush",	xco+10,yco-25,butw/2,19, &brush->flag, 0, 0, 0, 0, "Keep applying paint effect while holding mouse (spray)");
		uiDefButF(block, NUM, evt_nop, "", xco+10 + butw/2,yco-25,butw/2,19, &brush->rate, 0.01, 1.0, 0, 0, "Number of paints per second for Airbrush");
		uiBlockEndAlign(block);
		
		if (fromsima) {
			uiDefButBitS(block, TOG|BIT, BRUSH_TORUS, evt_change, "Wrap",	xco+10,yco-45,butw,19, &brush->flag, 0, 0, 0, 0, "Enables torus wrapping");
			yco -= 25;
		}
		else {
			yco -= 25;
			uiBlockBeginAlign(block);
			uiDefButBitS(block, TOGN|BIT, IMAGEPAINT_PROJECT_DISABLE, B_REDR, "Project Paint",	xco+10,yco-25,butw,19, &settings->imapaint.flag, 0, 0, 0, 0, "Use projection painting for improved consistency in the brush strokes");
			
			if ((settings->imapaint.flag & IMAGEPAINT_PROJECT_DISABLE)==0) {
				/* Projection Painting */
				
				uiDefButBitS(block, TOGN|BIT, IMAGEPAINT_PROJECT_XRAY, B_NOP, "Occlude",	xco+10,yco-45,butw/2,19, &settings->imapaint.flag, 0, 0, 0, 0, "Only paint onto the faces directly under the brush (slower)");
				uiDefButBitS(block, TOGN|BIT, IMAGEPAINT_PROJECT_BACKFACE, B_NOP, "Cull",	xco+10+butw/2,yco-45,butw/2,19, &settings->imapaint.flag, 0, 0, 0, 0, "Ignore faces pointing away from the view (faster)");
				
				uiDefButBitS(block, TOGN|BIT, IMAGEPAINT_PROJECT_FLAT, B_NOP, "Normal",	xco+10,yco-65,butw/2,19, &settings->imapaint.flag, 0, 0, 0, 0, "Paint most on faces pointing towards the view");
				uiDefButS(block, NUM, B_NOP, "", xco+10 +(butw/2),yco-65,butw/2,19, &settings->imapaint.normal_angle, 10.0, 90.0, 0, 0, "Paint most on faces pointing towards the view acording to this angle");
				
				uiDefButS(block, NUM, B_NOP, "Bleed: ", xco+10,yco-85,butw,19, &settings->imapaint.seam_bleed, 0.0, 8.0, 0, 0, "Extend paint beyond the faces UVs to reduce seams (in pixels, slower)");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG|BIT, IMAGEPAINT_PROJECT_LAYER_MASK, B_NOP, "Stencil Layer",	xco+10,yco-110,butw-30,19, &settings->imapaint.flag, 0, 0, 0, 0, "Set the mask layer from the UV layer buttons");
				uiDefButBitS(block, TOG|BIT, IMAGEPAINT_PROJECT_LAYER_MASK_INV, B_NOP, "Inv",	xco+10 + butw-30,yco-110,30,19, &settings->imapaint.flag, 0, 0, 0, 0, "Invert the mask");
				uiBlockEndAlign(block);
				
			}
			uiBlockEndAlign(block);
		}
		
		uiBlockBeginAlign(block);
		uiDefButF(block, COL, B_VPCOLSLI, "",					0,yco,200,19, brush->rgb, 0, 0, 0, 0, "");
		uiDefButF(block, NUMSLI, evt_nop, "Opacity ",		0,yco-20,180,19, &brush->alpha, 0.0, 1.0, 0, 0, "The amount of pressure on the brush");
		uiDefIconButBitS(block, TOG|BIT, BRUSH_ALPHA_PRESSURE, evt_nop, ICON_STYLUS_PRESSURE,	180,yco-20,20,19, &brush->flag, 0, 0, 0, 0, "Enables pressure sensitivity for tablets");
		uiDefButI(block, NUMSLI, evt_nop, "Size ",		0,yco-40,180,19, &brush->size, 1, 200, 0, 0, "The size of the brush");
		uiDefIconButBitS(block, TOG|BIT, BRUSH_SIZE_PRESSURE, evt_nop, ICON_STYLUS_PRESSURE,	180,yco-40,20,19, &brush->flag, 0, 0, 0, 0, "Enables pressure sensitivity for tablets");
		uiDefButF(block, NUMSLI, evt_nop, "Falloff ",		0,yco-60,180,19, &brush->innerradius, 0.0, 1.0, 0, 0, "The fall off radius of the brush");
		uiDefIconButBitS(block, TOG|BIT, BRUSH_RAD_PRESSURE, evt_nop, ICON_STYLUS_PRESSURE,	180,yco-60,20,19, &brush->flag, 0, 0, 0, 0, "Enables pressure sensitivity for tablets");
		uiDefButF(block, NUMSLI, evt_nop, "Spacing ",0,yco-80,180,19, &brush->spacing, 1.0, 100.0, 0, 0, "Repeating paint on %% of brush diameter");
		uiDefIconButBitS(block, TOG|BIT, BRUSH_SPACING_PRESSURE, evt_nop, ICON_STYLUS_PRESSURE,	180,yco-80,20,19, &brush->flag, 0, 0, 0, 0, "Enables pressure sensitivity for tablets");
		uiBlockEndAlign(block);
		
		yco -= 110;
		
		if(fromsima && settings->imapaint.tool == PAINT_TOOL_CLONE) {
			id= (ID*)brush->clone.image;
			xco= 200; // std_libbuttons(block, 0, yco, 0, NULL, B_SIMACLONEBROWSE, ID_IM, 0, id, 0, menupoin, 0, 0, B_SIMACLONEDELETE, 0, 0);
			if(id) {
				butw= 320-(xco+5);
				uiDefButF(block, NUMSLI, evt_change, "B ",xco+5,yco,butw,19, &brush->clone.alpha , 0.0, 1.0, 0, 0, "Opacity of clone image display");
			}
		}
		else {
			if (
				(fromsima==0) && /* 3D View */
				(settings->imapaint.flag & IMAGEPAINT_PROJECT_DISABLE)==0 && /* Projection Painting */
				(settings->imapaint.tool == PAINT_TOOL_CLONE)
				) {
				butw = 130;
				uiDefButBitS(block, TOG|BIT, IMAGEPAINT_PROJECT_LAYER_CLONE, B_REDR, "Clone Layer",	0,yco,butw,20, &settings->imapaint.flag, 0, 0, 0, 0, "Use another UV layer as clone source, otherwise use 3D the cursor as the source");
			}
			else {
				MTex *mtex= brush->mtex[brush->texact];
				
				id= (mtex)? (ID*)mtex->tex: NULL;
				xco= 200; // std_libbuttons(block, 0, yco, 0, NULL, evt_texbrowse, ID_TE, 0, id, NULL, menupoin, 0, 0, evt_texdel, 0, 0);
				/*uiDefButBitS(block, TOG|BIT, BRUSH_FIXED_TEX, evt_change, "Fixed",	xco+5,yco,butw,19, &brush->flag, 0, 0, 0, 0, "Keep texture origin in fixed position");*/
			}
		}
	}
	
#if 0
	uiDefButBitS(block, TOG|BIT, IMAGEPAINT_DRAW_TOOL_DRAWING, B_SIMABRUSHCHANGE, "TD", 0,1,50,19, &settings->imapaint.flag.flag, 0, 0, 0, 0, "Enables brush shape while drawing");
	uiDefButBitS(block, TOG|BIT, IMAGEPAINT_DRAW_TOOL, B_SIMABRUSHCHANGE, "TP", 50,1,50,19, &settings->imapaint.flag.flag, 0, 0, 0, 0, "Enables brush shape while not drawing");
#endif
}

static int image_panel_paint_poll(const bContext *C, PanelType *pt)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	
	return (sima->image && (sima->flag & SI_DRAWTOOL));
}

static void image_panel_paintcolor(const bContext *C, Panel *pa)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ToolSettings *settings= CTX_data_tool_settings(C);
	Brush *brush= paint_brush(&settings->imapaint.paint);
	uiBlock *block;
	static float hsv[3], old[3];	// used as temp mem for picker
	static char hexcol[128];
	
	if(!sima->image || (sima->flag & SI_DRAWTOOL)==0)
		return;
	
	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_image_panel_events, NULL);

	if(brush)
		uiBlockPickerButtons(block, brush->rgb, hsv, old, hexcol, 'f', B_REDR);
}

static void image_panel_paint(const bContext *C, Panel *pa)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	uiBlock *block;
	
	if(!sima->image || (sima->flag & SI_DRAWTOOL)==0)
		return;
	
	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_image_panel_events, NULL);
	
	brush_buttons(C, block, 1, B_SIMANOTHING, B_SIMABRUSHCHANGE, B_SIMABRUSHBROWSE, B_SIMABRUSHLOCAL, B_SIMABRUSHDELETE, B_KEEPDATA, B_SIMABTEXBROWSE, B_SIMABTEXDELETE);
}

static void image_panel_curves_reset(bContext *C, void *cumap_v, void *ibuf_v)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	CurveMapping *cumap = cumap_v;
	int a;
	
	for(a=0; a<CM_TOT; a++)
		curvemap_reset(cumap->cm+a, &cumap->clipr);
	
	cumap->black[0]=cumap->black[1]=cumap->black[2]= 0.0f;
	cumap->white[0]=cumap->white[1]=cumap->white[2]= 1.0f;
	curvemapping_set_black_white(cumap, NULL, NULL);
	
	curvemapping_changed(cumap, 0);
	curvemapping_do_ibuf(cumap, ibuf_v);
	
	WM_event_add_notifier(C, NC_IMAGE, sima->image);
}


static void image_panel_curves(const bContext *C, Panel *pa)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ImBuf *ibuf;
	uiBlock *block;
	uiBut *bt;
	
	/* and we check for spare */
	ibuf= ED_space_image_buffer(sima);
	
	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_image_panel_events, NULL);
	
	if (ibuf) {
		rctf rect;
		
		if(sima->cumap==NULL)
			sima->cumap= curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
		
		rect.xmin= 110; rect.xmax= 310;
		rect.ymin= 10; rect.ymax= 200;
		curvemap_buttons(block, sima->cumap, 'c', B_SIMACURVES, B_REDR, &rect);
		
		/* curvemap min/max only works for RGBA */
		if(ibuf->channels==4) {
			bt=uiDefBut(block, BUT, B_SIMARANGE, "Reset",	10, 160, 90, 19, NULL, 0.0f, 0.0f, 0, 0, "Reset Black/White point and curves");
			uiButSetFunc(bt, image_panel_curves_reset, sima->cumap, ibuf);
		
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_SIMARANGE, "Min R:",	10, 120, 90, 19, sima->cumap->black, -1000.0f, 1000.0f, 10, 2, "Black level");
			uiDefButF(block, NUM, B_SIMARANGE, "Min G:",	10, 100, 90, 19, sima->cumap->black+1, -1000.0f, 1000.0f, 10, 2, "Black level");
			uiDefButF(block, NUM, B_SIMARANGE, "Min B:",	10, 80, 90, 19, sima->cumap->black+2, -1000.0f, 1000.0f, 10, 2, "Black level");
			
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_SIMARANGE, "Max R:",	10, 50, 90, 19, sima->cumap->white, -1000.0f, 1000.0f, 10, 2, "White level");
			uiDefButF(block, NUM, B_SIMARANGE, "Max G:",	10, 30, 90, 19, sima->cumap->white+1, -1000.0f, 1000.0f, 10, 2, "White level");
			uiDefButF(block, NUM, B_SIMARANGE, "Max B:",	10, 10, 90, 19, sima->cumap->white+2, -1000.0f, 1000.0f, 10, 2, "White level");
		}
	}
}

#if 0
/* 0: disable preview 
   otherwise refresh preview
*/
void image_preview_event(int event)
{
	int exec= 0;
	
	if(event==0) {
		G.scene->r.scemode &= ~R_COMP_CROP;
		exec= 1;
	}
	else {
		if(image_preview_active(curarea, NULL, NULL)) {
			G.scene->r.scemode |= R_COMP_CROP;
			exec= 1;
		}
		else
			G.scene->r.scemode &= ~R_COMP_CROP;
	}
	
	if(exec && G.scene->nodetree) {
		/* should work when no node editor in screen..., so we execute right away */
		
		ntreeCompositTagGenerators(G.scene->nodetree);

		G.afbreek= 0;
		G.scene->nodetree->timecursor= set_timecursor;
		G.scene->nodetree->test_break= blender_test_break;
		
		BIF_store_spare();
		
		ntreeCompositExecTree(G.scene->nodetree, &G.scene->r, 1);	/* 1 is do_previews */
		
		G.scene->nodetree->timecursor= NULL;
		G.scene->nodetree->test_break= NULL;
		
		scrarea_do_windraw(curarea);
		waitcursor(0);
		
		WM_event_add_notifier(C, NC_IMAGE, ima_v);
	}	
}


/* nothing drawn here, we use it to store values */
static void preview_cb(struct ScrArea *sa, struct uiBlock *block)
{
	SpaceImage *sima= sa->spacedata.first;
	rctf dispf;
	rcti *disprect= &G.scene->r.disprect;
	int winx= (G.scene->r.size*G.scene->r.xsch)/100;
	int winy= (G.scene->r.size*G.scene->r.ysch)/100;
	short mval[2];
	
	if(G.scene->r.mode & R_BORDER) {
		winx*= (G.scene->r.border.xmax - G.scene->r.border.xmin);
		winy*= (G.scene->r.border.ymax - G.scene->r.border.ymin);
	}
	
	/* while dragging we need to update the rects, otherwise it doesn't end with correct one */

	BLI_init_rctf(&dispf, 15.0f, (block->maxx - block->minx)-15.0f, 15.0f, (block->maxy - block->miny)-15.0f);
	ui_graphics_to_window_rct(sa->win, &dispf, disprect);
	
	/* correction for gla draw */
	BLI_translate_rcti(disprect, -curarea->winrct.xmin, -curarea->winrct.ymin);
	
	calc_image_view(sima, 'p');
//	printf("winrct %d %d %d %d\n", disprect->xmin, disprect->ymin,disprect->xmax, disprect->ymax);
	/* map to image space coordinates */
	mval[0]= disprect->xmin; mval[1]= disprect->ymin;
	areamouseco_to_ipoco(v2d, mval, &dispf.xmin, &dispf.ymin);
	mval[0]= disprect->xmax; mval[1]= disprect->ymax;
	areamouseco_to_ipoco(v2d, mval, &dispf.xmax, &dispf.ymax);
	
	/* map to render coordinates */
	disprect->xmin= dispf.xmin;
	disprect->xmax= dispf.xmax;
	disprect->ymin= dispf.ymin;
	disprect->ymax= dispf.ymax;
	
	CLAMP(disprect->xmin, 0, winx);
	CLAMP(disprect->xmax, 0, winx);
	CLAMP(disprect->ymin, 0, winy);
	CLAMP(disprect->ymax, 0, winy);
//	printf("drawrct %d %d %d %d\n", disprect->xmin, disprect->ymin,disprect->xmax, disprect->ymax);

}

static int is_preview_allowed(ScrArea *cur)
{
	SpaceImage *sima= cur->spacedata.first;
	ScrArea *sa;

	/* check if another areawindow has preview set */
	for(sa=G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa!=cur && sa->spacetype==SPACE_IMAGE) {
			if(image_preview_active(sa, NULL, NULL))
			   return 0;
		}
	}
	/* check image type */
	if(sima->image==NULL || sima->image->type!=IMA_TYPE_COMPOSITE)
		return 0;
	
	return 1;
}


static void image_panel_preview(ScrArea *sa, short cntrl)	// IMAGE_HANDLER_PREVIEW
{
	uiBlock *block;
	SpaceImage *sima= sa->spacedata.first;
	int ofsx, ofsy;
	
	if(is_preview_allowed(sa)==0) {
		rem_blockhandler(sa, IMAGE_HANDLER_PREVIEW);
		G.scene->r.scemode &= ~R_COMP_CROP;	/* quite weak */
		return;
	}
	
	block= uiBeginBlock(C, ar, "image_panel_preview", UI_EMBOSS);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | UI_PNL_SCALE | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_PREVIEW);  // for close and esc
	
	ofsx= -150+(sa->winx/2)/sima->blockscale;
	ofsy= -100+(sa->winy/2)/sima->blockscale;
	if(uiNewPanel(C, ar, block, "Preview", "Image", ofsx, ofsy, 300, 200)==0) return;
	
	uiBlockSetDrawExtraFunc(block, preview_cb);
	
}

static void image_panel_gpencil(short cntrl)	// IMAGE_HANDLER_GREASEPENCIL
{
	uiBlock *block;
	SpaceImage *sima;
	
	sima= curarea->spacedata.first;

	block= uiBeginBlock(C, ar, "image_panel_gpencil", UI_EMBOSS);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE  | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_GREASEPENCIL);  // for close and esc
	if (uiNewPanel(C, ar, block, "Grease Pencil", "SpaceImage", 100, 30, 318, 204)==0) return;
	
	/* allocate memory for gpd if drawing enabled (this must be done first or else we crash) */
	if (sima->flag & SI_DISPGP) {
		if (sima->gpd == NULL)
			gpencil_data_setactive(curarea, gpencil_data_addnew());
	}
	
	if (sima->flag & SI_DISPGP) {
		bGPdata *gpd= sima->gpd;
		short newheight;
		
		/* this is a variable height panel, newpanel doesnt force new size on existing panels */
		/* so first we make it default height */
		uiNewPanelHeight(block, 204);
		
		/* draw button for showing gpencil settings and drawings */
		uiDefButBitI(block, TOG, SI_DISPGP, B_REDR, "Use Grease Pencil", 10, 225, 150, 20, &sima->flag, 0, 0, 0, 0, "Display freehand annotations overlay over this Image/UV Editor (draw using Shift-LMB)");
		
		/* extend the panel if the contents won't fit */
		newheight= draw_gpencil_panel(block, gpd, curarea); 
		uiNewPanelHeight(block, newheight);
	}
	else {
		uiDefButBitI(block, TOG, SI_DISPGP, B_REDR, "Use Grease Pencil", 10, 225, 150, 20, &sima->flag, 0, 0, 0, 0, "Display freehand annotations overlay over this Image/UV Editor");
		uiDefBut(block, LABEL, 1, " ",	160, 180, 150, 20, NULL, 0.0, 0.0, 0, 0, "");
	}
}
#endif


/* ********************* callbacks for standard image buttons *************** */

/* called from fileselect or button */
static void load_image_cb(bContext *C, char *str, void *ima_pp_v, void *iuser_v)	
{
	Image **ima_pp= (Image **)ima_pp_v;
	Image *ima= NULL;
	
	ima= BKE_add_image_file(str, 0);
	if(ima) {
		if(*ima_pp) {
			(*ima_pp)->id.us--;
		}
		*ima_pp= ima;
		
		BKE_image_signal(ima, iuser_v, IMA_SIGNAL_RELOAD);
		WM_event_add_notifier(C, NC_IMAGE, ima);
		
		/* button event gets lost when it goes via filewindow */
//		if(G.buts && G.buts->lockpoin) {
//			Tex *tex= G.buts->lockpoin;
//			if(GS(tex->id.name)==ID_TE) {
//				BIF_preview_changed(ID_TE);
//				allqueue(REDRAWBUTSSHADING, 0);
//				allqueue(REDRAWVIEW3D, 0);
//				allqueue(REDRAWOOPS, 0);
//			}
//		}
	}
	
	ED_undo_push(C, "Load image");
}

static char *layer_menu(RenderResult *rr, short *curlay)
{
	RenderLayer *rl;
	int len= 64 + 32*BLI_countlist(&rr->layers);
	short a, nr= 0;
	char *str= MEM_callocN(len, "menu layers");
	
	strcpy(str, "Layer %t");
	a= strlen(str);
	
	/* compo result */
	if(rr->rectf) {
		a+= sprintf(str+a, "|Composite %%x0");
		nr= 1;
	}
	for(rl= rr->layers.first; rl; rl= rl->next, nr++) {
		a+= sprintf(str+a, "|%s %%x%d", rl->name, nr);
	}
	
	/* no curlay clip here, on render (redraws) the amount of layers can be 1 fir single-layer render */
	
	return str;
}

/* rl==NULL means composite result */
static char *pass_menu(RenderLayer *rl, short *curpass)
{
	RenderPass *rpass;
	int len= 64 + 32*(rl?BLI_countlist(&rl->passes):1);
	short a, nr= 0;
	char *str= MEM_callocN(len, "menu layers");
	
	strcpy(str, "Pass %t");
	a= strlen(str);
	
	/* rendered results don't have a Combined pass */
	if(rl==NULL || rl->rectf) {
		a+= sprintf(str+a, "|Combined %%x0");
		nr= 1;
	}
	
	if(rl)
		for(rpass= rl->passes.first; rpass; rpass= rpass->next, nr++)
			a+= sprintf(str+a, "|%s %%x%d", rpass->name, nr);
	
	if(*curpass >= nr)
		*curpass= 0;
	
	return str;
}

static void set_frames_cb(bContext *C, void *ima_v, void *iuser_v)
{
	Scene *scene= CTX_data_scene(C);
	Image *ima= ima_v;
	ImageUser *iuser= iuser_v;
	
	if(ima->anim) {
		iuser->frames = IMB_anim_get_duration(ima->anim);
		BKE_image_user_calc_imanr(iuser, scene->r.cfra, 0);
	}
}

static void image_src_change_cb(bContext *C, void *ima_v, void *iuser_v)
{
	BKE_image_signal(ima_v, iuser_v, IMA_SIGNAL_SRC_CHANGE);
}

/* buttons have 2 arg callbacks, filewindow has 3 args... so thats why the wrapper below */
static void image_browse_cb1(bContext *C, void *ima_pp_v, void *iuser_v)
{
	Image **ima_pp= (Image **)ima_pp_v;
	ImageUser *iuser= iuser_v;
	
	if(ima_pp) {
		Image *ima= *ima_pp;
		
		if(iuser->menunr== -2) {
			// XXX activate_databrowse_args(&ima->id, ID_IM, 0, &iuser->menunr, image_browse_cb1, ima_pp, iuser);
		} 
		else if (iuser->menunr>0) {
			Image *newima= (Image*) BLI_findlink(&CTX_data_main(C)->image, iuser->menunr-1);
			
			if (newima && newima!=ima) {
				*ima_pp= newima;
				id_us_plus(&newima->id);
				if(ima) ima->id.us--;
				
				BKE_image_signal(newima, iuser, IMA_SIGNAL_USER_NEW_IMAGE);
				
				ED_undo_push(C, "Browse image");
			}
		}
	}
}

static void image_browse_cb(bContext *C, void *ima_pp_v, void *iuser_v)
{
	image_browse_cb1(C, ima_pp_v, iuser_v);
}

static void image_reload_cb(bContext *C, void *ima_v, void *iuser_v)
{
	if(ima_v) {
		BKE_image_signal(ima_v, iuser_v, IMA_SIGNAL_RELOAD);
	}
}

static void image_field_test(bContext *C, void *ima_v, void *iuser_v)
{
	Image *ima= ima_v;
	
	if(ima) {
		ImBuf *ibuf= BKE_image_get_ibuf(ima, iuser_v);
		if(ibuf) {
			short nr= 0;
			if( !(ima->flag & IMA_FIELDS) && (ibuf->flags & IB_fields) ) nr= 1;
			if( (ima->flag & IMA_FIELDS) && !(ibuf->flags & IB_fields) ) nr= 1;
			if(nr) {
				BKE_image_signal(ima, iuser_v, IMA_SIGNAL_FREE);
			}
		}
	}
}

static void image_unlink_cb(bContext *C, void *ima_pp_v, void *unused)
{
	Image **ima_pp= (Image **)ima_pp_v;
	
	if(ima_pp && *ima_pp) {
		Image *ima= *ima_pp;
		/* (for time being, texturefaces are no users, conflict in design...) */
		if(ima->id.us>1)
			ima->id.us--;
		*ima_pp= NULL;
	}
}

static void image_load_fs_cb(bContext *C, void *ima_pp_v, void *iuser_v)
{
	ScrArea *sa= CTX_wm_area(C);
//	Image **ima_pp= (Image **)ima_pp_v;
	
	if(sa->spacetype==SPACE_IMAGE)
		WM_operator_name_call(C, "IMAGE_OT_open", WM_OP_INVOKE_REGION_WIN, NULL);
	else
		printf("not supported yet\n");
}

/* 5 layer button callbacks... */
static void image_multi_cb(bContext *C, void *rr_v, void *iuser_v) 
{
	BKE_image_multilayer_index(rr_v, iuser_v); 
	WM_event_add_notifier(C, NC_IMAGE|ND_DRAW, NULL);
}
static void image_multi_inclay_cb(bContext *C, void *rr_v, void *iuser_v) 
{
	RenderResult *rr= rr_v;
	ImageUser *iuser= iuser_v;
	int tot= BLI_countlist(&rr->layers) + (rr->rectf?1:0);  /* fake compo result layer */

	if(iuser->layer<tot-1) {
		iuser->layer++;
		BKE_image_multilayer_index(rr, iuser); 
		WM_event_add_notifier(C, NC_IMAGE|ND_DRAW, NULL);
	}
}
static void image_multi_declay_cb(bContext *C, void *rr_v, void *iuser_v) 
{
	ImageUser *iuser= iuser_v;

	if(iuser->layer>0) {
		iuser->layer--;
		BKE_image_multilayer_index(rr_v, iuser); 
		WM_event_add_notifier(C, NC_IMAGE|ND_DRAW, NULL);
	}
}
static void image_multi_incpass_cb(bContext *C, void *rr_v, void *iuser_v) 
{
	RenderResult *rr= rr_v;
	ImageUser *iuser= iuser_v;
	RenderLayer *rl= BLI_findlink(&rr->layers, iuser->layer);

	if(rl) {
		int tot= BLI_countlist(&rl->passes) + (rl->rectf?1:0);	/* builtin render result has no combined pass in list */
		if(iuser->pass<tot-1) {
			iuser->pass++;
			BKE_image_multilayer_index(rr, iuser); 
			WM_event_add_notifier(C, NC_IMAGE|ND_DRAW, NULL);
		}
	}
}
static void image_multi_decpass_cb(bContext *C, void *rr_v, void *iuser_v) 
{
	ImageUser *iuser= iuser_v;

	if(iuser->pass>0) {
		iuser->pass--;
		BKE_image_multilayer_index(rr_v, iuser); 
		WM_event_add_notifier(C, NC_IMAGE|ND_DRAW, NULL);
	}
}

static void image_pack_cb(bContext *C, void *ima_v, void *iuser_v) 
{
	if(ima_v) {
		Image *ima= ima_v;
		if(ima->source!=IMA_SRC_SEQUENCE && ima->source!=IMA_SRC_MOVIE) {
			if (ima->packedfile) {
				if (G.fileflags & G_AUTOPACK) {
					if (okee("Disable AutoPack ?")) {
						G.fileflags &= ~G_AUTOPACK;
					}
				}
				
				if ((G.fileflags & G_AUTOPACK) == 0) {
					unpackImage(NULL, ima, PF_ASK); /* XXX report errors */
					ED_undo_push(C, "Unpack image");
				}
			} 
			else {
				ImBuf *ibuf= BKE_image_get_ibuf(ima, iuser_v);
				if (ibuf && (ibuf->userflags & IB_BITMAPDIRTY)) {
					// XXX error("Can't pack painted image. Save image or use Repack as PNG.");
				} else {
					ima->packedfile = newPackedFile(NULL, ima->name); /* XXX report errors */
					ED_undo_push(C, "Pack image");
				}
			}
		}
	}
}

static void image_load_cb(bContext *C, void *ima_pp_v, void *iuser_v)
{
	if(ima_pp_v) {
		Image *ima= *((Image **)ima_pp_v);
		ImBuf *ibuf= BKE_image_get_ibuf(ima, iuser_v);
		char str[FILE_MAX];
	
		/* name in ima has been changed by button! */
		BLI_strncpy(str, ima->name, FILE_MAX);
		if(ibuf) BLI_strncpy(ima->name, ibuf->name, FILE_MAX);
		
		load_image_cb(C, str, ima_pp_v, iuser_v);
	}
}

static void image_freecache_cb(bContext *C, void *ima_v, void *unused) 
{
	Scene *scene= CTX_data_scene(C);
	BKE_image_free_anim_ibufs(ima_v, scene->r.cfra);
	WM_event_add_notifier(C, NC_IMAGE, ima_v);
}

static void image_generated_change_cb(bContext *C, void *ima_v, void *iuser_v)
{
	BKE_image_signal(ima_v, iuser_v, IMA_SIGNAL_FREE);
}

static void image_user_change(bContext *C, void *iuser_v, void *unused)
{
	Scene *scene= CTX_data_scene(C);
	BKE_image_user_calc_imanr(iuser_v, scene->r.cfra, 0);
}

static void uiblock_layer_pass_buttons(uiBlock *block, RenderResult *rr, ImageUser *iuser, int event, int x, int y, int w)
{
	uiBut *but;
	RenderLayer *rl= NULL;
	int wmenu1, wmenu2;
	char *strp;

	/* layer menu is 1/3 larger than pass */
	wmenu1= (3*w)/5;
	wmenu2= (2*w)/5;
	
	/* menu buts */
	strp= layer_menu(rr, &iuser->layer);
	but= uiDefButS(block, MENU, event, strp,					x, y, wmenu1, 20, &iuser->layer, 0,0,0,0, "Select Layer");
	uiButSetFunc(but, image_multi_cb, rr, iuser);
	MEM_freeN(strp);
	
	rl= BLI_findlink(&rr->layers, iuser->layer - (rr->rectf?1:0)); /* fake compo layer, return NULL is meant to be */
	strp= pass_menu(rl, &iuser->pass);
	but= uiDefButS(block, MENU, event, strp,					x+wmenu1, y, wmenu2, 20, &iuser->pass, 0,0,0,0, "Select Pass");
	uiButSetFunc(but, image_multi_cb, rr, iuser);
	MEM_freeN(strp);	
}

static void uiblock_layer_pass_arrow_buttons(uiBlock *block, RenderResult *rr, ImageUser *iuser, int imagechanged) 
{
	uiBut *but;
	
	if(rr==NULL || iuser==NULL)
		return;
	if(rr->layers.first==NULL) {
		uiDefBut(block, LABEL, 0, "No Layers in Render Result,",	10, 107, 300, 20, NULL, 1, 0, 0, 0, "");
		return;
	}
	
	uiBlockBeginAlign(block);

	/* decrease, increase arrows */
	but= uiDefIconBut(block, BUT, imagechanged, ICON_TRIA_LEFT,	10,107,17,20, NULL, 0, 0, 0, 0, "Previous Layer");
	uiButSetFunc(but, image_multi_declay_cb, rr, iuser);
	but= uiDefIconBut(block, BUT, imagechanged, ICON_TRIA_RIGHT,	27,107,18,20, NULL, 0, 0, 0, 0, "Next Layer");
	uiButSetFunc(but, image_multi_inclay_cb, rr, iuser);

	uiblock_layer_pass_buttons(block, rr, iuser, imagechanged, 45, 107, 230);

	/* decrease, increase arrows */
	but= uiDefIconBut(block, BUT, imagechanged, ICON_TRIA_LEFT,	275,107,17,20, NULL, 0, 0, 0, 0, "Previous Pass");
	uiButSetFunc(but, image_multi_decpass_cb, rr, iuser);
	but= uiDefIconBut(block, BUT, imagechanged, ICON_TRIA_RIGHT,	292,107,18,20, NULL, 0, 0, 0, 0, "Next Pass");
	uiButSetFunc(but, image_multi_incpass_cb, rr, iuser);

	uiBlockEndAlign(block);
			 
}

// XXX HACK!
static int packdummy=0;

/* The general Image panel with the loadsa callbacks! */
void ED_image_uiblock_panel(const bContext *C, uiBlock *block, Image **ima_pp, ImageUser *iuser, 
						 short redraw, short imagechanged)
{
	Scene *scene= CTX_data_scene(C);
	SpaceImage *sima= CTX_wm_space_image(C);
	Image *ima= *ima_pp;
	uiBut *but;
	char str[128], *strp;
	
	/* different stuff when we show viewer */
	if(ima && ima->source==IMA_SRC_VIEWER) {
		ImBuf *ibuf= BKE_image_get_ibuf(ima, iuser);
		
		image_info(ima, ibuf, str);
		uiDefBut(block, LABEL, 0, ima->id.name+2,	10, 180, 300, 20, NULL, 1, 0, 0, 0, "");
		uiDefBut(block, LABEL, 0, str,				10, 160, 300, 20, NULL, 1, 0, 0, 0, "");
		
		if(ima->type==IMA_TYPE_COMPOSITE) {
			iuser= ntree_get_active_iuser(scene->nodetree);
			if(iuser) {
				uiBlockBeginAlign(block);
				uiDefIconTextBut(block, BUT, B_SIMA_RECORD, ICON_REC, "Record",	10,120,100,20, 0, 0, 0, 0, 0, "");
				uiDefIconTextBut(block, BUT, B_SIMA_PLAY, ICON_PLAY, "Play",	110,120,100,20, 0, 0, 0, 0, 0, "");
				but= uiDefBut(block, BUT, B_NOP, "Free Cache",	210,120,100,20, 0, 0, 0, 0, 0, "");
				uiButSetFunc(but, image_freecache_cb, ima, NULL);
				
				if(iuser->frames)
					sprintf(str, "(%d) Frames:", iuser->framenr);
				else strcpy(str, "Frames:");
				uiBlockBeginAlign(block);
				uiDefButI(block, NUM, imagechanged, str,		10, 90,150, 20, &iuser->frames, 0.0, MAXFRAMEF, 0, 0, "Sets the number of images of a movie to use");
				uiDefButI(block, NUM, imagechanged, "StartFr:",	160,90,150,20, &iuser->sfra, 1.0, MAXFRAMEF, 0, 0, "Sets the global starting frame of the movie");
			}
		}
		else if(ima->type==IMA_TYPE_R_RESULT) {
			/* browse layer/passes */
			uiblock_layer_pass_arrow_buttons(block, RE_GetResult(RE_GetRender(scene->id.name)), iuser, imagechanged);
		}
		return;
	}
	
	/* the main ima source types */
	if(ima) {
// XXX		uiSetButLock(ima->id.lib!=NULL, ERROR_LIBDATA_MESSAGE);
		uiBlockBeginAlign(block);
		uiBlockSetFunc(block, image_src_change_cb, ima, iuser);
		uiDefButS(block, ROW, imagechanged, "Still",		0, 180, 105, 20, &ima->source, 0.0, IMA_SRC_FILE, 0, 0, "Single Image file");
		uiDefButS(block, ROW, imagechanged, "Movie",		105, 180, 105, 20, &ima->source, 0.0, IMA_SRC_MOVIE, 0, 0, "Movie file");
		uiDefButS(block, ROW, imagechanged, "Sequence",		210, 180, 105, 20, &ima->source, 0.0, IMA_SRC_SEQUENCE, 0, 0, "Multiple Image files, as a sequence");
		uiDefButS(block, ROW, imagechanged, "Generated",	315, 180, 105, 20, &ima->source, 0.0, IMA_SRC_GENERATED, 0, 0, "Generated Image");
		uiBlockSetFunc(block, NULL, NULL, NULL);
	}
	else
		uiDefBut(block, LABEL, 0, " ",					0, 180, 440, 20, 0, 0, 0, 0, 0, "");	/* for align in panel */
				 
	 /* Browse */
	 IMAnames_to_pupstring(&strp, NULL, NULL, &(CTX_data_main(C)->image), NULL, &iuser->menunr);
	 
	 uiBlockBeginAlign(block);
	 but= uiDefButS(block, MENU, imagechanged, strp,		0,155,40,20, &iuser->menunr, 0, 0, 0, 0, "Selects an existing Image or Movie");
	 uiButSetFunc(but, image_browse_cb, ima_pp, iuser);
	 
	 MEM_freeN(strp);
	 
	 /* name + options, or only load */
	 if(ima) {
		 int drawpack= (ima->source!=IMA_SRC_SEQUENCE && ima->source!=IMA_SRC_MOVIE && ima->ok);

		 but= uiDefBut(block, TEX, B_IDNAME, "IM:",				40, 155, 220, 20, ima->id.name+2, 0.0, 21.0, 0, 0, "Current Image Datablock name.");
		 uiButSetFunc(but, test_idbutton_cb, ima->id.name, NULL);
		 but= uiDefBut(block, BUT, imagechanged, "Reload",		260, 155, 70, 20, NULL, 0, 0, 0, 0, "Reloads Image or Movie");
		 uiButSetFunc(but, image_reload_cb, ima, iuser);
		 
		 but= uiDefIconBut(block, BUT, imagechanged, ICON_X,	330, 155, 40, 20, 0, 0, 0, 0, 0, "Unlink Image block");
		 uiButSetFunc(but, image_unlink_cb, ima_pp, NULL);
		 sprintf(str, "%d", ima->id.us);
		 uiDefBut(block, BUT, B_NOP, str,					370, 155, 40, 20, 0, 0, 0, 0, 0, "Only displays number of users of Image block");
		 uiBlockEndAlign(block);
		 
		 uiBlockBeginAlign(block);
		 but= uiDefIconBut(block, BUT, imagechanged, ICON_FILESEL,	0, 130, 40, 20, 0, 0, 0, 0, 0, "Open Fileselect to load new Image");
		 uiButSetFunc(but, image_load_fs_cb, ima_pp, iuser);
		 but= uiDefBut(block, TEX, imagechanged, "",				40,130, 340+(drawpack?0:20),20, ima->name, 0.0, 239.0, 0, 0, "Image/Movie file name, change to load new");
		 uiButSetFunc(but, image_load_cb, ima_pp, iuser);
		 uiBlockEndAlign(block);
		 
		 if(drawpack) {
			 if (ima->packedfile) packdummy = 1;
			 else packdummy = 0;
			 but= uiDefIconButBitI(block, TOG, 1, redraw, ICON_PACKAGE, 380, 130, 40, 20, &packdummy, 0, 0, 0, 0, "Toggles Packed status of this Image");
			 uiButSetFunc(but, image_pack_cb, ima, iuser);
		 }
		 
	 }
	 else {
		 but= uiDefBut(block, BUT, imagechanged, "Load",		33, 155, 200,20, NULL, 0, 0, 0, 0, "Load new Image of Movie");
		 uiButSetFunc(but, image_load_fs_cb, ima_pp, iuser);
	 }
	 uiBlockEndAlign(block);
	 
	 if(ima) {
		 ImBuf *ibuf= BKE_image_get_ibuf(ima, iuser);
		 
		 /* check for re-render, only buttons */
		 if(imagechanged==B_IMAGECHANGED) {
			 if(iuser->flag & IMA_ANIM_REFRESHED) {
				 iuser->flag &= ~IMA_ANIM_REFRESHED;
				 WM_event_add_notifier(C, NC_IMAGE, ima);
			 }
		 }
		 
		 /* multilayer? */
		 if(ima->type==IMA_TYPE_MULTILAYER && ima->rr) {
			 uiblock_layer_pass_arrow_buttons(block, ima->rr, iuser, imagechanged);
		 }
		 else {
			 image_info(ima, ibuf, str);
			 uiDefBut(block, LABEL, 0, str,		10, 112, 300, 20, NULL, 1, 0, 0, 0, "");
		 }
		 
		 /* exception, let's do because we only use this panel 3 times in blender... but not real good code! */
		 if( (paint_facesel_test(CTX_data_active_object(C))) && sima && &sima->iuser==iuser)
			 return;
		 /* left side default per-image options, right half the additional options */
		 
		 /* fields */
		 
		 but= uiDefButBitS(block, TOGBUT, IMA_FIELDS, imagechanged, "Fields",	0, 80, 200, 20, &ima->flag, 0, 0, 0, 0, "Click to enable use of fields in Image");
		 uiButSetFunc(but, image_field_test, ima, iuser);
		 uiDefButBitS(block, TOGBUT, IMA_STD_FIELD, B_NOP, "Odd",				0, 55, 200, 20, &ima->flag, 0, 0, 0, 0, "Standard Field Toggle");
		
		 
		 uiBlockSetFunc(block, image_reload_cb, ima, iuser);
		 uiDefButBitS(block, TOGBUT, IMA_ANTIALI, B_NOP, "Anti",				0, 5, 200, 20, &ima->flag, 0, 0, 0, 0, "Toggles Image anti-aliasing, only works with solid colors");
		 uiDefButBitS(block, TOGBUT, IMA_DO_PREMUL, imagechanged, "Premul",		0, -20, 200, 20, &ima->flag, 0, 0, 0, 0, "Toggles premultiplying alpha");
		 
		 
		 if( ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
			 sprintf(str, "(%d) Frames:", iuser->framenr);
			 
			 //uiBlockBeginAlign(block);
			 uiBlockSetFunc(block, image_user_change, iuser, NULL);
			 			 
			 if(ima->anim) {
			 	 uiBlockBeginAlign(block);
				 uiDefButI(block, NUM, imagechanged, str,						220, 80, 160, 20, &iuser->frames, 0.0, MAXFRAMEF, 0, 0, "Sets the number of images of a movie to use");
				 but= uiDefBut(block, BUT, redraw, "<",							380, 80, 40, 20, 0, 0, 0, 0, 0, "Copies number of frames in movie file to Frames: button");
				 uiButSetFunc(but, set_frames_cb, ima, iuser);
				 uiBlockEndAlign(block);
			 }
			 else 
				 uiDefButI(block, NUM, imagechanged, str,						220, 80, 200, 20, &iuser->frames, 0.0, MAXFRAMEF, 0, 0, "Sets the number of images of a movie to use");
			 
			 uiDefButI(block, NUM, imagechanged, "Start Frame:",				220, 55, 200, 20, &iuser->sfra, 1.0, MAXFRAMEF, 0, 0, "Sets the global starting frame of the movie");
			 uiDefButI(block, NUM, imagechanged, "Offset:",						220, 30, 200, 20, &iuser->offset, -MAXFRAMEF, MAXFRAMEF, 0, 0, "Offsets the number of the frame to use in the animation");
			 uiDefButS(block, NUM, imagechanged, "Fields:",						0, 30, 200, 20, &iuser->fie_ima, 1.0, 200.0, 0, 0, "The number of fields per rendered frame (2 fields is 1 image)");
			 
			uiDefButBitS(block, TOG, IMA_ANIM_ALWAYS, B_NOP, "Auto Refresh",	220, 5, 200, 20, &iuser->flag, 0, 0, 0, 0, "Always refresh Image on frame changes");

			 uiDefButS(block, TOG, imagechanged, "Cyclic",						220, -20, 200, 20, &iuser->cycl, 0.0, 1.0, 0, 0, "Cycle the images in the movie");
			 
			 uiBlockSetFunc(block, NULL, iuser, NULL);
		 }
		 else if(ima->source==IMA_SRC_GENERATED) {
			 
			 uiDefBut(block, LABEL, 0, "Size:",					220, 80, 200, 20, 0, 0, 0, 0, 0, "");	

			 uiBlockBeginAlign(block);
			 uiBlockSetFunc(block, image_generated_change_cb, ima, iuser);
			 uiDefButS(block, NUM, imagechanged, "X:",	220, 55,200,20, &ima->gen_x, 1.0, 5000.0, 0, 0, "Image size x");
			 uiDefButS(block, NUM, imagechanged, "Y:",	220, 35,200,20, &ima->gen_y, 1.0, 5000.0, 0, 0, "Image size y");
			 uiBlockEndAlign(block);
			 
			 uiDefButS(block, TOGBUT, imagechanged, "UV Test grid", 220,10,200,20, &ima->gen_type, 0.0, 1.0, 0, 0, "");
			 uiBlockSetFunc(block, NULL, NULL, NULL);
		 }
	 }
	 uiBlockEndAlign(block);
}	

void uiTemplateImageLayers(uiLayout *layout, bContext *C, Image *ima, ImageUser *iuser)
{
	uiBlock *block= uiLayoutFreeBlock(layout);
	Scene *scene= CTX_data_scene(C);
	RenderResult *rr;

	/* render layers and passes */
	if(ima && iuser) {
		rr= BKE_image_get_renderresult(scene, ima);

		if(rr) {
			uiBlockBeginAlign(block);
			uiblock_layer_pass_buttons(block, rr, iuser, 0, 0, 0, 160);
			uiBlockEndAlign(block);
		}
	}
}

static void image_panel_properties(const bContext *C, Panel *pa)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	uiBlock *block;
	
	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_image_panel_events, NULL);

	/* note, it draws no bottom half in facemode, for vertex buttons */
	ED_image_uiblock_panel(C, block, &sima->image, &sima->iuser, B_REDR, B_REDR);
	image_editvertex_buts(C, block);
}	

void image_buttons_register(ARegionType *art)
{
	PanelType *pt;

	pt= MEM_callocN(sizeof(PanelType), "spacetype image panel properties");
	strcpy(pt->idname, "IMAGE_PT_properties");
	strcpy(pt->label, "Image Properties");
	pt->draw= image_panel_properties;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype image panel paint");
	strcpy(pt->idname, "IMAGE_PT_paint");
	strcpy(pt->label, "Paint");
	pt->draw= image_panel_paint;
	pt->poll= image_panel_paint_poll;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype image panel paint color");
	strcpy(pt->idname, "IMAGE_PT_paint_color");
	strcpy(pt->label, "Paint Color");
	pt->draw= image_panel_paintcolor;
	pt->poll= image_panel_paint_poll;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype image panel curves");
	strcpy(pt->idname, "IMAGE_PT_curves");
	strcpy(pt->label, "Curves");
	pt->draw= image_panel_curves;
	BLI_addtail(&art->paneltypes, pt);
	
	pt= MEM_callocN(sizeof(PanelType), "spacetype image panel gpencil");
	strcpy(pt->idname, "IMAGE_PT_gpencil");
	strcpy(pt->label, "Grease Pencil");
	pt->draw= gpencil_panel_standard;
	BLI_addtail(&art->paneltypes, pt);
}

static int image_properties(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= image_has_buttons_region(sa);
	
	if(ar) {
		ar->flag ^= RGN_FLAG_HIDDEN;
		ar->v2d.flag &= ~V2D_IS_INITIALISED; /* XXX should become hide/unhide api? */
		
		ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
		ED_area_tag_redraw(sa);
	}
	return OPERATOR_FINISHED;
}

void IMAGE_OT_properties(wmOperatorType *ot)
{
	ot->name= "Properties";
	ot->idname= "IMAGE_OT_properties";
	
	ot->exec= image_properties;
	ot->poll= ED_operator_image_active;
	
	/* flags */
	ot->flag= 0;
}



