/**
 * header_text.c oct-2003
 *
 * Functions to draw the "Text Editor" window header
 * and handle user events sent to it.
 * 
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BMF_Api.h"
#include "BIF_language.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#include "BSE_headerbuttons.h"

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"

#include "BIF_drawtext.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_sca.h"
#include "BKE_text.h"
#include "BPY_extern.h"
#include "BSE_filesel.h"

#include "blendef.h"
#include "mydevice.h"

void do_text_buttons(unsigned short event)
{
	SpaceText *st= curarea->spacedata.first;
	ID *id, *idtest;
	int nr= 1;
	Text *text;
		
	if (!st) return;
	if (st->spacetype != SPACE_TEXT) return;
	
	switch (event) {
	case B_TEXTBROWSE:
		if (st->menunr==-2) {
			activate_databrowse((ID *)st->text, ID_TXT, 0, B_TEXTBROWSE,
											&st->menunr, do_text_buttons);
			break;
		}
		if(st->menunr < 0) break;
			
		text= st->text;

		nr= 1;
		id= (ID *)text;
		
		if (st->menunr==32767) {
			st->text= (Text *)add_empty_text();

			st->top= 0;
			
			allqueue(REDRAWTEXT, 0);
			allqueue(REDRAWHEADERS, 0); 
		}
		else if (st->menunr==32766) {
			activate_fileselect(FILE_SPECIAL, "LOAD TEXT FILE", G.sce, add_text_fs); 
			return;
		}
		else {		
			idtest= G.main->text.first;
			while(idtest) {
				if(nr==st->menunr) {
					break;
				}
				nr++;
				idtest= idtest->next;
			}
			if(idtest==0) { /* new text */
				activate_fileselect(FILE_SPECIAL, "LOAD TEXT FILE",
												G.sce, add_text_fs); 
				return;
			}
			if(idtest!=id) {
				st->text= (Text *)idtest;
				st->top= 0;
				
				pop_space_text(st);
			
				allqueue(REDRAWTEXT, 0);
				allqueue(REDRAWHEADERS, 0);
			}
		}
		break;
				
	case B_TEXTDELETE:
		
		text= st->text;
		if (!text) return;
		
		BPY_clear_bad_scriptlinks(text);
		free_text_controllers(text);
		
		unlink_text(text);
		free_libblock(&G.main->text, text);
		
		break;
		
/*
	case B_TEXTSTORE:
		st->text->flags ^= TXT_ISEXT;
		
		allqueue(REDRAWHEADERS, 0);
		break;
*/		 
	case B_TEXTLINENUM:
		if(st->showlinenrs)
			st->showlinenrs = 0;
		else
			st->showlinenrs = 1;

		allqueue(REDRAWTEXT, 0);
		allqueue(REDRAWHEADERS, 0);
		break;

	case B_TEXTFONT:
		switch(st->font_id) {
		case 0:
			st->lheight= 12; break;
		case 1:
			st->lheight= 15; break;
		}

		allqueue(REDRAWTEXT, 0);
		allqueue(REDRAWHEADERS, 0);

		break;
	}
}

/* action executed after clicking in File menu */
static void do_text_filemenu(void *arg, int event)
{
	SpaceText *st= curarea->spacedata.first;
	Text *text= st->text;
	ScrArea *sa;

	switch(event) {
	case 1:
		st->text= add_empty_text();
		st->top=0;

		allqueue(REDRAWTEXT, 0);
		allqueue(REDRAWHEADERS, 0);
		break;
	case 2:
		activate_fileselect(FILE_SPECIAL, "LOAD TEXT FILE", G.sce, add_text_fs);
		break;
	case 3:
		if (text->compiled) BPY_free_compiled_text(text);
		        text->compiled = NULL;
			if (okee("Reopen Text")) {
				if (!reopen_text(text)) {
					error("Could not reopen file");
				}
			}
		break;
	case 5:
		text->flags |= TXT_ISMEM;
	case 4:
		txt_write_file(text);
		break;
	case 6:
		run_python_script(st);
		break;
	default:
		break;
	}
	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		SpaceText *st= sa->spacedata.first;
		if (st && st->spacetype==SPACE_TEXT) {
			scrarea_queue_redraw(sa);
		}
	}
}

/* action executed after clicking in Edit menu */
static void do_text_editmenu(void *arg, int event)
{
	SpaceText *st= curarea->spacedata.first;
	Text *text= st->text;
	ScrArea *sa;
	
	switch(event) {
	case 1:
		txt_do_undo(text);
		break;
	case 2:
		txt_do_redo(text);
		break;
	case 3:
		txt_cut_sel(text);
		pop_space_text(st);
		break;
	case 4:
		txt_copy_sel(text);
		break;
	case 5:
		txt_paste(text);
		break;
	case 6:
		txt_print_cutbuffer();
		break;
	case 7:
		jumptoline_interactive(st);
		break;
	case 8:
		txt_find_panel(st,1);
		break;
	case 9:
		txt_find_panel(st,0);
		break;
	case 10:
		txt_export_to_object(text);
		break;
	default:
		break;
	}

	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		SpaceText *st= sa->spacedata.first;
		if (st && st->spacetype==SPACE_TEXT) {
			scrarea_queue_redraw(sa);
		}
	}
}

/* action executed after clicking in View menu */
static void do_text_editmenu_viewmenu(void *arg, int event)
{
	SpaceText *st= curarea->spacedata.first;
	Text *text= st->text;
	ScrArea *sa;
	
	switch(event) {
		case 1:
			txt_move_bof(text, 0);
			pop_space_text(st);
			break;
		case 2:
			txt_move_eof(text, 0);
			pop_space_text(st);
			break;
		default:
			break;
	}

	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		SpaceText *st= sa->spacedata.first;
		if (st && st->spacetype==SPACE_TEXT) {
			scrarea_queue_redraw(sa);
		}
	}
}

/* action executed after clicking in Select menu */
static void do_text_editmenu_selectmenu(void *arg, int event)
{
	SpaceText *st= curarea->spacedata.first;
	Text *text= st->text;
	ScrArea *sa;
	
	switch(event) {
	case 1:
		txt_sel_all(text);
		break;
			
	case 2:
		txt_sel_line(text);
		break;
	default:
		break;
	}

	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		SpaceText *st= sa->spacedata.first;
		if (st && st->spacetype==SPACE_TEXT) {
			scrarea_queue_redraw(sa);
		}
	}
}

/* View menu */
static uiBlock *text_editmenu_viewmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "text_editmenu_viewmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_text_editmenu_viewmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Top of File", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Bottom of File", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}

/* Select menu */
static uiBlock *text_editmenu_selectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "text_editmenu_selectmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_text_editmenu_selectmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select All|Ctrl A", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select Line", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	
	return block;
}

/* Edit menu */
static uiBlock *text_editmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "text_editmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_text_editmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo|Alt U", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Redo|Alt Shift U", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cut|Alt X", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy|Alt C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Paste|Alt V", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Print Cut Buffer", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBlockBut(block, text_editmenu_viewmenu, NULL, ICON_RIGHTARROW_THIN, "View|Alt Shift V   ", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, text_editmenu_selectmenu, NULL, ICON_RIGHTARROW_THIN, "Select|Alt Shift S   ", 0, yco-=20, 120, 19, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump...|Alt J", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Find...|Alt Ctrl F", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Find Again|Alt F", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Export|Alt M", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
	
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

/* File menu */
static uiBlock *text_filemenu(void *arg_unused)
{
	SpaceText *st= curarea->spacedata.first;
	Text *text= st->text;
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "text_filemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_text_filemenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "New|Alt N", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Open...|Alt O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	if(text) {
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reopen|Alt R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save|Alt S", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save as...", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Run|Alt P", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	}

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

/* header */
void text_buttons(void)
{
	uiBlock *block;
	SpaceText *st= curarea->spacedata.first;
	Text *text= st->text;
	short xco, xmax;
	char naam[256];
	
	if (!st || st->spacetype != SPACE_TEXT) return;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_TEXT;

	xco = 8;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	xco+= XIC+22;
	
	/* pull down menus */
	uiBlockSetEmboss(block, UI_EMBOSSP);

	xmax= GetButStringLength("File");
	uiDefBlockBut(block,text_filemenu, NULL, "File", xco, 0, xmax, 20, "");
	xco+=xmax;

	if(text) {
	xmax= GetButStringLength("Edit");
	uiDefBlockBut(block,text_editmenu, NULL, "Edit", xco, 0, xmax, 20, "");
	xco+=xmax;
	}

	uiBlockSetEmboss(block, UI_EMBOSSX);
	xco += 10;
	
	/* FULL WINDOW */
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
		
	if(st->showlinenrs)
		uiDefIconBut(block, BUT, B_TEXTLINENUM, ICON_SHORTDISPLAY, xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Hides line numbers");
	else
		uiDefIconBut(block, BUT, B_TEXTLINENUM, ICON_LONGDISPLAY, xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Displays line numbers");


	/* STD TEXT BUTTONS */
	if (!BPY_spacetext_is_pywin(st)) {
		xco+= 2*XIC;
		xco= std_libbuttons(block, xco, 0, 0, NULL, B_TEXTBROWSE, (ID*)st->text, 0, &(st->menunr), 0, 0, B_TEXTDELETE, 0, 0);

		/*
		if (st->text) {
			if (st->text->flags & TXT_ISDIRTY && (st->text->flags & TXT_ISEXT || !(st->text->flags & TXT_ISMEM)))
				uiDefIconBut(block, BUT,0, ICON_ERROR, xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "The text has been changed");
			if (st->text->flags & TXT_ISEXT) 
				uiDefBut(block, BUT,B_TEXTSTORE, ICON(),	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Stores text in project file");
			else 
				uiDefBut(block, BUT,B_TEXTSTORE, ICON(),	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Disables storing of text in project file");
			xco+=10;
		}
		*/		

		xco+=XIC;
		if(st->font_id>1) st->font_id= 0;
		uiDefButI(block, MENU, B_TEXTFONT, "Screen 12 %x0|Screen 15%x1", xco,0,100,YIC, &st->font_id, 0, 0, 0, 0, "Displays available fonts");
		xco+=100;
	}
	
	/* always as last  */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}
