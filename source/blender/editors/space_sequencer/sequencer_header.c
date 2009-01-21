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

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_sequence.h"

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

#include "sequencer_intern.h"


/* ************************ header area region *********************** */

#define B_FULL			1
#define B_VIEW2DZOOM	2
#define B_REDR			3
#define B_IPOBORDER		4
#define B_SEQCLEAR		5

static void do_viewmenu(bContext *C, void *arg, int event)
{
	
}

static uiBlock *seq_viewmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceSeq *sseq= sa->spacedata.first;
	View2D *v2d= UI_view2d_fromcontext(C);

	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, handle->region, "seq_viewmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_viewmenu, NULL);

	

	if (sseq->mainb) {
		uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL,
				 "Grease Pencil...", 0, yco-=20,
				 menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");

		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	}

	if (sseq->mainb == 0) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1,
				 "Play Back Animation "
				 "in all Sequence Areas|Alt A", 0, yco-=20,
				 menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	} else {
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

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View All|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View Selected|NumPad .", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	/* Lock Time */
#define V2D_VIEWLOCK 0 // XXX add back

	uiDefIconTextBut(block, BUTM, 1, (v2d->flag & V2D_VIEWLOCK)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT,
			"Lock Time to Other Windows|", 0, yco-=20,
			menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");

	/* Draw time or frames.*/
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	if(sseq->flag & SEQ_DRAWFRAMES)
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Seconds|T", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	else
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Frames|T", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");

	
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


static void do_selectmenu(bContext *C, void *arg, int event)
{

}

static uiBlock *seq_selectmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceSeq *sseq= sa->spacedata.first;
	View2D *v2d= UI_view2d_fromcontext(C);

	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiBeginBlock(C, handle->region, "seq_selectmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_selectmenu, NULL);



	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Strips to the Left", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Strips to the Right", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Surrounding Handles", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Left Handles", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Right Handles", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Linked|Ctrl L", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All Strips|A", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All Markers|Ctrl A", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");



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


static void do_markermenu(bContext *C, void *arg, int event)
{
#if 0
	switch(event)
	{
		case 1:
			add_marker(CFRA);
			break;
		case 2:
			duplicate_marker();
			break;
		case 3:
			remove_marker();
			break;
		case 4:
			rename_marker();
			break;
		case 5:
			transform_markers('g', 0);
			break;
		case 6:
			sseq->flag ^= SEQ_MARKER_TRANS;
			break;

	}
#endif
}

static uiBlock *seq_markermenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceSeq *sseq= sa->spacedata.first;
	View2D *v2d= UI_view2d_fromcontext(C);

	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiBeginBlock(C, handle->region, "seq_markermenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_markermenu, NULL);




	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Add Marker|Ctrl Alt M", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate Marker|Ctrl Shift D", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete Marker|Shift X", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "(Re)Name Marker|Ctrl M", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Move Marker|Ctrl G", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

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


static void do_seq_addmenu_effectmenu(void *arg, int event)
{
#if 0
	switch(event)
	{
	case 0:
		add_sequence(SEQ_ADD);
		break;
	case 1:
		add_sequence(SEQ_SUB);
		break;
	case 2:
		add_sequence(SEQ_MUL);
		break;
	case 3:
		add_sequence(SEQ_CROSS);
		break;
	case 4:
		add_sequence(SEQ_GAMCROSS);
		break;
	case 5:
		add_sequence(SEQ_ALPHAOVER);
		break;
	case 6:
		add_sequence(SEQ_ALPHAUNDER);
		break;
	case 7:
		add_sequence(SEQ_OVERDROP);
		break;
	case 8:
		add_sequence(SEQ_PLUGIN);
		break;
	case 9:
		add_sequence(SEQ_WIPE);
		break;
	case 10:
		add_sequence(SEQ_GLOW);
		break;
	case 11:
		add_sequence(SEQ_TRANSFORM);
		break;
	case 12:
		add_sequence(SEQ_COLOR);
		break;
	case 13:
		add_sequence(SEQ_SPEED);
		break;
	}
#endif
}

static uiBlock *seq_addmenu_effectmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiBeginBlock(C, handle->region, "seq_addmenu_effectmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_seq_addmenu_effectmenu, NULL);


	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Add", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Subtract", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Multiply", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cross", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Gamma Cross", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Alpha Over", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Alpha Under", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Alpha Over Drop", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Wipe", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Glow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Color Generator", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 12, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Speed Control", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 13, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Plugin...", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");


	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);
	return block;
}


static void do_addmenu(bContext *C, void *arg, int event)
{
#if 0
	switch(event)
	{
	case 0:
		add_sequence(SEQ_IMAGE);
		break;
	case 1:
		add_sequence(SEQ_MOVIE);
		break;
	case 2:
		add_sequence(SEQ_RAM_SOUND);
		break;
	case 3:
		add_sequence(SEQ_HD_SOUND);
		break;
	case 4:
		add_sequence(SEQ_SCENE);
		break;
	case 5:
		add_sequence(SEQ_MOVIE_AND_HD_SOUND);
		break;
	}
#endif
}

static uiBlock *seq_addmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceSeq *sseq= sa->spacedata.first;
	View2D *v2d= UI_view2d_fromcontext(C);

	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiBeginBlock(C, handle->region, "seq_addmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_addmenu, NULL);

	

	uiDefIconTextBlockBut(block, seq_addmenu_effectmenu, NULL, ICON_RIGHTARROW_THIN, "Effect", 0, yco-=20, 120, 19, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

#ifdef WITH_FFMPEG
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Audio (RAM)", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Audio (HD)", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
#else
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Audio (Wav)", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
#endif
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Scene", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Images", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Movie", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
#ifdef WITH_FFMPEG
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Movie + Audio (HD)", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
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


static void do_editmenu(bContext *C, void *arg, int event)
{

}

static uiBlock *seq_editmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceSeq *sseq= sa->spacedata.first;
	View2D *v2d= UI_view2d_fromcontext(C);
	Scene *scene= CTX_data_scene(C);
	Editing *ed= scene->ed;

	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiBeginBlock(C, handle->region, "seq_editmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_editmenu, NULL);



	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Move|G", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Extend from frame|E", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap to Current Frame|Shift S, 1", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cut (hard) at Current Frame|K", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cut (soft) at Current Frame|Shift-K", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 23, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Separate Images to Strips|Y", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 16, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete|X", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");

	if (ed->act_seq != NULL && ed->act_seq->type != SEQ_MOVIE) {
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		if(ed->act_seq->type >= SEQ_EFFECT) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Change Effect...|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reassign Inputs|R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 14, "");
		}
		else if(ed->act_seq->type == SEQ_IMAGE) uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Change Image...|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Change Scene...|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");

		if(ed->act_seq->type==SEQ_IMAGE)
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Remap Paths...|Shift R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 15, "");

	}

	if (ed->act_seq != NULL && ed->act_seq->type == SEQ_MOVIE) {
/*		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Set Filter Y|F", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, ""); */
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Remap Paths...|Shift R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 15, "");
	}


	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Meta Strip...|M", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Separate Meta Strip...|Alt M", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	if ((ed != NULL) && (ed->metastack.first > 0)){
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Enter/Exit Meta Strip|Tab", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
	}
	else {
		if (ed->act_seq != NULL && ed->act_seq->type == SEQ_META) {
			uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Enter/Exit Meta Strip|Tab", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
		}
	}

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reload Strip Data...|Alt R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 17, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Lock Strips...|Shift L", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 18, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Unlock Strips...|Alt-Shift L", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 19, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Mute Strips...|H", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 20, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Unmute Strips...|Alt H", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 21, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Mute Deselected Strips...|Shift H", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 22, "");



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


static void do_sequencer_buttons(bContext *C, void *arg, int event)
{
	switch(event) {
	}
}


void sequencer_header_buttons(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceSeq *sseq= sa->spacedata.first;
	Scene *scene= CTX_data_scene(C);
	Editing *ed= scene->ed;
	
	uiBlock *block;
	int xco, yco= 3;
	
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS, UI_HELV);
	uiBlockSetHandleFunc(block, do_sequencer_buttons, NULL);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		int xmax;
		
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
		
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, seq_viewmenu, sa, "View", xco, yco-2, xmax-3, 24, "");
		xco+=xmax;

		xmax= GetButStringLength("Select");
		uiDefPulldownBut(block, seq_selectmenu, sa, "Select", xco, yco-2, xmax-3, 24, "");
		xco+=xmax;

		xmax= GetButStringLength("Marker");
		uiDefPulldownBut(block, seq_markermenu, sa, "Marker", xco, yco-2, xmax-3, 24, "");
		xco+=xmax;

		xmax= GetButStringLength("Add");
		uiDefPulldownBut(block, seq_addmenu, sa, "Add", xco, yco-2, xmax-3, 24, "");
		xco+=xmax;

		xmax= GetButStringLength("Strip");
		uiDefPulldownBut(block, seq_editmenu, sa, "Strip", xco, yco-2, xmax-3, 24, "");
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
			  xco,0,XIC+10,YIC, &sseq->mainb, 0.0, 3.0,
			  0, 0,
			  "Shows the sequence output image preview");

	xco+= 8 + XIC+10;

	if(sseq->mainb) {
		int minchan = 0;

		/* CHANNEL shown in image preview */

		if (ed && ed->metastack.first)
			minchan = -BLI_countlist(&ed->metastack);

		uiDefButS(block, NUM, B_REDR, "Chan:",
			  xco, 0, 3.5 * XIC,YIC,
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
				  xco,0,3.0 * XIC, YIC, &sseq->zebra,
				  0,0,0,0,
				  "Show overexposed "
				  "areas with zebra stripes");

			xco+= 8 + XIC*3.0;

			uiDefButBitI(block, TOG, SEQ_DRAW_SAFE_MARGINS,
				     B_REDR, "T",
				     xco,0,XIC,YIC, &sseq->flag,
				     0, 0, 0, 0,
				     "Draw title safe margins in preview");
			xco+= 8 + XIC;
		}

		if (sseq->mainb == SEQ_DRAW_IMG_WAVEFORM) {
			uiDefButBitI(block, TOG, SEQ_DRAW_COLOR_SEPERATED,
				     B_REDR, "CS",
				     xco,0,XIC,YIC, &sseq->flag,
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
			      xco,0,XIC,YIC, &viewmovetemp,
			      0, 0, 0, 0,
			      "Zooms view in and out (Ctrl MiddleMouse)");
		xco += XIC;
		uiDefIconBut(block, BUT, B_IPOBORDER,
			     ICON_BORDERMOVE,
			     xco,0,XIC,YIC, 0,
			     0, 0, 0, 0,
			     "Zooms view to fit area");
		uiBlockEndAlign(block);
		xco += 8 + XIC;
	}

	uiDefBut(block, BUT, B_SEQCLEAR, "Refresh", xco,0,3*XIC,YIC, 0, 0, 0, 0, 0, "Clears all buffered images in memory");


	uiBlockSetEmboss(block, UI_EMBOSS);

	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}

