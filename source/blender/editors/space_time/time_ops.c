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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_utildefines.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"


/* ****************** Start/End Frame Operators *******************************/

static int time_set_sfra_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	int frame= CFRA;
	
	if (scene == NULL)
		return OPERATOR_CANCELLED;
		
	/* if 'end frame' (Preview Range or Actual) is less than 'frame', 
	 * clamp 'frame' to 'end frame'
	 */
	if (PEFRA < frame) frame= PEFRA;
		
	/* if Preview Range is defined, set the 'start' frame for that */
	if (scene->r.psfra)
		scene->r.psfra= frame;
	else
		scene->r.sfra= frame;
	
	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene);
	
	return OPERATOR_FINISHED;
}

void TIME_OT_start_frame_set (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Start Frame";
	ot->idname= "TIME_OT_start_frame_set";
	ot->description="Set the start frame.";
	
	/* api callbacks */
	ot->exec= time_set_sfra_exec;
	ot->poll= ED_operator_timeline_active;
	
	// XXX properties???
}	


static int time_set_efra_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	int frame= CFRA;
	
	if (scene == NULL)
		return OPERATOR_CANCELLED;
		
	/* if 'start frame' (Preview Range or Actual) is greater than 'frame', 
	 * clamp 'frame' to 'end frame'
	 */
	if (PSFRA > frame) frame= PSFRA;
		
	/* if Preview Range is defined, set the 'end' frame for that */
	if (scene->r.pefra)
		scene->r.pefra= frame;
	else
		scene->r.efra= frame;
	
	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene);
	
	return OPERATOR_FINISHED;
}

void TIME_OT_end_frame_set (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set End Frame";
	ot->idname= "TIME_OT_end_frame_set";
	ot->description="Set the end frame.";
	
	/* api callbacks */
	ot->exec= time_set_efra_exec;
	ot->poll= ED_operator_timeline_active;
	
	// XXX properties???
}

/* ************************** registration **********************************/

void time_operatortypes(void)
{
	WM_operatortype_append(TIME_OT_start_frame_set);
	WM_operatortype_append(TIME_OT_end_frame_set);
}

void time_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap= WM_keymap_find(keyconf, "TimeLine", SPACE_TIME, 0);
	
	WM_keymap_add_item(keymap, "TIME_OT_start_frame_set", SKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "TIME_OT_end_frame_set", EKEY, KM_PRESS, 0, 0);
}

