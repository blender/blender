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

static uiBlock *text_template_scriptsmenu(bContext *C, void *args_unused)
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

static uiBlock *text_plugin_scriptsmenu(bContext *C, void *args_unused)
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
	uiMenuItemO(head, 0, "TEXT_OT_markers_clear");
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

static int text_menu_edit_poll(bContext *C)
{
	return (CTX_data_edit_text(C) != NULL);
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
	uiMenuItemO(head, 0, "TEXT_OT_properties");

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

static void text_header_draw(const bContext *C, uiLayout *layout)
{
	bScreen *sc= CTX_wm_screen(C);
	SpaceText *st= (SpaceText*)CTX_wm_space_data(C);
	PointerRNA spaceptr;
	Text *text= st->text;
	
	RNA_pointer_create(&sc->id, &RNA_SpaceTextEditor, st, &spaceptr);

	uiTemplateHeaderMenus(layout);
	uiItemMenu(layout, UI_TSLOT_HEADER, "Text", 0, text_filemenu);
	if(text) {
		uiItemMenu(layout, UI_TSLOT_HEADER, "Edit", 0, text_editmenu);
		uiItemMenu(layout, UI_TSLOT_HEADER, "Format", 0, text_formatmenu);
	}

	/* warning button if text is out of date */
	if(text && text_file_modified(text)) {
		uiTemplateHeaderButtons(layout);
		uiTemplateSetColor(layout, TH_REDALERT);
		uiItemO(layout, UI_TSLOT_HEADER, "", ICON_HELP, "TEXT_OT_resolve_conflict");
	}

	uiTemplateHeaderButtons(layout);
	uiItemR(layout, UI_TSLOT_HEADER, "", ICON_LINENUMBERS_OFF, &spaceptr, "line_numbers");
	uiItemR(layout, UI_TSLOT_HEADER, "", ICON_WORDWRAP_OFF, &spaceptr, "word_wrap");
	uiItemR(layout, UI_TSLOT_HEADER, "", ICON_SYNTAX_OFF, &spaceptr, "syntax_highlight");
	// XXX uiItemR(layout, "", ICON_SCRIPTPLUGINS, &spaceptr, "do_python_plugins");

	uiTemplateHeaderID(layout, &spaceptr, "text",
		UI_ID_BROWSE|UI_ID_RENAME|UI_ID_ADD_NEW|UI_ID_OPEN|UI_ID_DELETE,
		text_idpoin_handle);

	/* file info */
	if(text) {
		char fname[HEADER_PATH_MAX];
		static char headtxt[HEADER_PATH_MAX+17];
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

		uiTemplateHeaderButtons(layout);
		uiItemLabel(layout, UI_TSLOT_HEADER, headtxt, 0);
	}
}

void text_header_register(ARegionType *art)
{
	HeaderType *ht;

	/* header */
	ht= MEM_callocN(sizeof(HeaderType), "spacetype text header");
	ht->idname= "TEXT_HT_header";
	ht->name= "Header";
	ht->draw= text_header_draw;
	BLI_addhead(&art->headertypes, ht);
}

/************************** properties ******************************/

static void text_properties_panel_draw(const bContext *C, Panel *panel)
{
	bScreen *sc= CTX_wm_screen(C);
	SpaceText *st= CTX_wm_space_text(C);
	uiLayout *layout= panel->layout;
	PointerRNA spaceptr;
	
	RNA_pointer_create(&sc->id, &RNA_SpaceTextEditor, st, &spaceptr);

	uiTemplateColumn(layout);
	uiItemR(layout, UI_TSLOT_COLUMN_1, NULL, ICON_LINENUMBERS_OFF, &spaceptr, "line_numbers");
	uiItemR(layout, UI_TSLOT_COLUMN_1, NULL, ICON_WORDWRAP_OFF, &spaceptr, "word_wrap");
	uiItemR(layout, UI_TSLOT_COLUMN_1, NULL, ICON_SYNTAX_OFF, &spaceptr, "syntax_highlight");

	uiTemplateColumn(layout);
	uiItemR(layout, UI_TSLOT_COLUMN_1, NULL, 0, &spaceptr, "font_size");
	uiItemR(layout, UI_TSLOT_COLUMN_1, NULL, 0, &spaceptr, "tab_width");
}

static void text_find_panel_draw(const bContext *C, Panel *panel)
{
	bScreen *sc= CTX_wm_screen(C);
	SpaceText *st= CTX_wm_space_text(C);
	uiLayout *layout= panel->layout;
	PointerRNA spaceptr;
	
	RNA_pointer_create(&sc->id, &RNA_SpaceTextEditor, st, &spaceptr);

	/* find */
	uiTemplateLeftRight(layout);
	uiItemR(layout, UI_TSLOT_LR_LEFT, "", 0, &spaceptr, "find_text");
	uiItemO(layout, UI_TSLOT_LR_RIGHT, "", ICON_TEXT, "TEXT_OT_find_set_selected");
	uiTemplateColumn(layout);
	uiItemO(layout, UI_TSLOT_COLUMN_1, NULL, 0, "TEXT_OT_find");

	/* replace */
	uiTemplateLeftRight(layout);
	uiItemR(layout, UI_TSLOT_LR_LEFT, "", 0, &spaceptr, "replace_text");
	uiItemO(layout, UI_TSLOT_LR_RIGHT, "", ICON_TEXT, "TEXT_OT_replace_set_selected");
	uiTemplateColumn(layout);
	uiItemO(layout, UI_TSLOT_COLUMN_1, NULL, 0, "TEXT_OT_replace");

	/* mark */
	uiTemplateColumn(layout);
	uiItemO(layout, UI_TSLOT_COLUMN_1, NULL, 0, "TEXT_OT_mark_all");

	/* settings */
	uiTemplateColumn(layout);
	uiItemR(layout, UI_TSLOT_COLUMN_1, "Wrap", 0, &spaceptr, "find_wrap");
	uiItemR(layout, UI_TSLOT_COLUMN_2, "All", 0, &spaceptr, "find_all");
}

void text_properties_register(ARegionType *art)
{
	PanelType *pt;

	/* panels: properties */
	pt= MEM_callocN(sizeof(PanelType), "spacetype text panel");
	pt->idname= "TEXT_PT_properties";
	pt->name= "Properties";
	pt->draw= text_properties_panel_draw;
	BLI_addtail(&art->paneltypes, pt);

	/* panels: find */
	pt= MEM_callocN(sizeof(PanelType), "spacetype text panel");
	pt->idname= "TEXT_PT_find";
	pt->name= "Find";
	pt->draw= text_find_panel_draw;
	BLI_addtail(&art->paneltypes, pt);
}

ARegion *text_has_properties_region(ScrArea *sa)
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
	
	arnew= MEM_callocN(sizeof(ARegion), "properties region");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype= RGN_TYPE_UI;
	arnew->alignment= RGN_ALIGN_LEFT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}

static int properties_poll(bContext *C)
{
	SpaceText *st= CTX_wm_space_text(C);
	Text *text= CTX_data_edit_text(C);

	return (st && text);
}

static int properties_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= text_has_properties_region(sa);
	
	if(ar) {
		ar->flag ^= RGN_FLAG_HIDDEN;
		ar->v2d.flag &= ~V2D_IS_INITIALISED; /* XXX should become hide/unhide api? */
		
		ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
		ED_area_tag_redraw(sa);
	}

	return OPERATOR_FINISHED;
}

void TEXT_OT_properties(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Properties";
	ot->idname= "TEXT_OT_properties";
	
	/* api callbacks */
	ot->exec= properties_exec;
	ot->poll= properties_poll;
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

