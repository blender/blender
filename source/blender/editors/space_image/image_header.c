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
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_customdata_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_image.h"
#include "BKE_mesh.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_types.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "RE_pipeline.h"

#include "image_intern.h"

/* ************************ header area region *********************** */

#define B_NOP				-1
#define B_REDR 				1
#define B_SIMAGEPAINTTOOL	4
#define B_SIMA_USE_ALPHA	5
#define B_SIMA_SHOW_ALPHA	6
#define B_SIMA_SHOW_ZBUF	7
#define B_SIMA_RECORD		8
#define B_SIMA_PLAY			9

#if 0
static void do_image_imagemenu(void *arg, int event)
{
	/* events >=20 are registered bpython scripts */
#ifndef DISABLE_PYTHON
	if (event >= 20) BPY_menu_do_python(PYMENU_IMAGE, event - 20);
#endif	
}

#ifndef DISABLE_PYTHON
	{
		BPyMenu *pym;
		int i = 0;

		/* note that we acount for the N previous entries with i+20: */
		for (pym = BPyMenuTable[PYMENU_IMAGE]; pym; pym = pym->next, i++) {

			uiDefIconTextBut(block, BUTM, 1, ICON_PYTHON, pym->name, 0, yco-=20, menuwidth, 19, 
					 NULL, 0.0, 0.0, 1, i+20, 
					 pym->tooltip?pym->tooltip:pym->filename);
		}
	}
#endif
#endif

#if 0
#ifndef DISABLE_PYTHON
static void do_image_uvs_scriptsmenu(void *arg, int event)
{
	BPY_menu_do_python(PYMENU_UV, event);

	allqueue(REDRAWIMAGE, 0);
}

static void image_uvs_scriptsmenu (void *args_unused)
{
	uiBlock *block;
	BPyMenu *pym;
	int i= 0;
	short yco = 20, menuwidth = 120;
	
	block= uiNewBlock(&curarea->uiblocks, "image_uvs_scriptsmenu", UI_EMBOSSP);
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
#endif /* DISABLE_PYTHON */
#endif

#if 0
static void do_image_buttons(bContext *C, void *arg, int event)
{
	switch(event) {
		case B_REDR:
			ED_area_tag_redraw(CTX_wm_area(C));
			break;
	}

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
	case B_SIMABROWSE:	
		if(sima->imanr== -2) {
			if(G.qual & LR_CTRLKEY) {
				activate_databrowse_imasel((ID *)sima->image, ID_IM, 0, B_SIMABROWSE,
											&sima->imanr, do_image_buttons);
			} else {
				activate_databrowse((ID *)sima->image, ID_IM, 0, B_SIMABROWSE,
											&sima->imanr, do_image_buttons);
			}
			return;
		}
		if(sima->imanr < 0) break;
	
		nr= 1;
		id= (ID *)sima->image;

		idtest= BLI_findlink(&G.main->image, sima->imanr-1);
		if(idtest==NULL) { /* no new */
			return;
		}
	
		if(idtest!=id) {
			sima->image= (Image *)idtest;
			if(idtest->us==0) idtest->us= 1;
			BKE_image_signal(sima->image, &sima->iuser, IMA_SIGNAL_USER_NEW_IMAGE);
			allqueue(REDRAWIMAGE, 0);
		}
		/* also when image is the same: assign! 0==no tileflag: */
		image_changed(sima, (Image *)idtest);
		BIF_undo_push("Assign image UV");

		break;
	case B_SIMAGETILE:
		image_set_tile(sima, 1);		/* 1: only tileflag */
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

	case B_SIMAPACKIMA:
		pack_image_sima();
		break;
		
	case B_SIMA_REPACK:
		BKE_image_memorypack(sima->image);
		allqueue(REDRAWIMAGE, 0);
		break;
		
	case B_SIMA_USE_ALPHA:
		sima->flag &= ~(SI_SHOW_ALPHA|SI_SHOW_ZBUF);
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);
		break;
	case B_SIMA_SHOW_ALPHA:
		sima->flag &= ~(SI_USE_ALPHA|SI_SHOW_ZBUF);
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);
		break;
	case B_SIMA_SHOW_ZBUF:
		sima->flag &= ~(SI_SHOW_ALPHA|SI_USE_ALPHA);
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
		if(sima->image) {
			Image *ima;
			char str[FILE_MAXDIR+FILE_MAXFILE];
			
			/* name in ima has been changed by button! */
			BLI_strncpy(str, sima->image->name, sizeof(str));
			ima= BKE_add_image_file(str);
			if(ima) {
				BKE_image_signal(ima, &sima->iuser, IMA_SIGNAL_RELOAD);
				image_changed(sima, ima);
			}
			BIF_undo_push("Load image");
			allqueue(REDRAWIMAGE, 0);
		}
		break;
	case B_SIMAMULTI:
		if(sima && sima->image) {
			BKE_image_multilayer_index(sima->image->rr, &sima->iuser);
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
		
		ima = sima->image;
		if (ima) {
			if(ima->flag & IMA_TWINANIM) {
				nr= ima->xrep*ima->yrep;
				if(ima->twsta>=nr) ima->twsta= 1;
				if(ima->twend>=nr) ima->twend= nr-1;
				if(ima->twsta>ima->twend) ima->twsta= 1;
			}

			allqueue(REDRAWIMAGE, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	}	
	case B_SIMACLONEBROWSE:
		if(settings->imapaint.brush) {
			Brush *brush= settings->imapaint.brush;
		
			if(sima->menunr== -2) {
				if(G.qual & LR_CTRLKEY) {
					activate_databrowse_imasel((ID *)brush->clone.image, ID_IM, 0, B_SIMACLONEBROWSE,
												&sima->menunr, do_image_buttons);
				} else {
					activate_databrowse((ID *)brush->clone.image, ID_IM, 0, B_SIMACLONEBROWSE,
												&sima->menunr, do_image_buttons);
				}
				break;
			}
			if(sima->menunr < 0) break;

			if(brush_clone_image_set_nr(brush, sima->menunr))
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
		curvemapping_do_ibuf(sima->cumap, imagewindow_get_ibuf(sima));
		allqueue(REDRAWIMAGE, 0);
		break;
		
	case B_SIMARANGE:
		curvemapping_set_black_white(sima->cumap, NULL, NULL);
		curvemapping_do_ibuf(sima->cumap, imagewindow_get_ibuf(sima));
		allqueue(REDRAWIMAGE, 0);
		break;
		
	case B_SIMABRUSHBROWSE:
		if(sima->menunr==-2) {
			activate_databrowse((ID*)settings->imapaint.brush, ID_BR, 0, B_SIMABRUSHBROWSE, &sima->menunr, do_global_buttons);
			break;
		}
		else if(sima->menunr < 0) break;
		
		if(brush_set_nr(&settings->imapaint.brush, sima->menunr)) {
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
			
			if(sima->menunr==-2) {
				MTex *mtex= brush->mtex[brush->texact];
				ID *id= (ID*)((mtex)? mtex->tex: NULL);
				if(G.qual & LR_CTRLKEY) {
					activate_databrowse_imasel(id, ID_TE, 0, B_SIMABTEXBROWSE, &sima->menunr, do_image_buttons);
				} else {
					activate_databrowse(id, ID_TE, 0, B_SIMABTEXBROWSE, &sima->menunr, do_image_buttons);
				}
				break;
			}
			else if(sima->menunr < 0) break;
			
			if(brush_texture_set_nr(brush, sima->menunr)) {
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
#endif

/********************** toolbox operator *********************/

static int toolbox_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Object *obedit= CTX_data_edit_object(C);
	uiPopupMenu *pup;
	uiLayout *layout;
	int show_uvedit;

	show_uvedit= ED_space_image_show_uvedit(sima, obedit);

	pup= uiPupMenuBegin(C, "Toolbox", 0);
	layout= uiPupMenuLayout(pup);

	uiItemM(layout, C, NULL, 0, "IMAGE_MT_view");
	if(show_uvedit) uiItemM(layout, C, NULL, 0, "IMAGE_MT_select");
	uiItemM(layout, C, NULL, 0, "IMAGE_MT_image");
	if(show_uvedit) uiItemM(layout, C, NULL, 0, "IMAGE_MT_uvs");

	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

void IMAGE_OT_toolbox(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toolbox";
	ot->idname= "IMAGE_OT_toolbox";
	
	/* api callbacks */
	ot->invoke= toolbox_invoke;
	ot->poll= space_image_main_area_poll;
}

