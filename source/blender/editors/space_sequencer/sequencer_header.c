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

#include "DNA_space_types.h"
#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_utildefines.h"
#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_sequence.h"

#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_transform.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "sequencer_intern.h"


/* ************************ header area region *********************** */

#define B_FULL			1
#define B_VIEW2DZOOM	2
#define B_REDR			3
#define B_IPOBORDER		4
#define B_SEQCLEAR		5

static uiBlock *seq_viewmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceSeq *sseq= sa->spacedata.first;
	View2D *v2d= UI_view2d_fromcontext(C);

	uiBlock *block= uiBeginBlock(C, ar, "seq_viewmenu", UI_EMBOSSP, UI_HELV);
	short yco= 0, menuwidth=120;

	if (sseq->mainb == SEQ_DRAW_SEQUENCE) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1,
				 "Play Back Animation "
				 "in all Sequence Areas|Alt A", 0, yco-=20,
				 menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	}
	else {
		uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL,
				 "Grease Pencil...", 0, yco-=20,
				 menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		uiDefMenuSep(block);

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1,
				 "Play Back Animation "
				 "in this window|Alt A", 0, yco-=20,
				 menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	}
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1,
			 "Play Back Animation in all "
			 "3D Views and Sequence Areas|Alt Shift A",
			 0, yco-=20,
			 menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	uiDefMenuSep(block);

	uiDefMenuButO(block, "SEQUENCER_OT_view_all", NULL);
	uiDefMenuButO(block, "SEQUENCER_OT_view_selected", NULL);
	
	uiDefMenuSep(block);

	/* Lock Time */
	uiDefIconTextBut(block, BUTM, 1, (v2d->flag & V2D_VIEWSYNC_SCREEN_TIME)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT,
			"Lock Time to Other Windows|", 0, yco-=20,
			menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");

	/* Draw time or frames.*/
	uiDefMenuSep(block);

	if(sseq->flag & SEQ_DRAWFRAMES)
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Seconds|Ctrl T", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	else
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Frames|Ctrl T", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");

	
	if(!sa->full) uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0,0, "");
	else uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");


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

//static uiBlock *seq_selectmenu(bContext *C, ARegion *ar, void *arg_unused)
static void seq_selectmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuContext(head, WM_OP_INVOKE_REGION_WIN);

	uiMenuItemEnumO(head, "Strips to the Left", 0, "SEQUENCER_OT_select_active_side", "side", SEQ_SIDE_LEFT);
	uiMenuItemEnumO(head, "Strips to the Right", 0, "SEQUENCER_OT_select_active_side", "side", SEQ_SIDE_RIGHT);
	uiMenuSeparator(head);
	uiMenuItemEnumO(head, "Surrounding Handles", 0, "SEQUENCER_OT_select_handles", "side", SEQ_SIDE_BOTH);
	uiMenuItemEnumO(head, "Left Handles", 0, "SEQUENCER_OT_select_handles", "side", SEQ_SIDE_LEFT);
	uiMenuItemEnumO(head, "Right Handles", 0, "SEQUENCER_OT_select_handles", "side", SEQ_SIDE_RIGHT);
	uiMenuSeparator(head);
	uiMenuItemO(head, 0, "SEQUENCER_OT_select_linked");
	uiMenuSeparator(head);
	uiMenuItemO(head, 0, "SEQUENCER_OT_select_linked");
	uiMenuItemO(head, 0, "SEQUENCER_OT_select_all_toggle");
	uiMenuItemO(head, 0, "SEQUENCER_OT_select_invert");
}

static uiBlock *seq_markermenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceSeq *sseq= sa->spacedata.first;


	uiBlock *block= uiBeginBlock(C, ar, "seq_markermenu", UI_EMBOSSP, UI_HELV);
	short yco= 0, menuwidth=120;




	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Add Marker|Ctrl Alt M", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate Marker|Ctrl Shift D", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete Marker|Shift X", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");

	uiDefMenuSep(block);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "(Re)Name Marker|Ctrl M", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Move Marker|Ctrl G", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");

	uiDefMenuSep(block);

	uiDefIconTextBut(block, BUTM, 1, (sseq->flag & SEQ_MARKER_TRANS)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT,
					 "Transform Markers", 0, yco-=20,
	  				menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");


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

//static uiBlock *seq_addmenu_effectmenu(bContext *C, ARegion *ar, void *arg_unused)
static void seq_addmenu_effectmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuContext(head, WM_OP_INVOKE_REGION_WIN);
	
	uiMenuItemEnumO(head, "", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_ADD);
	uiMenuItemEnumO(head, "", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_SUB);
	uiMenuItemEnumO(head, "", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_MUL);
	uiMenuItemEnumO(head, "", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_CROSS);
	uiMenuItemEnumO(head, "", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_GAMCROSS);
	uiMenuItemEnumO(head, "", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_ALPHAOVER);
	uiMenuItemEnumO(head, "", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_ALPHAUNDER);
	uiMenuItemEnumO(head, "", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_OVERDROP);
	uiMenuItemEnumO(head, "", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_WIPE);
	uiMenuItemEnumO(head, "", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_GLOW);
	uiMenuItemEnumO(head, "", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_TRANSFORM);
	/* Color is an effect but moved to the other menu since its not that exciting */
	uiMenuItemEnumO(head, "", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_SPEED);
	uiMenuSeparator(head);
	uiMenuItemEnumO(head, "", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_PLUGIN);
}


//static uiBlock *seq_addmenu(bContext *C, ARegion *ar, void *arg_unused)
static void seq_addmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuLevel(head, "Effects...", seq_addmenu_effectmenu);
	uiMenuSeparator(head);

	uiMenuContext(head, WM_OP_INVOKE_REGION_WIN);

#ifdef WITH_FFMPEG
	uiMenuItemBooleanO(head, "Audio (RAM)", 0, "SEQUENCER_OT_add_sound_strip", "hd", FALSE);
	uiMenuItemBooleanO(head, "Audio (HD)", 0, "SEQUENCER_OT_add_sound_strip", "hd", TRUE);
#else
	uiMenuItemO(head, 0, "SEQUENCER_OT_add_sound_strip");
#endif
	uiMenuItemEnumO(head, "Add Color Strip", 0, "SEQUENCER_OT_add_effect_strip", "type", SEQ_COLOR);

	uiMenuItemO(head, 0, "SEQUENCER_OT_add_image_strip");
	uiMenuItemO(head, 0, "SEQUENCER_OT_add_movie_strip");
	uiMenuItemO(head, 0, "SEQUENCER_OT_add_scene_strip");
#ifdef WITH_FFMPEG
	uiMenuItemBooleanO(head, "Movie and Sound", 0, "SEQUENCER_OT_add_movie_strip", "sound", TRUE);
#endif
}

//static uiBlock *seq_editmenu(bContext *C, ARegion *ar, void *arg_unused)
static void seq_editmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	
	uiMenuContext(head, WM_OP_INVOKE_REGION_WIN);

	uiMenuItemEnumO(head, "", 0, "TFM_OT_transform", "mode", TFM_TRANSLATION);
	uiMenuItemEnumO(head, "", 0, "TFM_OT_transform", "mode", TFM_TIME_EXTEND);

	// uiMenuItemO(head, 0, "SEQUENCER_OT_strip_snap"); // TODO - add this operator

	uiMenuItemEnumO(head, "Cut Hard", 0, "SEQUENCER_OT_cut", "type", SEQ_CUT_HARD);
	uiMenuItemEnumO(head, "Cut Soft", 0, "SEQUENCER_OT_cut", "type", SEQ_CUT_SOFT);

	uiMenuItemO(head, 0, "SEQUENCER_OT_separate_images");
	uiMenuSeparator(head);
	uiMenuItemO(head, 0, "SEQUENCER_OT_duplicate_add");
	uiMenuItemO(head, 0, "SEQUENCER_OT_delete");

	if (ed && ed->act_seq) {
		switch(ed->act_seq->type) {
		case SEQ_EFFECT:
			uiMenuSeparator(head);
			uiMenuItemO(head, 0, "SEQUENCER_OT_effect_change");
			uiMenuItemO(head, 0, "SEQUENCER_OT_effect_reassign_inputs");
			break;
		case SEQ_IMAGE:
			uiMenuSeparator(head);
			uiMenuItemO(head, 0, "SEQUENCER_OT_image_change"); // Change Scene...
			break;
		case SEQ_SCENE:
			uiMenuSeparator(head);
			uiMenuItemO(head, 0, "SEQUENCER_OT_scene_change"); // Remap Paths...
			break;
		case SEQ_MOVIE:
			uiMenuSeparator(head);
			uiMenuItemO(head, 0, "SEQUENCER_OT_movie_change"); // Remap Paths...
			break;
		}
	}

	uiMenuSeparator(head);

	uiMenuItemO(head, 0, "SEQUENCER_OT_meta_make");
	uiMenuItemO(head, 0, "SEQUENCER_OT_meta_separate");

	if (ed && (ed->metastack.first || (ed->act_seq && ed->act_seq->type == SEQ_META))) {
		uiMenuSeparator(head);
		uiMenuItemO(head, 0, "SEQUENCER_OT_meta_toggle");
	}
	uiMenuSeparator(head);
	uiMenuItemO(head, 0, "SEQUENCER_OT_reload");
	uiMenuSeparator(head);
	uiMenuItemO(head, 0, "SEQUENCER_OT_lock");
	uiMenuItemO(head, 0, "SEQUENCER_OT_unlock");
	uiMenuItemO(head, 0, "SEQUENCER_OT_mute");
	uiMenuItemO(head, 0, "SEQUENCER_OT_unmute");

	uiMenuItemEnumO(head, "Mute Deselected Strips", 0, "SEQUENCER_OT_mute", "type", SEQ_UNSELECTED);
}

void sequencer_header_buttons(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceSeq *sseq= sa->spacedata.first;
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	
	uiBlock *block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS, UI_HELV);
	int xco=3, yco= 3;
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		int xmax;
		
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
		
		xmax= GetButStringLength("View");

		//uiDefMenuBut(block, seq_viewmenu, NULL, "View", xco, 0, xmax-3, 24, ""); // TODO
		uiDefPulldownBut(block, seq_viewmenu, sa, "View", xco, 0, xmax-3, 24, ""); 
		xco+=xmax;

		xmax= GetButStringLength("Select");
		uiDefMenuBut(block, seq_selectmenu, NULL, "Select", xco, 0, xmax-3, 24, "");
		//uiDefPulldownBut(block, seq_selectmenu, sa, "Select", xco, 0, xmax-3, 24, "");
		xco+=xmax;

		xmax= GetButStringLength("Marker");
		//uiDefMenuBut(block, seq_markermenu, NULL, "Marker", xco, 0, xmax-3, 24, "");
		uiDefPulldownBut(block, seq_markermenu, sa, "Marker", xco, 0, xmax-3, 24, ""); // TODO
		xco+=xmax;

		xmax= GetButStringLength("Add");
		uiDefMenuBut(block, seq_addmenu, NULL, "Add", xco, 0, xmax-3, 24, "");
		//uiDefPulldownBut(block, seq_addmenu, sa, "Add", xco, 0, xmax-3, 24, "");
		xco+=xmax;

		xmax= GetButStringLength("Strip");
		uiDefMenuBut(block, seq_editmenu, NULL, "Strip", xco, 0, xmax-3, 24, "");
		//uiDefPulldownBut(block, seq_editmenu, sa, "Strip", xco, 0, xmax-3, 24, "");
		xco+=xmax;
	}

	uiBlockSetEmboss(block, UI_EMBOSS);

	/* IMAGE */
	uiDefIconTextButS(block, ICONTEXTROW,B_REDR, ICON_SEQ_SEQUENCER,
			  "Image Preview: %t"
			  "|Sequence %x0"
			  "|Image Preview %x1"
			  "|Luma Waveform %x2"
			  "|Chroma Vectorscope %x3"
			  "|Histogram %x4",
			  xco,yco,XIC+10,YIC, &sseq->mainb, 0.0, 3.0,
			  0, 0,
			  "Shows the sequence output image preview");

	xco+= 8 + XIC+10;

	if(sseq->mainb != SEQ_DRAW_SEQUENCE) {
		int minchan = 0;

		/* CHANNEL shown in image preview */

		if (ed && ed->metastack.first)
			minchan = -BLI_countlist(&ed->metastack);

		uiDefButS(block, NUM, B_REDR, "Chan:",
			  xco, yco, 3.5 * XIC,YIC,
			  &sseq->chanshown, minchan, MAXSEQ, 0, 0,
			  "The channel number shown in the image preview. 0 is the result of all strips combined.");

		xco+= 8 + XIC*3.5;

		if (sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
			uiDefButS(block, MENU, B_REDR,
				  "Show zebra: %t"
				  "|Z 110 %x110"
				  "|Z 100 %x100"
				  "|Z 95  %x95"
				  "|Z 90  %x90"
				  "|Z 70  %x70"
				  "|Z Off %x0",
				  xco,yco,3.0 * XIC, YIC, &sseq->zebra,
				  0,0,0,0,
				  "Show overexposed "
				  "areas with zebra stripes");

			xco+= 8 + XIC*3.0;

			uiDefButBitI(block, TOG, SEQ_DRAW_SAFE_MARGINS,
				     B_REDR, "T",
				     xco,yco,XIC,YIC, &sseq->flag,
				     0, 0, 0, 0,
				     "Draw title safe margins in preview");
			xco+= 8 + XIC;
		}

		if (sseq->mainb == SEQ_DRAW_IMG_WAVEFORM) {
			uiDefButBitI(block, TOG, SEQ_DRAW_COLOR_SEPERATED,
				     B_REDR, "CS",
				     xco,yco,XIC,YIC, &sseq->flag,
				     0, 0, 0, 0,
				     "Seperate color channels in preview");
			xco+= 8 + XIC;
		}
	} else {
		/* ZOOM and BORDER */
		static int viewmovetemp; // XXX dummy var
		
		uiBlockBeginAlign(block);
		uiDefIconButI(block, TOG, B_VIEW2DZOOM,
			      ICON_VIEWZOOM,
			      xco,yco,XIC,YIC, &viewmovetemp,
			      0, 0, 0, 0,
			      "Zooms view in and out (Ctrl MiddleMouse)");
		xco += XIC;
		uiDefIconButO(block, BUT, "VIEW2D_OT_zoom_border", WM_OP_INVOKE_REGION_WIN, ICON_BORDERMOVE, xco,yco,XIC,YIC, "Zooms view to fit area");
		
		uiBlockEndAlign(block);
		xco += 8 + XIC;
	}

	uiDefButO(block, BUT, "SEQUENCER_OT_refresh_all", WM_OP_EXEC_DEFAULT, "Refresh", xco, yco, 3*XIC, YIC, "Clears all buffered images in memory");

	uiBlockSetEmboss(block, UI_EMBOSS);

	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}

