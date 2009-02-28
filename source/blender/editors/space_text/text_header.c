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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* file time checking */
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#include "BLI_winstuff.h"
#endif

#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_text_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_text.h"

#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_text.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#ifndef DISABLE_PYTHON
#include "BPY_extern.h"
// XXX #include "BPY_menus.h"
#endif

#include "text_intern.h"

#define HEADER_PATH_MAX	260

/* ************************ header area region *********************** */

#ifndef DISABLE_PYTHON
static void do_text_template_scriptsmenu(bContext *C, void *arg, int event)
{
	// XXX BPY_menu_do_python(PYMENU_SCRIPTTEMPLATE, event);
}

uiBlock *text_template_scriptsmenu(bContext *C, void *args_unused)
{
	ARegion *ar= CTX_wm_region(C);
	uiBlock *block;
	// XXX BPyMenu *pym;
	// int i= 0;
	// short yco = 20, menuwidth = 120;
	
	block= uiBeginBlock(C, ar, "text_template_scriptsmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_text_template_scriptsmenu, NULL);
	
	/* note that we acount for the N previous entries with i+20: */
	/* XXX for (pym = BPyMenuTable[PYMENU_SCRIPTTEMPLATE]; pym; pym = pym->next, i++) {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_PYTHON, pym->name, 0, yco-=20, menuwidth, 19, 
						 NULL, 0.0, 0.0, 1, i, 
						 pym->tooltip?pym->tooltip:pym->filename);
	}*/
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	uiEndBlock(C, block);
	uiDrawBlock(C, block);
	
	return block;
}

static void do_text_plugin_scriptsmenu(bContext *C, void *arg, int event)
{
	// XXX BPY_menu_do_python(PYMENU_TEXTPLUGIN, event);
}

uiBlock *text_plugin_scriptsmenu(bContext *C, void *args_unused)
{
	ARegion *ar= CTX_wm_region(C);
	uiBlock *block;
	// XXX BPyMenu *pym;
	// int i= 0;
	// short yco = 20, menuwidth = 120;
	
	block= uiBeginBlock(C, ar, "text_plugin_scriptsmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_text_plugin_scriptsmenu, NULL);
	
	/* note that we acount for the N previous entries with i+20: */
	/* XXX for (pym = BPyMenuTable[PYMENU_TEXTPLUGIN]; pym; pym = pym->next, i++) {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_PYTHON, pym->name, 0, yco-=20, menuwidth, 19, 
						 NULL, 0.0, 0.0, 1, i, 
						 pym->tooltip?pym->tooltip:pym->filename);
	}*/
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	uiEndBlock(C, block);
	uiDrawBlock(C, block);

	return block;
}
#endif

static void text_editmenu_viewmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuItemEnumO(head, "Top of File", 0, "TEXT_OT_move", "type", FILE_TOP);
	uiMenuItemEnumO(head, "Bottom of File", 0, "TEXT_OT_move", "type", FILE_BOTTOM);
}

static void text_editmenu_selectmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuItemO(head, 0, "TEXT_OT_select_all");
	uiMenuItemO(head, 0, "TEXT_OT_select_line");
}

static void text_editmenu_markermenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuItemO(head, 0, "TEXT_OT_clear_all_markers");
	uiMenuItemO(head, 0, "TEXT_OT_next_marker");
	uiMenuItemO(head, 0, "TEXT_OT_previous_marker");
}

static void text_formatmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuItemO(head, 0, "TEXT_OT_indent");
	uiMenuItemO(head, 0, "TEXT_OT_unindent");

	uiMenuSeparator(head);

	uiMenuItemO(head, 0, "TEXT_OT_comment");
	uiMenuItemO(head, 0, "TEXT_OT_uncomment");

	uiMenuSeparator(head);

	uiMenuLevelEnumO(head, "TEXT_OT_convert_whitespace", "type");
}

static void text_editmenu_to3dmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuItemBooleanO(head, "One Object", 0, "TEXT_OT_to_3d_object", "split_lines", 0);
	uiMenuItemBooleanO(head, "One Object Per Line", 0, "TEXT_OT_to_3d_object", "split_lines", 1);
}

static void text_editmenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	uiMenuItemO(head, 0, "ED_OT_undo");
	uiMenuItemO(head, 0, "ED_OT_redo");

	uiMenuSeparator(head);

	uiMenuItemO(head, 0, "TEXT_OT_cut");
	uiMenuItemO(head, 0, "TEXT_OT_copy");
	uiMenuItemO(head, 0, "TEXT_OT_paste");

	uiMenuSeparator(head);

	uiMenuLevel(head, "View", text_editmenu_viewmenu);
	uiMenuLevel(head, "Select", text_editmenu_selectmenu);
	uiMenuLevel(head, "Markers", text_editmenu_markermenu);

	uiMenuSeparator(head);

	uiMenuItemO(head, 0, "TEXT_OT_jump");
	uiMenuItemO(head, 0, "TEXT_OT_find_and_replace");

	uiMenuSeparator(head);

	uiMenuLevel(head, "Text to 3D Object", text_editmenu_to3dmenu);
}

static void text_filemenu(bContext *C, uiMenuItem *head, void *arg_unused)
{
	SpaceText *st= (SpaceText*)CTX_wm_space_data(C);
	Text *text= st->text;

	uiMenuItemO(head, 0, "TEXT_OT_new");
	uiMenuItemO(head, 0, "TEXT_OT_open");
	
	if(text) {
		uiMenuItemO(head, 0, "TEXT_OT_reload");
		
		uiMenuSeparator(head);
		
		uiMenuItemO(head, 0, "TEXT_OT_save");
		uiMenuItemO(head, 0, "TEXT_OT_save_as");
		
		if(text->name)
			uiMenuItemO(head, 0, "TEXT_OT_make_internal");

		uiMenuSeparator(head);
		
		uiMenuItemO(head, 0, "TEXT_OT_run_script");

#ifndef DISABLE_PYTHON
		if(BPY_is_pyconstraint(text))
			uiMenuItemO(head, 0, "TEXT_OT_refresh_pyconstraints");
#endif
	}

#ifndef DISABLE_PYTHON
	// XXX uiMenuSeparator(head);

	// XXX uiDefIconTextBlockBut(block, text_template_scriptsmenu, NULL, ICON_RIGHTARROW_THIN, "Script Templates", 0, yco-=20, 120, 19, "");
	// XXX uiDefIconTextBlockBut(block, text_plugin_scriptsmenu, NULL, ICON_RIGHTARROW_THIN, "Text Plugins", 0, yco-=20, 120, 19, "");
#endif
}

/*********************** datablock browse *************************/

static void text_unlink(Main *bmain, Text *text)
{
	bScreen *scr;
	ScrArea *area;
	SpaceLink *sl;

	/* XXX this ifdef is in fact dangerous, if python is
	 * disabled it will leave invalid pointers in files! */

#ifndef DISABLE_PYTHON
	// XXX BPY_clear_bad_scriptlinks(text);
	// XXX BPY_free_pyconstraint_links(text);
	// XXX free_text_controllers(text);

	/* check if this text was used as script link:
	 * this check function unsets the pointers and returns how many
	 * script links used this Text */
	if(0) // XXX BPY_text_check_all_scriptlinks (text))
		; // XXX notifier: allqueue(REDRAWBUTSSCRIPT, 0);

	/* equivalently for pynodes: */
	if(0) // XXX nodeDynamicUnlinkText ((ID*)text))
		; // XXX notifier: allqueue(REDRAWNODE, 0);
#endif
	
	for(scr= bmain->screen.first; scr; scr= scr->id.next) {
		for(area= scr->areabase.first; area; area= area->next) {
			for(sl= area->spacedata.first; sl; sl= sl->next) {
				if(sl->spacetype==SPACE_TEXT) {
					SpaceText *st= (SpaceText*) sl;
					
					if(st->text==text) {
						st->text= NULL;
						st->top= 0;
						
						if(st==area->spacedata.first)
							ED_area_tag_redraw(area);
					}
				}
			}
		}
	}

	free_libblock(&bmain->text, text);
}

static void text_idpoin_handle(bContext *C, ID *id, int event)
{
	SpaceText *st= (SpaceText*)CTX_wm_space_data(C);
	Text *text;

	switch(event) {
		case UI_ID_BROWSE:
			st->text= (Text*)id;
			st->top= 0;

			text_update_edited(st->text);
			WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, st->text);

			ED_undo_push(C, "Browse Text");
			break;
		case UI_ID_DELETE:
			text= st->text;

			/* make the previous text active, if its not there make the next text active */
			if(text->id.prev) {
				st->text = text->id.prev;
				WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, st->text);
			}
			else if(text->id.next) {
				st->text = text->id.next;
				WM_event_add_notifier(C, NC_TEXT|ND_CURSOR, st->text);
			}

			text_unlink(CTX_data_main(C), text);
			WM_event_add_notifier(C, NC_TEXT|NA_REMOVED, text);

			ED_undo_push(C, "Delete Text");
			break;
		case UI_ID_RENAME:
			break;
		case UI_ID_ADD_NEW:
			WM_operator_name_call(C, "TEXT_OT_new", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case UI_ID_OPEN:
			WM_operator_name_call(C, "TEXT_OT_open", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
	}
}

/********************** header buttons ***********************/

void text_header_buttons(const bContext *C, ARegion *ar)
{
	bScreen *sc= CTX_wm_screen(C);
	SpaceText *st= (SpaceText*)CTX_wm_space_data(C);
	PointerRNA spaceptr;
	Text *text= st->text;
	ScrArea *sa= CTX_wm_area(C);
	uiBlock *block;
	int xco, yco= 3, xmax, oldcol;
	
	RNA_pointer_create(&sc->id, &RNA_SpaceTextEditor, st, &spaceptr);

	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS, UI_HELV);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
		
		xmax= GetButStringLength("Text");
		uiDefMenuBut(block, text_filemenu, NULL, "Text", xco, yco-2, xmax-3, 24, "");
		xco+=xmax;
	
		if(text) {
			xmax= GetButStringLength("Edit");
			uiDefMenuBut(block, text_editmenu, NULL, "Edit", xco, yco-2, xmax-3, 24, "");
			xco+=xmax;
			
			xmax= GetButStringLength("Format");
			uiDefMenuBut(block, text_formatmenu, NULL, "Format", xco, yco-2, xmax-3, 24, "");
			xco+=xmax;
		}
	}
	
	uiBlockSetEmboss(block, UI_EMBOSS);

	uiBlockBeginAlign(block);
	uiDefIconButR(block, ICONTOG, 0, ICON_LINENUMBERS_OFF, xco, yco,XIC,YIC, &spaceptr, "line_numbers", 0, 0, 0, 0, 0, NULL);
	uiDefIconButR(block, ICONTOG, 0, ICON_WORDWRAP_OFF, xco+=XIC, yco,XIC,YIC, &spaceptr, "word_wrap", 0, 0, 0, 0, 0, NULL);
	uiDefIconButR(block, ICONTOG, 0, ICON_SYNTAX_OFF, xco+=XIC, yco,XIC,YIC, &spaceptr, "syntax_highlight", 0, 0, 0, 0, 0, NULL);
	// uiDefIconButR(block, ICONTOG, 0, ICON_SCRIPTPLUGINS, xco+=XIC, yco,XIC,YIC, &spaceptr, "do_python_plugins", 0, 0, 0, 0, 0, "Enables Python text plugins");
	uiBlockEndAlign(block);
	
	/* warning button if text is out of date */
	if(text && text_file_modified(text)) {
		xco+= XIC;

		oldcol= uiBlockGetCol(block);
		uiBlockSetCol(block, TH_REDALERT);
		uiDefIconButO(block, BUT, "TEXT_OT_resolve_conflict", WM_OP_INVOKE_DEFAULT, ICON_HELP,	xco+=XIC,yco,XIC,YIC, "External text is out of sync, click for options to resolve the conflict");
		uiBlockSetCol(block, oldcol);
	}
	
	/* browse text datablock */
	xco+= 2*XIC;
	xco= uiDefIDPoinButs(block, CTX_data_main(C), NULL, (ID*)st->text, ID_TXT, NULL, xco, yco,
		text_idpoin_handle, UI_ID_BROWSE|UI_ID_RENAME|UI_ID_ADD_NEW|UI_ID_OPEN|UI_ID_DELETE);
	xco+=XIC;
	
	/*
	if(st->text) {
		if(st->text->flags & TXT_ISDIRTY && (st->text->flags & TXT_ISEXT || !(st->text->flags & TXT_ISMEM)))
			uiDefIconBut(block, BUT,0, ICON_ERROR, xco+=XIC,yco,XIC,YIC, 0, 0, 0, 0, 0, "The text has been changed");
		if(st->text->flags & TXT_ISEXT) 
			uiDefBut(block, BUT,B_TEXTSTORE, ICON(),	xco+=XIC,yco,XIC,YIC, 0, 0, 0, 0, 0, "Stores text in project file");
		else 
			uiDefBut(block, BUT,B_TEXTSTORE, ICON(),	xco+=XIC,yco,XIC,YIC, 0, 0, 0, 0, 0, "Disables storing of text in project file");
		xco+=10;
	}
	*/		
	
	/* display settings */
	if(st->font_id>1) st->font_id= 0;
	uiDefButR(block, MENU, 0, NULL, xco,yco,100,YIC, &spaceptr, "font_size", 0, 0, 0, 0, 0, NULL);
	xco+=105;
	
	uiDefButR(block, NUM, 0, "Tab:", xco,yco,XIC+50,YIC, &spaceptr, "tab_width", 0, 0, 0, 0, 0, NULL);
	xco+= XIC+50;

	/* file info */
	if(text) {
		char fname[HEADER_PATH_MAX], headtxt[HEADER_PATH_MAX+17];
		int len;

		if(text->name) {
			len = strlen(text->name);
			if(len > HEADER_PATH_MAX-1)
				len = HEADER_PATH_MAX-1;
			strncpy(fname, text->name, len);
			fname[len]='\0';
			if(text->flags & TXT_ISDIRTY)
				sprintf(headtxt, "File: *%s (unsaved)", fname);
			else
				sprintf(headtxt, "File: %s", fname);
		}
		else
			sprintf(headtxt, text->id.lib? "Text: External": "Text: Internal");

		UI_ThemeColor(TH_MENU_TEXT);
		UI_RasterPos(xco+=XIC, yco+6);

		UI_DrawString(G.font, headtxt, 0);
		xco += UI_GetStringWidth(G.font, headtxt, 0);
	}

	uiEndBlock(C, block);
	uiDrawBlock(C, block);

	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);
}

/************************* find & replace ***************************/

void text_find_buttons(const bContext *C, ARegion *ar)
{
	bScreen *sc= CTX_wm_screen(C);
	SpaceText *st= CTX_wm_space_text(C);
	PointerRNA spaceptr;
	uiBlock *block;
	int xco= 5, yco= 3;
	
	RNA_pointer_create(&sc->id, &RNA_SpaceTextEditor, st, &spaceptr);

	block= uiBeginBlock(C, ar, "find buttons", UI_EMBOSS, UI_HELV);

	/* find */
	uiBlockBeginAlign(block);
	uiDefButR(block, TEX, 0, "Find: ", xco, yco,220,20, &spaceptr, "find_text", 0, 0, 0, 0, 0, NULL);
	xco += 220;
	uiDefIconButO(block, BUT, "TEXT_OT_find_set_selected", WM_OP_INVOKE_DEFAULT, ICON_TEXT, xco,yco,20,20, "Copy from selection");
	xco += 20+XIC;
	uiBlockEndAlign(block);

	/* replace */
	uiBlockBeginAlign(block);
	uiDefButR(block, TEX, 0, "Replace: ", xco, yco,220,20, &spaceptr, "replace_text", 0, 0, 0, 0, 0, NULL);
	xco += 220;
	uiDefIconButO(block, BUT, "TEXT_OT_replace_set_selected", WM_OP_INVOKE_DEFAULT, ICON_TEXT, xco,yco,20,20, "Copy from selection");
	xco += 20+XIC;
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButR(block, TOG, 0, "Wrap", xco, yco,60,20, &spaceptr, "find_wrap", 0, 0, 0, 0, 0, NULL);
	xco += 60;
	uiDefButR(block, TOG, 0, "All", xco, yco,60,20, &spaceptr, "find_all", 0, 0, 0, 0, 0, NULL);
	xco += 50+XIC;
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButO(block, BUT, "TEXT_OT_find", WM_OP_INVOKE_REGION_WIN, "Find", xco,yco,50,20, "Find next.");
	xco += 50;
	uiDefButO(block, BUT, "TEXT_OT_replace", WM_OP_INVOKE_REGION_WIN, "Replace", xco,yco,70,20, "Replace then find next.");
	xco += 70;
	uiDefButO(block, BUT, "TEXT_OT_mark_all", WM_OP_INVOKE_REGION_WIN, "Mark All", xco,yco,80,20, "Mark each occurrence to edit all from one");
	xco += 80;
	uiBlockEndAlign(block);
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);

	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);
}

ARegion *text_has_find_region(ScrArea *sa)
{
	ARegion *ar, *arnew;
	
	for(ar= sa->regionbase.first; ar; ar= ar->next)
		if(ar->regiontype==RGN_TYPE_UI)
			return ar;
	
	/* add subdiv level; after header */
	for(ar= sa->regionbase.first; ar; ar= ar->next)
		if(ar->regiontype==RGN_TYPE_HEADER)
			break;
	
	/* is error! */
	if(ar==NULL) return NULL;
	
	arnew= MEM_callocN(sizeof(ARegion), "find and replace region");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype= RGN_TYPE_UI;
	arnew->alignment= RGN_ALIGN_BOTTOM;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}

static int find_and_replace_poll(bContext *C)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *text= CTX_data_edit_text(C);

	return (st && text);
}

static int find_and_replace_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= text_has_find_region(sa);
	
	if(ar) {
		ar->flag ^= RGN_FLAG_HIDDEN;
		ar->v2d.flag &= ~V2D_IS_INITIALISED; /* XXX should become hide/unhide api? */
		
		ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
		ED_area_tag_redraw(sa);
	}

	return OPERATOR_FINISHED;
}

void TEXT_OT_find_and_replace(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Find and Replace";
	ot->idname= "TEXT_OT_find_and_replace";
	
	/* api callbacks */
	ot->exec= find_and_replace_exec;
	ot->poll= find_and_replace_poll;
}

/******************** XXX popup menus *******************/

#if 0
{
	// RMB

	uiMenuItem *head;

	if(text) {
		head= uiPupMenuBegin("Text", 0);
		if(txt_has_sel(text)) {
			uiMenuItemO(head, 0, "TEXT_OT_cut");
			uiMenuItemO(head, 0, "TEXT_OT_copy");
		}
		uiMenuItemO(head, 0, "TEXT_OT_paste");
		uiMenuItemO(head, 0, "TEXT_OT_new");
		uiMenuItemO(head, 0, "TEXT_OT_open");
		uiMenuItemO(head, 0, "TEXT_OT_save");
		uiMenuItemO(head, 0, "TEXT_OT_save_as");
		uiMenuItemO(head, 0, "TEXT_OT_run_script");
		uiPupMenuEnd(C, head);
	}
	else {
		head= uiPupMenuBegin("File", 0);
		uiMenuItemO(head, 0, "TEXT_OT_new");
		uiMenuItemO(head, 0, "TEXT_OT_open");
		uiPupMenuEnd(C, head);
	}
}

{
	// Alt+Shift+E

	uiMenuItem *head;

	head= uiPupMenuBegin("Edit", 0);
	uiMenuItemO(head, 0, "TEXT_OT_cut");
	uiMenuItemO(head, 0, "TEXT_OT_copy");
	uiMenuItemO(head, 0, "TEXT_OT_paste");
	uiPupMenuEnd(C, head);
}

{
	// Alt+Shift+F

	uiMenuItem *head;

	if(text) {
		head= uiPupMenuBegin("Text", 0);
		uiMenuItemO(head, 0, "TEXT_OT_new");
		uiMenuItemO(head, 0, "TEXT_OT_open");
		uiMenuItemO(head, 0, "TEXT_OT_save");
		uiMenuItemO(head, 0, "TEXT_OT_save_as");
		uiMenuItemO(head, 0, "TEXT_OT_run_script");
		uiPupMenuEnd(C, head);
	}
	else {
		head= uiPupMenuBegin("File", 0);
		uiMenuItemO(head, 0, "TEXT_OT_new");
		uiMenuItemO(head, 0, "TEXT_OT_open");
		uiPupMenuEnd(C, head);
	}
}

{
	// Alt+Shift+V

	uiMenuItem *head;

	head= uiPupMenuBegin("Text", 0);
	uiMenuItemEnumO(head, "Top of File", 0, "TEXT_OT_move", "type", FILE_TOP);
	uiMenuItemEnumO(head, "Bottom of File", 0, "TEXT_OT_move", "type", FILE_BOTTOM);
	uiMenuItemEnumO(head, "Page Up", 0, "TEXT_OT_move", "type", PREV_PAGE);
	uiMenuItemEnumO(head,  "Page Down", 0, "TEXT_OT_move", "type", NEXT_PAGE);
	uiPupMenuEnd(C, head);
}
#endif

