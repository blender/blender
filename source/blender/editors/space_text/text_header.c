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

#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"



#ifndef DISABLE_PYTHON
#include "BPY_extern.h"
// XXX #include "BPY_menus.h"
#endif

#include "text_intern.h"

#define HEADER_PATH_MAX	260

/* ************************ header area region *********************** */

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

static int properties_poll(bContext *C)
{
	return (CTX_wm_space_text(C) != NULL);
}

static int properties_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= text_has_properties_region(sa);
	
	if(ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void TEXT_OT_properties(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Properties";
	ot->description= "Toggle text properties panel";
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

