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
 * The Original Code is Copyright (C) 2009 by Blender Foundation
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include <string.h>
#include <stdio.h>


#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"


#include "ED_screen.h"


#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"

#include "sequencer_intern.h"


static void do_sequencer_panel_events(bContext *C, void *arg, int event)
{

}


static void sequencer_panel_view_properties(const bContext *C, Panel *pa)
{
	uiBlock *block;

	block= uiLayoutAbsoluteBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_sequencer_panel_events, NULL);
	
}


static void sequencer_panel_properties(const bContext *C, Panel *pa)
{
	uiBlock *block;
	
	block= uiLayoutAbsoluteBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_sequencer_panel_events, NULL);

}	

void sequencer_buttons_register(ARegionType *art)
{
	PanelType *pt;

	pt= MEM_callocN(sizeof(PanelType), "spacetype sequencer strip properties");
	strcpy(pt->idname, "SEQUENCER_PT_properties");
	strcpy(pt->label, "Strip Properties");
	pt->draw= sequencer_panel_properties;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype sequencer view properties");
	strcpy(pt->idname, "SEQUENCER_PT_view_properties");
	strcpy(pt->label, "View Properties");
	pt->draw= sequencer_panel_view_properties;
	BLI_addtail(&art->paneltypes, pt);

}

/* **************** operator to open/close properties view ************* */

static int sequencer_properties(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= sequencer_has_buttons_region(sa);
	
	if(ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_properties(wmOperatorType *ot)
{
	ot->name= "Properties";
	ot->idname= "SEQUENCER_OT_properties";
	ot->description= "Open sequencer properties panel";
	
	ot->exec= sequencer_properties;
	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= 0;
}

