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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
 #include <string.h>
#include <stdio.h>


#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_screen.h"

#include "BIF_gl.h"


#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "console_intern.h"	// own include

static void console_update_rect(const bContext *C, ARegion *ar)
{
	SpaceConsole *sc= CTX_wm_space_console(C);
	View2D *v2d= &ar->v2d;

	UI_view2d_totRect_set(v2d, ar->winx-1, console_text_height(sc, ar, CTX_wm_reports(C)));
}

/* ******************** default callbacks for console space ***************** */

static SpaceLink *console_new(const bContext *C)
{
	ARegion *ar;
	SpaceConsole *sconsole;
	
	sconsole= MEM_callocN(sizeof(SpaceConsole), "initconsole");
	sconsole->spacetype= SPACE_CONSOLE;
	
	sconsole->lheight=	14;
	sconsole->type=		CONSOLE_TYPE_PYTHON;
	sconsole->rpt_mask=	CONSOLE_RPT_OP; /* ? - not sure whats a good default here?*/
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for console");
	
	BLI_addtail(&sconsole->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for text");
	
	BLI_addtail(&sconsole->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	
	ar->v2d.scroll |= (V2D_SCROLL_RIGHT);
	ar->v2d.align |= V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_NEG_Y; /* align bottom left */
	ar->v2d.keepofs |= V2D_LOCKOFS_X;
	ar->v2d.keepzoom = (V2D_LOCKZOOM_X|V2D_LOCKZOOM_Y|V2D_LIMITZOOM|V2D_KEEPASPECT);
	ar->v2d.keeptot= V2D_KEEPTOT_BOUNDS;
	ar->v2d.minzoom= ar->v2d.maxzoom= 1.0f;

	/* for now, aspect ratio should be maintained, and zoom is clamped within sane default limits */
	//ar->v2d.keepzoom= (V2D_KEEPASPECT|V2D_LIMITZOOM);

	return (SpaceLink *)sconsole;
}

/* not spacelink itself */
static void console_free(SpaceLink *sl)
{
	SpaceConsole *sc= (SpaceConsole*) sl;
	
	while(sc->scrollback.first)
		console_scrollback_free(sc, sc->scrollback.first);
	
	while(sc->history.first)
		console_history_free(sc, sc->history.first);
}


/* spacetype; init callback */
static void console_init(struct wmWindowManager *wm, ScrArea *sa)
{

}

static SpaceLink *console_duplicate(SpaceLink *sl)
{
	SpaceConsole *sconsolen= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	
	/* TODO - duplicate?, then we also need to duplicate the py namespace */
	sconsolen->scrollback.first= sconsolen->scrollback.last= NULL;
	sconsolen->history.first= sconsolen->history.last= NULL;
	
	return (SpaceLink *)sconsolen;
}



/* add handlers, stuff you only do once or on area/region changes */
static void console_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);

	/* own keymap */
	keymap= WM_keymap_find(wm->defaultconf, "Console", SPACE_CONSOLE, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void console_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceConsole *sc= CTX_wm_space_console(C);
	View2D *v2d= &ar->v2d;
	View2DScrollers *scrollers;
	float col[3];
	
	if((sc->type==CONSOLE_TYPE_PYTHON) && (sc->scrollback.first==NULL))
		WM_operator_name_call((bContext *)C, "CONSOLE_OT_banner", WM_OP_EXEC_DEFAULT, NULL);

	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	console_update_rect(C, ar);

	/* worlks best with no view2d matrix set */
	UI_view2d_view_ortho(C, v2d);

	/* data... */

	console_history_verify(C); /* make sure we have some command line */
	console_text_main(sc, ar, CTX_wm_reports(C));
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers= UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_GRID_CLAMP);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

void console_operatortypes(void)
{
	/* console_ops.c */
	WM_operatortype_append(CONSOLE_OT_move);
	WM_operatortype_append(CONSOLE_OT_delete);
	WM_operatortype_append(CONSOLE_OT_insert);
	
	/* for use by python only */
	WM_operatortype_append(CONSOLE_OT_history_append); 
	WM_operatortype_append(CONSOLE_OT_scrollback_append);
	
	WM_operatortype_append(CONSOLE_OT_clear); 
	WM_operatortype_append(CONSOLE_OT_history_cycle);
	WM_operatortype_append(CONSOLE_OT_copy);
	WM_operatortype_append(CONSOLE_OT_paste);
	WM_operatortype_append(CONSOLE_OT_select_set);

	/* console_report.c */
	WM_operatortype_append(CONSOLE_OT_select_pick);
	WM_operatortype_append(CONSOLE_OT_select_all_toggle);
	WM_operatortype_append(CONSOLE_OT_select_border);

	WM_operatortype_append(CONSOLE_OT_report_replay);
	WM_operatortype_append(CONSOLE_OT_report_delete);
	WM_operatortype_append(CONSOLE_OT_report_copy);
}

void console_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap= WM_keymap_find(keyconf, "Console", SPACE_CONSOLE, 0);
	wmKeyMapItem *kmi;
	
#ifdef __APPLE__
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", LEFTARROWKEY, KM_PRESS, KM_OSKEY, 0)->ptr, "type", LINE_BEGIN);
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", RIGHTARROWKEY, KM_PRESS, KM_OSKEY, 0)->ptr, "type", LINE_END);
#endif

	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", LEFTARROWKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", PREV_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", RIGHTARROWKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", NEXT_WORD);
	
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", HOMEKEY, KM_PRESS, 0, 0)->ptr, "type", LINE_BEGIN);
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", ENDKEY, KM_PRESS, 0, 0)->ptr, "type", LINE_END);
	
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

	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", LEFTARROWKEY, KM_PRESS, 0, 0)->ptr, "type", PREV_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", RIGHTARROWKEY, KM_PRESS, 0, 0)->ptr, "type", NEXT_CHAR);
	
	RNA_boolean_set(WM_keymap_add_item(keymap, "CONSOLE_OT_history_cycle", UPARROWKEY, KM_PRESS, 0, 0)->ptr, "reverse", 1);
	WM_keymap_add_item(keymap, "CONSOLE_OT_history_cycle", DOWNARROWKEY, KM_PRESS, 0, 0);
	
	/*
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", LEFTARROWKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", PREV_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", RIGHTARROWKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", NEXT_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", UPARROWKEY, KM_PRESS, 0, 0)->ptr, "type", PREV_LINE);
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", DOWNARROWKEY, KM_PRESS, 0, 0)->ptr, "type", NEXT_LINE);
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", PAGEUPKEY, KM_PRESS, 0, 0)->ptr, "type", PREV_PAGE);
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_move", PAGEDOWNKEY, KM_PRESS, 0, 0)->ptr, "type", NEXT_PAGE);


	//RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_delete", DELKEY, KM_PRESS, 0, 0)->ptr, "type", DEL_NEXT_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_delete", DKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", DEL_NEXT_CHAR);
	//RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_delete", BACKSPACEKEY, KM_PRESS, 0, 0)->ptr, "type", DEL_PREV_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_delete", DELKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", DEL_NEXT_WORD);
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_delete", BACKSPACEKEY, KM_PRESS, KM_CTRL, 0)->ptr, "type", DEL_PREV_WORD);
	*/
	
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_delete", DELKEY, KM_PRESS, 0, 0)->ptr, "type", DEL_NEXT_CHAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "CONSOLE_OT_delete", BACKSPACEKEY, KM_PRESS, 0, 0)->ptr, "type", DEL_PREV_CHAR);

#ifndef DISABLE_PYTHON
	WM_keymap_add_item(keymap, "CONSOLE_OT_execute", RETKEY, KM_PRESS, 0, 0); /* python operator - space_text.py */
	WM_keymap_add_item(keymap, "CONSOLE_OT_execute", PADENTER, KM_PRESS, 0, 0);
	
	//WM_keymap_add_item(keymap, "CONSOLE_OT_autocomplete", TABKEY, KM_PRESS, 0, 0); /* python operator - space_text.py */
	WM_keymap_add_item(keymap, "CONSOLE_OT_autocomplete", SPACEKEY, KM_PRESS, KM_CTRL, 0); /* python operator - space_text.py */
#endif

	/* report selection */
	WM_keymap_add_item(keymap, "CONSOLE_OT_select_pick", SELECTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CONSOLE_OT_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CONSOLE_OT_select_border", BKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "CONSOLE_OT_report_replay", RKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CONSOLE_OT_report_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CONSOLE_OT_report_delete", DELKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CONSOLE_OT_report_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "CONSOLE_OT_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "CONSOLE_OT_paste", VKEY, KM_PRESS, KM_CTRL, 0);
#ifdef __APPLE__
	WM_keymap_add_item(keymap, "CONSOLE_OT_copy", CKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "CONSOLE_OT_paste", VKEY, KM_PRESS, KM_OSKEY, 0);
#endif
	
	WM_keymap_add_item(keymap, "CONSOLE_OT_select_set", LEFTMOUSE, KM_PRESS, 0, 0);

	RNA_string_set(WM_keymap_add_item(keymap, "CONSOLE_OT_insert", TABKEY, KM_PRESS, 0, 0)->ptr, "text", "    "); /* fake tabs */
	WM_keymap_add_item(keymap, "CONSOLE_OT_insert", KM_TEXTINPUT, KM_ANY, KM_ANY, 0); // last!
}

/****************** header region ******************/

/* add handlers, stuff you only do once or on area/region changes */
static void console_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	ED_region_header_init(ar);
}

static void console_header_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void console_main_area_listener(ScrArea *sa, wmNotifier *wmn)
{
	SpaceConsole *sc= sa->spacedata.first;

	/* context changes */
	switch(wmn->category) {
		case NC_SPACE:
			if(wmn->data == ND_SPACE_CONSOLE) { /* generic redraw request */
				ED_area_tag_redraw(sa);
			}
			else if(wmn->data == ND_SPACE_CONSOLE_REPORT && sc->type==CONSOLE_TYPE_REPORT) {
				/* redraw also but only for report view, could do less redraws by checking the type */
				ED_area_tag_redraw(sa);
			}
			break;
	}
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_console(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype console");
	ARegionType *art;
	
	st->spaceid= SPACE_CONSOLE;
	strncpy(st->name, "Console", BKE_ST_MAXNAME);
	
	st->new= console_new;
	st->free= console_free;
	st->init= console_init;
	st->duplicate= console_duplicate;
	st->operatortypes= console_operatortypes;
	st->keymap= console_keymap;
	st->listener= console_main_area_listener;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype console region");
	art->regionid = RGN_TYPE_WINDOW;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;

	art->init= console_main_area_init;
	art->draw= console_main_area_draw;
	
	
	

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype console region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_HEADER;
	
	art->init= console_header_area_init;
	art->draw= console_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);


	BKE_spacetype_register(st);
}
