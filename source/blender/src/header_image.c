/**
 * header_image.c oct-2003
 *
 * Functions to draw the "UV/Image Editor" window header
 * and handle user events sent to it.
 * 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "DNA_brush_types.h"
#include "DNA_ID.h"
#include "DNA_image_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_customdata_types.h" /* for UV layer menu */

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "BDR_drawmesh.h"
#include "BDR_unwrapper.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"
#include "BLI_editVert.h" /* for UV layer menu */
#include "BKE_customdata.h" /* ditto */

#include "BIF_butspace.h"
#include "BIF_drawimage.h"
#include "BIF_editsima.h"
#include "BIF_imasel.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_transform.h"
#include "BIF_toolbox.h"
#include "BIF_editmesh.h"

#include "BSE_drawview.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_trans_types.h"
#include "BSE_edit.h"

#include "BPY_extern.h"
#include "BPY_menus.h"

#include "IMB_imbuf_types.h"

#include "RE_pipeline.h"

#include "blendef.h"
#include "butspace.h"
#include "mydevice.h"

void do_image_buttons(unsigned short event)
{
	ToolSettings *settings= G.scene->toolsettings;
	ID *id, *idtest;
	int nr;

	if(curarea->win==0) return;

	if(event<=100) {
		if(event<=50) do_global_buttons2(event);
		else do_global_buttons(event);
		return;
	}
	
	switch(event) {
	case B_SIMAPIN:
		allqueue (REDRAWIMAGE, 0);
		break;
	case B_SIMAGEHOME:
		image_home();
		break;

	case B_SIMABROWSE:	
		if(G.sima->imanr== -2) {
			if(G.qual & LR_CTRLKEY) {
				activate_databrowse_imasel((ID *)G.sima->image, ID_IM, 0, B_SIMABROWSE,
											&G.sima->imanr, do_image_buttons);
			} else {
				activate_databrowse((ID *)G.sima->image, ID_IM, 0, B_SIMABROWSE,
											&G.sima->imanr, do_image_buttons);
			}
			return;
		}
		if(G.sima->imanr < 0) break;
	
		nr= 1;
		id= (ID *)G.sima->image;

		idtest= BLI_findlink(&G.main->image, G.sima->imanr-1);
		if(idtest==NULL) { /* no new */
			return;
		}
	
		if(idtest!=id) {
			G.sima->image= (Image *)idtest;
			if(idtest->us==0) idtest->us= 1;
			BKE_image_signal(G.sima->image, &G.sima->iuser, IMA_SIGNAL_USER_NEW_IMAGE);
			allqueue(REDRAWIMAGE, 0);
		}
		/* also when image is the same: assign! 0==no tileflag: */
		image_changed(G.sima, (Image *)idtest);
		BIF_undo_push("Assign image UV");

		break;
	case B_SIMAGETILE:
		image_set_tile(G.sima, 1);		/* 1: only tileflag */
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
		break;
	case B_SIMA3DVIEWDRAW:
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_SIMA_REDR_IMA_3D:
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
		break;
	case B_SIMAGEPAINTTOOL:
		if(G.sima->flag & SI_DRAWTOOL)
			/* add new brush if none exists */
			brush_check_exists(&G.scene->toolsettings->imapaint.brush);
		allqueue(REDRAWBUTSSHADING, 0);
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;

	case B_SIMAPACKIMA:
		pack_image_sima();
		break;
		
	case B_SIMA_REPACK:
		BKE_image_memorypack(G.sima->image);
		allqueue(REDRAWIMAGE, 0);
		break;
		
	case B_SIMA_USE_ALPHA:
		G.sima->flag &= ~(SI_SHOW_ALPHA|SI_SHOW_ZBUF);
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);
		break;
	case B_SIMA_SHOW_ALPHA:
		G.sima->flag &= ~(SI_USE_ALPHA|SI_SHOW_ZBUF);
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);
		break;
	case B_SIMA_SHOW_ZBUF:
		G.sima->flag &= ~(SI_SHOW_ALPHA|SI_USE_ALPHA);
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);
		break;
	case B_SIMARELOAD:
		reload_image_sima();
		break;
	case B_SIMAGELOAD:
		open_image_sima(0);
		break;
	case B_SIMANAME:
		if(G.sima->image) {
			Image *ima;
			char str[FILE_MAXDIR+FILE_MAXFILE];
			
			/* name in ima has been changed by button! */
			BLI_strncpy(str, G.sima->image->name, sizeof(str));
			ima= BKE_add_image_file(str);
			if(ima) {
				BKE_image_signal(ima, &G.sima->iuser, IMA_SIGNAL_RELOAD);
				image_changed(G.sima, ima);
			}
			BIF_undo_push("Load image");
			allqueue(REDRAWIMAGE, 0);
		}
		break;
	case B_SIMAMULTI:
		if(G.sima && G.sima->image) {
			BKE_image_multilayer_index(G.sima->image->rr, &G.sima->iuser);
			allqueue(REDRAWIMAGE, 0);
		}
		break;
	case B_TRANS_IMAGE:
		image_editvertex_buts(NULL);
		break;
	case B_CURSOR_IMAGE:
		image_editcursor_buts(NULL);
		break;
		
	case B_TWINANIM:
	{
		Image *ima;
		int nr;
		
		ima = G.sima->image;
		if (ima) {
			if(ima->flag & IMA_TWINANIM) {
				nr= ima->xrep*ima->yrep;
				if(ima->twsta>=nr) ima->twsta= 1;
				if(ima->twend>=nr) ima->twend= nr-1;
				if(ima->twsta>ima->twend) ima->twsta= 1;
				allqueue(REDRAWIMAGE, 0);
			}
		}
		break;
	}	
	case B_SIMACLONEBROWSE:
		if(settings->imapaint.brush) {
			Brush *brush= settings->imapaint.brush;
		
			if(G.sima->menunr== -2) {
				if(G.qual & LR_CTRLKEY) {
					activate_databrowse_imasel((ID *)brush->clone.image, ID_IM, 0, B_SIMACLONEBROWSE,
												&G.sima->menunr, do_image_buttons);
				} else {
					activate_databrowse((ID *)brush->clone.image, ID_IM, 0, B_SIMACLONEBROWSE,
												&G.sima->menunr, do_image_buttons);
				}
				break;
			}
			if(G.sima->menunr < 0) break;

			if(brush_clone_image_set_nr(brush, G.sima->menunr))
				allqueue(REDRAWIMAGE, 0);
		}
		break;
		
	case B_SIMACLONEDELETE:
		if (settings->imapaint.brush)
			if (brush_clone_image_delete(settings->imapaint.brush))
				allqueue(REDRAWIMAGE, 0);
		break;
		
	case B_SIMABRUSHCHANGE:
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
		
	case B_SIMACURVES:
		curvemapping_do_ibuf(G.sima->cumap, imagewindow_get_ibuf(G.sima));
		allqueue(REDRAWIMAGE, 0);
		break;
		
	case B_SIMARANGE:
		curvemapping_set_black_white(G.sima->cumap, NULL, NULL);
		curvemapping_do_ibuf(G.sima->cumap, imagewindow_get_ibuf(G.sima));
		allqueue(REDRAWIMAGE, 0);
		break;
		
	case B_SIMABRUSHBROWSE:
		if(G.sima->menunr==-2) {
			activate_databrowse((ID*)settings->imapaint.brush, ID_BR, 0, B_SIMABRUSHBROWSE, &G.sima->menunr, do_global_buttons);
			break;
		}
		else if(G.sima->menunr < 0) break;
		
		if(brush_set_nr(&settings->imapaint.brush, G.sima->menunr)) {
			BIF_undo_push("Browse Brush");
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWIMAGE, 0);
		}
		break;
	case B_SIMABRUSHDELETE:
		if(brush_delete(&settings->imapaint.brush)) {
			BIF_undo_push("Unlink Brush");
			allqueue(REDRAWIMAGE, 0);
			allqueue(REDRAWBUTSEDIT, 0);
		}
		break;
	case B_KEEPDATA:
		brush_toggled_fake_user(settings->imapaint.brush);
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_SIMABRUSHLOCAL:
		if(settings->imapaint.brush && settings->imapaint.brush->id.lib) {
			if(okee("Make local")) {
				make_local_brush(settings->imapaint.brush);
				allqueue(REDRAWIMAGE, 0);
				allqueue(REDRAWBUTSEDIT, 0);
			}
		}
		break;
	case B_SIMABTEXBROWSE:
		if(settings->imapaint.brush) {
			Brush *brush= settings->imapaint.brush;
			
			if(G.sima->menunr==-2) {
				MTex *mtex= brush->mtex[brush->texact];
				ID *id= (ID*)((mtex)? mtex->tex: NULL);
				if(G.qual & LR_CTRLKEY) {
					activate_databrowse_imasel(id, ID_TE, 0, B_SIMABTEXBROWSE, &G.sima->menunr, do_image_buttons);
				} else {
					activate_databrowse(id, ID_TE, 0, B_SIMABTEXBROWSE, &G.sima->menunr, do_image_buttons);
				}
				break;
			}
			else if(G.sima->menunr < 0) break;
			
			if(brush_texture_set_nr(brush, G.sima->menunr)) {
				BIF_undo_push("Browse Brush Texture");
				allqueue(REDRAWBUTSSHADING, 0);
				allqueue(REDRAWBUTSEDIT, 0);
				allqueue(REDRAWIMAGE, 0);
			}
		}
		break;
	case B_SIMABTEXDELETE:
		if(settings->imapaint.brush) {
			if (brush_texture_delete(settings->imapaint.brush)) {
				BIF_undo_push("Unlink Brush Texture");
				allqueue(REDRAWBUTSSHADING, 0);
				allqueue(REDRAWBUTSEDIT, 0);
				allqueue(REDRAWIMAGE, 0);
			}
		}
		break;
	case B_SIMA_PLAY:
		play_anim(0);
		break;
	case B_SIMA_RECORD:
		imagespace_composite_flipbook(curarea);
		break;
	}
}

static void do_image_buttons_set_uvlayer_callback(void *act, void *data)
{
	CustomData_set_layer_active(&G.editMesh->fdata, CD_MTFACE, *((int *)act));
	
	BIF_undo_push("Set Active UV Texture");
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWIMAGE, 0);
}

static void do_image_view_viewnavmenu(void *arg, int event)
{
	switch(event) {
	case 1: /* Zoom In */
		image_viewzoom(PADPLUSKEY, 0);
		break;
	case 2: /* Zoom Out */
		image_viewzoom(PADMINUS, 0);
		break;
	case 3: /* Zoom 8:1 */
		image_viewzoom(PAD8, 0);
		break;
	case 4: /* Zoom 4:1 */
		image_viewzoom(PAD4, 0);
		break;
	case 5: /* Zoom 2:1 */
		image_viewzoom(PAD2, 0);
		break;
	case 6: /* Zoom 1:1 */
		image_viewzoom(PAD1, 0);
		break;
	case 7: /* Zoom 1:2 */
		image_viewzoom(PAD2, 1);
		break;
	case 8: /* Zoom 1:4 */
		image_viewzoom(PAD4, 1);
		break;
	case 9: /* Zoom 1:8 */
		image_viewzoom(PAD8, 1);
		break;
	}
	allqueue(REDRAWIMAGE, 0);
}

static uiBlock *image_view_viewnavmenu(void *arg_unused)
{
/*		static short tog=0; */
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "image_view_viewnavmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_image_view_viewnavmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom In|NumPad +", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom Out|NumPad -",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom 1:8|Shift+NumPad 8", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom 1:4|Shift+NumPad 4", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom 1:2|Shift+NumPad 2", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom 1:1|NumPad 1", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom 2:1|NumPad 2", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom 4:1|NumPad 4", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom 8:1|NumPad 8", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_image_viewmenu(void *arg, int event)
{

	switch(event) {
	case 0: /* Update Automatically */
		if(G.sima->lock) G.sima->lock = 0;
		else G.sima->lock = 1;
		break;
	case 1: /* View All */
		do_image_buttons(B_SIMAGEHOME);
		break;
	case 2: /* Maximize Window */
		/* using event B_FULL */
		break;
	case 4: /* Realtime Panel... */
		add_blockhandler(curarea, IMAGE_HANDLER_VIEW_PROPERTIES, UI_PNL_UNSTOW);
		break;
	case 7: /* Properties  Panel */
		add_blockhandler(curarea, IMAGE_HANDLER_PROPERTIES, UI_PNL_UNSTOW);
		break;
	case 8: /* Paint Panel... */
		add_blockhandler(curarea, IMAGE_HANDLER_PAINT, UI_PNL_UNSTOW);
		break;
	case 9:
		image_viewcenter();
		break;
	case 11: /* Curves Panel... */
		add_blockhandler(curarea, IMAGE_HANDLER_CURVES, UI_PNL_UNSTOW);
		break;
	case 12: /* composite preview */
		toggle_blockhandler(curarea, IMAGE_HANDLER_PREVIEW, 0);
		scrarea_queue_winredraw(curarea);
		break;
	case 13: /* Realtime Panel... */
		add_blockhandler(curarea, IMAGE_HANDLER_GAME_PROPERTIES, UI_PNL_UNSTOW);
		break;
	case 14: /* Draw active image UV's only*/
		G.sima->flag ^= SI_LOCAL_UV;
		allqueue(REDRAWIMAGE, 0);
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *image_viewmenu(void *arg_unused)
{
/*	static short tog=0; */
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "image_viewmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_image_viewmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "View Properties...",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Image Properties...|N",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Real-time Properties...",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 13, "");
	if (G.sima->image && (G.sima->flag & SI_DRAWTOOL)) {
		uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Paint Tool...|C",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	}
	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Curves Tool...",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Composite Preview...|Shift P",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 12, "");
	
	if(G.sima->flag & SI_LOCAL_UV) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "UV Local View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 14, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "UV Local View|NumPad /", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 14, "");
	if(!(G.sima->flag & SI_LOCAL_UV)) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "UV Global View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 14, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "UV Global View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 14, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, image_view_viewnavmenu, NULL, ICON_RIGHTARROW_THIN, "View Navigation", 0, yco-=20, 120, 19, "");

	if(G.sima->lock) {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Update Automatically|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	} else {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Update Automatically|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	} 

	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View Selected|NumPad .",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View All|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
	if(!curarea->full) uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	else uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	
	return block;
}

static void do_image_selectmenu(void *arg, int event)
{
	switch(event)
	{
	case 0: /* Border Select */
		borderselect_sima(UV_SELECT_ALL);
		break;
	case 8: /* Border Select Pinned */
		borderselect_sima(UV_SELECT_PINNED);
		break;
	case 1: /* Select/Deselect All */
		select_swap_tface_uv();
		break;
	case 9: /* Select Inverse */
		select_invert_tface_uv();
		break;
	case 2: /* Unlink Selection */
		unlink_selection();
		break;
	case 3: /* Linked UVs */
		select_linked_tface_uv(2);
		break;
	case 6: /* Toggle Active Face Select */
		if(G.sima->flag & SI_SELACTFACE)
			G.sima->flag &= ~SI_SELACTFACE;
		else
			G.sima->flag |= SI_SELACTFACE;
		allqueue(REDRAWIMAGE, 0);
		break;
	case 7: /* Pinned UVs */
		select_pinned_tface_uv();
		break;
	}
}

static uiBlock *image_selectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "image_selectmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_image_selectmenu, NULL);
	
	
	if ((G.sima->flag & SI_SYNC_UVSEL)==0 || (G.sima->flag & SI_SYNC_UVSEL && (G.scene->selectmode != SCE_SELECT_FACE))) {
		if(G.sima->flag & SI_SELACTFACE) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Active Face Select|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Active Face Select|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	
		uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	}
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select Pinned|Shift B", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");	

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Inverse|Ctrl I", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Unlink Selection|Alt L", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");	

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Pinned UVs|Shift P", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Linked UVs|Ctrl L", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);

	return block;
}

static void do_image_image_rtmappingmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* UV Co-ordinates */
		G.sima->image->flag &= ~IMA_REFLECT;
		break;
	case 1: /* Reflection */
		G.sima->image->flag |= IMA_REFLECT;
		break;
	}

 	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *image_image_rtmappingmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "image_image_rtmappingmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_image_image_rtmappingmenu, NULL);
	
	if (G.sima->image->flag & IMA_REFLECT) {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "UV Co-ordinates",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Reflection",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	} else {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "UV Co-ordinates",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Reflection",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	}

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_image_imagemenu(void *arg, int event)
{
	/* events >=20 are registered bpython scripts */
	if (event >= 20) BPY_menu_do_python(PYMENU_IMAGE, event - 20);
	
	switch(event)
	{
	case 0:
		open_image_sima((G.qual==LR_CTRLKEY));
		break;
	case 1:
		replace_image_sima((G.qual==LR_CTRLKEY));
		break;
	case 2:
		pack_image_sima();
		break;
	case 4: /* Texture Painting */
		brush_check_exists(&G.scene->toolsettings->imapaint.brush);
		if(G.sima->flag & SI_DRAWTOOL) G.sima->flag &= ~SI_DRAWTOOL;
		else G.sima->flag |= SI_DRAWTOOL;
		allqueue(REDRAWBUTSSHADING, 0);
		break;
	case 5:
		save_as_image_sima();
		break;
	case 6:
		reload_image_sima();
		break;
	case 7:
		new_image_sima();
		break;
	case 8:
		save_image_sima();
		break;
	case 9:
		save_image_sequence_sima();
		break;
	case 10:
		BKE_image_memorypack(G.sima->image);
		allqueue(REDRAWIMAGE, 0);
		break;
	}
}

static uiBlock *image_imagemenu(void *arg_unused)
{
	ImBuf *ibuf= BKE_image_get_ibuf(G.sima->image, &G.sima->iuser);
	uiBlock *block;
	short yco= 0, menuwidth=150;
	BPyMenu *pym;
	int i = 0;

	block= uiNewBlock(&curarea->uiblocks, "image_imagemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_image_imagemenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "New...|Alt N", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Open...|Alt O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	
	if (G.sima->image) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Replace...", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reload|Alt R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");	

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save|Alt S", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save As...", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
		if(G.sima->image->source==IMA_SRC_SEQUENCE)
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save Changed Images", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");	
		
		if (G.sima->image->packedfile) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Unpack Image...", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
		} else {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Pack Image", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
		}
		
		/* only for dirty && specific image types */
		if(ibuf && (ibuf->userflags & IB_BITMAPDIRTY))
			if( ELEM(G.sima->image->source, IMA_SRC_FILE, IMA_SRC_GENERATED))
				if(G.sima->image->type!=IMA_TYPE_MULTILAYER)
					uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Pack Image as PNG", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
		
		uiDefBut(block, SEPR, 0, "",        0, yco-=7, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		if(G.sima->flag & SI_DRAWTOOL) {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Texture Painting", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
		} else {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Texture Painting", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
		}
		
		uiDefBut(block, SEPR, 0, "",        0, yco-=7, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, image_image_rtmappingmenu, NULL, ICON_RIGHTARROW_THIN, "Realtime Texture Mapping", 0, yco-=20, 120, 19, "");
		// uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Realtime Texture Animation|",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	}
	
	/* note that we acount for the N previous entries with i+20: */
	for (pym = BPyMenuTable[PYMENU_IMAGE]; pym; pym = pym->next, i++) {

		uiDefIconTextBut(block, BUTM, 1, ICON_PYTHON, pym->name, 0, yco-=20, menuwidth, 19, 
				 NULL, 0.0, 0.0, 1, i+20, 
				 pym->tooltip?pym->tooltip:pym->filename);
	}
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 80);

	return block;
}

static void do_image_uvs_showhidemenu(void *arg, int event)
{
	switch(event) {
	case 4: /* show hidden faces */
		reveal_tface_uv();
		break;
	case 5: /* hide selected faces */
		hide_tface_uv(0);
		break;
	case 6: /* hide deselected faces */
		hide_tface_uv(1);
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *image_uvs_showhidemenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "image_uvs_showhidemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_image_uvs_showhidemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden Faces|Alt H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected Faces|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected Faces|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_image_uvs_propfalloffmenu(void *arg, int event)
{
	G.scene->prop_mode= event;
	allqueue(REDRAWVIEW3D, 1);
}

static uiBlock *image_uvs_propfalloffmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "image_uvs_propfalloffmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_image_uvs_propfalloffmenu, NULL);
	
	if (G.scene->prop_mode==PROP_SMOOTH) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Smooth|Shift O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, PROP_SMOOTH, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Smooth|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, PROP_SMOOTH, "");
	if (G.scene->prop_mode==PROP_SPHERE) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Sphere|Shift O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, PROP_SPHERE, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Sphere|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, PROP_SPHERE, "");
	if (G.scene->prop_mode==PROP_ROOT) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Root|Shift O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, PROP_ROOT, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Root|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, PROP_ROOT, "");
	if (G.scene->prop_mode==PROP_SHARP) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Sharp|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, PROP_SHARP, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Sharp|Shift O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, PROP_SHARP, "");
	if (G.scene->prop_mode==PROP_LIN) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Linear|Shift O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, PROP_LIN, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Linear|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, PROP_LIN, "");
	if (G.scene->prop_mode==PROP_CONST) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Constant|Shift O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, PROP_CONST, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Constant|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, PROP_CONST, "");
		
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_image_uvs_transformmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* Grab */
		initTransform(TFM_TRANSLATION, CTX_NONE);
		Transform();
		break;
	case 1: /* Rotate */
		initTransform(TFM_ROTATION, CTX_NONE);
		Transform();
		break;
	case 2: /* Scale */
		initTransform(TFM_RESIZE, CTX_NONE);
		Transform();
		break;
	}
}

static uiBlock *image_uvs_transformmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "image_uvs_transformmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_image_uvs_transformmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Move|G", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Rotate|R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Scale|S", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_image_uvs_mirrormenu(void *arg, int event)
{
	float mat[3][3];
	
	Mat3One(mat);
	
	switch(event) {
	case 0: /* X axis */
		initTransform(TFM_MIRROR, CTX_NO_PET|CTX_AUTOCONFIRM);
		BIF_setSingleAxisConstraint(mat[0], " on global X axis");
		Transform();
		break;
	case 1: /* Y axis */
		initTransform(TFM_MIRROR, CTX_NO_PET|CTX_AUTOCONFIRM);
		BIF_setSingleAxisConstraint(mat[1], " on global Y axis");
		Transform();
		break;
	}
	
	BIF_undo_push("Mirror UV");
}

static uiBlock *image_uvs_mirrormenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "image_uvs_mirrormenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_image_uvs_mirrormenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "X Axis|M, 1", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Y Axis|M, 2", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_image_uvs_weldalignmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* Weld */
		weld_align_tface_uv('w');
		break;
	case 1: /* Align Auto */
		weld_align_tface_uv('a');
		break;
	case 2: /* Align X */
		weld_align_tface_uv('x');
		break;
	case 3: /* Align Y */
		weld_align_tface_uv('y');
		break;
	}
	
	if(event==0) BIF_undo_push("Weld UV");
	else if(ELEM3(event, 1, 2, 3)) BIF_undo_push("Align UV");
}

static uiBlock *image_uvs_weldalignmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "image_uvs_weldalignmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_image_uvs_weldalignmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Weld|W, 1", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Align Auto|W, 2", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Align X|W, 3", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Align Y|W, 4", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_image_uvs_scriptsmenu(void *arg, int event)
{
	BPY_menu_do_python(PYMENU_UV, event);

	allqueue(REDRAWIMAGE, 0);
}

static uiBlock *image_uvs_scriptsmenu (void *args_unused)
{
	uiBlock *block;
	BPyMenu *pym;
	int i= 0;
	short yco = 20, menuwidth = 120;
	
	block= uiNewBlock(&curarea->uiblocks, "image_uvs_scriptsmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_image_uvs_scriptsmenu, NULL);
	
	/* note that we acount for the N previous entries with i+20: */
	for (pym = BPyMenuTable[PYMENU_UV]; pym; pym = pym->next, i++) {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_PYTHON, pym->name, 0, yco-=20, menuwidth, 19, 
						 NULL, 0.0, 0.0, 1, i, 
						 pym->tooltip?pym->tooltip:pym->filename);
	}
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}

static void do_image_uvsmenu(void *arg, int event)
{

	switch(event) {
//	case 0: /* UV Transform Properties Panel... */
//		add_blockhandler(curarea, IMAGE_HANDLER_TRANSFORM_PROPERTIES, UI_PNL_UNSTOW);
//		break;
	case 1: /* UVs Constrained Rectangular */
		if(G.sima->flag & SI_BE_SQUARE) G.sima->flag &= ~SI_BE_SQUARE;
		else G.sima->flag |= SI_BE_SQUARE;
		break;
	case 2: /* UVs Clipped to Image Size */
		if(G.sima->flag & SI_CLIP_UV) G.sima->flag &= ~SI_CLIP_UV;
		else G.sima->flag |= SI_CLIP_UV;
		break;
	case 3: /* Limit Stitch UVs */
		stitch_limit_uv_tface();
		break;
	case 4: /* Stitch UVs */
		stitch_vert_uv_tface();
		break;
	case 5: /* Proportional Edit (toggle) */
		if(G.scene->proportional)
			G.scene->proportional= 0;
		else
			G.scene->proportional= 1;
		break;
	case 7: /* UVs Snap to Pixel */
		G.sima->flag ^= SI_PIXELSNAP;
		break;
    case 8:
		pin_tface_uv(1);
		break;
    case 9:
		pin_tface_uv(0);
		break;
    case 10:
		unwrap_lscm(0);
		break;
	case 11:
		if(G.sima->flag & SI_LIVE_UNWRAP) G.sima->flag &= ~SI_LIVE_UNWRAP;
		else G.sima->flag |= SI_LIVE_UNWRAP;
		break;
	case 12:
		minimize_stretch_tface_uv();
		break;
	case 13:
		pack_charts_tface_uv();
		break;
	case 14:
		average_charts_tface_uv();
		break;
	}
}

static uiBlock *image_uvsmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "image_uvsmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_image_uvsmenu, NULL);
	
	//uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Transform Properties...|N", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	if(G.sima->flag & SI_PIXELSNAP) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Snap to Pixels|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap to Pixels|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");

	if(G.sima->flag & SI_BE_SQUARE) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Quads Constrained Rectangular|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Quads Constrained Rectangular|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	
	if(G.sima->flag & SI_CLIP_UV) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Layout Clipped to Image Size|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Layout Clipped to Image Size|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");	

	if(G.sima->flag & SI_LIVE_UNWRAP) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Live Unwrap Transform", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Live Unwrap Transform", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Unwrap|E", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Unpin|Alt P", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Pin|P", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");	

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Pack Islands|Ctrl P", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 13, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Average Islands Scale|Ctrl A", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 14, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Minimize Stretch|Ctrl V", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 12, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Limit Stitch...|Shift V", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Stitch|V", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, image_uvs_transformmenu, NULL, ICON_RIGHTARROW_THIN, "Transform", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, image_uvs_mirrormenu, NULL, ICON_RIGHTARROW_THIN, "Mirror", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, image_uvs_weldalignmenu, NULL, ICON_RIGHTARROW_THIN, "Weld/Align", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	if(G.scene->proportional)
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Proportional Editing|O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	else
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Proportional Editing|O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");

	uiDefIconTextBlockBut(block, image_uvs_propfalloffmenu, NULL, ICON_RIGHTARROW_THIN, "Proportional Falloff", 0, yco-=20, 120, 19, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");	

	uiDefIconTextBlockBut(block, image_uvs_showhidemenu, NULL, ICON_RIGHTARROW_THIN, "Show/Hide Faces", 0, yco-=20, menuwidth, 19, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");	
	
	uiDefIconTextBlockBut(block, image_uvs_scriptsmenu, NULL, ICON_RIGHTARROW_THIN, "Scripts", 0, yco-=20, 120, 19, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);

	return block;
}

void image_buttons(void)
{
	Image *ima;
	ImBuf *ibuf;
	uiBlock *block;
	short xco, xmax;
	char naam[256], *menuname;
	char is_render; /* true if the image is a render or composite */
	
	int allow_pin= B_SIMAPIN;
	
	/* This should not be a static var */
	static int headerbuttons_packdummy;
	
	
	is_render = ((G.sima->image!=NULL) && ((G.sima->image->type == IMA_TYPE_R_RESULT) || (G.sima->image->type == IMA_TYPE_COMPOSITE)));

	headerbuttons_packdummy = 0;
		
	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	what_image(G.sima);
	ima= G.sima->image;
	ibuf= BKE_image_get_ibuf(ima, &G.sima->iuser);

	curarea->butspacetype= SPACE_IMAGE;

	xco = 8;
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current Window Type. Click for menu of available types.");
	xco+= XIC+14;

	uiBlockSetEmboss(block, UI_EMBOSSN);
	if(curarea->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_RIGHT,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Show pulldown menus");
	} else {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_DOWN,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Hide pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;

	if((curarea->flag & HEADER_NO_PULLDOWN)==0) {
		
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
	
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, image_viewmenu, NULL, "View", xco, -2, xmax-3, 24, "");
		xco+= xmax;
		
		if((EM_texFaceCheck()) && !(ima && (G.sima->flag & SI_DRAWTOOL))) {
			xmax= GetButStringLength("Select");
			uiDefPulldownBut(block, image_selectmenu, NULL, "Select", xco, -2, xmax-3, 24, "");
			xco+= xmax;
		}
		
		if (ibuf && (ibuf->userflags & IB_BITMAPDIRTY))
			menuname= "Image*";
		else
			menuname= "Image";
		xmax= GetButStringLength(menuname);
		uiDefPulldownBut(block, image_imagemenu, NULL, menuname, xco, -2, xmax-3, 24, "");
		xco+= xmax;
		if((EM_texFaceCheck()) && !(ima && (G.sima->flag & SI_DRAWTOOL))) {
			xmax= GetButStringLength("UVs");
			uiDefPulldownBut(block, image_uvsmenu, NULL, "UVs", xco, -2, xmax-3, 24, "");
			xco+= xmax;
		}
	}
	
	/* other buttons: */
	uiBlockSetEmboss(block, UI_EMBOSS);

	if (is_render)
		allow_pin = 0;
	
	xco= 8 + std_libbuttons(block, xco, 0, allow_pin, &G.sima->pin, B_SIMABROWSE, ID_IM, 0, (ID *)ima, 0, &(G.sima->imanr), 0, 0, B_IMAGEDELETE, 0, 0);
	
	if( ima && !ELEM3(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE, IMA_SRC_VIEWER) && ima->ok) {

		if (ima->packedfile) {
			headerbuttons_packdummy = 1;
		}
		if (ima->packedfile && ibuf && (ibuf->userflags & IB_BITMAPDIRTY))
			uiDefIconButBitI(block, TOG, 1, B_SIMA_REPACK, ICON_UGLYPACKAGE,	xco,0,XIC,YIC, &headerbuttons_packdummy, 0, 0, 0, 0, "Re-Pack this image as PNG");
		else
			uiDefIconButBitI(block, TOG, 1, B_SIMAPACKIMA, ICON_PACKAGE,	xco,0,XIC,YIC, &headerbuttons_packdummy, 0, 0, 0, 0, "Pack/Unpack this image");
			
		xco+= XIC+8;
	}
	
	/* UV EditMode buttons, not painting or rencering or compositing */
	if ( EM_texFaceCheck() && (G.sima->flag & SI_DRAWTOOL)==0 && !is_render) {
		uiBut *ubut;
		int layercount;
		
		uiDefIconTextButS(block, ICONTEXTROW, B_NOP, ICON_ROTATE,
				"Pivot: %t|Bounding Box Center %x0|Median Point %x3|2D Cursor %x1",
				xco,0,XIC+10,YIC, &(G.v2d->around), 0, 3.0, 0, 0,
				"Rotation/Scaling Pivot (Hotkeys: Comma, Shift Comma, Period)");
		xco+= XIC + 18;
		
		
		uiDefIconButBitI(block, TOG, SI_SYNC_UVSEL, B_REDR, ICON_EDIT, xco,0,XIC,YIC, &G.sima->flag, 0, 0, 0, 0, "Sync UV and Mesh Selection");
		xco+= XIC+8;
		if (G.sima->flag & SI_SYNC_UVSEL) {
			uiBlockBeginAlign(block);
			
			/* B_SEL_VERT & B_SEL_FACE are not defined here which is a bit bad, BUT it works even if image editor is fullscreen */
			uiDefIconButBitS(block, TOG, SCE_SELECT_VERTEX, B_SEL_VERT, ICON_VERTEXSEL,
				xco,0,XIC,YIC, &G.scene->selectmode, 1.0, 0.0, 0, 0, "Vertex select mode (Ctrl Tab 1)");
			/* no edge */
			/*uiDefIconButBitS(block, TOG, SCE_SELECT_EDGE, B_SEL_EDGE, ICON_EDGESEL, xco,0,XIC,YIC, &G.scene->selectmode, 1.0, 0.0, 0, 0, "Edge select mode (Ctrl Tab 2)");
			xco+= XIC; */
			uiDefIconButBitS(block, TOG, SCE_SELECT_FACE, B_SEL_FACE, ICON_FACESEL,
				xco+=XIC,0,XIC,YIC, &G.scene->selectmode, 1.0, 0.0, 0, 0, "Face select mode (Ctrl Tab 3)");
			uiBlockEndAlign(block);
			
		} else {
			uiBlockBeginAlign(block);
			
			uiDefIconButS(block,  ROW, B_REDR, ICON_VERTEXSEL,
				xco,0,XIC,YIC, &G.sima->selectmode, 0.0, SI_SELECT_VERTEX, 0, 0, "UV vertex select mode");
			uiDefIconButS(block,  ROW, B_REDR, ICON_FACESEL,
				xco+=XIC,0,XIC,YIC, &G.sima->selectmode, 0.0, SI_SELECT_FACE, 0, 0, "UV Face select mode");
			uiDefIconButS(block,  ROW, B_REDR, ICON_MESH,
				xco+=XIC,0,XIC,YIC, &G.sima->selectmode, 0.0, SI_SELECT_ISLAND, 0, 0, "UV Island select mode");
			uiBlockEndAlign(block);

			/* would use these if const's could go in strings 
			 * SI_STICKY_LOC SI_STICKY_DISABLE SI_STICKY_VERTEX */
			ubut = uiDefIconTextButC(block, ICONTEXTROW, B_REDR, ICON_STICKY_UVS_LOC,
					"Sticky UV Selection: %t|Disable%x1|Shared Location%x0|Shared Vertex%x2",
					xco+=XIC+10,0,XIC+10,YIC, &(G.sima->sticky), 0, 3.0, 0, 0,
					"Sticky UV Selection (Hotkeys: Shift C, Alt C, Ctrl C)");
			
		}
		xco+= XIC + 16;
		
		/* Snap copied right out of view3d header */
			uiBlockBeginAlign(block);

			if (G.scene->snap_flag & SCE_SNAP) {
				uiDefIconButBitS(block, TOG, SCE_SNAP, B_REDR, ICON_SNAP_GEO,xco,0,XIC,YIC, &G.scene->snap_flag, 0, 0, 0, 0, "Use Snap or Grid (Shift Tab)");	
				xco+= XIC;
				uiDefButS(block, MENU, B_NOP, "Mode%t|Closest%x0|Center%x1|Median%x2",xco,0,70,YIC, &G.scene->snap_target, 0, 0, 0, 0, "Snap Target Mode");
				xco+= 70;
			} else {
				uiDefIconButBitS(block, TOG, SCE_SNAP, B_REDR, ICON_SNAP_GEAR,xco,0,XIC,YIC, &G.scene->snap_flag, 0, 0, 0, 0, "Snap while Ctrl is held during transform (Shift Tab)");	
				xco+= XIC;
			}

			uiBlockEndAlign(block);
			xco+= 10;
		/* end snap */
			
		/* Layer Menu */
		layercount = CustomData_number_of_layers(&G.editMesh->fdata, CD_MTFACE); 
		if (layercount>1 && layercount < 12) { /* could allow any number but limit of 11 means no malloc needed */
			static int act;
			char str_menu[384], *str_pt; /*384 allows for 11 layers */
			
			
			act = CustomData_get_active_layer(&G.editMesh->fdata, CD_MTFACE);
			
			/*str_pt = (char *)MEM_mallocN(layercount*40 , "uvmenu"); str[0]='\0';*/
			str_pt = str_menu;
			str_pt[0]='\0';
			mesh_layers_menu_concat(&G.editMesh->fdata, CD_MTFACE, str_pt);
			ubut = uiDefButI(block, MENU, B_NOP, str_menu ,xco,0,85,YIC, &act, 0, 0, 0, 0, "Active UV Layer for editing");
			uiButSetFunc(ubut, do_image_buttons_set_uvlayer_callback, &act, NULL);
			
			/*MEM_freeN(str);*/
			xco+= 90;
		}
	}
	
	if (ima) {
		RenderResult *rr= BKE_image_get_renderresult(ima);
		
		xco+= 8;
	
		if(rr) {
			uiBlockBeginAlign(block);
			uiblock_layer_pass_buttons(block, rr, &G.sima->iuser, B_REDR, xco, 0, 160);
			uiBlockEndAlign(block);
			xco+= 166;
		}
		uiDefIconButBitI(block, TOG, SI_DRAWTOOL, B_SIMAGEPAINTTOOL, ICON_TPAINT_HLT, xco,0,XIC,YIC, &G.sima->flag, 0, 0, 0, 0, "Enables painting textures on the image with left mouse button");
		
		xco+= XIC+8;

		uiBlockBeginAlign(block);
		if(ibuf==NULL || ibuf->channels==4) {
			uiDefIconButBitI(block, TOG, SI_USE_ALPHA, B_SIMA_USE_ALPHA, ICON_TRANSP_HLT, xco,0,XIC,YIC, &G.sima->flag, 0, 0, 0, 0, "Draws image with alpha");
			xco+= XIC;
			uiDefIconButBitI(block, TOG, SI_SHOW_ALPHA, B_SIMA_SHOW_ALPHA, ICON_DOT, xco,0,XIC,YIC, &G.sima->flag, 0, 0, 0, 0, "Draws only alpha");
			xco+= XIC;
		}
		if(ibuf) {
			if(ibuf->zbuf || ibuf->zbuf_float || (ibuf->channels==1)) {
				uiDefIconButBitI(block, TOG, SI_SHOW_ZBUF, B_SIMA_SHOW_ZBUF, ICON_SOLID, xco,0,XIC,YIC, &G.sima->flag, 0, 0, 0, 0, "Draws zbuffer values (mapped from camera clip start to end)");
				xco+= XIC;
			}
		}		
		xco+= 8;
		
		uiBlockBeginAlign(block);
		if(ima->type==IMA_TYPE_COMPOSITE) {
			uiDefIconBut(block, BUT, B_SIMA_RECORD, ICON_REC,  xco, 0, XIC, YIC, 0, 0, 0, 0, 0, "Record Composite");
			xco+= XIC;
		}
		if((ima->type==IMA_TYPE_COMPOSITE) || ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
			uiDefIconBut(block, BUT, B_SIMA_PLAY, ICON_PLAY, xco, 0, XIC, YIC, 0, 0, 0, 0, 0, "Play");
			xco+= XIC;
		}
		uiBlockEndAlign(block);
		xco+= 8;
	}
	
	/* draw LOCK */
	uiDefIconButS(block, ICONTOG, 0, ICON_UNLOCKED,	xco,0,XIC,YIC, &(G.sima->lock), 0, 0, 0, 0, "Updates other affected window spaces automatically to reflect changes in real time");

	/* Always do this last */
	curarea->headbutlen= xco+2*XIC;
	
	uiDrawBlock(block);
}

