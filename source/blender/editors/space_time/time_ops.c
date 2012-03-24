/*
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

/** \file blender/editors/space_time/time_ops.c
 *  \ingroup sptime
 */


#include <stdlib.h>
#include <math.h>

#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "time_intern.h"

/* ****************** Start/End Frame Operators *******************************/
static int time_set_sfra_exec (bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene= CTX_data_scene(C);
	int frame;

	if (scene == NULL)
		return OPERATOR_CANCELLED;

	frame= CFRA;

	/* if Preview Range is defined, set the 'start' frame for that */
	if (PRVRANGEON)
		scene->r.psfra= frame;
	else
		scene->r.sfra= frame;
	
	if (PEFRA < frame) {
		if (PRVRANGEON)
			scene->r.pefra= frame;
		else
			scene->r.efra= frame;
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene);
	
	return OPERATOR_FINISHED;
}

static void TIME_OT_start_frame_set (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Start Frame";
	ot->idname = "TIME_OT_start_frame_set";
	ot->description = "Set the start frame";
	
	/* api callbacks */
	ot->exec = time_set_sfra_exec;
	ot->poll = ED_operator_timeline_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}	


static int time_set_efra_exec (bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene= CTX_data_scene(C);
	int frame;

	if (scene == NULL)
		return OPERATOR_CANCELLED;

	frame= CFRA;

	/* if Preview Range is defined, set the 'end' frame for that */
	if (PRVRANGEON)
		scene->r.pefra= frame;
	else
		scene->r.efra= frame;

	if (PSFRA > frame) {
		if (PRVRANGEON)
			scene->r.psfra= frame;
		else
			scene->r.sfra= frame;
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene);
	
	return OPERATOR_FINISHED;
}

static void TIME_OT_end_frame_set (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set End Frame";
	ot->idname = "TIME_OT_end_frame_set";
	ot->description = "Set the end frame";
	
	/* api callbacks */
	ot->exec = time_set_efra_exec;
	ot->poll = ED_operator_timeline_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************************ View All Operator *******************************/

static int time_view_all_exec (bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene= CTX_data_scene(C);
	ARegion *ar= CTX_wm_region(C);
	View2D *v2d= (ar) ? &ar->v2d : NULL;
	float extra;
	
	if ELEM(NULL, scene, ar)
		return OPERATOR_CANCELLED;
		
	/* set extents of view to start/end frames (Preview Range too) */
	v2d->cur.xmin = (float)PSFRA;
	v2d->cur.xmax = (float)PEFRA;
	
	/* we need an extra "buffer" factor on either side so that the endpoints are visible */
	extra= 0.01f * (v2d->cur.xmax - v2d->cur.xmin);
	v2d->cur.xmin -= extra;
	v2d->cur.xmax += extra;
	
	/* this only affects this TimeLine instance, so just force redraw of this region */
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

static void TIME_OT_view_all (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View All";
	ot->idname = "TIME_OT_view_all";
	ot->description = "Show the entire playable frame range";
	
	/* api callbacks */
	ot->exec = time_view_all_exec;
	ot->poll = ED_operator_timeline_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************************** registration **********************************/

void time_operatortypes(void)
{
	WM_operatortype_append(TIME_OT_start_frame_set);
	WM_operatortype_append(TIME_OT_end_frame_set);
	WM_operatortype_append(TIME_OT_view_all);
}

void time_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Timeline", SPACE_TIME, 0);
	
	WM_keymap_add_item(keymap, "TIME_OT_start_frame_set", SKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "TIME_OT_end_frame_set", EKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "TIME_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
}

