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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "DNA_text_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"


#include "text_intern.h"	// own include

/* ******************** default callbacks for text space ***************** */

static SpaceLink *text_new(const bContext *C)
{
	ARegion *ar;
	SpaceText *stext;
	
	stext= MEM_callocN(sizeof(SpaceText), "inittext");
	stext->spacetype= SPACE_TEXT;

	stext->lheight= 12;
	stext->tabnumber= 4;
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for text");
	
	BLI_addtail(&stext->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for text");
	
	BLI_addtail(&stext->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	return (SpaceLink *)stext;
}

/* not spacelink itself */
static void text_free(SpaceLink *sl)
{	
	SpaceText *stext= (SpaceText*) sl;
	
	stext->text= NULL;
}


/* spacetype; init callback */
static void text_init(struct wmWindowManager *wm, ScrArea *sa)
{

}

static SpaceLink *text_duplicate(SpaceLink *sl)
{
	SpaceText *stextn= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	
	return (SpaceLink *)stextn;
}

static void text_listener(ScrArea *sa, wmNotifier *wmn)
{
	SpaceText *st= sa->spacedata.first;

	/* context changes */
	switch(wmn->category) {
		case NC_TEXT:
			if(!wmn->reference || wmn->reference == st->text) {
				ED_area_tag_redraw(sa);

				if(wmn->action == NA_EDITED)
					if(st->text)
						text_update_edited(st->text);
			}
			else if(wmn->data == ND_DISPLAY)
				ED_area_tag_redraw(sa);

			break;
		case NC_SPACE:
			if(wmn->data == ND_SPACE_TEXT)
				ED_area_tag_redraw(sa);
			break;
	}
}

static void text_operatortypes(void)
{
	WM_operatortype_append(TEXT_OT_new);
	WM_operatortype_append(TEXT_OT_open);
	WM_operatortype_append(TEXT_OT_reload);
	WM_operatortype_append(TEXT_OT_unlink);
	WM_operatortype_append(TEXT_OT_save);
	WM_operatortype_append(TEXT_OT_save_as);
	WM_operatortype_append(TEXT_OT_make_internal);
	WM_operatortype_append(TEXT_OT_run_script);
	WM_operatortype_append(TEXT_OT_refresh_pyconstraints);

	WM_operatortype_append(TEXT_OT_paste);
	WM_operatortype_append(TEXT_OT_copy);
	WM_operatortype_append(TEXT_OT_cut);

	WM_operatortype_append(TEXT_OT_convert_whitespace);
	WM_operatortype_append(TEXT_OT_uncomment);
	WM_operatortype_append(TEXT_OT_comment);
	WM_operatortype_append(TEXT_OT_unindent);
	WM_operatortype_append(TEXT_OT_indent);

	WM_operatortype_append(TEXT_OT_markers_clear);
	WM_operatortype_append(TEXT_OT_next_marker);
	WM_operatortype_append(TEXT_OT_previous_marker);

	WM_operatortype_append(TEXT_OT_select_line);
	WM_operatortype_append(TEXT_OT_select_all);

	WM_operatortype_append(TEXT_OT_jump);
	WM_operatortype_append(TEXT_OT_move);
	WM_operatortype_append(TEXT_OT_move_select);
	WM_operatortype_append(TEXT_OT_delete);
	WM_operatortype_append(TEXT_OT_overwrite_toggle);

	WM_operatortype_append(TEXT_OT_cursor_set);
	WM_operatortype_append(TEXT_OT_scroll);
	WM_operatortype_append(TEXT_OT_scroll_bar);
	WM_operatortype_append(TEXT_OT_line_number);

	WM_operatortype_append(TEXT_OT_line_break);
	WM_operatortype_append(TEXT_OT_insert);

	WM_operatortype_append(TEXT_OT_properties);

	WM_operatortype_append(TEXT_OT_find);
	WM_operatortype_append(TEXT_OT_find_set_selected);
	WM_operatortype_append(TEXT_OT_replace);
	WM_operatortype_append(TEXT_OT_replace_set_selected);
	WM_operatortype_append(TEXT_OT_mark_all);

	WM_operatortype_append(TEXT_OT_to_3d_object);

	WM_operatortype_append(TEXT_OT_resolve_conflict);
}

static void text_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	keymap= WM_keymap_find(keyconf, "Text", SPACE_TEXT, 0);
	
	#ifdef __APPLE__
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", LEFTARROWKEY, KM_PRESS, KM_OSKEY, 0)->ptr, "type", LINE_BEGIN);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", RIGHTARROWKEY, KM_PRESS, KM_OSKEY, 0)->ptr, "type", LINE_BEGIN);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move_select", LEFTARROWKEY, KM_PRESS, KM_SHIFT|KM_OSKEY, 0)->ptr, "type", LINE_BEGIN);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move_select", RIGHTARROWKEY, KM_PRESS, KM_SHIFT|KM_OSKEY, 0)->ptr, "type", LINE_BEGIN);
	WM_keymap_add_item(keymap, "TEXT_OT_save", SKEY, KM_PRESS, KM_ALT|KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_save_as", SKEY, KM_PRESS, KM_ALT|KM_SHIFT|KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_cut", XKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_copy", CKEY, KM_PRESS, KM_OSKEY, 0); 
	WM_keymap_add_item(keymap, "TEXT_OT_paste", VKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_properties", FKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_find_set_selected", EKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_find", GKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_select_all", AKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_select_line", AKEY, KM_PRESS, KM_SHIFT|KM_OSKEY, 0);
	#endif
	
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_cycle_int", WHEELUPMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_string_set(kmi->ptr, "path", "space_data.font_size");
	RNA_boolean_set(kmi->ptr, "reverse", 0);
	
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_cycle_int", WHEELDOWNMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_string_set(kmi->ptr, "path", "space_data.font_size");
	RNA_boolean_set(kmi->ptr, "reverse", 1);

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_cycle_int", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	RNA_string_set(kmi->ptr, "path", "space_data.font_size");
	RNA_boolean_set(kmi->ptr, "reverse", 0);
	
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_cycle_int", PADMINUS, KM_PRESS, KM_CTRL, 0);
	RNA_string_set(kmi->ptr, "path", "space_data.font_size");
	RNA_boolean_set(kmi->ptr, "reverse", 1);
	
	WM_keymap_add_item(keymap, "TEXT_OT_new", NKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_open", OKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_reload", RKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_save", SKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_save_as", SKEY, KM_PRESS, KM_ALT|KM_SHIFT|KM_CTRL, 0);

	WM_keymap_add_item(keymap, "TEXT_OT_run_script", PKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "TEXT_OT_cut", XKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_paste", VKEY, KM_PRESS, KM_CTRL, 0);

	if(U.uiflag & USER_MMB_PASTE) // XXX not dynamic
		RNA_boolean_set(WM_keymap_add_item(keymap, "TEXT_OT_paste", MIDDLEMOUSE, KM_PRESS, 0, 0)->ptr, "selection", 1);

	WM_keymap_add_item(keymap, "TEXT_OT_jump", JKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_find", GKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "TEXT_OT_properties", FKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_replace", HKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "TEXT_OT_to_3d_object", MKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "TEXT_OT_select_all", AKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_select_line", AKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	

	WM_keymap_add_item(keymap, "TEXT_OT_indent", TABKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_unindent", TABKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_uncomment", DKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);

	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", HOMEKEY, KM_PRESS, 0, 0)->ptr, "type", LINE_BEGIN);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", ENDKEY, KM_PRESS, 0, 0)->ptr, "type", LINE_END);
	
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", EKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", LINE_END);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", EKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0)->ptr, "type", LINE_END);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", LEFTARROWKEY, KM_PRESS, 0, 0)->ptr, "type", PREV_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", RIGHTARROWKEY, KM_PRESS, 0, 0)->ptr, "type", NEXT_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", LEFTARROWKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", PREV_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", RIGHTARROWKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", NEXT_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", UPARROWKEY, KM_PRESS, 0, 0)->ptr, "type", PREV_LINE);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", DOWNARROWKEY, KM_PRESS, 0, 0)->ptr, "type", NEXT_LINE);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", PAGEUPKEY, KM_PRESS, 0, 0)->ptr, "type", PREV_PAGE);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move", PAGEDOWNKEY, KM_PRESS, 0, 0)->ptr, "type", NEXT_PAGE);

	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move_select", HOMEKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", LINE_BEGIN);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move_select", ENDKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", LINE_END);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move_select", LEFTARROWKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", PREV_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move_select", RIGHTARROWKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", NEXT_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move_select", LEFTARROWKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0)->ptr, "type", PREV_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move_select", RIGHTARROWKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0)->ptr, "type", NEXT_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move_select", UPARROWKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", PREV_LINE);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move_select", DOWNARROWKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", NEXT_LINE);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move_select", PAGEUPKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", PREV_PAGE);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_move_select", PAGEDOWNKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "type", NEXT_PAGE);

	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_delete", DELKEY, KM_PRESS, 0, 0)->ptr, "type", DEL_NEXT_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_delete", DKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", DEL_NEXT_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_delete", BACKSPACEKEY, KM_PRESS, 0, 0)->ptr, "type", DEL_PREV_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_delete", DELKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", DEL_NEXT_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "TEXT_OT_delete", BACKSPACEKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", DEL_PREV_WORD);

	WM_keymap_add_item(keymap, "TEXT_OT_overwrite_toggle", INSERTKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "TEXT_OT_scroll", MIDDLEMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_scroll", MOUSEPAN, 0, 0, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_scroll_bar", LEFTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_cursor_set", LEFTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "TEXT_OT_cursor_set", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "select", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "TEXT_OT_scroll", WHEELUPMOUSE, KM_PRESS, 0, 0)->ptr, "lines", -1);
	RNA_int_set(WM_keymap_add_item(keymap, "TEXT_OT_scroll", WHEELDOWNMOUSE, KM_PRESS, 0, 0)->ptr, "lines", 1);

	WM_keymap_add_item(keymap, "TEXT_OT_line_break", RETKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_line_break", PADENTER, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "TEXT_OT_line_number", KM_TEXTINPUT, KM_ANY, KM_ANY, 0);
	WM_keymap_add_item(keymap, "TEXT_OT_insert", KM_TEXTINPUT, KM_ANY, KM_ANY, 0); // last!
}

static int text_context(const bContext *C, const char *member, bContextDataResult *result)
{
	SpaceText *st= CTX_wm_space_text(C);

	if(CTX_data_dir(member)) {
		static const char *dir[] = {"edit_text", NULL};
		CTX_data_dir_set(result, dir);
		return 1;
	}
	else if(CTX_data_equals(member, "edit_text")) {
		CTX_data_id_pointer_set(result, &st->text->id);
		return 1;
	}

	return 0;
}

/********************* main region ********************/

/* add handlers, stuff you only do once or on area/region changes */
static void text_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	ListBase *lb;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_STANDARD, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_find(wm->defaultconf, "Text", SPACE_TEXT, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
	
	/* add drop boxes */
	lb = WM_dropboxmap_find("Text", SPACE_TEXT, RGN_TYPE_WINDOW);
	
	WM_event_add_dropbox_handler(&ar->handlers, lb);
}

static void text_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceText *st= CTX_wm_space_text(C);
	//View2D *v2d= &ar->v2d;
	
	/* clear and setup matrix */
	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);
	
	// UI_view2d_view_ortho(C, v2d);
		
	/* data... */
	draw_text_main(st, ar);
	
	/* reset view matrix */
	// UI_view2d_view_restore(C);
	
	/* scrollers? */
}

static void text_cursor(wmWindow *win, ScrArea *sa, ARegion *ar)
{
	WM_cursor_set(win, BC_TEXTEDITCURSOR);
}



/* ************* dropboxes ************* */

static int text_drop_poll(bContext *C, wmDrag *drag, wmEvent *event)
{
	if(drag->type==WM_DRAG_PATH)
		if(ELEM(drag->icon, 0, ICON_FILE_BLANK))	/* rule might not work? */
			return 1;
	return 0;
}

static void text_drop_copy(wmDrag *drag, wmDropBox *drop)
{
	/* copy drag path to properties */
	RNA_string_set(drop->ptr, "path", drag->path);
}

/* this region dropbox definition */
static void text_dropboxes(void)
{
	ListBase *lb= WM_dropboxmap_find("Text", SPACE_TEXT, RGN_TYPE_WINDOW);
	
	WM_dropbox_add(lb, "TEXT_OT_open", text_drop_poll, text_drop_copy);

}

/* ************* end drop *********** */


/****************** header region ******************/

/* add handlers, stuff you only do once or on area/region changes */
static void text_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	ED_region_header_init(ar);
}

static void text_header_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

/****************** properties region ******************/

/* add handlers, stuff you only do once or on area/region changes */
static void text_properties_area_init(wmWindowManager *wm, ARegion *ar)
{
	ED_region_panels_init(wm, ar);
}

static void text_properties_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, NULL, -1);
}

/********************* registration ********************/

/* only called once, from space/spacetypes.c */
void ED_spacetype_text(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype text");
	ARegionType *art;
	
	st->spaceid= SPACE_TEXT;
	strncpy(st->name, "Text", BKE_ST_MAXNAME);
	
	st->new= text_new;
	st->free= text_free;
	st->init= text_init;
	st->duplicate= text_duplicate;
	st->operatortypes= text_operatortypes;
	st->keymap= text_keymap;
	st->listener= text_listener;
	st->context= text_context;
	st->dropboxes = text_dropboxes;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype text region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= text_main_area_init;
	art->draw= text_main_area_draw;
	art->cursor= text_cursor;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: properties */
	art= MEM_callocN(sizeof(ARegionType), "spacetype text region");
	art->regionid = RGN_TYPE_UI;
	art->prefsizex= UI_COMPACT_PANEL_WIDTH;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
	art->init= text_properties_area_init;
	art->draw= text_properties_area_draw;
	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype text region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_HEADER;
	
	art->init= text_header_area_init;
	art->draw= text_header_area_draw;

	BLI_addhead(&st->regiontypes, art);

	BKE_spacetype_register(st);
}

