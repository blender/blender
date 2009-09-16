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

#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_paint.h"

#include "ED_sculpt.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "paint_intern.h"

#include <string.h>

/* Brush operators */
static int brush_add_exec(bContext *C, wmOperator *op)
{
	/*int type = RNA_enum_get(op->ptr, "type");*/
	Brush *br = NULL;

	br = add_brush("Brush");

	if(br)
		paint_brush_set(paint_get_active(CTX_data_scene(C)), br);
	
	return OPERATOR_FINISHED;
}

static EnumPropertyItem brush_type_items[] = {
	{OB_MODE_SCULPT, "SCULPT", ICON_SCULPTMODE_HLT, "Sculpt", ""},
	{OB_MODE_VERTEX_PAINT, "VERTEX_PAINT", ICON_VPAINT_HLT, "Vertex Paint", ""},
	{OB_MODE_WEIGHT_PAINT, "WEIGHT_PAINT", ICON_WPAINT_HLT, "Weight Paint", ""},
	{OB_MODE_TEXTURE_PAINT, "TEXTURE_PAINT", ICON_TPAINT_HLT, "Texture Paint", ""},
	{0, NULL, 0, NULL, NULL}};

void BRUSH_OT_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Brush";
    ot->description= "Add brush by mode type.";
	ot->idname= "BRUSH_OT_add";
	
	/* api callbacks */
	ot->exec= brush_add_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", brush_type_items, OB_MODE_VERTEX_PAINT, "Type", "Which paint mode to create the brush for.");
}

static int vertex_color_set_exec(bContext *C, wmOperator *op)
{
	int selected = RNA_boolean_get(op->ptr, "selected");
	Scene *scene = CTX_data_scene(C);

	clear_vpaint(scene, selected);
	
	return OPERATOR_FINISHED;
}

void PAINT_OT_vertex_color_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Vertex Colors";
	ot->idname= "PAINT_OT_vertex_color_set";
	
	/* api callbacks */
	ot->exec= vertex_color_set_exec;
	ot->poll= vertex_paint_mode_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "selected", 0, "Type", "Only color selected faces.");
}

/**************************** registration **********************************/

void ED_operatortypes_paint(void)
{
	/* brush */
	WM_operatortype_append(BRUSH_OT_add);
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
	WM_operatortype_append(PAINT_OT_vertex_color_set);
}

