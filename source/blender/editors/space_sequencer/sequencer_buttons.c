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
 * The Original Code is Copyright (C) 2009 by Blender Foundation
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include <string.h>
#include <stdio.h>

#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "ED_screen.h"
#include "ED_sequencer.h"
#include "ED_util.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "sequencer_intern.h"


static void do_sequencer_panel_events(bContext *C, void *arg, int event)
{

}


static void sequencer_panel_view_properties(const bContext *C, Panel *pa)
{
	uiBlock *block;

	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_sequencer_panel_events, NULL);
	
}


static void sequencer_panel_properties(const bContext *C, Panel *pa)
{
	uiBlock *block;
	
	block= uiLayoutFreeBlock(pa->layout);
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
	
	if(ar) {
		ar->flag ^= RGN_FLAG_HIDDEN;
		ar->v2d.flag &= ~V2D_IS_INITIALISED; /* XXX should become hide/unhide api? */
		
		ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
		ED_area_tag_redraw(sa);
	}
	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_properties(wmOperatorType *ot)
{
	ot->name= "Properties";
	ot->idname= "SEQUENCER_OT_properties";
	
	ot->exec= sequencer_properties;
	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= 0;
}

