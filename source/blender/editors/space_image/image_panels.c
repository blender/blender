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

#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_uvedit.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "image_intern.h"


#if 0

/* ************ panel stuff ************* */

/* this function gets the values for cursor and vertex number buttons */
static void image_transform_but_attr(int *imx, int *imy, int *step, int *digits) /*, float *xcoord, float *ycoord)*/
{
	ImBuf *ibuf= imagewindow_get_ibuf(G.sima);
	if(ibuf) {
		*imx= ibuf->x;
		*imy= ibuf->y;
	}
	
	if (G.sima->flag & SI_COORDFLOATS) {
		*step= 1;
		*digits= 3;
	}
	else {
		*step= 100;
		*digits= 2;
	}
}


/* is used for both read and write... */
void image_editvertex_buts(uiBlock *block)
{
	static float ocent[2];
	float cent[2]= {0.0, 0.0};
	int imx= 256, imy= 256;
	int nactive= 0, step, digits;
	EditMesh *em = G.editMesh;
	EditFace *efa;
	MTFace *tf;
	
	if( is_uv_tface_editing_allowed_silent()==0 ) return;
	
	image_transform_but_attr(&imx, &imy, &step, &digits);
	
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
			if (G.sima->flag & SI_COORDFLOATS) {
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
			
		if (G.sima->flag & SI_COORDFLOATS) {
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
			
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
	}
}


/* is used for both read and write... */
void image_editcursor_buts(uiBlock *block)
{
	static float ocent[2];
	int imx= 256, imy= 256;
	int step, digits;
	
	if( is_uv_tface_editing_allowed_silent()==0 ) return;
	
	image_transform_but_attr(&imx, &imy, &step, &digits);
		
	if(block) {	// do the buttons
		ocent[0]= G.v2d->cursor[0];
		ocent[1]= G.v2d->cursor[1];
		if (G.sima->flag & SI_COORDFLOATS) {
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
		if (G.sima->flag & SI_COORDFLOATS) {
			G.v2d->cursor[0]= ocent[0];
			G.v2d->cursor[1]= ocent[1];
		}
		else {
			G.v2d->cursor[0]= ocent[0]/imx;
			G.v2d->cursor[1]= ocent[1]/imy;
		}
		allqueue(REDRAWIMAGE, 0);
	}
}


void image_info(Image *ima, ImBuf *ibuf, char *str)
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

static void image_panel_game_properties(short cntrl)	// IMAGE_HANDLER_GAME_PROPERTIES
{
	ImBuf *ibuf= BKE_image_get_ibuf(G.sima->image, &G.sima->iuser);
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "image_panel_game_properties", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_GAME_PROPERTIES);  // for close and esc
	if(uiNewPanel(curarea, block, "Real-time Properties", "Image", 10, 10, 318, 204)==0)
		return;

	if (ibuf) {
		char str[128];
		
		image_info(G.sima->image, ibuf, str);
		uiDefBut(block, LABEL, B_NOP, str,		10,180,300,19, 0, 0, 0, 0, 0, "");

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, IMA_TWINANIM, B_TWINANIM, "Anim", 10,150,140,19, &G.sima->image->tpageflag, 0, 0, 0, 0, "Toggles use of animated texture");
		uiDefButS(block, NUM, B_TWINANIM, "Start:",		10,130,140,19, &G.sima->image->twsta, 0.0, 128.0, 0, 0, "Displays the start frame of an animated texture");
		uiDefButS(block, NUM, B_TWINANIM, "End:",		10,110,140,19, &G.sima->image->twend, 0.0, 128.0, 0, 0, "Displays the end frame of an animated texture");
		uiDefButS(block, NUM, B_NOP, "Speed", 				10,90,140,19, &G.sima->image->animspeed, 1.0, 100.0, 0, 0, "Displays Speed of the animation in frames per second");
		uiBlockEndAlign(block);

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, IMA_TILES, B_SIMAGETILE, "Tiles",	160,150,140,19, &G.sima->image->tpageflag, 0, 0, 0, 0, "Toggles use of tilemode for faces (Shift LMB to pick the tile for selected faces)");
		uiDefButS(block, NUM, B_SIMA_REDR_IMA_3D, "X:",		160,130,70,19, &G.sima->image->xrep, 1.0, 16.0, 0, 0, "Sets the degree of repetition in the X direction");
		uiDefButS(block, NUM, B_SIMA_REDR_IMA_3D, "Y:",		230,130,70,19, &G.sima->image->yrep, 1.0, 16.0, 0, 0, "Sets the degree of repetition in the Y direction");
		uiBlockBeginAlign(block);

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, IMA_CLAMP_U, B_SIMA3DVIEWDRAW, "ClampX",	160,100,70,19, &G.sima->image->tpageflag, 0, 0, 0, 0, "Disable texture repeating horizontaly");
		uiDefButBitS(block, TOG, IMA_CLAMP_V, B_SIMA3DVIEWDRAW, "ClampY",	230,100,70,19, &G.sima->image->tpageflag, 0, 0, 0, 0, "Disable texture repeating vertically");
		uiBlockEndAlign(block);
	}
}

static void image_panel_view_properties(short cntrl)	// IMAGE_HANDLER_VIEW_PROPERTIES
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "image_view_properties", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_VIEW_PROPERTIES);  // for close and esc
	if(uiNewPanel(curarea, block, "View Properties", "Image", 10, 10, 318, 204)==0)
		return;
	
	
	uiDefButBitI(block, TOG, SI_DRAW_TILE, B_REDR, "Repeat Image",	10,160,140,19, &G.sima->flag, 0, 0, 0, 0, "Repeat/Tile the image display");
	uiDefButBitI(block, TOG, SI_COORDFLOATS, B_REDR, "Normalized Coords",	165,160,145,19, &G.sima->flag, 0, 0, 0, 0, "Display coords from 0.0 to 1.0 rather then in pixels");
	
	
	if (G.sima->image) {
		uiDefBut(block, LABEL, B_NOP, "Image Display:",		10,140,140,19, 0, 0, 0, 0, 0, "");
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_REDR, "AspX:", 10,120,140,19, &G.sima->image->aspx, 0.1, 5000.0, 100, 0, "X Display Aspect for this image, does not affect renderingm 0 disables.");
		uiDefButF(block, NUM, B_REDR, "AspY:", 10,100,140,19, &G.sima->image->aspy, 0.1, 5000.0, 100, 0, "X Display Aspect for this image, does not affect rendering 0 disables.");
		uiBlockEndAlign(block);
	}
	
	
	if (EM_texFaceCheck()) {
		uiDefBut(block, LABEL, B_NOP, "Draw Type:",		10, 80,120,19, 0, 0, 0, 0, 0, "");
		uiBlockBeginAlign(block);
		uiDefButC(block,  ROW, B_REDR, "Outline",		10,60,58,19, &G.sima->dt_uv, 0.0, SI_UVDT_OUTLINE, 0, 0, "Outline Wire UV drawtype");
		uiDefButC(block,  ROW, B_REDR, "Dash",			68, 60,58,19, &G.sima->dt_uv, 0.0, SI_UVDT_DASH, 0, 0, "Dashed Wire UV drawtype");
		uiDefButC(block,  ROW, B_REDR, "Black",			126, 60,58,19, &G.sima->dt_uv, 0.0, SI_UVDT_BLACK, 0, 0, "Black Wire UV drawtype");
		uiDefButC(block,  ROW, B_REDR, "White",			184,60,58,19, &G.sima->dt_uv, 0.0, SI_UVDT_WHITE, 0, 0, "White Wire UV drawtype");
		
		uiBlockEndAlign(block);
		uiDefButBitI(block, TOG, SI_SMOOTH_UV, B_REDR, "Smooth",	250,60,60,19,  &G.sima->flag, 0, 0, 0, 0, "Display smooth lines in the UV view");
		
		
		uiDefButBitI(block, TOG, G_DRAWFACES, B_REDR, "Faces",		10,30,60,19,  &G.f, 0, 0, 0, 0, "Displays all faces as shades in the 3d view and UV editor");
		uiDefButBitI(block, TOG, G_DRAWEDGES, B_REDR, "Edges", 70, 30,60,19, &G.f, 0, 0, 0, 0, "Displays selected edges using hilights in the 3d view and UV editor");
		
		uiDefButBitI(block, TOG, SI_DRAWSHADOW, B_REDR, "Final Shadow", 130, 30,110,19, &G.sima->flag, 0, 0, 0, 0, "Draw the final result from the objects modifiers");
		
		uiDefButBitI(block, TOG, SI_DRAW_STRETCH, B_REDR, "UV Stretch",	10,0,100,19,  &G.sima->flag, 0, 0, 0, 0, "Difference between UV's and the 3D coords (blue for low distortion, red is high)");
		if (G.sima->flag & SI_DRAW_STRETCH) {
			uiBlockBeginAlign(block);
			uiDefButC(block,  ROW, B_REDR, "Area",			120,0,60,19, &G.sima->dt_uvstretch, 0.0, SI_UVDT_STRETCH_AREA, 0, 0, "Area distortion between UV's and 3D coords");
			uiDefButC(block,  ROW, B_REDR, "Angle",		180,0,60,19, &G.sima->dt_uvstretch, 0.0, SI_UVDT_STRETCH_ANGLE, 0, 0, "Angle distortion between UV's and 3D coords");
			uiBlockEndAlign(block);
		}
		
	}
	image_editcursor_buts(block);
}

static void image_panel_paint(short cntrl)	// IMAGE_HANDLER_PAINT
{
	uiBlock *block;
	if ((G.sima->image && (G.sima->flag & SI_DRAWTOOL))==0) {
		return;
	}
	
	block= uiNewBlock(&curarea->uiblocks, "image_panel_paint", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_PAINT);  // for close and esc
	if(uiNewPanel(curarea, block, "Image Paint", "Image", 10, 230, 318, 204)==0)
		return;
	
	brush_buttons(block, 1, B_SIMANOTHING, B_SIMABRUSHCHANGE, B_SIMABRUSHBROWSE, B_SIMABRUSHLOCAL, B_SIMABRUSHDELETE, B_KEEPDATA, B_SIMABTEXBROWSE, B_SIMABTEXDELETE);
}

static void image_panel_curves_reset(void *cumap_v, void *ibuf_v)
{
	CurveMapping *cumap = cumap_v;
	int a;
	
	for(a=0; a<CM_TOT; a++)
		curvemap_reset(cumap->cm+a, &cumap->clipr);
	
	cumap->black[0]=cumap->black[1]=cumap->black[2]= 0.0f;
	cumap->white[0]=cumap->white[1]=cumap->white[2]= 1.0f;
	curvemapping_set_black_white(cumap, NULL, NULL);
	
	curvemapping_changed(cumap, 0);
	curvemapping_do_ibuf(cumap, ibuf_v);
	
	allqueue(REDRAWIMAGE, 0);
}


static void image_panel_curves(short cntrl)	// IMAGE_HANDLER_CURVES
{
	ImBuf *ibuf;
	uiBlock *block;
	uiBut *bt;
	
	/* and we check for spare */
	ibuf= imagewindow_get_ibuf(G.sima);
	
	block= uiNewBlock(&curarea->uiblocks, "image_panel_curves", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_CURVES);  // for close and esc
	if(uiNewPanel(curarea, block, "Curves", "Image", 10, 450, 318, 204)==0)
		return;
	
	if (ibuf) {
		rctf rect;
		
		if(G.sima->cumap==NULL)
			G.sima->cumap= curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
		
		rect.xmin= 110; rect.xmax= 310;
		rect.ymin= 10; rect.ymax= 200;
		curvemap_buttons(block, G.sima->cumap, 'c', B_SIMACURVES, B_REDR, &rect);
		
		/* curvemap min/max only works for RGBA */
		if(ibuf->channels==4) {
			bt=uiDefBut(block, BUT, B_SIMARANGE, "Reset",	10, 160, 90, 19, NULL, 0.0f, 0.0f, 0, 0, "Reset Black/White point and curves");
			uiButSetFunc(bt, image_panel_curves_reset, G.sima->cumap, ibuf);
		
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_SIMARANGE, "Min R:",	10, 120, 90, 19, G.sima->cumap->black, -1000.0f, 1000.0f, 10, 2, "Black level");
			uiDefButF(block, NUM, B_SIMARANGE, "Min G:",	10, 100, 90, 19, G.sima->cumap->black+1, -1000.0f, 1000.0f, 10, 2, "Black level");
			uiDefButF(block, NUM, B_SIMARANGE, "Min B:",	10, 80, 90, 19, G.sima->cumap->black+2, -1000.0f, 1000.0f, 10, 2, "Black level");
			
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_SIMARANGE, "Max R:",	10, 50, 90, 19, G.sima->cumap->white, -1000.0f, 1000.0f, 10, 2, "White level");
			uiDefButF(block, NUM, B_SIMARANGE, "Max G:",	10, 30, 90, 19, G.sima->cumap->white+1, -1000.0f, 1000.0f, 10, 2, "White level");
			uiDefButF(block, NUM, B_SIMARANGE, "Max B:",	10, 10, 90, 19, G.sima->cumap->white+2, -1000.0f, 1000.0f, 10, 2, "White level");
		}
	}
}
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
		
		allqueue(REDRAWNODE, 1);
	}	
}


/* nothing drawn here, we use it to store values */
static void preview_cb(struct ScrArea *sa, struct uiBlock *block)
{
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
	
	calc_image_view(G.sima, 'p');
//	printf("winrct %d %d %d %d\n", disprect->xmin, disprect->ymin,disprect->xmax, disprect->ymax);
	/* map to image space coordinates */
	mval[0]= disprect->xmin; mval[1]= disprect->ymin;
	areamouseco_to_ipoco(G.v2d, mval, &dispf.xmin, &dispf.ymin);
	mval[0]= disprect->xmax; mval[1]= disprect->ymax;
	areamouseco_to_ipoco(G.v2d, mval, &dispf.xmax, &dispf.ymax);
	
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
	
	block= uiNewBlock(&sa->uiblocks, "image_panel_preview", UI_EMBOSS, UI_HELV, sa->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | UI_PNL_SCALE | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_PREVIEW);  // for close and esc
	
	ofsx= -150+(sa->winx/2)/sima->blockscale;
	ofsy= -100+(sa->winy/2)/sima->blockscale;
	if(uiNewPanel(sa, block, "Preview", "Image", ofsx, ofsy, 300, 200)==0) return;
	
	uiBlockSetDrawExtraFunc(block, preview_cb);
	
}

static void image_panel_gpencil(short cntrl)	// IMAGE_HANDLER_GREASEPENCIL
{
	uiBlock *block;
	SpaceImage *sima;
	
	sima= curarea->spacedata.first;

	block= uiNewBlock(&curarea->uiblocks, "image_panel_gpencil", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE  | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_GREASEPENCIL);  // for close and esc
	if (uiNewPanel(curarea, block, "Grease Pencil", "SpaceImage", 100, 30, 318, 204)==0) return;
	
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

static void image_blockhandlers(ScrArea *sa)
{
	SpaceImage *sima= sa->spacedata.first;
	short a;

	/* warning; blocks need to be freed each time, handlers dont remove  */
	uiFreeBlocksWin(&sa->uiblocks, sa->win);
	
	for(a=0; a<SPACE_MAXHANDLER; a+=2) {
		switch(sima->blockhandler[a]) {
		case IMAGE_HANDLER_PROPERTIES:
			image_panel_properties(sima->blockhandler[a+1]);
			break;
		case IMAGE_HANDLER_GAME_PROPERTIES:
			image_panel_game_properties(sima->blockhandler[a+1]);
			break;
		case IMAGE_HANDLER_VIEW_PROPERTIES:
			image_panel_view_properties(sima->blockhandler[a+1]);
			break;
		case IMAGE_HANDLER_PAINT:
			image_panel_paint(sima->blockhandler[a+1]);
			break;		
		case IMAGE_HANDLER_CURVES:
			image_panel_curves(sima->blockhandler[a+1]);
			break;		
		case IMAGE_HANDLER_PREVIEW:
			image_panel_preview(sa, sima->blockhandler[a+1]);
			break;	
		case IMAGE_HANDLER_GREASEPENCIL:
			image_panel_gpencil(sima->blockhandler[a+1]);
			break;
		}
		/* clear action value for event */
		sima->blockhandler[a+1]= 0;
	}
	uiDrawBlocksPanels(sa, 0);
}
#endif

static void image_panel_properties(const bContext *C, ARegion *ar)	
{
	uiBlock *block;
	
	block= uiBeginBlock(C, ar, "image_panel_properties", UI_EMBOSS, UI_HELV);
	if(uiNewPanel(C, ar, block, "Image Properties", "Image", 10, 10, 318, 204)==0)
		return;
	
	/* note, it draws no bottom half in facemode, for vertex buttons */
//	uiblock_image_panel(block, &G.sima->image, &G.sima->iuser, B_REDR, B_REDR);
//	image_editvertex_buts(block);
	
	uiEndBlock(C, block);
}	



void image_buttons_area_defbuts(const bContext *C, ARegion *ar)
{
	
	image_panel_properties(C, ar);
	
	uiDrawPanels(C, 1);		/* 1 = align */
	uiMatchPanelsView2d(ar); /* sets v2d->totrct */
	
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



