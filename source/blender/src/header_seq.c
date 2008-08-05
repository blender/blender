/**
 * header_seq.c oct-2003
 *
 * Functions to draw the "Video Sequence Editor" window header
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
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BMF_Api.h"
#include "BIF_language.h"

#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "BLI_blenlib.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BIF_drawseq.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_editseq.h"
#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_sequence.h"
#include "BSE_time.h"
#include "blendef.h"
#include "mydevice.h"

static int viewmovetemp = 0;

static void do_seq_viewmenu(void *arg, int event)
{
	extern int play_anim(int mode);
	Sequence * last_seq = get_last_seq();
	SpaceSeq * sseq = curarea->spacedata.first;

	switch(event)
	{
	case 1: /* Play Back Animation */
		play_anim(0);
		break;
	case 2: /* Play Back Animation in All */
		play_anim(1);
		break;
	case 3:
		seq_home();
		break;
	case 4:
		if(last_seq) {
				CFRA= last_seq->startdisp;
				G.v2d->cur.xmin= last_seq->startdisp- (last_seq->len/20);
				G.v2d->cur.xmax= last_seq->enddisp+ (last_seq->len/20);
				update_for_newframe();
		}
		break;
	case 5: /* Lock time */
		G.v2d->flag ^= V2D_VIEWLOCK;
		if (G.v2d->flag & V2D_VIEWLOCK) {
			view2d_do_locks(curarea, 0);
		}
		break;
	case 6: /* Draw time/frames */
		sseq->flag ^= SEQ_DRAWFRAMES;
		break;
	case 7: /* Grease Pencil */
		add_blockhandler(curarea, SEQ_HANDLER_GREASEPENCIL, UI_PNL_UNSTOW);
		break;
	}
}

static uiBlock *seq_viewmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	SpaceSeq * sseq = curarea->spacedata.first;

	block= uiNewBlock(&curarea->uiblocks, "seq_viewmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_seq_viewmenu, NULL);
	
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
       uiDefIconTextBut(block, BUTM, 1, (G.v2d->flag & V2D_VIEWLOCK)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT,
			"Lock Time to Other Windows|", 0, yco-=20, 
			menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
       /* Draw time or frames.*/
       uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

       if(sseq->flag & SEQ_DRAWFRAMES)
               uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Seconds|T", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
       else
               uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Frames|T", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");


	if(!curarea->full) uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0,0, "");
	else uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");

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

static void do_seq_selectmenu(void *arg, int event)
{
	switch(event)
	{
	case 0:
		borderselect_seq();
		break;
	case 1:
		swap_select_seq();
		break;
	case 2:
		select_dir_from_last(1);
		break;
	case 3:
		select_dir_from_last(2);
		break;
	case 4:
		select_surround_from_last();
		break;
	case 5:
		select_neighbor_from_last(1);
		break;
	case 6:
		select_neighbor_from_last(2);
		break;
	case 7:
		select_linked_seq(2);
		break;
	case 8:
		deselect_markers(1, 0);
		allqueue(REDRAWMARKER, 0);
		break;
	}
}

static uiBlock *seq_selectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "seq_selectmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_seq_selectmenu, NULL);
	
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

static void do_seq_addmenu_effectmenu(void *arg, int event)
{
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
}

static uiBlock *seq_addmenu_effectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "seq_addmenu_effectmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
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
	uiTextBoundsBlock(block, 60);

	return block;
}


static void do_seq_addmenu(void *arg, int event)
{
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
}

static uiBlock *seq_addmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "seq_addmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_seq_addmenu, NULL);

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
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
	}

	uiTextBoundsBlock(block, 50);

	return block;
}

static void do_seq_editmenu(void *arg, int event)
{
	SpaceSeq *sseq;

	sseq= curarea->spacedata.first;

	switch(event)
	{
	case 1: /* Change Strip... */
		change_sequence();
		break;
	case 2: /* Make Meta Strip */
		make_meta();
		break;
	case 3: /* Separate Meta Strip */
		un_meta();
		break;
	case 4: /* former Properties... */
		break;
	case 5: /* Duplicate */
		add_duplicate_seq();
		break;
	case 6: /* Delete */
		del_seq();
		break;
	case 7: /* Grab/Extend */
		transform_seq('e', 0);
		break;
	case 8:
		set_filter_seq();
		break;
	case 9:
		enter_meta();
		break;
	case 10:
		exit_meta();
		break;
	case 11: /* grab/move */
		transform_seq('g', 0);
		break;
	case 12: /* Snap to Current Frame */
		seq_snap(event);
		break;
	case 13: /* Cut at Current Frame */
		seq_cut(CFRA, 1);
		break;
	case 14:
		reassign_inputs_seq_effect();
		break;
	case 15:
		seq_remap_paths();
		break;
	case 16:
		seq_separate_images();
		break;
	case 17:
		reload_sequence();
		break;
	case 18:
		seq_lock_sel(1);
		break;
	case 19:
		seq_lock_sel(0);
		break;
	case 20:
		seq_mute_sel(1);
		break;
	case 21:
		seq_mute_sel(0);
		break;
	case 22:
		seq_mute_sel(0);
		break;
	case 23:
		seq_cut(CFRA, 0);
		break;
	}
}

static uiBlock *seq_editmenu(void *arg_unused)
{
	uiBlock *block;
	Editing *ed;
	short yco= 0, menuwidth=120;
	Sequence * last_seq = get_last_seq();

	ed = G.scene->ed;

	block= uiNewBlock(&curarea->uiblocks, "seq_editmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_seq_editmenu, NULL);
	
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

	if (last_seq != NULL && last_seq->type != SEQ_MOVIE) {
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		if(last_seq->type >= SEQ_EFFECT) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Change Effect...|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reassign Inputs|R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 14, "");
		}
		else if(last_seq->type == SEQ_IMAGE) uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Change Image...|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Change Scene...|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
		
		if(last_seq->type==SEQ_IMAGE)
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Remap Paths...|Shift R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 15, "");
			
	}

	if (last_seq != NULL && last_seq->type == SEQ_MOVIE) {
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
		if (last_seq != NULL && last_seq->type == SEQ_META) {
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

static void do_seq_markermenu(void *arg, int event)
{	
	SpaceSeq *sseq= curarea->spacedata.first;
	
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
	
	allqueue(REDRAWMARKER, 0);
}

static uiBlock *seq_markermenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	SpaceSeq *sseq= curarea->spacedata.first;

	block= uiNewBlock(&curarea->uiblocks, "ipo_markermenu", 
					   UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_seq_markermenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Add Marker|Ctrl Alt M", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate Marker|Ctrl Shift D", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete Marker", 0, yco-=20,
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
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	} else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);

	return block;
}

void do_seq_buttons(short event)
{
	Editing *ed;
	
	ed= G.scene->ed;
	if(ed==0) return;
	
	switch(event) {
	case B_HOME:
		seq_home();
		break;
	case B_SEQCLEAR:
		free_imbuf_seq();
		allqueue(REDRAWSEQ, 1);
		break;
	}
}

void seq_buttons()
{
	SpaceSeq *sseq;
	short xco;
	char naam[20];
	uiBlock *block;
	short xmax;

	sseq= curarea->spacedata.first;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_SEQ;

	xco = 8;
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");
	xco+= XIC+14;

	uiBlockSetEmboss(block, UI_EMBOSSN);
	if(curarea->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_RIGHT,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Enables display of pulldown menus");
	} else {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_DOWN,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Hides pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;


	/* pull down menus */
	if((curarea->flag & HEADER_NO_PULLDOWN)==0) {
		uiBlockSetEmboss(block, UI_EMBOSSP);

		xmax= GetButStringLength("View");
		uiDefPulldownBut(block,seq_viewmenu, NULL, "View", xco, -2, xmax-3, 24, "");
		xco+=xmax;
		if (sseq->mainb == 0) {
			xmax= GetButStringLength("Select");
			uiDefPulldownBut(block,seq_selectmenu, NULL, "Select", xco, -2, xmax-3, 24, "");
			xco+=xmax;

			xmax= GetButStringLength("Marker");
			uiDefPulldownBut(block,seq_markermenu, NULL, "Marker", xco, -2, xmax-3, 24, "");
			xco+=xmax;
		
			xmax= GetButStringLength("Add");
			uiDefPulldownBut(block, seq_addmenu, NULL, "Add", xco, -2, xmax-3, 24, "");
			xco+= xmax;

			xmax= GetButStringLength("Strip");
			uiDefPulldownBut(block, seq_editmenu, NULL, "Strip", xco, -2, xmax-3, 24, "");
			xco+= xmax;
		}

		/* end of pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSS);
	}

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

		if (G.scene->ed && ((Editing*)G.scene->ed)->metastack.first)
			minchan = -BLI_countlist(&((Editing*)G.scene->ed)->metastack);

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
		xco += 8 + XIC;
	}

	uiDefBut(block, BUT, B_SEQCLEAR, "Refresh", xco,0,3*XIC,YIC, 0, 0, 0, 0, 0, "Clears all buffered images in memory");

	uiDrawBlock(block);
}
