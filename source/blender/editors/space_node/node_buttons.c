/**
 * $Id:
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_ID.h"
#include "DNA_gpencil_types.h"
#include "DNA_node_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_idprop.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "node_intern.h"	// own include


/* ******************* node space & buttons ************** */
#define B_NOP		1
#define B_REDR		2

#if 0 // XXX not used...
static void do_node_region_buttons(bContext *C, void *arg, int event)
{
	//SpaceNode *snode= CTX_wm_space_node(C);
	
	switch(event) {
	case B_REDR:
		ED_area_tag_redraw(CTX_wm_area(C));
		return; /* no notifier! */
	}
}
#endif

/* ******************* node buttons registration ************** */

void node_buttons_register(ARegionType *art)
{
	PanelType *pt;
	
	// XXX active node
	
	pt= MEM_callocN(sizeof(PanelType), "spacetype node panel gpencil");
	strcpy(pt->idname, "NODE_PT_gpencil");
	strcpy(pt->label, "Grease Pencil");
	pt->draw= gpencil_panel_standard;
	BLI_addtail(&art->paneltypes, pt);
}

static int node_properties(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= node_has_buttons_region(sa);
	
	if(ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

/* non-standard poll operator which doesn't care if there are any nodes */
static int node_properties_poll(bContext *C)
{
	ScrArea *sa= CTX_wm_area(C);
	return (sa && (sa->spacetype == SPACE_NODE));
}

void NODE_OT_properties(wmOperatorType *ot)
{
	ot->name= "Properties";
	ot->description= "Toggles the properties panel display.";
	ot->idname= "NODE_OT_properties";
	
	ot->exec= node_properties;
	ot->poll= node_properties_poll;
	
	/* flags */
	ot->flag= 0;
}
