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
static int simaFaceDraw_Check() {return 0;}
static int simaUVSel_Check() {return 0;}
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
		ofs= sprintf(str, "Movie");
		if(ima->anim) 
			ofs+= sprintf(str+ofs, "%d frs", IMB_anim_get_duration(ima->anim));
	}
	else
	 	ofs= sprintf(str, "Image");
	
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
	ED_space_image_size(sima, imx, imy);
	
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
//		uiDefButF(block, NUMSLI, evt_nop, "Falloff ",		0,yco-60,180,19, &brush->innerradius, 0.0, 1.0, 0, 0, "The fall off radius of the brush");
//		uiDefIconButBitS(block, TOG|BIT, BRUSH_RAD_PRESSURE, evt_nop, ICON_STYLUS_PRESSURE,	180,yco-60,20,19, &brush->flag, 0, 0, 0, 0, "Enables pressure sensitivity for tablets");
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

static int image_panel_poll(const bContext *C, PanelType *pt)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	
	return ED_space_image_has_buffer(sima);
}

static void image_panel_curves(const bContext *C, Panel *pa)
{
	bScreen *sc= CTX_wm_screen(C);
	SpaceImage *sima= CTX_wm_space_image(C);
	ImBuf *ibuf;
	PointerRNA simaptr;
	int levels;
	void *lock;
	
	ibuf= ED_space_image_acquire_buffer(sima, &lock);
	
	if(ibuf) {
		if(sima->cumap==NULL)
			sima->cumap= curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);

		/* curvemap black/white levels only works for RGBA */
		levels= (ibuf->channels==4);

		RNA_pointer_create(&sc->id, &RNA_SpaceImageEditor, sima, &simaptr);
		uiTemplateCurveMapping(pa->layout, &simaptr, "curves", 'c', levels);
	}

	ED_space_image_release_buffer(sima, lock);
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
#endif


/* ********************* callbacks for standard image buttons *************** */

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

#if 0
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
#endif

#if 0
static void image_freecache_cb(bContext *C, void *ima_v, void *unused) 
{
	Scene *scene= CTX_data_scene(C);
	BKE_image_free_anim_ibufs(ima_v, scene->r.cfra);
	WM_event_add_notifier(C, NC_IMAGE, ima_v);
}
#endif

#if 0
static void image_user_change(bContext *C, void *iuser_v, void *unused)
{
	Scene *scene= CTX_data_scene(C);
	BKE_image_user_calc_imanr(iuser_v, scene->r.cfra, 0);
}
#endif

static void uiblock_layer_pass_buttons(uiLayout *layout, RenderResult *rr, ImageUser *iuser, int w)
{
	uiBlock *block= uiLayoutGetBlock(layout);
	uiBut *but;
	RenderLayer *rl= NULL;
	int wmenu1, wmenu2;
	char *strp;

	uiLayoutRow(layout, 1);

	/* layer menu is 1/3 larger than pass */
	wmenu1= (3*w)/5;
	wmenu2= (2*w)/5;
	
	/* menu buts */
	strp= layer_menu(rr, &iuser->layer);
	but= uiDefButS(block, MENU, 0, strp,					0, 0, wmenu1, 20, &iuser->layer, 0,0,0,0, "Select Layer");
	uiButSetFunc(but, image_multi_cb, rr, iuser);
	MEM_freeN(strp);
	
	rl= BLI_findlink(&rr->layers, iuser->layer - (rr->rectf?1:0)); /* fake compo layer, return NULL is meant to be */
	strp= pass_menu(rl, &iuser->pass);
	but= uiDefButS(block, MENU, 0, strp,					0, 0, wmenu2, 20, &iuser->pass, 0,0,0,0, "Select Pass");
	uiButSetFunc(but, image_multi_cb, rr, iuser);
	MEM_freeN(strp);	
}

static void uiblock_layer_pass_arrow_buttons(uiLayout *layout, RenderResult *rr, ImageUser *iuser) 
{
	uiBlock *block= uiLayoutGetBlock(layout);
	uiLayout *row;
	uiBut *but;
	
	row= uiLayoutRow(layout, 1);

	if(rr==NULL || iuser==NULL)
		return;
	if(rr->layers.first==NULL) {
		uiItemL(row, "No Layers in Render Result.", 0);
		return;
	}

	/* decrease, increase arrows */
	but= uiDefIconBut(block, BUT, 0, ICON_TRIA_LEFT,	0,0,17,20, NULL, 0, 0, 0, 0, "Previous Layer");
	uiButSetFunc(but, image_multi_declay_cb, rr, iuser);
	but= uiDefIconBut(block, BUT, 0, ICON_TRIA_RIGHT,	0,0,18,20, NULL, 0, 0, 0, 0, "Next Layer");
	uiButSetFunc(but, image_multi_inclay_cb, rr, iuser);

	uiblock_layer_pass_buttons(row, rr, iuser, 230);

	/* decrease, increase arrows */
	but= uiDefIconBut(block, BUT, 0, ICON_TRIA_LEFT,	0,0,17,20, NULL, 0, 0, 0, 0, "Previous Pass");
	uiButSetFunc(but, image_multi_decpass_cb, rr, iuser);
	but= uiDefIconBut(block, BUT, 0, ICON_TRIA_RIGHT,	0,0,18,20, NULL, 0, 0, 0, 0, "Next Pass");
	uiButSetFunc(but, image_multi_incpass_cb, rr, iuser);

	uiBlockEndAlign(block);
}

// XXX HACK!
// static int packdummy=0;

typedef struct RNAUpdateCb {
	PointerRNA ptr;
	PropertyRNA *prop;
	ImageUser *iuser;
} RNAUpdateCb;

static void rna_update_cb(bContext *C, void *arg_cb, void *arg_unused)
{
	RNAUpdateCb *cb= (RNAUpdateCb*)arg_cb;

	/* ideally this would be done by RNA itself, but there we have
	   no image user available, so we just update this flag here */
	cb->iuser->ok= 1;

	/* we call update here on the pointer property, this way the
	   owner of the image pointer can still define it's own update
	   and notifier */
	RNA_property_update(C, &cb->ptr, cb->prop);
}

void uiTemplateImage(uiLayout *layout, bContext *C, PointerRNA *ptr, char *propname, PointerRNA *userptr, int compact)
{
	PropertyRNA *prop;
	PointerRNA imaptr;
	RNAUpdateCb *cb;
	Image *ima;
	ImageUser *iuser;
	ImBuf *ibuf;
	Scene *scene= CTX_data_scene(C);
	uiLayout *row, *split, *col;
	uiBlock *block;
	uiBut *but;
	char str[128];
	void *lock;

	if(!ptr->data)
		return;
	
	prop= RNA_struct_find_property(ptr, propname);
	if(!prop) {
		printf("uiTemplateImage: property not found: %s\n", propname);
		return;
	}

	block= uiLayoutGetBlock(layout);

	imaptr= RNA_property_pointer_get(ptr, prop);
	ima= imaptr.data;
	iuser= userptr->data;

	cb= MEM_callocN(sizeof(RNAUpdateCb), "RNAUpdateCb");
	cb->ptr= *ptr;
	cb->prop= prop;
	cb->iuser= iuser;

	if(!compact)
		uiTemplateID(layout, C, ptr, propname, "IMAGE_OT_new", "IMAGE_OT_open", NULL);

	// XXX missing: reload, pack

	if(ima) {
		uiBlockSetNFunc(block, rna_update_cb, MEM_dupallocN(cb), NULL);

		if(ima->source == IMA_SRC_VIEWER) {
			ibuf= BKE_image_acquire_ibuf(ima, iuser, &lock);
			image_info(ima, ibuf, str);
			BKE_image_release_ibuf(ima, lock);

			uiItemL(layout, ima->id.name+2, 0);
			uiItemL(layout, str, 0);

			if(ima->type==IMA_TYPE_COMPOSITE) {
				// XXX not working yet
#if 0
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
#endif
			}
			else if(ima->type==IMA_TYPE_R_RESULT) {
				/* browse layer/passes */
				Render *re= RE_GetRender(scene->id.name);
				RenderResult *rr= RE_AcquireResultRead(re);
				uiblock_layer_pass_arrow_buttons(layout, rr, iuser);
				RE_ReleaseResult(re);
			}
		}
		else {
			row= uiLayoutRow(layout, 0);
			uiItemR(row, NULL, 0, &imaptr, "source", (compact)? 0: UI_ITEM_R_EXPAND);

			if(ima->source != IMA_SRC_GENERATED) {
				row= uiLayoutRow(layout, 0);
				uiItemR(row, "", 0, &imaptr, "filename", 0);
				//uiItemO(row, "Reload", 0, "image.reload");
			}

			// XXX what was this for?
#if 0
			 /* check for re-render, only buttons */
			if(imagechanged==B_IMAGECHANGED) {
				if(iuser->flag & IMA_ANIM_REFRESHED) {
					iuser->flag &= ~IMA_ANIM_REFRESHED;
					WM_event_add_notifier(C, NC_IMAGE, ima);
				}
			}
#endif

			/* multilayer? */
			if(ima->type==IMA_TYPE_MULTILAYER && ima->rr) {
				uiblock_layer_pass_arrow_buttons(layout, ima->rr, iuser);
			}
			else if(ima->source != IMA_SRC_GENERATED) {
				ibuf= BKE_image_acquire_ibuf(ima, iuser, &lock);
				image_info(ima, ibuf, str);
				BKE_image_release_ibuf(ima, lock);
				uiItemL(layout, str, 0);
			}
			
			if(ima->source != IMA_SRC_GENERATED) {
				uiItemS(layout);

				split= uiLayoutSplit(layout, 0);

				col= uiLayoutColumn(split, 0);
				uiItemR(col, NULL, 0, &imaptr, "fields", 0);
				row= uiLayoutRow(col, 0);
				uiItemR(row, NULL, 0, &imaptr, "field_order", UI_ITEM_R_EXPAND);
				uiLayoutSetActive(row, RNA_boolean_get(&imaptr, "fields"));

				col= uiLayoutColumn(split, 0);
				uiItemR(col, NULL, 0, &imaptr, "antialias", 0);
				uiItemR(col, NULL, 0, &imaptr, "premultiply", 0);
			}

			if(ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
				uiItemS(layout);
				
				split= uiLayoutSplit(layout, 0);

				col= uiLayoutColumn(split, 0);
				 
				sprintf(str, "(%d) Frames", iuser->framenr);
				row= uiLayoutRow(col, 1);
				uiItemR(col, str, 0, userptr, "frames", 0);
				if(ima->anim) {
					block= uiLayoutGetBlock(row);
					but= uiDefBut(block, BUT, 0, "<", 0, 0, UI_UNIT_X*2, UI_UNIT_Y, 0, 0, 0, 0, 0, "Set the number of frames from the movie or sequence.");
					uiButSetFunc(but, set_frames_cb, ima, iuser);
				}

				uiItemR(col, "Start", 0, userptr, "start_frame", 0);
				uiItemR(col, NULL, 0, userptr, "offset", 0);

				col= uiLayoutColumn(split, 0);
				uiItemR(col, "Fields", 0, userptr, "fields_per_frame", 0);
				uiItemR(col, NULL, 0, userptr, "auto_refresh", 0);
				uiItemR(col, NULL, 0, userptr, "cyclic", 0);
			}
			else if(ima->source==IMA_SRC_GENERATED) {
				split= uiLayoutSplit(layout, 0);

				col= uiLayoutColumn(split, 1);
				uiItemR(col, "X", 0, &imaptr, "generated_width", 0);
				uiItemR(col, "Y", 0, &imaptr, "generated_height", 0);

				col= uiLayoutColumn(split, 0);
				uiItemR(col, NULL, 0, &imaptr, "generated_type", UI_ITEM_R_EXPAND);
			}

					}

		uiBlockSetNFunc(block, NULL, NULL, NULL);
	}

	MEM_freeN(cb);
}

void uiTemplateImageLayers(uiLayout *layout, bContext *C, Image *ima, ImageUser *iuser)
{
	Scene *scene= CTX_data_scene(C);
	RenderResult *rr;

	/* render layers and passes */
	if(ima && iuser) {
		rr= BKE_image_acquire_renderresult(scene, ima);

		if(rr)
			uiblock_layer_pass_buttons(layout, rr, iuser, 160);

		BKE_image_release_renderresult(scene, ima);
	}
}

static int image_panel_uv_poll(const bContext *C, PanelType *pt)
{
	Object *obedit= CTX_data_edit_object(C);
	return ED_uvedit_test(obedit);
}

static void image_panel_uv(const bContext *C, Panel *pa)
{
	ARegion *ar= CTX_wm_region(C);
	uiBlock *block;
	
	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_image_panel_events, NULL);

	image_editvertex_buts(C, block);
	image_editcursor_buts(C, &ar->v2d, block);
}	

void image_buttons_register(ARegionType *art)
{
	PanelType *pt;

	pt= MEM_callocN(sizeof(PanelType), "spacetype image panel uv");
	strcpy(pt->idname, "IMAGE_PT_uv");
	strcpy(pt->label, "UV");
	pt->draw= image_panel_uv;
	pt->poll= image_panel_uv_poll;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype image panel curves");
	strcpy(pt->idname, "IMAGE_PT_curves");
	strcpy(pt->label, "Curves");
	pt->draw= image_panel_curves;
	pt->poll= image_panel_poll;
	pt->flag |= PNL_DEFAULT_CLOSED;
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
	
	if(ar)
		ED_region_toggle_hidden(C, ar);

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



