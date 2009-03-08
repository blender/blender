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
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_transform.h"

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

static void image_view_viewnavmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	int a;
	
	uiMenuItemO(head, 0, "IMAGE_OT_view_zoom_in");
	uiMenuItemO(head, 0, "IMAGE_OT_view_zoom_out");

	uiMenuSeparator(head);

	for(a=0; a<7; a++) {
		const int ratios[7][2] = {{1, 8}, {1, 4}, {1, 2}, {1, 1}, {2, 1}, {4, 1}, {8, 1}};
		char namestr[128];

		sprintf(namestr, "Zoom %d:%d", ratios[a][0], ratios[a][1]);
		uiMenuItemFloatO(head, namestr, 0, "IMAGE_OT_view_zoom_ratio", "ratio", (float)ratios[a][0]/(float)ratios[a][1]);
	}
}

#if 0
static void do_viewmenu(bContext *C, void *arg, int event)
{
	add_blockhandler(curarea, IMAGE_HANDLER_VIEW_PROPERTIES, UI_PNL_UNSTOW);
	add_blockhandler(curarea, IMAGE_HANDLER_PROPERTIES, UI_PNL_UNSTOW);
	add_blockhandler(curarea, IMAGE_HANDLER_PAINT, UI_PNL_UNSTOW);
	add_blockhandler(curarea, IMAGE_HANDLER_CURVES, UI_PNL_UNSTOW);

	toggle_blockhandler(curarea, IMAGE_HANDLER_PREVIEW, 0);
	scrarea_queue_winredraw(curarea);

	add_blockhandler(curarea, IMAGE_HANDLER_GAME_PROPERTIES, UI_PNL_UNSTOW);
	add_blockhandler(curarea, IMAGE_HANDLER_GREASEPENCIL, UI_PNL_UNSTOW);

	allqueue(REDRAWIMAGE, 0);
	allqueue(REDRAWVIEW3D, 0);
}
#endif

static void image_viewmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa= CTX_wm_area(C);
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	PointerRNA spaceptr, uvptr;
	int show_paint, show_render, show_uvedit;

	/* retrieve state */
	RNA_pointer_create(&sc->id, &RNA_SpaceImageEditor, sima, &spaceptr);
	RNA_pointer_create(&sc->id, &RNA_SpaceUVEditor, sima, &uvptr);

	show_render= ED_space_image_show_render(sima);
	show_paint= ED_space_image_show_paint(sima);
	show_uvedit= ED_space_image_show_uvedit(sima, CTX_data_edit_object(C));
	
	/* create menu */
	uiMenuItemO(head, ICON_MENU_PANEL, "IMAGE_OT_toggle_view_properties_panel"); // View Properties...
	uiMenuItemO(head, ICON_MENU_PANEL, "IMAGE_OT_toggle_image_properties_panel"); // Image Properties...|N
	uiMenuItemO(head, ICON_MENU_PANEL, "IMAGE_OT_toggle_realtime_properties_panel"); // Real-time properties...
	if(show_paint) uiMenuItemO(head, ICON_MENU_PANEL, "IMAGE_OT_toggle_paint_panel"); // Paint Tool...|C
	uiMenuItemO(head, ICON_MENU_PANEL, "IMAGE_OT_toggle_curves_panel"); // Curves Tool...
	if(show_render) uiMenuItemO(head, ICON_MENU_PANEL, "IMAGE_OT_toggle_compositing_preview_panel"); // Compositing Preview...|Shift P
	uiMenuItemO(head, ICON_MENU_PANEL, "IMAGE_OT_toggle_grease_pencil_panel"); // Grease Pencil...

	uiMenuSeparator(head);

	uiMenuItemBooleanR(head, &spaceptr, "update_automatically");
	// XXX if(show_uvedit) uiMenuItemBooleanR(head, &uvptr, "local_view"); // "UV Local View", Numpad /

	uiMenuSeparator(head);

	uiMenuLevel(head, "View Navigation", image_view_viewnavmenu);
	if(show_uvedit) uiMenuItemO(head, 0, "IMAGE_OT_view_selected");
	uiMenuItemO(head, 0, "IMAGE_OT_view_all");

	if(sa->full) uiMenuItemO(head, 0, "SCREEN_OT_screen_full_area"); // "Tile Window", Ctrl UpArrow
	else uiMenuItemO(head, 0, "SCREEN_OT_screen_full_area"); // "Maximize Window", Ctr DownArrow
}

static void image_selectmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuItemO(head, 0, "UV_OT_border_select");
	uiMenuItemBooleanO(head, "Border Select Pinned", 0, "UV_OT_border_select", "pinned", 1); // Border Select Pinned|Shift B

	uiMenuSeparator(head);
	
	uiMenuItemO(head, 0, "UV_OT_de_select_all");
	uiMenuItemO(head, 0, "UV_OT_select_invert");
	uiMenuItemO(head, 0, "UV_OT_unlink_selection");
	
	uiMenuSeparator(head);

	uiMenuItemO(head, 0, "UV_OT_select_pinned");
	uiMenuItemO(head, 0, "UV_OT_select_linked");
}

#if 0
static void do_image_imagemenu(void *arg, int event)
{
	/* events >=20 are registered bpython scripts */
#ifndef DISABLE_PYTHON
	if (event >= 20) BPY_menu_do_python(PYMENU_IMAGE, event - 20);
#endif	
}
#endif

static void image_imagemenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	bScreen *sc= CTX_wm_screen(C);
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	PointerRNA spaceptr, imaptr;
	Image *ima;
	ImBuf *ibuf;
	int show_render;
	
	/* retrieve state */
	ima= ED_space_image(sima);
	ibuf= ED_space_image_buffer(sima);

	show_render= ED_space_image_show_render(sima);

	RNA_pointer_create(&sc->id, &RNA_SpaceImageEditor, sima, &spaceptr);

	/* create menu */
	uiMenuItemO(head, 0, "IMAGE_OT_new"); // New...
	uiMenuItemO(head, 0, "IMAGE_OT_open"); // Open...

	if(ima) {
		if(!show_render) {
			uiMenuItemO(head, 0, "IMAGE_OT_replace"); // Replace...
			uiMenuItemO(head, 0, "IMAGE_OT_reload"); // Reload...
		}
		uiMenuItemO(head, 0, "IMAGE_OT_save"); // Save
		uiMenuItemO(head, 0, "IMAGE_OT_save_as"); // Save As...
		if(ima->source == IMA_SRC_SEQUENCE)
			uiMenuItemO(head, 0, "IMAGE_OT_save_sequence"); // Save Changed Sequence Images

		if(!show_render) {
			uiMenuSeparator(head);

			if(ima->packedfile) uiMenuItemO(head, 0, "IMAGE_OT_unpack"); // Unpack Image...
			else uiMenuItemO(head, 0, "IMAGE_OT_pack"); // Pack Image

			/* only for dirty && specific image types : XXX poll? */
			if(ibuf && (ibuf->userflags & IB_BITMAPDIRTY))
				if(ELEM(ima->source, IMA_SRC_FILE, IMA_SRC_GENERATED) && ima->type != IMA_TYPE_MULTILAYER)
					uiMenuItemBooleanO(head, "Pack As PNG", 0, "IMAGE_OT_pack", "as_png", 1); // Pack Image As PNG

			uiMenuSeparator(head);

			uiMenuItemBooleanR(head, &spaceptr, "image_painting");
			
			/* move to realtime properties panel */
			RNA_id_pointer_create(&ima->id, &imaptr);
			uiMenuLevelEnumR(head, &imaptr, "mapping");
		}
	}

#if 0
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
}

static void image_uvs_showhidemenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuItemO(head, 0, "UV_OT_reveal");
	uiMenuItemO(head, 0, "UV_OT_hide");
	uiMenuItemBooleanO(head, "Hide Unselected", 0, "UV_OT_hide", "unselected", 1);
}

static void image_uvs_transformmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuItemEnumO(head, "", 0, "TFM_OT_transform", "mode", TFM_TRANSLATION);
	uiMenuItemEnumO(head, "", 0, "TFM_OT_transform", "mode", TFM_ROTATION);
	uiMenuItemEnumO(head, "", 0, "TFM_OT_transform", "mode", TFM_RESIZE);
}

static void image_uvs_mirrormenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuItemEnumO(head, "", 0, "UV_OT_mirror", "axis", 'x'); // "X Axis", M, 1
	uiMenuItemEnumO(head, "", 0, "UV_OT_mirror", "axis", 'y'); // "Y Axis", M, 2
}

static void image_uvs_weldalignmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuItemO(head, 0, "UV_OT_weld"); // W, 1
	uiMenuItemsEnumO(head, "UV_OT_align", "axis"); // W, 2/3/4
}

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
#endif /* DISABLE_PYTHON */
#endif

static void image_uvsmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	bScreen *sc= CTX_wm_screen(C);
	Scene *scene= CTX_data_scene(C);
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	PointerRNA uvptr, sceneptr;
	Image *ima;
	ImBuf *ibuf;
	
	/* retrieve state */
	ima= ED_space_image(sima);
	ibuf= ED_space_image_buffer(sima);

	RNA_pointer_create(&sc->id, &RNA_SpaceUVEditor, sima, &uvptr);
	RNA_id_pointer_create(&scene->id, &sceneptr);

	/* create menu */
	uiMenuItemBooleanR(head, &uvptr, "snap_to_pixels");
	uiMenuItemBooleanR(head, &uvptr, "constrain_to_image_bounds");

	uiMenuSeparator(head);

	uiMenuItemBooleanR(head, &uvptr, "live_unwrap");
	uiMenuItemO(head, 0, "UV_OT_unwrap");
	uiMenuItemBooleanO(head, "Unpin", 0, "UV_OT_pin", "clear", 1);
	uiMenuItemO(head, 0, "UV_OT_pin");

	uiMenuSeparator(head);

	uiMenuItemO(head, 0, "UV_OT_pack_islands");
	uiMenuItemO(head, 0, "UV_OT_average_islands_scale");
	uiMenuItemO(head, 0, "UV_OT_minimize_stretch");
	uiMenuItemO(head, 0, "UV_OT_stitch");

	uiMenuSeparator(head);

	uiMenuLevel(head, "Transform", image_uvs_transformmenu);
	uiMenuLevel(head, "Mirror", image_uvs_mirrormenu);
	uiMenuLevel(head, "Weld/Align", image_uvs_weldalignmenu);

	uiMenuSeparator(head);

	uiMenuItemBooleanR(head, &sceneptr, "proportional_editing");
	uiMenuLevelEnumR(head, &sceneptr, "proportional_editing_falloff");

	uiMenuSeparator(head);

	uiMenuLevel(head, "Show/Hide Faces", image_uvs_showhidemenu);

#if 0
#ifndef DISABLE_PYTHON
	uiMenuSeparator(head);

	uiMenuLevel(head, "Scripts", image_uvs_scriptsmenu);
#endif
#endif
}

static void image_menu_uvlayers(Object *obedit, char *menustr, int *active)
{
	Mesh *me= (Mesh*)obedit->data;
	EditMesh *em= me->edit_mesh;
	CustomDataLayer *layer;
	int i, count = 0;

	menustr[0]= '\0';

	for(i=0; i<em->fdata.totlayer; i++) {
		layer = &em->fdata.layers[i];

		if(layer->type == CD_MTFACE) {
			menustr += sprintf(menustr, "%s%%x%d|", layer->name, count);
			count++;
		}
	}

	*active= CustomData_get_active_layer(&em->fdata, CD_MTFACE);
}

static void do_image_buttons(bContext *C, void *arg, int event)
{
	switch(event) {
		case B_REDR:
			ED_area_tag_redraw(CTX_wm_area(C));
			break;
	}

#if 0
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
#endif
}

#if 0
static void do_image_buttons_set_uvlayer_callback(void *act, void *data)
{
	CustomData_set_layer_active(&G.editMesh->fdata, CD_MTFACE, *((int *)act));
	
	BIF_undo_push("Set Active UV Texture");
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWIMAGE, 0);
}
#endif

static void sima_idpoin_handle(bContext *C, ID *id, int event)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);

	switch(event) {
		case UI_ID_BROWSE:
		case UI_ID_DELETE:
			ED_space_image_set(C, sima, scene, obedit, (Image*)id);
			ED_undo_push(C, "Assign Image UV");
			break;
		case UI_ID_RENAME:
			break;
		case UI_ID_ADD_NEW:
			WM_operator_name_call(C, "IMAGE_OT_new", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case UI_ID_OPEN:
			WM_operator_name_call(C, "IMAGE_OT_open", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case UI_ID_PIN:
			ED_area_tag_refresh(CTX_wm_area(C));
			break;
	}
}

void image_header_buttons(const bContext *C, ARegion *ar)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa= CTX_wm_area(C);
	Scene *scene= CTX_data_scene(C);
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	Image *ima;
	ImBuf *ibuf;
	uiBlock *block;
	uiBut *but;
	PointerRNA spaceptr, uvptr, sceneptr;
	int xco, yco= 3, show_uvedit, show_render, show_paint, pinflag;

	/* retrieve state */
	ima= ED_space_image(sima);
	ibuf= ED_space_image_buffer(sima);

	show_render= ED_space_image_show_render(sima);
	show_paint= ED_space_image_show_paint(sima);
	show_uvedit= ED_space_image_show_uvedit(sima, CTX_data_edit_object(C));

	RNA_pointer_create(&sc->id, &RNA_SpaceImageEditor, sima, &spaceptr);
	RNA_pointer_create(&sc->id, &RNA_SpaceUVEditor, sima, &uvptr);
	RNA_id_pointer_create(&scene->id, &sceneptr);
	
	/* create block */
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS, UI_HELV);
	uiBlockSetHandleFunc(block, do_image_buttons, NULL);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	/* create pulldown menus */
	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		char *menuname;
		int xmax;
		
		uiBlockSetEmboss(block, UI_EMBOSSP);
		
		xmax= GetButStringLength("View");
		uiDefMenuBut(block, image_viewmenu, NULL, "View", xco, yco-2, xmax-3, 24, "");
		xco+= xmax;
		
		if(show_uvedit) {
			xmax= GetButStringLength("Select");
			uiDefMenuBut(block, image_selectmenu, NULL, "Select", xco, yco-2, xmax-3, 24, "");
			xco+= xmax;
		}
		
		menuname= (ibuf && (ibuf->userflags & IB_BITMAPDIRTY))? "Image*": "Image";
		xmax= GetButStringLength(menuname);
		uiDefMenuBut(block, image_imagemenu, NULL, menuname, xco, yco-2, xmax-3, 24, "");
		xco+= xmax;

		if(show_uvedit) {
			xmax= GetButStringLength("UVs");
			uiDefMenuBut(block, image_uvsmenu, NULL, "UVs", xco, yco-2, xmax-3, 24, "");
			xco+= xmax;
		}
	}

	uiBlockSetEmboss(block, UI_EMBOSS);

	/* image select */

	pinflag= (show_render)? 0: UI_ID_PIN;
	xco= uiDefIDPoinButs(block, CTX_data_main(C), NULL, (ID*)sima->image, ID_IM, &sima->pin, xco, yco,
		sima_idpoin_handle, UI_ID_BROWSE|UI_ID_BROWSE_RENDER|UI_ID_RENAME|UI_ID_ADD_NEW|UI_ID_OPEN|UI_ID_DELETE|pinflag);
	xco += 8;

	if(ima && !ELEM3(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE, IMA_SRC_VIEWER) && ima->ok) {
		/* XXX this should not be a static var */
		static int headerbuttons_packdummy;
		
		headerbuttons_packdummy = 0;

		if (ima->packedfile) {
			headerbuttons_packdummy = 1;
		}
		if (ima->packedfile && ibuf && (ibuf->userflags & IB_BITMAPDIRTY))
			uiDefIconButBitI(block, TOG, 1, 0 /* XXX B_SIMA_REPACK */, ICON_UGLYPACKAGE,	xco,yco,XIC,YIC, &headerbuttons_packdummy, 0, 0, 0, 0, "Re-Pack this image as PNG");
		else
			uiDefIconButBitI(block, TOG, 1, 0 /* XXX B_SIMAPACKIMA */, ICON_PACKAGE,	xco,yco,XIC,YIC, &headerbuttons_packdummy, 0, 0, 0, 0, "Pack/Unpack this image");
			
		xco+= XIC+8;
	}
	
	/* uv editing */
	if(show_uvedit) {
		/* pivot */
		uiDefIconTextButS(block, ICONTEXTROW, B_NOP, ICON_ROTATE,
				"Pivot: %t|Bounding Box Center %x0|Median Point %x3|2D Cursor %x1",
				xco,yco,XIC+10,YIC, &ar->v2d.around, 0, 3.0, 0, 0,
				"Rotation/Scaling Pivot (Hotkeys: Comma, Shift Comma, Period)");
		xco+= XIC + 18;
		
		/* selection modes */
		uiDefIconButBitS(block, TOG, UV_SYNC_SELECTION, B_REDR, ICON_EDIT, xco,yco,XIC,YIC, &scene->toolsettings->uv_flag, 0, 0, 0, 0, "Sync UV and Mesh Selection");
		xco+= XIC+8;

		if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
			uiBlockBeginAlign(block);
			
			uiDefIconButBitS(block, TOG, SCE_SELECT_VERTEX, B_REDR, ICON_VERTEXSEL,
				xco,yco,XIC,YIC, &scene->selectmode, 1.0, 0.0, 0, 0, "Vertex select mode");
			uiDefIconButBitS(block, TOG, SCE_SELECT_EDGE, B_REDR, ICON_EDGESEL,
				xco+=XIC,yco,XIC,YIC, &scene->selectmode, 1.0, 0.0, 0, 0, "Edge select mode");
			uiDefIconButBitS(block, TOG, SCE_SELECT_FACE, B_REDR, ICON_FACESEL,
				xco+=XIC,yco,XIC,YIC, &scene->selectmode, 1.0, 0.0, 0, 0, "Face select mode");

			uiBlockEndAlign(block);
		}
		else {
			uiBlockBeginAlign(block);

			uiDefIconButS(block, ROW, B_REDR, ICON_VERTEXSEL,
				xco,yco,XIC,YIC, &scene->toolsettings->uv_selectmode, 1.0, UV_SELECT_VERTEX, 0, 0, "Vertex select mode");
			uiDefIconButS(block, ROW, B_REDR, ICON_EDGESEL,
				xco+=XIC,yco,XIC,YIC, &scene->toolsettings->uv_selectmode, 1.0, UV_SELECT_EDGE, 0, 0, "Edge select mode");
			uiDefIconButS(block, ROW, B_REDR, ICON_FACESEL,
				xco+=XIC,yco,XIC,YIC, &scene->toolsettings->uv_selectmode, 1.0, UV_SELECT_FACE, 0, 0, "Face select mode");
			uiDefIconButS(block, ROW, B_REDR, ICON_LINKEDSEL,
				xco+=XIC,yco,XIC,YIC, &scene->toolsettings->uv_selectmode, 1.0, UV_SELECT_ISLAND, 0, 0, "Island select mode");

			uiBlockEndAlign(block);

			/* would use these if const's could go in strings 
			 * SI_STICKY_LOC SI_STICKY_DISABLE SI_STICKY_VERTEX */
			but = uiDefIconTextButC(block, ICONTEXTROW, B_REDR, ICON_STICKY_UVS_LOC,
					"Sticky UV Selection: %t|Disable%x1|Shared Location%x0|Shared Vertex%x2",
					xco+=XIC+10,yco,XIC+10,YIC, &(sima->sticky), 0, 3.0, 0, 0,
					"Sticky UV Selection (Hotkeys: Shift C, Alt C, Ctrl C)");
		}

		xco+= XIC + 16;
		
		/* snap options, identical to options in 3d view header */
		uiBlockBeginAlign(block);

		if (scene->snap_flag & SCE_SNAP) {
			uiDefIconButBitS(block, TOG, SCE_SNAP, B_REDR, ICON_SNAP_GEO,xco,yco,XIC,YIC, &scene->snap_flag, 0, 0, 0, 0, "Use Snap or Grid (Shift Tab).");
			xco+= XIC;
			uiDefButS(block, MENU, B_NOP, "Mode%t|Closest%x0|Center%x1|Median%x2",xco,yco,70,YIC, &scene->snap_target, 0, 0, 0, 0, "Snap Target Mode.");
			xco+= 70;
		}
		else {
			uiDefIconButBitS(block, TOG, SCE_SNAP, B_REDR, ICON_SNAP_GEAR,xco,yco,XIC,YIC, &scene->snap_flag, 0, 0, 0, 0, "Snap while Ctrl is held during transform (Shift Tab).");	
			xco+= XIC;
		}

		uiBlockEndAlign(block);
		xco+= 8;

		/* uv layers */
		{
			Object *obedit= CTX_data_edit_object(C);
			char menustr[34*MAX_MTFACE];
			static int act;
			
			image_menu_uvlayers(obedit, menustr, &act);

			but = uiDefButI(block, MENU, B_NOP, menustr ,xco,yco,85,YIC, &act, 0, 0, 0, 0, "Active UV Layer for editing.");
			// uiButSetFunc(but, do_image_buttons_set_uvlayer_callback, &act, NULL);
			
			xco+= 85;
		}

		xco+= 8;
	}
	
	if(ima) {
		RenderResult *rr;
	
		/* render layers and passes */
		rr= BKE_image_get_renderresult(scene, ima);
		if(rr) {
			uiBlockBeginAlign(block);
#if 0
			uiblock_layer_pass_buttons(block, rr, &sima->iuser, B_REDR, xco, 0, 160);
#endif
			uiBlockEndAlign(block);
			xco+= 166;
		}

		/* painting */
		uiDefIconButR(block, TOG, B_REDR, ICON_TPAINT_HLT, xco,yco,XIC,YIC, &spaceptr, "image_painting", 0, 0, 0, 0, 0, NULL);
		xco+= XIC+8;

		/* image draw options */
		uiBlockBeginAlign(block);
		uiDefIconButR(block, ROW, B_REDR, ICON_IMAGE_RGB, xco,yco,XIC,YIC, &spaceptr, "draw_channels", 0, 0, 0, 0, 0, NULL);
		xco+= XIC;
		if(ibuf==NULL || ibuf->channels==4) {
			uiDefIconButR(block, ROW, B_REDR, ICON_IMAGE_RGB_ALPHA, xco,yco,XIC,YIC, &spaceptr, "draw_channels", 0, 0, SI_USE_ALPHA, 0, 0, NULL);
			xco+= XIC;
			uiDefIconButR(block, ROW, B_REDR, ICON_IMAGE_ALPHA, xco,yco,XIC,YIC, &spaceptr, "draw_channels", 0, 0, SI_SHOW_ALPHA, 0, 0, NULL);
			xco+= XIC;
		}
		if(ibuf) {
			if(ibuf->zbuf || ibuf->zbuf_float || (ibuf->channels==1)) {
				uiDefIconButR(block, ROW, B_REDR, ICON_IMAGE_ZDEPTH, xco,yco,XIC,YIC, &spaceptr, "draw_channels", 0, 0, SI_SHOW_ZBUF, 0, 0, NULL);
				xco+= XIC;
			}
		}		
		xco+= 8;
		
		/* record & play */
		uiBlockBeginAlign(block);
		if(ima->type==IMA_TYPE_COMPOSITE) {
			uiDefIconButO(block, BUT, "IMAGE_OT_record_composite", WM_OP_INVOKE_REGION_WIN, ICON_REC, xco, yco, XIC, YIC, NULL); // Record Composite
			xco+= XIC;
		}
		if((ima->type==IMA_TYPE_COMPOSITE) || ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
//XXX			uiDefIconButO(block, BUT, "IMAGE_OT_play_composite", WM_OP_INVOKE_REGION_WIN, ICON_PLAY, xco, yco, XIC, YIC, NULL); // PLAY
			xco+= XIC;
		}
		uiBlockEndAlign(block);
		xco+= 8;
	}
	
	/* draw lock */
	uiDefIconButR(block, ICONTOG, 0, ICON_UNLOCKED,	xco,yco,XIC,YIC, &spaceptr, "update_automatically", 0, 0, 0, 0, 0, NULL);

	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}

/********************** toolbox operator *********************/

static int toolbox_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	Object *obedit= CTX_data_edit_object(C);
	uiMenuItem *head;
	int show_uvedit;

	show_uvedit= ED_space_image_show_uvedit(sima, obedit);

	head= uiPupMenuBegin("Toolbox", 0);

	uiMenuLevel(head, "View", image_viewmenu);
	if(show_uvedit) uiMenuLevel(head, "Select", image_selectmenu);
	uiMenuLevel(head, "Image", image_imagemenu);
	if(show_uvedit) uiMenuLevel(head, "UVs", image_uvsmenu);

	uiPupMenuEnd(C, head);

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

