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

#include "ED_mesh.h"
#include "ED_screen.h"
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

static uiBlock *image_view_viewnavmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	uiBlock *block;
	uiBut *but;
	int a;
	
	/* create menu */
	block= uiBeginBlock(C, handle->region, "image_view_viewnavmenu", UI_EMBOSSP, UI_HELV);
	
	uiDefMenuButO(block, "IMAGE_OT_view_zoom_in", NULL);
	uiDefMenuButO(block, "IMAGE_OT_view_zoom_out", NULL);

	uiDefMenuSep(block);

	for(a=0; a<7; a++) {
		const int ratios[7][2] = {{1, 8}, {1, 4}, {1, 2}, {1, 1}, {2, 1}, {4, 1}, {8, 1}};
		char namestr[128];

		sprintf(namestr, "Zoom %d:%d", ratios[a][0], ratios[a][1]);

		but= uiDefMenuButO(block, "IMAGE_OT_view_zoom_ratio", namestr);
		RNA_float_set(uiButGetOperatorPtrRNA(but), "ratio", (float)ratios[a][0]/(float)ratios[a][1]);
	}

	/* XXX find key shortcut! */

	/* position menu */
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);

	return block;
}

#if 0
static void do_viewmenu(bContext *C, void *arg, int event)
{
	switch(event) {
	case 1: /* View All */
		do_image_buttons(B_SIMAGEHOME);
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
	case 15: /* Grease Pencil... */
		add_blockhandler(curarea, IMAGE_HANDLER_GREASEPENCIL, UI_PNL_UNSTOW);
		break;
	}

	allqueue(REDRAWIMAGE, 0);
	allqueue(REDRAWVIEW3D, 0);
}
#endif

static uiBlock *image_viewmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa= CTX_wm_area(C);
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	PointerRNA spaceptr, uvptr;
	uiBlock *block;
	int show_paint, show_render, show_uvedit;

	/* retrrieve state */
	RNA_pointer_create(&sc->id, &RNA_SpaceImageEditor, sima, &spaceptr);
	RNA_pointer_create(&sc->id, &RNA_SpaceUVEditor, sima, &uvptr);

	show_render= get_space_image_show_render(sima);
	show_paint= get_space_image_show_paint(sima);
	show_uvedit= get_space_image_show_uvedit(sima, CTX_data_edit_object(C));
	
	/* create menu */
	block= uiBeginBlock(C, handle->region, "image_viewmenu", UI_EMBOSSP, UI_HELV);
	
	uiDefMenuButO(block, "IMAGE_OT_toggle_view_properties_panel", NULL); // View Properties...
	uiDefMenuButO(block, "IMAGE_OT_toggle_image_properties_panel", NULL); // Image Properties...|N
	uiDefMenuButO(block, "IMAGE_OT_toggle_realtime_properties_panel", NULL); // Real-time properties...
	if(show_paint) uiDefMenuButO(block, "IMAGE_OT_toggle_paint_panel", NULL); // Paint Tool...|C
	uiDefMenuButO(block, "IMAGE_OT_toggle_curves_panel", NULL); // Curves Tool...
	if(show_render) uiDefMenuButO(block, "IMAGE_OT_toggle_compositing_preview_panel", NULL); // Compositing Preview...|Shift P
	uiDefMenuButO(block, "IMAGE_OT_toggle_grease_pencil_panel", NULL); // Grease Pencil...

	uiDefMenuSep(block);

	uiDefMenuTogR(block, &spaceptr, "update_automatically", NULL, NULL);
	// XXX if(show_uvedit) uiDefMenuTogR(block, &uvptr, "local_view", NULL, "UV Local View"); // Numpad /

	uiDefMenuSep(block);

	uiDefMenuSub(block, image_view_viewnavmenu, "View Navigation");
	if(show_uvedit) uiDefMenuButO(block, "IMAGE_OT_view_selected", NULL);
	uiDefMenuButO(block, "IMAGE_OT_view_all", NULL);

	if(sa->full) uiDefMenuButO(block, "SCREEN_OT_screen_full_area", "Tile Window"); // Ctrl UpArrow
	else uiDefMenuButO(block, "SCREEN_OT_screen_full_area", "Maximize Window"); // Ctr DownArrow
	
	/* position menu */
	if(sa->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	uiEndBlock(C, block);
	
	return block;
}

#if 0
static void do_selectmenu(bContext *C, void *arg, int event)
{
	switch(event)
	{
	case 0: /* Border Select */
		borderselect_sima(UV_SELECT_ALL);
		break;
	case 8: /* Border Select Pinned */
		borderselect_sima(UV_SELECT_PINNED);
		break;
	case 7: /* Pinned UVs */
		select_pinned_tface_uv();
		break;
	}
}
#endif

static uiBlock *image_selectmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *sa= CTX_wm_area(C);
	uiBlock *block;
	
	/* create menu */
	block= uiBeginBlock(C, handle->region, "image_selectmenu", UI_EMBOSSP, UI_HELV);

	uiDefMenuButO(block, "UV_OT_border_select", NULL); // Border Select|B
	uiDefMenuButO(block, "UV_OT_border_select_pinned", NULL); // Border Select Pinned|Shift B

	uiDefMenuSep(block);
	
	uiDefMenuButO(block, "UV_OT_de_select_all", NULL);
	uiDefMenuButO(block, "UV_OT_select_inverse", NULL);
	uiDefMenuButO(block, "UV_OT_unlink_selection", NULL); // Unlink Selection|Alt L
	
	uiDefMenuSep(block);

	uiDefMenuButO(block, "UV_OT_select_pinned", NULL); // Select Pinned|Shift P
	uiDefMenuButO(block, "UV_OT_select_linked", NULL); // Select Linked|Ctrl L

	/* position menu */
	if(sa->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	uiEndBlock(C, block);
	
	return block;
}

#if 0
static void do_image_imagemenu(void *arg, int event)
{
	/* events >=20 are registered bpython scripts */
#ifndef DISABLE_PYTHON
	if (event >= 20) BPY_menu_do_python(PYMENU_IMAGE, event - 20);
#endif	
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
		if(sima->flag & SI_DRAWTOOL) sima->flag &= ~SI_DRAWTOOL;
		else sima->flag |= SI_DRAWTOOL;
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
		BKE_image_memorypack(sima->image);
		allqueue(REDRAWIMAGE, 0);
		break;
	}
}
#endif

/* move to realtime properties panel */
#if 0
static void do_image_image_rtmappingmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* UV Co-ordinates */
		sima->image->flag &= ~IMA_REFLECT;
		break;
	case 1: /* Reflection */
		sima->image->flag |= IMA_REFLECT;
		break;
	}

 	allqueue(REDRAWVIEW3D, 0);
}
#endif

static uiBlock *image_imagemenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa= CTX_wm_area(C);
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	uiBlock *block;
	PointerRNA spaceptr;
	Image *ima;
	ImBuf *ibuf;
	int show_render;
	
	/* retrieve state */
	ima= get_space_image(sima);
	ibuf= get_space_image_buffer(sima);

	show_render= get_space_image_show_render(sima);

	RNA_pointer_create(&sc->id, &RNA_SpaceImageEditor, sima, &spaceptr);

	/* create menu */
	block= uiBeginBlock(C, handle->region, "image_imagemenu", UI_EMBOSSP, UI_HELV);

	uiDefMenuButO(block, "IMAGE_OT_new", NULL); // New...|Alt N
	uiDefMenuButO(block, "IMAGE_OT_open", NULL); // Open...|Alt O

	if(ima) {
		uiDefMenuButO(block, "IMAGE_OT_replace", NULL); // Replace...
		uiDefMenuButO(block, "IMAGE_OT_reload", NULL); // Reload...|Alt R
		uiDefMenuButO(block, "IMAGE_OT_save", NULL); // Save|Alt S
		uiDefMenuButO(block, "IMAGE_OT_save_as", NULL); // Save As...
		if(ima->source == IMA_SRC_SEQUENCE)
			uiDefMenuButO(block, "IMAGE_OT_save_changed", NULL); // Save Changed Images

		if(!show_render) {
			uiDefMenuSep(block);

			if(ima->packedfile) uiDefMenuButO(block, "IMAGE_OT_unpack", NULL); // Unpack Image...
			else uiDefMenuButO(block, "IMAGE_OT_pack", NULL); // Pack Image

			/* only for dirty && specific image types : XXX poll? */
			if(ibuf && (ibuf->userflags & IB_BITMAPDIRTY))
				if(ELEM(ima->source, IMA_SRC_FILE, IMA_SRC_GENERATED) && ima->type != IMA_TYPE_MULTILAYER)
					uiDefMenuButO(block, "IMAGE_OT_pack_as_png", NULL); // Pack Image As PNG

			uiDefMenuSep(block);

			/* XXX check state better */
			uiDefMenuTogR(block, &spaceptr, "image_painting", NULL, NULL);
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

	/* position menu */
	if(sa->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 80);
	uiEndBlock(C, block);
	
	return block;
}

#if 0
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
#endif

static uiBlock *image_uvs_showhidemenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	uiBlock *block;
	
	/* create menu */
	block= uiBeginBlock(C, handle->region, "image_uvs_showhidemenu", UI_EMBOSSP, UI_HELV);

	uiDefMenuButO(block, "UV_OT_show_hidden_faces", NULL); // Show Hidden Faces|Alt H
	uiDefMenuButO(block, "UV_OT_hide_selected_faces", NULL); // Hide Selected Faces|H
	uiDefMenuButO(block, "UV_OT_hide_deselected_faces", NULL); // Hide Deselected Faces|Shift H

	/* position menu */
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	uiEndBlock(C, block);

	return block;
}

#if 0
static void do_image_uvs_propfalloffmenu(void *arg, int event)
{
	G.scene->prop_mode= event;
	allqueue(REDRAWVIEW3D, 1);
}
#endif

static uiBlock *image_uvs_propfalloffmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	Scene *scene= CTX_data_scene(C);
	PointerRNA sceneptr;
	uiBlock *block;
	
	/* retrieve state */
	RNA_id_pointer_create(&scene->id, &sceneptr);
	
	/* create menu */
	block= uiBeginBlock(C, handle->region, "image_uvs_propfalloffmenu", UI_EMBOSSP, UI_HELV);

	uiDefMenuTogR(block, &sceneptr, "proportional_editing_falloff", "SMOOTH", NULL); // Smooth|Shift O
	uiDefMenuTogR(block, &sceneptr, "proportional_editing_falloff", "SPHERE", NULL); // Sphere|Shift O
	uiDefMenuTogR(block, &sceneptr, "proportional_editing_falloff", "ROOT", NULL); // Root|Shift O
	uiDefMenuTogR(block, &sceneptr, "proportional_editing_falloff", "SHARP", NULL); // Sharp|Shift O
	uiDefMenuTogR(block, &sceneptr, "proportional_editing_falloff", "LINEAR", NULL); // Linear|Shift O
	uiDefMenuTogR(block, &sceneptr, "proportional_editing_falloff", "RANDOM", NULL); // Random|Shift O
	uiDefMenuTogR(block, &sceneptr, "proportional_editing_falloff", "CONSTANT", NULL); // Constant|Shift O

	/* position menu */
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	uiEndBlock(C, block);

	return block;
}

#if 0
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
#endif

static uiBlock *image_uvs_transformmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	uiBlock *block;
	
	/* create menu */
	block= uiBeginBlock(C, handle->region, "image_uvs_transformmenu", UI_EMBOSSP, UI_HELV);

	uiDefMenuButO(block, "UV_OT_grab", NULL); // Grab/Move|G
	uiDefMenuButO(block, "UV_OT_rotate", NULL); // Rotate|R
	uiDefMenuButO(block, "UV_OT_scale", NULL); // Scale|S

	/* position menu */
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	uiEndBlock(C, block);

	return block;
}

#if 0
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
#endif

static uiBlock *image_uvs_mirrormenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	uiBlock *block;
	uiBut *but;
	
	/* create menu */
	block= uiBeginBlock(C, handle->region, "image_uvs_mirrormenu", UI_EMBOSSP, UI_HELV);

	but= uiDefMenuButO(block, "UV_OT_mirror", "X Axis"); // M, 1
	RNA_enum_set(uiButGetOperatorPtrRNA(but), "axis", 'x');
	but= uiDefMenuButO(block, "UV_OT_mirror", "Y Axis"); // M, 2
	RNA_enum_set(uiButGetOperatorPtrRNA(but), "axis", 'y');

	/* position menu */
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	uiEndBlock(C, block);

	return block;
}

#if 0
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
#endif

static uiBlock *image_uvs_weldalignmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	uiBlock *block;
	uiBut *but;
	
	/* create menu */
	block= uiBeginBlock(C, handle->region, "image_uvs_weldalignmenu", UI_EMBOSSP, UI_HELV);

	but= uiDefMenuButO(block, "UV_OT_weld", NULL); // W, 1
	but= uiDefMenuButO(block, "UV_OT_align", "Align Auto"); // W, 2
	RNA_enum_set(uiButGetOperatorPtrRNA(but), "axis", 'a');
	but= uiDefMenuButO(block, "UV_OT_align", "Align X"); // W, 3
	RNA_enum_set(uiButGetOperatorPtrRNA(but), "axis", 'x');
	but= uiDefMenuButO(block, "UV_OT_align", "Align Y"); // W, 4
	RNA_enum_set(uiButGetOperatorPtrRNA(but), "axis", 'y');

	/* position menu */
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	uiEndBlock(C, block);

	return block;
}

#if 0
#ifndef DISABLE_PYTHON
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
#endif /* DISABLE_PYTHON */
#endif

#if 0
static void do_uvsmenu(bContext *C, void *arg, int event)
{
	switch(event) {
	case 1: /* UVs Constrained Rectangular */
		if(sima->flag & SI_BE_SQUARE) sima->flag &= ~SI_BE_SQUARE;
		else sima->flag |= SI_BE_SQUARE;
		break;
	case 2: /* UVs Clipped to Image Size */
		if(sima->flag & SI_CLIP_UV) sima->flag &= ~SI_CLIP_UV;
		else sima->flag |= SI_CLIP_UV;
		break;
	case 5: /* Proportional Edit (toggle) */
		if(G.scene->proportional)
			G.scene->proportional= 0;
		else
			G.scene->proportional= 1;
		break;
	case 7: /* UVs Snap to Pixel */
		sima->flag ^= SI_PIXELSNAP;
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
		if(sima->flag & SI_LIVE_UNWRAP) sima->flag &= ~SI_LIVE_UNWRAP;
		else sima->flag |= SI_LIVE_UNWRAP;
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
#endif

static uiBlock *image_uvsmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa= CTX_wm_area(C);
	Scene *scene= CTX_data_scene(C);
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	uiBlock *block;
	PointerRNA uvptr, sceneptr;
	Image *ima;
	ImBuf *ibuf;
	
	/* retrieve state */
	ima= get_space_image(sima);
	ibuf= get_space_image_buffer(sima);

	RNA_pointer_create(&sc->id, &RNA_SpaceUVEditor, sima, &uvptr);
	RNA_id_pointer_create(&scene->id, &sceneptr);

	/* create menu */
	block= uiBeginBlock(C, handle->region, "image_imagemenu", UI_EMBOSSP, UI_HELV);

	uiDefMenuTogR(block, &uvptr, "snap_to_pixels", 0, NULL);
	uiDefMenuTogR(block, &uvptr, "constrain_quads_rectangular", 0, NULL);
	uiDefMenuTogR(block, &uvptr, "constrain_to_image_bounds", 0, NULL);

	uiDefMenuSep(block);

	uiDefMenuTogR(block, &uvptr, "live_unwrap", 0, NULL);
	uiDefMenuButO(block, "UV_OT_unwrap", NULL); // Unwrap|E
	uiDefMenuButO(block, "UV_OT_unpin", NULL); // Unpin|Alt P
	uiDefMenuButO(block, "UV_OT_pin", NULL); // Pin|P

	uiDefMenuSep(block);

	uiDefMenuButO(block, "UV_OT_pack_islands", NULL); // Pack Islands|Ctr P
	uiDefMenuButO(block, "UV_OT_average_islands", NULL); // Average Islands Scale|Ctrl A
	uiDefMenuButO(block, "UV_OT_minimize_stretch", NULL); // Minimize Stretch...|Ctrl V
	uiDefMenuButO(block, "UV_OT_stitch", NULL);

	uiDefMenuSep(block);

	uiDefMenuSub(block, image_uvs_transformmenu, "Transform");
	uiDefMenuSub(block, image_uvs_mirrormenu, "Mirror");
	uiDefMenuSub(block, image_uvs_weldalignmenu, "Weld/Align");

	uiDefMenuSep(block);

	uiDefMenuTogR(block, &sceneptr, "proportional_editing", 0, NULL);
	uiDefMenuSub(block, image_uvs_propfalloffmenu, "Proportional Falloff");

	uiDefMenuSep(block);

	uiDefMenuSub(block, image_uvs_showhidemenu, "Show/Hide Faces");

#if 0
#ifndef DISABLE_PYTHON
	uiDefMenuSep(block);

	uiDefMenuSub(block, image_uvs_scriptsmenu, "Scripts");
#endif
#endif

	if(sa->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	uiEndBlock(C, block);
	
	return block;
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
	case B_SIMAPIN:
		allqueue (REDRAWIMAGE, 0);
		break;
	case B_SIMAGEHOME:
		image_home();
		break;

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
	case B_SIMAGEPAINTTOOL:
		if(sima->flag & SI_DRAWTOOL)
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
	int xco, yco= 3, show_uvedit, show_render, show_paint;

	/* retrieve state */
	ima= get_space_image(sima);
	ibuf= get_space_image_buffer(sima);

	show_render= get_space_image_show_render(sima);
	show_paint= get_space_image_show_paint(sima);
	show_uvedit= get_space_image_show_uvedit(sima, CTX_data_edit_object(C));

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
		uiDefPulldownBut(block, image_viewmenu, NULL, "View", xco, yco-2, xmax-3, 24, "");
		xco+= xmax;
		
		if(show_uvedit) {
			xmax= GetButStringLength("Select");
			uiDefPulldownBut(block, image_selectmenu, NULL, "Select", xco, yco-2, xmax-3, 24, "");
			xco+= xmax;
		}
		
		menuname= (ibuf && (ibuf->userflags & IB_BITMAPDIRTY))? "Image*": "Image";
		xmax= GetButStringLength(menuname);
		uiDefPulldownBut(block, image_imagemenu, NULL, menuname, xco, yco-2, xmax-3, 24, "");
		xco+= xmax;

		if(show_uvedit) {
			xmax= GetButStringLength("UVs");
			uiDefPulldownBut(block, image_uvsmenu, NULL, "UVs", xco, yco-2, xmax-3, 24, "");
			xco+= xmax;
		}
	}

	uiBlockSetEmboss(block, UI_EMBOSS);

	/* image select */
#if 0
	char naam[256];
	
	/* This should not be a static var */
	static int headerbuttons_packdummy;
	
	headerbuttons_packdummy = 0;

	int allow_pin= (show_render)? 0: B_SIMAPIN;
	
	xco= 8 + std_libbuttons(block, xco, yco, allow_pin, &sima->pin, B_SIMABROWSE, ID_IM, 0, (ID *)ima, 0, &(sima->imanr), 0, 0, B_IMAGEDELETE, 0, 0);
	
	if(ima && !ELEM3(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE, IMA_SRC_VIEWER) && ima->ok) {

		if (ima->packedfile) {
			headerbuttons_packdummy = 1;
		}
		if (ima->packedfile && ibuf && (ibuf->userflags & IB_BITMAPDIRTY))
			uiDefIconButBitI(block, TOG, 1, B_SIMA_REPACK, ICON_UGLYPACKAGE,	xco,yco,XIC,YIC, &headerbuttons_packdummy, 0, 0, 0, 0, "Re-Pack this image as PNG");
		else
			uiDefIconButBitI(block, TOG, 1, B_SIMAPACKIMA, ICON_PACKAGE,	xco,yco,XIC,YIC, &headerbuttons_packdummy, 0, 0, 0, 0, "Pack/Unpack this image");
			
		xco+= XIC+8;
	}
#endif
	
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
			uiDefIconButS(block, ROW, B_REDR, ICON_MESH,
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
		xco+= 10;

		/* uv layers */
		{
			Object *obedit= CTX_data_edit_object(C);
			char menustr[34*MAX_MTFACE];
			static int act;
			
			image_menu_uvlayers(obedit, menustr, &act);

			but = uiDefButI(block, MENU, B_NOP, menustr ,xco,yco,85,YIC, &act, 0, 0, 0, 0, "Active UV Layer for editing.");
			// uiButSetFunc(but, do_image_buttons_set_uvlayer_callback, &act, NULL);
			
			xco+= 90;
		}
	}
	
	if(ima) {
		RenderResult *rr;
		
		xco+= 8;
	
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
		uiDefIconButR(block, ROW, B_REDR, ICON_TEXTURE, xco,yco,XIC,YIC, &spaceptr, "draw_channels", 0, 0, 0, 0, 0, NULL);
		xco+= XIC;
		if(ibuf==NULL || ibuf->channels==4) {
			uiDefIconButR(block, ROW, B_REDR, ICON_TRANSP_HLT, xco,yco,XIC,YIC, &spaceptr, "draw_channels", 0, 0, SI_USE_ALPHA, 0, 0, NULL);
			xco+= XIC;
			uiDefIconButR(block, ROW, B_REDR, ICON_DOT, xco,yco,XIC,YIC, &spaceptr, "draw_channels", 0, 0, SI_SHOW_ALPHA, 0, 0, NULL);
			xco+= XIC;
		}
		if(ibuf) {
			if(ibuf->zbuf || ibuf->zbuf_float || (ibuf->channels==1)) {
				uiDefIconButR(block, ROW, B_REDR, ICON_SOLID, xco,yco,XIC,YIC, &spaceptr, "draw_channels", 0, 0, SI_SHOW_ZBUF, 0, 0, NULL);
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
			uiDefIconButO(block, BUT, "IMAGE_OT_play_composite", WM_OP_INVOKE_REGION_WIN, ICON_PLAY, xco, yco, XIC, YIC, NULL); // PLAY
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

