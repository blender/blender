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
 * The Original Code is Copyright (C) 2009 by Blender Foundation
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_logic/logic_buttons.c
 *  \ingroup splogic
 */


#include <string.h>
#include <stdio.h>

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "interface_intern.h"
#include "logic_intern.h"

static int logic_properties_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = logic_has_buttons_region(sa);
	
	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void LOGIC_OT_properties(wmOperatorType *ot)
{
	ot->name = "Properties";
	ot->description = "Toggle the properties region visibility";
	ot->idname = "LOGIC_OT_properties";
	
	ot->exec = logic_properties_toggle_exec;
	ot->poll = ED_operator_logic_active;
	
	/* flags */
	ot->flag = 0;
}

/* Remove Logic Bricks Connections */
/* ********************** Cut Link operator ***************** */

#define LINK_RESOL 12
static int cut_links_intersect(uiLinkLine *line, float mcoords[][2], int tot)
{
	float coord_array[LINK_RESOL+1][2];
	int i, b;
	rcti rectlink;

	rectlink.xmin = (int)BLI_rctf_cent_x(&line->from->rect);
	rectlink.ymin = (int)BLI_rctf_cent_y(&line->from->rect);
	rectlink.xmax = (int)BLI_rctf_cent_x(&line->to->rect);
	rectlink.ymax = (int)BLI_rctf_cent_y(&line->to->rect);

	if (ui_link_bezier_points(&rectlink, coord_array, LINK_RESOL)) {
		for (i=0; i<tot-1; i++)
			for (b=0; b<LINK_RESOL-1; b++)
				if (isect_seg_seg_v2(mcoords[i], mcoords[i + 1], coord_array[b], coord_array[b + 1]) > 0)
					return 1;
	}
	return 0;
}

static int cut_links_exec(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	float mcoords[256][2];
	int i = 0;
	
	RNA_BEGIN (op->ptr, itemptr, "path")
	{
		float loc[2];
		
		RNA_float_get_array(&itemptr, "loc", loc);
		UI_view2d_region_to_view(&ar->v2d,
		                         (int)loc[0], (int)loc[1],
		                         &mcoords[i][0], &mcoords[i][1]);
		i++;
		if (i >= 256) break;
	}
	RNA_END;

	if (i>1) {
		uiBlock *block;
		uiLinkLine *line, *nline;
		uiBut *but;
		for (block = ar->uiblocks.first; block; block = block->next) {
			but = block->buttons.first;
			while (but) {
				if (but->type==UI_BTYPE_LINK && but->link) {
					for (line = but->link->lines.first; line; line = nline) {
						nline = line->next;

						if (cut_links_intersect(line, mcoords, i)) {
							ui_linkline_remove(line, but);
						}
					}
				}
				but = but->next;
			}
		}
		return OPERATOR_FINISHED;
	}
	return OPERATOR_CANCELLED|OPERATOR_PASS_THROUGH;
}

void LOGIC_OT_links_cut(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	ot->name = "Cut Links";
	ot->idname = "LOGIC_OT_links_cut";
	ot->description = "Remove logic brick connections";
	
	ot->invoke = WM_gesture_lines_invoke;
	ot->modal = WM_gesture_lines_modal;
	ot->exec = cut_links_exec;
	ot->cancel = WM_gesture_lines_cancel;
	
	ot->poll = ED_operator_logic_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	prop = RNA_def_property(ot->srna, "path", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_runtime(prop, &RNA_OperatorMousePath);
	/* internal */
	RNA_def_int(ot->srna, "cursor", BC_KNIFECURSOR, 0, INT_MAX, "Cursor", "", 0, INT_MAX);
}

