/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "DNA_scene_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_paint.h"

#include "ED_sculpt.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "paint_intern.h"

#include <string.h>

/* Brush operators */
static int new_brush_exec(bContext *C, wmOperator *op)
{
	int sculpt_tool = RNA_enum_get(op->ptr, "sculpt_tool");
	const char *name = NULL;
	Brush *br = NULL;

	RNA_enum_name(brush_sculpt_tool_items, sculpt_tool, &name);
	br = add_brush(name);

	if(br) {
		br->sculpt_tool = sculpt_tool;
		paint_brush_set(paint_get_active(CTX_data_scene(C)), br);
	}
	
	return OPERATOR_FINISHED;
}

void BRUSH_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Brush";
	ot->idname= "BRUSH_OT_new";
	
	/* api callbacks */
	ot->exec= new_brush_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* TODO: add enum props for other paint modes */
	RNA_def_enum(ot->srna, "sculpt_tool", brush_sculpt_tool_items, 0, "Sculpt Tool", "");
}

/* Paint operators */
static int paint_poll(bContext *C)
{
	return !!paint_get_active(CTX_data_scene(C));
}

static int brush_slot_add_exec(bContext *C, wmOperator *op)
{
	Paint *p = paint_get_active(CTX_data_scene(C));

	paint_brush_slot_add(p);

	return OPERATOR_FINISHED;
}

void PAINT_OT_brush_slot_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Brush Slot";
	ot->idname= "PAINT_OT_brush_slot_add";
       
	/* api callbacks */
	ot->poll= paint_poll;
	ot->exec= brush_slot_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int brush_slot_remove_exec(bContext *C, wmOperator *op)
{
	Paint *p = paint_get_active(CTX_data_scene(C));

	paint_brush_slot_remove(p);

	return OPERATOR_FINISHED;
}

void PAINT_OT_brush_slot_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Brush Slot";
	ot->idname= "PAINT_OT_brush_slot_remove";
       
	/* api callbacks */
	ot->poll= paint_poll;
	ot->exec= brush_slot_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/**************************** registration **********************************/

void ED_operatortypes_paint(void)
{
	/* paint */
	WM_operatortype_append(PAINT_OT_brush_slot_add);
	WM_operatortype_append(PAINT_OT_brush_slot_remove);

	/* brush */
	WM_operatortype_append(BRUSH_OT_new);
	WM_operatortype_append(BRUSH_OT_curve_preset);

	/* image */
	WM_operatortype_append(PAINT_OT_texture_paint_toggle);
	WM_operatortype_append(PAINT_OT_texture_paint_radial_control);
	WM_operatortype_append(PAINT_OT_image_paint);
	WM_operatortype_append(PAINT_OT_image_paint_radial_control);
	WM_operatortype_append(PAINT_OT_sample_color);
	WM_operatortype_append(PAINT_OT_grab_clone);
	WM_operatortype_append(PAINT_OT_clone_cursor_set);

	/* weight */
	WM_operatortype_append(PAINT_OT_weight_paint_toggle);
	WM_operatortype_append(PAINT_OT_weight_paint_radial_control);
	WM_operatortype_append(PAINT_OT_weight_paint);

	/* vertex */
	WM_operatortype_append(PAINT_OT_vertex_paint_radial_control);
	WM_operatortype_append(PAINT_OT_vertex_paint_toggle);
	WM_operatortype_append(PAINT_OT_vertex_paint);
}

