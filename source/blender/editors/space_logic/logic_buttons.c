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
#include "DNA_node_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "logic_intern.h"

#if 0
static void do_logic_panel_events(bContext *C, void *arg, int event)
{
	
	switch(event) {
		
	}
}


/* *** */

static void logic_panel_properties(const bContext *C, Panel *pa)
{
//	SpaceLogic *slogic= CTX_wm_space_logic(C);
	uiBlock *block;
	
	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_logic_panel_events, NULL);

}	

static void logic_panel_view_properties(const bContext *C, Panel *pa)
{
	//	SpaceLogic *slogic= CTX_wm_space_logic(C);
	uiBlock *block;
	
	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_logic_panel_events, NULL);
	
}	
#endif

void logic_buttons_register(ARegionType *art)
{
#if 0
	PanelType *pt;

	pt= MEM_callocN(sizeof(PanelType), "spacetype logic panel properties");
	strcpy(pt->idname, "LOGIC_PT_properties");
	strcpy(pt->label, "Logic Properties");
	pt->draw= logic_panel_properties;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype logic view properties");
	strcpy(pt->idname, "LOGIC_PT_view_properties");
	strcpy(pt->label, "View Properties");
	pt->draw= logic_panel_view_properties;
	BLI_addtail(&art->paneltypes, pt);
#endif

}

static int logic_properties(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= logic_has_buttons_region(sa);
	
	if(ar) {
		ar->flag ^= RGN_FLAG_HIDDEN;
		ar->v2d.flag &= ~V2D_IS_INITIALISED; /* XXX should become hide/unhide api? */
		
		ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
		ED_area_tag_redraw(sa);
	}
	return OPERATOR_FINISHED;
}

void LOGIC_OT_properties(wmOperatorType *ot)
{
	ot->name= "Properties";
	ot->idname= "LOGIC_OT_properties";
	
	ot->exec= logic_properties;
	ot->poll= ED_operator_logic_active;
	
	/* flags */
	ot->flag= 0;
}



