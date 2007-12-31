/**
 *
 * $Id:
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"
#include "DNA_userdef_types.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_butspace.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BSE_drawipo.h"
#include "BSE_drawview.h"
#include "BSE_editipo.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_seqaudio.h"
#include "BSE_time.h"

#include "blendef.h"
#include "butspace.h"
#include "mydevice.h"

void do_time_buttons(ScrArea *sa, unsigned short event)
{
	SpaceTime *stime= sa->spacedata.first;
	
	switch(event) {

	case B_TL_REW:
		CFRA= PSFRA;
		update_for_newframe();
		break;
	case B_TL_PLAY:
		add_screenhandler(G.curscreen, SCREEN_HANDLER_ANIM, stime->redraws);
		if(stime->redraws & TIME_WITH_SEQ_AUDIO)
			audiostream_start( CFRA );

		break;
	case B_TL_STOP:
		rem_screenhandler(G.curscreen, SCREEN_HANDLER_ANIM);
		if(stime->redraws & TIME_WITH_SEQ_AUDIO)
			audiostream_stop();
		allqueue(REDRAWALL, 0);
		
		break;
	case B_TL_FF:
		/* end frame */
		CFRA= PEFRA;
		update_for_newframe();
		break;
	case B_TL_PREVKEY:
		/* previous keyframe */
		nextprev_timeline_key(-1);
		break;
	case B_TL_NEXTKEY:
		/* next keyframe */
		nextprev_timeline_key(1);
		break;
		
	case B_TL_PREVIEWON:
		if (G.scene->r.psfra) {
			/* turn on preview range */
			G.scene->r.psfra= G.scene->r.sfra;
			G.scene->r.pefra= G.scene->r.efra;
		}
		else {
			/* turn off preview range */
			G.scene->r.psfra= 0;
			G.scene->r.pefra= 0;
		}
		BIF_undo_push("Set anim-preview range");
		allqueue(REDRAWALL, 0);
		break;
	}
}

static void do_time_redrawmenu(void *arg, int event)
{
	SpaceTime *stime= curarea->spacedata.first;
	
	if(event < 1001) {
		
		stime->redraws ^= event;
		/* update handler when it's running */
		if(has_screenhandler(G.curscreen, SCREEN_HANDLER_ANIM))
			add_screenhandler(G.curscreen, SCREEN_HANDLER_ANIM, stime->redraws);
	}
	else {
		if(event==1001) {
			button(&G.scene->r.frs_sec,1,120,"FPS:");
		}
	}
}
			

static uiBlock *time_redrawmenu(void *arg_unused)
{
	SpaceTime *stime= curarea->spacedata.first;
	uiBlock *block;
	short yco= 0, menuwidth=120, icon;
	char str[32];
	
	block= uiNewBlock(&curarea->uiblocks, "time_redrawmenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_time_redrawmenu, NULL);
	
	if(stime->redraws & TIME_LEFTMOST_3D_WIN) icon= ICON_CHECKBOX_HLT;
	else icon= ICON_CHECKBOX_DEHLT;
	uiDefIconTextBut(block, BUTM, 1, icon, "Top-Left 3D Window",	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, TIME_LEFTMOST_3D_WIN, "");

	if(stime->redraws & TIME_ALL_3D_WIN) icon= ICON_CHECKBOX_HLT;
	else icon= ICON_CHECKBOX_DEHLT;
	uiDefIconTextBut(block, BUTM, 1, icon, "All 3D Windows",	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, TIME_ALL_3D_WIN, "");
	
	if(stime->redraws & TIME_ALL_ANIM_WIN) icon= ICON_CHECKBOX_HLT;
	else icon= ICON_CHECKBOX_DEHLT;
	uiDefIconTextBut(block, BUTM, 1, icon, "Animation Windows",	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, TIME_ALL_ANIM_WIN, "");
	
	if(stime->redraws & TIME_ALL_BUTS_WIN) icon= ICON_CHECKBOX_HLT;
	else icon= ICON_CHECKBOX_DEHLT;
	uiDefIconTextBut(block, BUTM, 1, icon, "Buttons Windows",	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, TIME_ALL_BUTS_WIN, "");
	
	if(stime->redraws & TIME_ALL_IMAGE_WIN) icon= ICON_CHECKBOX_HLT;
	else icon= ICON_CHECKBOX_DEHLT;
	uiDefIconTextBut(block, BUTM, 1, icon, "Image Windows",      0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, TIME_ALL_IMAGE_WIN, "");
	
	/* Add sequencer only redraw*/
	if(stime->redraws & TIME_SEQ) icon= ICON_CHECKBOX_HLT;
	else icon= ICON_CHECKBOX_DEHLT;
	uiDefIconTextBut(block, BUTM, 1, icon, "Sequencer Windows",      0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, TIME_SEQ, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	sprintf(str, "Set Frames/Sec (%d/%f)", G.scene->r.frs_sec, G.scene->r.frs_sec_base);
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, str,	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1001, "");
	
	
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

static void do_time_viewmenu(void *arg, int event)
{
	SpaceTime *stime= curarea->spacedata.first;
	int first;
	
	switch(event) {
		case 2: /* Play Back Animation */
			add_screenhandler(G.curscreen, SCREEN_HANDLER_ANIM, stime->redraws);
			break;
		case 3: /* View All */
			first= G.scene->r.sfra;
			if(first >= G.scene->r.efra) first= G.scene->r.efra;
			G.v2d->cur.xmin=G.v2d->tot.xmin= (float)first-2;
			G.v2d->cur.xmax=G.v2d->tot.xmax= (float)G.scene->r.efra+2;
			
			test_view2d(G.v2d, curarea->winx, curarea->winy);
			scrarea_queue_winredraw(curarea);
			break;
		case 4: /* Maximize Window */
				/* using event B_FULL */
			break;
		case 5:	/* show time or frames */
			stime->flag ^= TIME_DRAWFRAMES;
			break;
		case 6:
			nextprev_marker(1);
			break;
		case 7:
			nextprev_marker(-1);
			break;
		case 8:
			nextprev_timeline_key(1);
			break;
		case 9:
			nextprev_timeline_key(-1);
			break;
		case 10:
			timeline_frame_to_center();
			break;
		case 11:
			G.v2d->flag ^= V2D_VIEWLOCK;
			if(G.v2d->flag & V2D_VIEWLOCK)
				view2d_do_locks(curarea, 0);
			break;
		case 12: /* only show keyframes from selected data */
			stime->flag ^= TIME_ONLYACTSEL;
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *time_viewmenu(void *arg_unused)
{
	SpaceTime *stime= curarea->spacedata.first;
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "time_viewmenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_time_viewmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Play Back Animation", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if(stime->flag & TIME_DRAWFRAMES)
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Seconds|T", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	else 
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Frames|T", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	uiDefIconTextBut(block, BUTM, 1, (stime->flag & TIME_ONLYACTSEL)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT, 
					 "Only Selected Data Keys|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump To Next Marker|PageUp", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump To Prev Marker|PageDown", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump To Next Key|Ctrl PageUp", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump To Prev Key|Ctrl PageDown", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Center View|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View All|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, (G.v2d->flag & V2D_VIEWLOCK)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT, 
					 "Lock Time to Other Windows|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
	
	if (!curarea->full) 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	else 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	
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

static void do_time_framemenu(void *arg, int event)
{
	switch(event) {
		case 1: /*Set as Start */
			if (G.scene->r.psfra) {
				if (G.scene->r.pefra < CFRA)
					G.scene->r.pefra= CFRA;
				G.scene->r.psfra= CFRA;
			}				
			else
				G.scene->r.sfra = CFRA;
			allqueue(REDRAWALL, 1);
			break;
		case 2: /* Set as End */
			if (G.scene->r.psfra) {
				if (CFRA < G.scene->r.psfra)
					G.scene->r.psfra= CFRA;
				G.scene->r.pefra= CFRA;
			}				
			else
				G.scene->r.efra = CFRA;
			allqueue(REDRAWALL, 1);
			break;
		case 3: /* Add Marker */
			add_marker(CFRA);
			break;
		case 4: /* Remove Marker */
			remove_marker();
			break;
		case 5: /* Rename Marker */
			rename_marker();
			break;
		case 6: /* Grab Marker */
			transform_markers('g', 0);
			break;
		case 7: /* duplicate marker */
			duplicate_marker();
			break;
	}
	allqueue(REDRAWTIME, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWSOUND, 0);
}

static uiBlock *time_framemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "time_framemenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_time_framemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Add Marker|M", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate Marker|Shift D", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete Marker|X", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
					 
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Name Marker|Ctrl M", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Move Marker|G", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
					 
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Set as Start|S", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Set as End|E", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

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

void time_buttons(ScrArea *sa)
{
	SpaceTime *stime= sa->spacedata.first;
	uiBlock *block;
	short xco, xmax;
	char name[256];
	
	sprintf(name, "header %d", sa->headwin);
	block= uiNewBlock(&sa->uiblocks, name, UI_EMBOSS, UI_HELV, sa->headwin);

	if(area_is_active_area(sa)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);
	
	sa->butspacetype= SPACE_TIME;

	xco = 8;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, 
					  windowtype_pup(), xco, 0, XIC+10, YIC, 
					  &(sa->butspacetype), 1.0, SPACEICONMAX, 0, 0, 
					  "Displays Current Window Type. "
					  "Click for menu of available types.");

	xco += XIC + 14;

	uiBlockSetEmboss(block, UI_EMBOSSN);
	if (sa->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, 
					  ICON_DISCLOSURE_TRI_RIGHT,
					  xco,2,XIC,YIC-2,
					  &(sa->flag), 0, 0, 0, 0, 
					  "Show pulldown menus");
	}
	else {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, 
					  ICON_DISCLOSURE_TRI_DOWN,
					  xco,2,XIC,YIC-2,
					  &(sa->flag), 0, 0, 0, 0, 
					  "Hide pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;

	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
	
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, time_viewmenu, NULL, 
					  "View", xco, -2, xmax-3, 24, "");
		xco+= xmax;
		xmax= GetButStringLength("Frame");
		uiDefPulldownBut(block, time_framemenu, NULL, 
					  "Frame", xco, -2, xmax-3, 24, "");
		xco+= xmax;

		xmax= GetButStringLength("Playback");
		uiDefPulldownBut(block, time_redrawmenu, NULL, 
						 "Playback", xco, -2, xmax-3, 24, "");
		xco+= xmax;
	}

	uiBlockSetEmboss(block, UI_EMBOSS);
	
	uiBlockBeginAlign(block);
	
	uiDefButI(block, TOG, B_TL_PREVIEWON,"Preview",	
		xco,0, XIC, YIC,
		&G.scene->r.psfra,0, 1, 0, 0,
		"Show settings for frame range of animation preview");
		
	xco += XIC;
	
	if (G.scene->r.psfra) {
		uiDefButI(block, NUM, REDRAWALL,"Start:",	
			xco,0, 4.5*XIC, YIC,
			&G.scene->r.psfra,MINFRAMEF, MAXFRAMEF, 0, 0,
			"The start frame of the animation preview (inclusive)");

		xco += 4.5*XIC;

		uiDefButI(block, NUM, REDRAWALL,"End:",	
			xco,0,4.5*XIC,YIC,
			&G.scene->r.pefra,PSFRA,MAXFRAMEF, 0, 0,
			"The end frame of the animation preview (inclusive)");
	}
	else {
		uiDefButI(block, NUM, REDRAWALL,"Start:",	
 		xco,0, 4.5*XIC, YIC,
 		&G.scene->r.sfra,MINFRAMEF, MAXFRAMEF, 0, 0,
 		"The start frame of the animation (inclusive)");

		xco += 4.5*XIC;

		uiDefButI(block, NUM, REDRAWALL,"End:",	
			xco,0,4.5*XIC,YIC,
			&G.scene->r.efra,SFRA,MAXFRAMEF, 0, 0,
			"The end frame of the animation (inclusive)");
	}
	uiBlockEndAlign(block);

	xco += 4.5*XIC+16;
	
	uiDefButI(block, NUM, B_NEWFRAME, "",
		xco,0,3.5*XIC,YIC,
		&(G.scene->r.cfra), MINFRAMEF, MAXFRAMEF, 0, 0,
		"Displays Current Frame of animation");
	
	xco += 3.5*XIC+16;
	
	uiDefIconBut(block, BUT, B_TL_REW, ICON_REW,
			xco, 0, XIC, YIC, 0, 0, 0, 0, 0, "Skip to Start frame (Shift DownArrow)");
	xco+= XIC+4;
	uiDefIconBut(block, BUT, B_TL_PREVKEY, ICON_PREV_KEYFRAME,
			xco, 0, XIC, YIC, 0, 0, 0, 0, 0, "Skip to previous keyframe (Ctrl PageDown)");
	xco+= XIC+4;
	
	if(has_screenhandler(G.curscreen, SCREEN_HANDLER_ANIM))
	   uiDefIconBut(block, BUT, B_TL_STOP, ICON_PAUSE,
			xco, 0, XIC, YIC, 0, 0, 0, 0, 0, "Stop Playing Timeline");
	else 	   
		uiDefIconBut(block, BUT, B_TL_PLAY, ICON_PLAY,
			xco, 0, XIC, YIC, 0, 0, 0, 0, 0, "Play Timeline ");
	
	xco+= XIC+4;
	uiDefIconBut(block, BUT, B_TL_NEXTKEY, ICON_NEXT_KEYFRAME,
			xco, 0, XIC, YIC, 0, 0, 0, 0, 0, "Skip to next keyframe (Ctrl PageUp)");
	xco+= XIC+4;
	uiDefIconBut(block, BUT, B_TL_FF, ICON_FF,
			xco, 0, XIC, YIC, 0, 0, 0, 0, 0, "Skip to End frame (Shift UpArrow)");
	xco+= XIC+8;
	
	uiDefIconButBitI(block, TOG, G_RECORDKEYS, REDRAWINFO, ICON_REC,
			xco, 0, XIC, YIC, &(G.flags), 0, 0, 0, 0, "Automatically insert keyframes in Object and Action Ipo curves");
	
	xco+= XIC+16;

	uiDefIconButBitI(block, TOG, TIME_WITH_SEQ_AUDIO, B_DIFF, ICON_SPEAKER,
					 xco, 0, XIC, YIC, &(stime->redraws), 0, 0, 0, 0, "Play back and sync with audio from Sequence Editor");
	
	/* always as last  */
	sa->headbutlen= xco+XIC+80; // +80 because the last button is not an icon

	uiDrawBlock(block);
}
