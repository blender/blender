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
#include "BKE_scene.h"
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
#if 0
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
	
	block= uiBeginBlock(C, ar, "text_template_scriptsmenu", UI_EMBOSSP);
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
	
	block= uiBeginBlock(C, ar, "text_plugin_scriptsmenu", UI_EMBOSSP);
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
#endif

/************************** properties ******************************/

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

void text_toggle_properties_region(bContext *C, ScrArea *sa, ARegion *ar)
{
	ar->flag ^= RGN_FLAG_HIDDEN;
	ar->v2d.flag &= ~V2D_IS_INITIALISED; /* XXX should become hide/unhide api? */
	
	ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
	ED_area_tag_redraw(sa);
}

static int properties_poll(bContext *C)
{
	return (CTX_wm_space_text(C) != NULL);
}

static int properties_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= text_has_properties_region(sa);
	
	if(ar)
		text_toggle_properties_region(C, sa, ar);

	return OPERATOR_FINISHED;
}

void TEXT_OT_properties(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Properties";
    ot->description= "Toggle text properties panel.";
	ot->idname= "TEXT_OT_properties";
	
	/* api callbacks */
	ot->exec= properties_exec;
	ot->poll= properties_poll;
}

/******************** XXX popup menus *******************/

#if 0
{
	// RMB

	uiPopupMenu *pup;

	if(text) {
		pup= uiPupMenuBegin(C, "Text", 0);
		if(txt_has_sel(text)) {
			uiItemO(layout, NULL, 0, "TEXT_OT_cut");
			uiItemO(layout, NULL, 0, "TEXT_OT_copy");
		}
		uiItemO(layout, NULL, 0, "TEXT_OT_paste");
		uiItemO(layout, NULL, 0, "TEXT_OT_new");
		uiItemO(layout, NULL, 0, "TEXT_OT_open");
		uiItemO(layout, NULL, 0, "TEXT_OT_save");
		uiItemO(layout, NULL, 0, "TEXT_OT_save_as");
		uiItemO(layout, NULL, 0, "TEXT_OT_run_script");
		uiPupMenuEnd(C, pup);
	}
	else {
		pup= uiPupMenuBegin(C, "File", 0);
		uiItemO(layout, NULL, 0, "TEXT_OT_new");
		uiItemO(layout, NULL, 0, "TEXT_OT_open");
		uiPupMenuEnd(C, pup);
	}
}

{
	// Alt+Shift+E

	uiPopupMenu *pup;

	pup= uiPupMenuBegin(C, "Edit", 0);
	uiItemO(layout, NULL, 0, "TEXT_OT_cut");
	uiItemO(layout, NULL, 0, "TEXT_OT_copy");
	uiItemO(layout, NULL, 0, "TEXT_OT_paste");
	uiPupMenuEnd(C, pup);
}

{
	// Alt+Shift+F

	uiPopupMenu *pup;

	if(text) {
		pup= uiPupMenuBegin(C, "Text", 0);
		uiItemO(layout, NULL, 0, "TEXT_OT_new");
		uiItemO(layout, NULL, 0, "TEXT_OT_open");
		uiItemO(layout, NULL, 0, "TEXT_OT_save");
		uiItemO(layout, NULL, 0, "TEXT_OT_save_as");
		uiItemO(layout, NULL, 0, "TEXT_OT_run_script");
		uiPupMenuEnd(C, pup);
	}
	else {
		pup= uiPupMenuBegin(C, "File", 0);
		uiItemO(layout, NULL, 0, "TEXT_OT_new");
		uiItemO(layout, NULL, 0, "TEXT_OT_open");
		uiPupMenuEnd(C, pup);
	}
}

{
	// Alt+Shift+V

	uiPopupMenu *pup;

	pup= uiPupMenuBegin(C, "Text", 0);
	uiItemEnumO(layout, "Top of File", 0, "TEXT_OT_move", "type", FILE_TOP);
	uiItemEnumO(layout, "Bottom of File", 0, "TEXT_OT_move", "type", FILE_BOTTOM);
	uiItemEnumO(layout, "Page Up", 0, "TEXT_OT_move", "type", PREV_PAGE);
	uiItemEnumO(layout,  "Page Down", 0, "TEXT_OT_move", "type", NEXT_PAGE);
	uiPupMenuEnd(C, pup);
}
#endif

