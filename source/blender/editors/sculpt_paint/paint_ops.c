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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/sculpt_paint/paint_ops.c
 *  \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include <stdlib.h>
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_paint.h"
#include "BKE_main.h"

#include "ED_paint.h"
#include "ED_screen.h"
#include "ED_image.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "paint_intern.h"
#include "sculpt_intern.h"

#include <string.h>
//#include <stdio.h>
#include <stddef.h>

/* Brush operators */
static int brush_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	/*int type = RNA_enum_get(op->ptr, "type");*/
	Paint *paint = BKE_paint_get_active_from_context(C);
	Brush *br = BKE_paint_brush(paint);
	Main *bmain = CTX_data_main(C);
	PaintMode mode = BKE_paintmode_get_active_from_context(C);

	if (br)
		br = BKE_brush_copy(bmain, br);
	else
		br = BKE_brush_add(bmain, "Brush", BKE_paint_object_mode_from_paint_mode(mode));

	BKE_paint_brush_set(paint, br);

	return OPERATOR_FINISHED;
}

static void BRUSH_OT_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Brush";
	ot->description = "Add brush by mode type";
	ot->idname = "BRUSH_OT_add";
	
	/* api callbacks */
	ot->exec = brush_add_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int brush_scale_size_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Paint  *paint =  BKE_paint_get_active_from_context(C);
	Brush  *brush =  BKE_paint_brush(paint);
	// Object *ob = CTX_data_active_object(C);
	float scalar = RNA_float_get(op->ptr, "scalar");

	if (brush) {
		// pixel radius
		{
			const int old_size = BKE_brush_size_get(scene, brush);
			int size = (int)(scalar * old_size);

			if (abs(old_size - size) < U.pixelsize) {
				if (scalar > 1) {
					size += U.pixelsize;
				}
				else if (scalar < 1) {
					size -= U.pixelsize;
				}
			}

			BKE_brush_size_set(scene, brush, size);
		}

		// unprojected radius
		{
			float unprojected_radius = scalar * BKE_brush_unprojected_radius_get(scene, brush);

			if (unprojected_radius < 0.001f) // XXX magic number
				unprojected_radius = 0.001f;

			BKE_brush_unprojected_radius_set(scene, brush, unprojected_radius);
		}

		WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);
	}

	return OPERATOR_FINISHED;
}

static void BRUSH_OT_scale_size(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Scale Sculpt/Paint Brush Size";
	ot->description = "Change brush size by a scalar";
	ot->idname = "BRUSH_OT_scale_size";
	
	/* api callbacks */
	ot->exec = brush_scale_size_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_float(ot->srna, "scalar", 1, 0, 2, "Scalar", "Factor to scale brush size by", 0, 2);
}

/* Palette operators */

static int palette_new_exec(bContext *C, wmOperator *UNUSED(op))
{
	Paint *paint = BKE_paint_get_active_from_context(C);
	Main *bmain = CTX_data_main(C);
	Palette *palette;

	palette = BKE_palette_add(bmain, "Palette");

	BKE_paint_palette_set(paint, palette);

	return OPERATOR_FINISHED;
}

static void PALETTE_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add New Palette";
	ot->description = "Add new palette";
	ot->idname = "PALETTE_OT_new";

	/* api callbacks */
	ot->exec = palette_new_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int palette_poll(bContext *C)
{
	Paint *paint = BKE_paint_get_active_from_context(C);

	if (paint && paint->palette != NULL)
		return true;

	return false;
}

static int palette_color_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	Paint *paint = BKE_paint_get_active_from_context(C);
	Brush *brush = paint->brush;
	PaintMode mode = BKE_paintmode_get_active_from_context(C);
	Palette *palette = paint->palette;
	PaletteColor *color;

	color = BKE_palette_color_add(palette);
	palette->active_color = BLI_listbase_count(&palette->colors) - 1;

	if (ELEM(mode, ePaintTextureProjective, ePaintTexture2D, ePaintVertex)) {
		copy_v3_v3(color->rgb, BKE_brush_color_get(scene, brush));
		color->value = 0.0;
	}
	else if (mode == ePaintWeight) {
		zero_v3(color->rgb);
		color->value = brush->weight;
	}

	return OPERATOR_FINISHED;
}

static void PALETTE_OT_color_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Palette Color";
	ot->description = "Add new color to active palette";
	ot->idname = "PALETTE_OT_color_add";

	/* api callbacks */
	ot->exec = palette_color_add_exec;
	ot->poll = palette_poll;
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int palette_color_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	Paint *paint = BKE_paint_get_active_from_context(C);
	Palette *palette = paint->palette;
	PaletteColor *color = BLI_findlink(&palette->colors, palette->active_color);

	if (color) {
		BKE_palette_color_remove(palette, color);
	}

	return OPERATOR_FINISHED;
}

static void PALETTE_OT_color_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Palette Color";
	ot->description = "Remove active color from palette";
	ot->idname = "PALETTE_OT_color_delete";

	/* api callbacks */
	ot->exec = palette_color_delete_exec;
	ot->poll = palette_poll;
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}



static int vertex_color_set_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	Object *obact = CTX_data_active_object(C);
	unsigned int paintcol = vpaint_get_current_col(scene, scene->toolsettings->vpaint);

	if (ED_vpaint_fill(obact, paintcol)) {
		ED_region_tag_redraw(CTX_wm_region(C)); // XXX - should redraw all 3D views
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static void PAINT_OT_vertex_color_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Vertex Colors";
	ot->idname = "PAINT_OT_vertex_color_set";
	ot->description = "Fill the active vertex color layer with the current paint color";
	
	/* api callbacks */
	ot->exec = vertex_color_set_exec;
	ot->poll = vertex_paint_mode_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int vertex_color_smooth_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obact = CTX_data_active_object(C);
	if (ED_vpaint_smooth(obact)) {
		ED_region_tag_redraw(CTX_wm_region(C)); // XXX - should redraw all 3D views
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static void PAINT_OT_vertex_color_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Smooth Vertex Colors";
	ot->idname = "PAINT_OT_vertex_color_smooth";
	ot->description = "Smooth colors across vertices";

	/* api callbacks */
	ot->exec = vertex_color_smooth_exec;
	ot->poll = vertex_paint_mode_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/** \name Vertex Color Transformations
 * \{ */

struct VPaintTx_BrightContrastData {
	/* pre-calculated */
	float gain;
	float offset;
};

static void vpaint_tx_brightness_contrast(const float col[3], const void *user_data, float r_col[3])
{
	const struct VPaintTx_BrightContrastData *data = user_data;

	for (int i = 0; i < 3; i++) {
		r_col[i] = data->gain * col[i] + data->offset;
	}
}

static int vertex_color_brightness_contrast_exec(bContext *C, wmOperator *op)
{
	Object *obact = CTX_data_active_object(C);

	float gain, offset;
	{
		float brightness = RNA_float_get(op->ptr, "brightness");
		float contrast = RNA_float_get(op->ptr, "contrast");
		brightness /= 100.0f;
		float delta = contrast / 200.0f;
		gain = 1.0f - delta * 2.0f;
		/*
		 * The algorithm is by Werner D. Streidt
		 * (http://visca.com/ffactory/archives/5-99/msg00021.html)
		 * Extracted of OpenCV demhist.c
		 */
		if (contrast > 0) {
			gain = 1.0f / ((gain != 0.0f) ? gain : FLT_EPSILON);
			offset = gain * (brightness - delta);
		}
		else {
			delta *= -1;
			offset = gain * (brightness + delta);
		}
	}

	const struct VPaintTx_BrightContrastData user_data = {
		.gain = gain,
		.offset = offset,
	};

	if (ED_vpaint_color_transform(obact, vpaint_tx_brightness_contrast, &user_data)) {
		ED_region_tag_redraw(CTX_wm_region(C));
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static void PAINT_OT_vertex_color_brightness_contrast(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Vertex Paint Bright/Contrast";
	ot->idname = "PAINT_OT_vertex_color_brightness_contrast";
	ot->description = "Adjust vertex color brightness/contrast";

	/* api callbacks */
	ot->exec = vertex_color_brightness_contrast_exec;
	ot->poll = vertex_paint_mode_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* params */
	const float min = -100, max = +100;
	prop = RNA_def_float(ot->srna, "brightness", 0.0f, min, max, "Brightness", "", min, max);
	prop = RNA_def_float(ot->srna, "contrast", 0.0f, min, max, "Contrast", "", min, max);
	RNA_def_property_ui_range(prop, min, max, 1, 1);
}

struct VPaintTx_HueSatData {
	float hue;
	float sat;
	float val;
};

static void vpaint_tx_hsv(const float col[3], const void *user_data, float r_col[3])
{
	const struct VPaintTx_HueSatData *data = user_data;
	float hsv[3];
	rgb_to_hsv_v(col, hsv);

	hsv[0] += (data->hue - 0.5f);
	if (hsv[0] > 1.0f) {
		hsv[0] -= 1.0f;
	}
	else if (hsv[0] < 0.0f) {
		hsv[0] += 1.0f;
	}
	hsv[1] *= data->sat;
	hsv[2] *= data->val;

	hsv_to_rgb_v(hsv, r_col);
}

static int vertex_color_hsv_exec(bContext *C, wmOperator *op)
{
	Object *obact = CTX_data_active_object(C);

	const struct VPaintTx_HueSatData user_data = {
		.hue = RNA_float_get(op->ptr, "h"),
		.sat = RNA_float_get(op->ptr, "s"),
		.val = RNA_float_get(op->ptr, "v"),
	};

	if (ED_vpaint_color_transform(obact, vpaint_tx_hsv, &user_data)) {
		ED_region_tag_redraw(CTX_wm_region(C));
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static void PAINT_OT_vertex_color_hsv(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Vertex Paint Hue Saturation Value";
	ot->idname = "PAINT_OT_vertex_color_hsv";
	ot->description = "Adjust vertex color HSV values";

	/* api callbacks */
	ot->exec = vertex_color_hsv_exec;
	ot->poll = vertex_paint_mode_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* params */
	RNA_def_float(ot->srna, "h", 0.5f, 0.0f, 1.0f, "Hue", "", 0.0f, 1.0f);
	RNA_def_float(ot->srna, "s", 1.0f, 0.0f, 2.0f, "Saturation", "", 0.0f, 2.0f);
	RNA_def_float(ot->srna, "v", 1.0f, 0.0f, 2.0f, "Value", "", 0.0f, 2.0f);
}

static void vpaint_tx_invert(const float col[3], const void *UNUSED(user_data), float r_col[3])
{
	for (int i = 0; i < 3; i++) {
		r_col[i] = 1.0f - col[i];
	}
}

static int vertex_color_invert_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obact = CTX_data_active_object(C);

	if (ED_vpaint_color_transform(obact, vpaint_tx_invert, NULL)) {
		ED_region_tag_redraw(CTX_wm_region(C));
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static void PAINT_OT_vertex_color_invert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Vertex Paint Invert";
	ot->idname = "PAINT_OT_vertex_color_invert";
	ot->description = "Invert RGB values";

	/* api callbacks */
	ot->exec = vertex_color_invert_exec;
	ot->poll = vertex_paint_mode_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


struct VPaintTx_LevelsData {
	float gain;
	float offset;
};

static void vpaint_tx_levels(const float col[3], const void *user_data, float r_col[3])
{
	const struct VPaintTx_LevelsData *data = user_data;
	for (int i = 0; i < 3; i++) {
		r_col[i] = data->gain * (col[i] + data->offset);
	}
}

static int vertex_color_levels_exec(bContext *C, wmOperator *op)
{
	Object *obact = CTX_data_active_object(C);

	const struct VPaintTx_LevelsData user_data = {
		.gain = RNA_float_get(op->ptr, "gain"),
		.offset = RNA_float_get(op->ptr, "offset"),
	};

	if (ED_vpaint_color_transform(obact, vpaint_tx_levels, &user_data)) {
		ED_region_tag_redraw(CTX_wm_region(C));
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static void PAINT_OT_vertex_color_levels(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Vertex Paint Levels";
	ot->idname = "PAINT_OT_vertex_color_levels";
	ot->description = "Adjust levels of vertex colors";

	/* api callbacks */
	ot->exec = vertex_color_levels_exec;
	ot->poll = vertex_paint_mode_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* params */
	RNA_def_float(ot->srna, "offset", 0.0f, -1.0f, 1.0f, "Offset", "Value to add to colors", -1.0f, 1.0f);
	RNA_def_float(ot->srna, "gain", 1.0f, 0.0f, FLT_MAX, "Gain", "Value to multiply colors by", 0.0f, 10.0f);
}

/** \} */


static int brush_reset_exec(bContext *C, wmOperator *UNUSED(op))
{
	Paint *paint = BKE_paint_get_active_from_context(C);
	Brush *brush = BKE_paint_brush(paint);
	Object *ob = CTX_data_active_object(C);

	if (!ob || !brush) return OPERATOR_CANCELLED;

	/* TODO: other modes */
	if (ob->mode & OB_MODE_SCULPT) {
		BKE_brush_sculpt_reset(brush);
	}
	else {
		return OPERATOR_CANCELLED;
	}
	WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);

	return OPERATOR_FINISHED;
}

static void BRUSH_OT_reset(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reset Brush";
	ot->description = "Return brush to defaults based on current tool";
	ot->idname = "BRUSH_OT_reset";
	
	/* api callbacks */
	ot->exec = brush_reset_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int brush_tool(const Brush *brush, size_t tool_offset)
{
	return *(((char *)brush) + tool_offset);
}

static void brush_tool_set(const Brush *brush, size_t tool_offset, int tool)
{
	*(((char *)brush) + tool_offset) = tool;
}

/* generic functions for setting the active brush based on the tool */
static Brush *brush_tool_cycle(Main *bmain, Brush *brush_orig, const int tool, const size_t tool_offset, const int ob_mode)
{
	Brush *brush, *first_brush;

	if (!brush_orig && !(brush_orig = bmain->brush.first)) {
		return NULL;
	}

	if (brush_tool(brush_orig, tool_offset) != tool) {
		/* If current brush's tool is different from what we need,
		 * start cycling from the beginning of the list.
		 * Such logic will activate the same exact brush not relating from
		 * which tool user requests other tool.
		 */
		first_brush = bmain->brush.first;
	}
	else {
		/* If user wants to switch to brush with the same  tool as
		 * currently active brush do a cycling via all possible
		 * brushes with requested tool.
		 */
		first_brush = brush_orig->id.next ? brush_orig->id.next : bmain->brush.first;
	}

	/* get the next brush with the active tool */
	brush = first_brush;
	do {
		if ((brush->ob_mode & ob_mode) &&
		    (brush_tool(brush, tool_offset) == tool))
		{
			return brush;
		}

		brush = brush->id.next ? brush->id.next : bmain->brush.first;
	} while (brush != first_brush);

	return NULL;
}

static Brush *brush_tool_toggle(Main *bmain, Brush *brush_orig, const int tool, const size_t tool_offset, const int ob_mode)
{
	if (!brush_orig || brush_tool(brush_orig, tool_offset) != tool) {
		Brush *br;
		/* if the current brush is not using the desired tool, look
		 * for one that is */
		br = brush_tool_cycle(bmain, brush_orig, tool, tool_offset, ob_mode);
		/* store the previously-selected brush */
		if (br)
			br->toggle_brush = brush_orig;
		
		return br;
	}
	else if (brush_orig->toggle_brush) {
		/* if current brush is using the desired tool, try to toggle
		 * back to the previously selected brush. */
		return brush_orig->toggle_brush;
	}
	else
		return NULL;
}

static int brush_generic_tool_set(Main *bmain, Paint *paint, const int tool,
                                  const size_t tool_offset, const int ob_mode,
                                  const char *tool_name, const bool create_missing,
                                  const bool toggle)
{
	Brush *brush, *brush_orig = BKE_paint_brush(paint);

	if (toggle)
		brush = brush_tool_toggle(bmain, brush_orig, tool, tool_offset, ob_mode);
	else
		brush = brush_tool_cycle(bmain, brush_orig, tool, tool_offset, ob_mode);

	if (!brush && brush_tool(brush_orig, tool_offset) != tool && create_missing) {
		brush = BKE_brush_add(bmain, tool_name, ob_mode);
		brush_tool_set(brush, tool_offset, tool);
		brush->toggle_brush = brush_orig;
	}

	if (brush) {
		BKE_paint_brush_set(paint, brush);
		BKE_paint_invalidate_overlay_all();

		WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

/* used in the PAINT_OT_brush_select operator */
#define OB_MODE_ACTIVE 0

static int brush_select_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	ToolSettings *toolsettings = CTX_data_tool_settings(C);
	Paint *paint = NULL;
	int tool, paint_mode = RNA_enum_get(op->ptr, "paint_mode");
	const bool create_missing = RNA_boolean_get(op->ptr, "create_missing");
	const bool toggle = RNA_boolean_get(op->ptr, "toggle");
	const char *tool_name = "Brush";
	size_t tool_offset;

	if (paint_mode == OB_MODE_ACTIVE) {
		Object *ob = CTX_data_active_object(C);
		if (ob) {
			/* select current paint mode */
			paint_mode = ob->mode & OB_MODE_ALL_PAINT;
		}
		else {
			return OPERATOR_CANCELLED;
		}
	}

	switch (paint_mode) {
		case OB_MODE_SCULPT:
			paint = &toolsettings->sculpt->paint;
			tool_offset = offsetof(Brush, sculpt_tool);
			tool = RNA_enum_get(op->ptr, "sculpt_tool");
			RNA_enum_name_from_value(rna_enum_brush_sculpt_tool_items, tool, &tool_name);
			break;
		case OB_MODE_VERTEX_PAINT:
			paint = &toolsettings->vpaint->paint;
			tool_offset = offsetof(Brush, vertexpaint_tool);
			tool = RNA_enum_get(op->ptr, "vertex_paint_tool");
			RNA_enum_name_from_value(rna_enum_brush_vertex_tool_items, tool, &tool_name);
			break;
		case OB_MODE_WEIGHT_PAINT:
			paint = &toolsettings->wpaint->paint;
			/* vertexpaint_tool is used for weight paint mode */
			tool_offset = offsetof(Brush, vertexpaint_tool);
			tool = RNA_enum_get(op->ptr, "weight_paint_tool");
			RNA_enum_name_from_value(rna_enum_brush_vertex_tool_items, tool, &tool_name);
			break;
		case OB_MODE_TEXTURE_PAINT:
			paint = &toolsettings->imapaint.paint;
			tool_offset = offsetof(Brush, imagepaint_tool);
			tool = RNA_enum_get(op->ptr, "texture_paint_tool");
			RNA_enum_name_from_value(rna_enum_brush_image_tool_items, tool, &tool_name);
			break;
		default:
			/* invalid paint mode */
			return OPERATOR_CANCELLED;
	}

	return brush_generic_tool_set(bmain, paint, tool, tool_offset,
	                              paint_mode, tool_name, create_missing,
	                              toggle);
}

static void PAINT_OT_brush_select(wmOperatorType *ot)
{
	static EnumPropertyItem paint_mode_items[] = {
		{OB_MODE_ACTIVE, "ACTIVE", 0, "Current", "Set brush for active paint mode"},
		{OB_MODE_SCULPT, "SCULPT", ICON_SCULPTMODE_HLT, "Sculpt", ""},
		{OB_MODE_VERTEX_PAINT, "VERTEX_PAINT", ICON_VPAINT_HLT, "Vertex Paint", ""},
		{OB_MODE_WEIGHT_PAINT, "WEIGHT_PAINT", ICON_WPAINT_HLT, "Weight Paint", ""},
		{OB_MODE_TEXTURE_PAINT, "TEXTURE_PAINT", ICON_TPAINT_HLT, "Texture Paint", ""},
		{0, NULL, 0, NULL, NULL}
	};
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Brush Select";
	ot->description = "Select a paint mode's brush by tool type";
	ot->idname = "PAINT_OT_brush_select";

	/* api callbacks */
	ot->exec = brush_select_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_enum(ot->srna, "paint_mode", paint_mode_items, OB_MODE_ACTIVE, "Paint Mode", "");
	RNA_def_enum(ot->srna, "sculpt_tool", rna_enum_brush_sculpt_tool_items, 0, "Sculpt Tool", "");
	RNA_def_enum(ot->srna, "vertex_paint_tool", rna_enum_brush_vertex_tool_items, 0, "Vertex Paint Tool", "");
	RNA_def_enum(ot->srna, "weight_paint_tool", rna_enum_brush_vertex_tool_items, 0, "Weight Paint Tool", "");
	RNA_def_enum(ot->srna, "texture_paint_tool", rna_enum_brush_image_tool_items, 0, "Texture Paint Tool", "");

	prop = RNA_def_boolean(ot->srna, "toggle", 0, "Toggle", "Toggle between two brushes rather than cycling");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "create_missing", 0, "Create Missing", "If the requested brush type does not exist, create a new brush");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static wmKeyMapItem *keymap_brush_select(wmKeyMap *keymap, int paint_mode,
                                         int tool, int keymap_type,
                                         int keymap_modifier)
{
	wmKeyMapItem *kmi;
	kmi = WM_keymap_add_item(keymap, "PAINT_OT_brush_select",
	                         keymap_type, KM_PRESS, keymap_modifier, 0);

	RNA_enum_set(kmi->ptr, "paint_mode", paint_mode);
	
	switch (paint_mode) {
		case OB_MODE_SCULPT:
			RNA_enum_set(kmi->ptr, "sculpt_tool", tool);
			break;
		case OB_MODE_VERTEX_PAINT:
			RNA_enum_set(kmi->ptr, "vertex_paint_tool", tool);
			break;
		case OB_MODE_WEIGHT_PAINT:
			RNA_enum_set(kmi->ptr, "weight_paint_tool", tool);
			break;
		case OB_MODE_TEXTURE_PAINT:
			RNA_enum_set(kmi->ptr, "texture_paint_tool", tool);
			break;
	}

	return kmi;
}

static int brush_uv_sculpt_tool_set_exec(bContext *C, wmOperator *op)
{
	Brush *brush;
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = scene->toolsettings;
	ts->uv_sculpt_tool = RNA_enum_get(op->ptr, "tool");
	brush = ts->uvsculpt->paint.brush;
	/* To update toolshelf */
	WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);

	return OPERATOR_FINISHED;
}

static void BRUSH_OT_uv_sculpt_tool_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "UV Sculpt Tool Set";
	ot->description = "Set the UV sculpt tool";
	ot->idname = "BRUSH_OT_uv_sculpt_tool_set";

	/* api callbacks */
	ot->exec = brush_uv_sculpt_tool_set_exec;
	ot->poll = uv_sculpt_poll;

	/* flags */
	ot->flag = 0;

	/* props */
	ot->prop = RNA_def_enum(ot->srna, "tool", rna_enum_uv_sculpt_tool_items, 0, "Tool", "");
}

/***** Stencil Control *****/

typedef enum {
	STENCIL_TRANSLATE,
	STENCIL_SCALE,
	STENCIL_ROTATE
} StencilControlMode;

typedef enum {
	STENCIL_PRIMARY = 0,
	STENCIL_SECONDARY = 1
} StencilTextureMode;


typedef enum {
	STENCIL_CONSTRAINT_X = 1,
	STENCIL_CONSTRAINT_Y = 2
} StencilConstraint;

typedef struct {
	float init_mouse[2];
	float init_spos[2];
	float init_sdim[2];
	float init_rot;
	float init_angle;
	float lenorig;
	float area_size[2];
	StencilControlMode mode;
	StencilConstraint constrain_mode;
	int mask; /* we are twaking mask or colour stencil */
	Brush *br;
	float *dim_target;
	float *rot_target;
	float *pos_target;
	short event_type;
} StencilControlData;

static void stencil_set_target(StencilControlData *scd)
{
	Brush *br = scd->br;
	float mdiff[2];
	if (scd->mask) {
		copy_v2_v2(scd->init_sdim, br->mask_stencil_dimension);
		copy_v2_v2(scd->init_spos, br->mask_stencil_pos);
		scd->init_rot = br->mask_mtex.rot;

		scd->dim_target = br->mask_stencil_dimension;
		scd->rot_target = &br->mask_mtex.rot;
		scd->pos_target = br->mask_stencil_pos;

		sub_v2_v2v2(mdiff, scd->init_mouse, br->mask_stencil_pos);
	}
	else {
		copy_v2_v2(scd->init_sdim, br->stencil_dimension);
		copy_v2_v2(scd->init_spos, br->stencil_pos);
		scd->init_rot = br->mtex.rot;

		scd->dim_target = br->stencil_dimension;
		scd->rot_target = &br->mtex.rot;
		scd->pos_target = br->stencil_pos;

		sub_v2_v2v2(mdiff, scd->init_mouse, br->stencil_pos);
	}

	scd->lenorig = len_v2(mdiff);

	scd->init_angle = atan2f(mdiff[1], mdiff[0]);
}

static int stencil_control_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Paint *paint = BKE_paint_get_active_from_context(C);
	Brush *br = BKE_paint_brush(paint);
	float mvalf[2] = {event->mval[0], event->mval[1]};
	ARegion *ar = CTX_wm_region(C);
	StencilControlData *scd;
	int mask = RNA_enum_get(op->ptr, "texmode");

	if (mask) {
		if (br->mask_mtex.brush_map_mode != MTEX_MAP_MODE_STENCIL)
			return OPERATOR_CANCELLED;
	}
	else {
		if (br->mtex.brush_map_mode != MTEX_MAP_MODE_STENCIL)
			return OPERATOR_CANCELLED;
	}

	scd = MEM_mallocN(sizeof(StencilControlData), "stencil_control");
	scd->mask = mask;
	scd->br = br;

	copy_v2_v2(scd->init_mouse, mvalf);

	stencil_set_target(scd);

	scd->mode = RNA_enum_get(op->ptr, "mode");
	scd->event_type = event->type;
	scd->area_size[0] = ar->winx;
	scd->area_size[1] = ar->winy;


	op->customdata = scd;
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static void stencil_restore(StencilControlData *scd)
{
	copy_v2_v2(scd->dim_target, scd->init_sdim);
	copy_v2_v2(scd->pos_target, scd->init_spos);
	*scd->rot_target = scd->init_rot;
}

static void stencil_control_cancel(bContext *UNUSED(C), wmOperator *op)
{
	StencilControlData *scd = op->customdata;

	stencil_restore(scd);
	MEM_freeN(op->customdata);
}

static void stencil_control_calculate(StencilControlData *scd, const int mval[2])
{
#define PIXEL_MARGIN 5

	float mdiff[2];
	float mvalf[2] = {mval[0], mval[1]};
	switch (scd->mode) {
		case STENCIL_TRANSLATE:
			sub_v2_v2v2(mdiff, mvalf, scd->init_mouse);
			add_v2_v2v2(scd->pos_target, scd->init_spos,
			            mdiff);
			CLAMP(scd->pos_target[0],
			      -scd->dim_target[0] + PIXEL_MARGIN,
			      scd->area_size[0] + scd->dim_target[0] - PIXEL_MARGIN);

			CLAMP(scd->pos_target[1],
			      -scd->dim_target[1] + PIXEL_MARGIN,
			      scd->area_size[1] + scd->dim_target[1] - PIXEL_MARGIN);

			break;
		case STENCIL_SCALE:
		{
			float len, factor;
			sub_v2_v2v2(mdiff, mvalf, scd->pos_target);
			len = len_v2(mdiff);
			factor = len / scd->lenorig;
			copy_v2_v2(mdiff, scd->init_sdim);
			if (scd->constrain_mode != STENCIL_CONSTRAINT_Y)
				mdiff[0] = factor * scd->init_sdim[0];
			if (scd->constrain_mode != STENCIL_CONSTRAINT_X)
				mdiff[1] = factor * scd->init_sdim[1];
			CLAMP(mdiff[0], 5.0f, 10000.0f);
			CLAMP(mdiff[1], 5.0f, 10000.0f);
			copy_v2_v2(scd->dim_target, mdiff);
			break;
		}
		case STENCIL_ROTATE:
		{
			float angle;
			sub_v2_v2v2(mdiff, mvalf, scd->pos_target);
			angle = atan2f(mdiff[1], mdiff[0]);
			angle = scd->init_rot + angle - scd->init_angle;
			if (angle < 0.0f)
				angle += (float)(2 * M_PI);
			if (angle > (float)(2 * M_PI))
				angle -= (float)(2 * M_PI);
			*scd->rot_target = angle;
			break;
		}
	}
#undef PIXEL_MARGIN
}

static int stencil_control_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	StencilControlData *scd = op->customdata;

	if (event->type == scd->event_type && event->val == KM_RELEASE) {
		MEM_freeN(op->customdata);
		WM_event_add_notifier(C, NC_WINDOW, NULL);
		return OPERATOR_FINISHED;
	}

	switch (event->type) {
		case MOUSEMOVE:
			stencil_control_calculate(scd, event->mval);
			break;
		case ESCKEY:
			if (event->val == KM_PRESS) {
				stencil_control_cancel(C, op);
				WM_event_add_notifier(C, NC_WINDOW, NULL);
				return OPERATOR_CANCELLED;
			}
			break;
		case XKEY:
			if (event->val == KM_PRESS) {

				if (scd->constrain_mode == STENCIL_CONSTRAINT_X)
					scd->constrain_mode = 0;
				else
					scd->constrain_mode = STENCIL_CONSTRAINT_X;

				stencil_control_calculate(scd, event->mval);
			}
			break;
		case YKEY:
			if (event->val == KM_PRESS) {
				if (scd->constrain_mode == STENCIL_CONSTRAINT_Y)
					scd->constrain_mode = 0;
				else
					scd->constrain_mode = STENCIL_CONSTRAINT_Y;

				stencil_control_calculate(scd, event->mval);
			}
			break;
		default:
			break;
	}

	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_RUNNING_MODAL;
}

static int stencil_control_poll(bContext *C)
{
	PaintMode mode = BKE_paintmode_get_active_from_context(C);

	Paint *paint;
	Brush *br;

	if (!paint_supports_texture(mode))
		return false;

	paint = BKE_paint_get_active_from_context(C);
	br = BKE_paint_brush(paint);
	return (br &&
	        (br->mtex.brush_map_mode == MTEX_MAP_MODE_STENCIL ||
	         br->mask_mtex.brush_map_mode == MTEX_MAP_MODE_STENCIL));
}

static void BRUSH_OT_stencil_control(wmOperatorType *ot)
{
	static EnumPropertyItem stencil_control_items[] = {
		{STENCIL_TRANSLATE, "TRANSLATION", 0, "Translation", ""},
		{STENCIL_SCALE, "SCALE", 0, "Scale", ""},
		{STENCIL_ROTATE, "ROTATION", 0, "Rotation", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem stencil_texture_items[] = {
		{STENCIL_PRIMARY, "PRIMARY", 0, "Primary", ""},
		{STENCIL_SECONDARY, "SECONDARY", 0, "Secondary", ""},
		{0, NULL, 0, NULL, NULL}
	};
	/* identifiers */
	ot->name = "Stencil Brush Control";
	ot->description = "Control the stencil brush";
	ot->idname = "BRUSH_OT_stencil_control";

	/* api callbacks */
	ot->invoke = stencil_control_invoke;
	ot->modal = stencil_control_modal;
	ot->cancel = stencil_control_cancel;
	ot->poll = stencil_control_poll;

	/* flags */
	ot->flag = 0;

	RNA_def_enum(ot->srna, "mode", stencil_control_items, STENCIL_TRANSLATE, "Tool", "");
	RNA_def_enum(ot->srna, "texmode", stencil_texture_items, STENCIL_PRIMARY, "Tool", "");
}


static int stencil_fit_image_aspect_exec(bContext *C, wmOperator *op)
{
	Paint *paint = BKE_paint_get_active_from_context(C);
	Brush *br = BKE_paint_brush(paint);
	bool use_scale = RNA_boolean_get(op->ptr, "use_scale");
	bool use_repeat = RNA_boolean_get(op->ptr, "use_repeat");
	bool do_mask = RNA_boolean_get(op->ptr, "mask");
	Tex *tex = NULL;
	MTex *mtex = NULL;
	if (br) {
		mtex = do_mask ? &br->mask_mtex : &br->mtex;
		tex = mtex->tex;
	}

	if (tex && tex->type == TEX_IMAGE && tex->ima) {
		float aspx, aspy;
		Image *ima = tex->ima;
		float orig_area, stencil_area, factor;
		ED_image_get_uv_aspect(ima, NULL, &aspx, &aspy);

		if (use_scale) {
			aspx *= mtex->size[0];
			aspy *= mtex->size[1];
		}

		if (use_repeat && tex->extend == TEX_REPEAT) {
			aspx *= tex->xrepeat;
			aspy *= tex->yrepeat;
		}

		orig_area = aspx * aspy;

		if (do_mask) {
			stencil_area = br->mask_stencil_dimension[0] * br->mask_stencil_dimension[1];
		}
		else {
			stencil_area = br->stencil_dimension[0] * br->stencil_dimension[1];
		}

		factor = sqrtf(stencil_area / orig_area);

		if (do_mask) {
			br->mask_stencil_dimension[0] = factor * aspx;
			br->mask_stencil_dimension[1] = factor * aspy;

		}
		else {
			br->stencil_dimension[0] = factor * aspx;
			br->stencil_dimension[1] = factor * aspy;
		}
	}

	WM_event_add_notifier(C, NC_WINDOW, NULL);

	return OPERATOR_FINISHED;
}


static void BRUSH_OT_stencil_fit_image_aspect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Image Aspect";
	ot->description = "When using an image texture, adjust the stencil size to fit the image aspect ratio";
	ot->idname = "BRUSH_OT_stencil_fit_image_aspect";

	/* api callbacks */
	ot->exec = stencil_fit_image_aspect_exec;
	ot->poll = stencil_control_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "use_repeat", 1, "Use Repeat", "Use repeat mapping values");
	RNA_def_boolean(ot->srna, "use_scale", 1, "Use Scale", "Use texture scale values");
	RNA_def_boolean(ot->srna, "mask", 0, "Modify Mask Stencil", "Modify either the primary or mask stencil");
}


static int stencil_reset_transform_exec(bContext *C, wmOperator *op)
{
	Paint *paint = BKE_paint_get_active_from_context(C);
	Brush *br = BKE_paint_brush(paint);
	bool do_mask = RNA_boolean_get(op->ptr, "mask");

	if (!br)
		return OPERATOR_CANCELLED;
	
	if (do_mask) {
		br->mask_stencil_pos[0] = 256;
		br->mask_stencil_pos[1] = 256;

		br->mask_stencil_dimension[0] = 256;
		br->mask_stencil_dimension[1] = 256;

		br->mask_mtex.rot = 0;
	}
	else {
		br->stencil_pos[0] = 256;
		br->stencil_pos[1] = 256;

		br->stencil_dimension[0] = 256;
		br->stencil_dimension[1] = 256;

		br->mtex.rot = 0;
	}

	WM_event_add_notifier(C, NC_WINDOW, NULL);

	return OPERATOR_FINISHED;
}


static void BRUSH_OT_stencil_reset_transform(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reset Transform";
	ot->description = "Reset the stencil transformation to the default";
	ot->idname = "BRUSH_OT_stencil_reset_transform";

	/* api callbacks */
	ot->exec = stencil_reset_transform_exec;
	ot->poll = stencil_control_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "mask", 0, "Modify Mask Stencil", "Modify either the primary or mask stencil");
}


static void ed_keymap_stencil(wmKeyMap *keymap)
{
	wmKeyMapItem *kmi;

	kmi = WM_keymap_add_item(keymap, "BRUSH_OT_stencil_control", RIGHTMOUSE, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "mode", STENCIL_TRANSLATE);
	kmi = WM_keymap_add_item(keymap, "BRUSH_OT_stencil_control", RIGHTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "mode", STENCIL_SCALE);
	kmi = WM_keymap_add_item(keymap, "BRUSH_OT_stencil_control", RIGHTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "mode", STENCIL_ROTATE);

	kmi = WM_keymap_add_item(keymap, "BRUSH_OT_stencil_control", RIGHTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "mode", STENCIL_TRANSLATE);
	RNA_enum_set(kmi->ptr, "texmode", STENCIL_SECONDARY);
	kmi = WM_keymap_add_item(keymap, "BRUSH_OT_stencil_control", RIGHTMOUSE, KM_PRESS, KM_SHIFT | KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "texmode", STENCIL_SECONDARY);
	RNA_enum_set(kmi->ptr, "mode", STENCIL_SCALE);
	kmi = WM_keymap_add_item(keymap, "BRUSH_OT_stencil_control", RIGHTMOUSE, KM_PRESS, KM_CTRL | KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "texmode", STENCIL_SECONDARY);
	RNA_enum_set(kmi->ptr, "mode", STENCIL_ROTATE);
}

/**************************** registration **********************************/

void ED_operatormacros_paint(void)
{
	wmOperatorType *ot;
	wmOperatorTypeMacro *otmacro;

	ot = WM_operatortype_append_macro("PAINTCURVE_OT_add_point_slide", "Add Curve Point and Slide",
	                                  "Add new curve point and slide it", OPTYPE_UNDO);
	ot->description = "Add new curve point and slide it";
	WM_operatortype_macro_define(ot, "PAINTCURVE_OT_add_point");
	otmacro = WM_operatortype_macro_define(ot, "PAINTCURVE_OT_slide");
	RNA_boolean_set(otmacro->ptr, "align", true);
	RNA_boolean_set(otmacro->ptr, "select", false);
}


void ED_operatortypes_paint(void)
{
	/* palette */
	WM_operatortype_append(PALETTE_OT_new);
	WM_operatortype_append(PALETTE_OT_color_add);
	WM_operatortype_append(PALETTE_OT_color_delete);

	/* paint curve */
	WM_operatortype_append(PAINTCURVE_OT_new);
	WM_operatortype_append(PAINTCURVE_OT_add_point);
	WM_operatortype_append(PAINTCURVE_OT_delete_point);
	WM_operatortype_append(PAINTCURVE_OT_select);
	WM_operatortype_append(PAINTCURVE_OT_slide);
	WM_operatortype_append(PAINTCURVE_OT_draw);
	WM_operatortype_append(PAINTCURVE_OT_cursor);

	/* brush */
	WM_operatortype_append(BRUSH_OT_add);
	WM_operatortype_append(BRUSH_OT_scale_size);
	WM_operatortype_append(BRUSH_OT_curve_preset);
	WM_operatortype_append(BRUSH_OT_reset);
	WM_operatortype_append(BRUSH_OT_stencil_control);
	WM_operatortype_append(BRUSH_OT_stencil_fit_image_aspect);
	WM_operatortype_append(BRUSH_OT_stencil_reset_transform);

	/* note, particle uses a different system, can be added with existing operators in wm.py */
	WM_operatortype_append(PAINT_OT_brush_select);
	WM_operatortype_append(BRUSH_OT_uv_sculpt_tool_set);

	/* image */
	WM_operatortype_append(PAINT_OT_texture_paint_toggle);
	WM_operatortype_append(PAINT_OT_image_paint);
	WM_operatortype_append(PAINT_OT_sample_color);
	WM_operatortype_append(PAINT_OT_grab_clone);
	WM_operatortype_append(PAINT_OT_project_image);
	WM_operatortype_append(PAINT_OT_image_from_view);
	WM_operatortype_append(PAINT_OT_brush_colors_flip);
	WM_operatortype_append(PAINT_OT_add_texture_paint_slot);
	WM_operatortype_append(PAINT_OT_delete_texture_paint_slot);
	WM_operatortype_append(PAINT_OT_add_simple_uvs);

	/* weight */
	WM_operatortype_append(PAINT_OT_weight_paint_toggle);
	WM_operatortype_append(PAINT_OT_weight_paint);
	WM_operatortype_append(PAINT_OT_weight_set);
	WM_operatortype_append(PAINT_OT_weight_from_bones);
	WM_operatortype_append(PAINT_OT_weight_gradient);
	WM_operatortype_append(PAINT_OT_weight_sample);
	WM_operatortype_append(PAINT_OT_weight_sample_group);

	/* uv */
	WM_operatortype_append(SCULPT_OT_uv_sculpt_stroke);

	/* vertex selection */
	WM_operatortype_append(PAINT_OT_vert_select_all);
	WM_operatortype_append(PAINT_OT_vert_select_ungrouped);

	/* vertex */
	WM_operatortype_append(PAINT_OT_vertex_paint_toggle);
	WM_operatortype_append(PAINT_OT_vertex_paint);
	WM_operatortype_append(PAINT_OT_vertex_color_set);
	WM_operatortype_append(PAINT_OT_vertex_color_smooth);

	WM_operatortype_append(PAINT_OT_vertex_color_brightness_contrast);
	WM_operatortype_append(PAINT_OT_vertex_color_hsv);
	WM_operatortype_append(PAINT_OT_vertex_color_invert);
	WM_operatortype_append(PAINT_OT_vertex_color_levels);

	/* face-select */
	WM_operatortype_append(PAINT_OT_face_select_linked);
	WM_operatortype_append(PAINT_OT_face_select_linked_pick);
	WM_operatortype_append(PAINT_OT_face_select_all);
	WM_operatortype_append(PAINT_OT_face_select_hide);
	WM_operatortype_append(PAINT_OT_face_select_reveal);

	/* partial visibility */
	WM_operatortype_append(PAINT_OT_hide_show);

	/* paint masking */
	WM_operatortype_append(PAINT_OT_mask_flood_fill);
	WM_operatortype_append(PAINT_OT_mask_lasso_gesture);
}


static void ed_keymap_paint_brush_switch(wmKeyMap *keymap, const char *mode)
{
	wmKeyMapItem *kmi;
	int i;
	/* index 0-9 (zero key is tenth), shift key for index 10-19 */
	for (i = 0; i < 20; i++) {
		kmi = WM_keymap_add_item(keymap, "BRUSH_OT_active_index_set",
		                         ZEROKEY + ((i + 1) % 10), KM_PRESS, i < 10 ? 0 : KM_SHIFT, 0);
		RNA_string_set(kmi->ptr, "mode", mode);
		RNA_int_set(kmi->ptr, "index", i);
	}
}

static void ed_keymap_paint_brush_size(wmKeyMap *keymap, const char *UNUSED(path))
{
	wmKeyMapItem *kmi;

	kmi = WM_keymap_add_item(keymap, "BRUSH_OT_scale_size", LEFTBRACKETKEY, KM_PRESS, 0, 0);
	RNA_float_set(kmi->ptr, "scalar", 0.9);

	kmi = WM_keymap_add_item(keymap, "BRUSH_OT_scale_size", RIGHTBRACKETKEY, KM_PRESS, 0, 0);
	RNA_float_set(kmi->ptr, "scalar", 10.0 / 9.0); // 1.1111....
}

static void set_brush_rc_path(PointerRNA *ptr, const char *brush_path,
                              const char *output_name, const char *input_name)
{
	char *path;

	path = BLI_sprintfN("%s.%s", brush_path, input_name);
	RNA_string_set(ptr, output_name, path);
	MEM_freeN(path);
}

void set_brush_rc_props(PointerRNA *ptr, const char *paint,
                        const char *prop, const char *secondary_prop,
                        RCFlags flags)
{
	const char *ups_path = "tool_settings.unified_paint_settings";
	char *brush_path;

	brush_path = BLI_sprintfN("tool_settings.%s.brush", paint);

	set_brush_rc_path(ptr, brush_path, "data_path_primary", prop);
	if (secondary_prop) {
		set_brush_rc_path(ptr, ups_path, "use_secondary", secondary_prop);
		set_brush_rc_path(ptr, ups_path, "data_path_secondary", prop);
	}
	else {
		RNA_string_set(ptr, "use_secondary", "");
		RNA_string_set(ptr, "data_path_secondary", "");
	}
	set_brush_rc_path(ptr, brush_path, "color_path", "cursor_color_add");
	if (flags & RC_SECONDARY_ROTATION)
		set_brush_rc_path(ptr, brush_path, "rotation_path", "mask_texture_slot.angle");
	else
		set_brush_rc_path(ptr, brush_path, "rotation_path", "texture_slot.angle");
	RNA_string_set(ptr, "image_id", brush_path);

	if (flags & RC_COLOR) {
		set_brush_rc_path(ptr, brush_path, "fill_color_path", "color");
	}
	else {
		RNA_string_set(ptr, "fill_color_path", "");
	}

	if (flags & RC_COLOR_OVERRIDE) {
		RNA_string_set(ptr, "fill_color_override_path", "tool_settings.unified_paint_settings.color");
		RNA_string_set(ptr, "fill_color_override_test_path", "tool_settings.unified_paint_settings.use_unified_color");
	}
	else {
		RNA_string_set(ptr, "fill_color_override_path", "");
		RNA_string_set(ptr, "fill_color_override_test_path", "");
	}

	if (flags & RC_ZOOM)
		RNA_string_set(ptr, "zoom_path", "space_data.zoom");
	else
		RNA_string_set(ptr, "zoom_path", "");

	RNA_boolean_set(ptr, "secondary_tex", (flags & RC_SECONDARY_ROTATION) != 0);

	MEM_freeN(brush_path);
}

static void ed_keymap_paint_brush_radial_control(wmKeyMap *keymap, const char *paint,
                                                 RCFlags flags)
{
	wmKeyMapItem *kmi;
	/* only size needs to follow zoom, strength shows fixed size circle */
	int flags_nozoom = flags & (~RC_ZOOM);
	int flags_noradial_secondary = flags & (~(RC_SECONDARY_ROTATION | RC_ZOOM));

	kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, 0, 0);
	set_brush_rc_props(kmi->ptr, paint, "size", "use_unified_size", flags);

	kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, KM_SHIFT, 0);
	set_brush_rc_props(kmi->ptr, paint, "strength", "use_unified_strength", flags_nozoom);

	if (flags & RC_WEIGHT) {
		kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", WKEY, KM_PRESS, 0, 0);
		set_brush_rc_props(kmi->ptr, paint, "weight", "use_unified_weight", flags_nozoom);
	}

	if (flags & RC_ROTATION) {
		kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, KM_CTRL, 0);
		set_brush_rc_props(kmi->ptr, paint, "texture_slot.angle", NULL, flags_noradial_secondary);
	}

	if (flags & RC_SECONDARY_ROTATION) {
		kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
		set_brush_rc_props(kmi->ptr, paint, "mask_texture_slot.angle", NULL, flags_nozoom);
	}
}

static void paint_partial_visibility_keys(wmKeyMap *keymap)
{
	wmKeyMapItem *kmi;
	
	/* Partial visibility */
	kmi = WM_keymap_add_item(keymap, "PAINT_OT_hide_show", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "action", PARTIALVIS_SHOW);
	RNA_enum_set(kmi->ptr, "area", PARTIALVIS_INSIDE);
	kmi = WM_keymap_add_item(keymap, "PAINT_OT_hide_show", HKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", PARTIALVIS_HIDE);
	RNA_enum_set(kmi->ptr, "area", PARTIALVIS_INSIDE);
	kmi = WM_keymap_add_item(keymap, "PAINT_OT_hide_show", HKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "action", PARTIALVIS_SHOW);
	RNA_enum_set(kmi->ptr, "area", PARTIALVIS_ALL);
}


static void paint_keymap_curve(wmKeyMap *keymap)
{
	wmKeyMapItem *kmi;

	WM_keymap_add_item(keymap, "PAINTCURVE_OT_add_point_slide", ACTIONMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "PAINTCURVE_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "PAINTCURVE_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	WM_keymap_add_item(keymap, "PAINTCURVE_OT_slide", ACTIONMOUSE, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "PAINTCURVE_OT_slide", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "align", true);
	kmi = WM_keymap_add_item(keymap, "PAINTCURVE_OT_select", AKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "toggle", true);

	WM_keymap_add_item(keymap, "PAINTCURVE_OT_cursor", ACTIONMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "PAINTCURVE_OT_delete_point", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "PAINTCURVE_OT_delete_point", DELKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "PAINTCURVE_OT_draw", RETKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "PAINTCURVE_OT_draw", PADENTER, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "TRANSFORM_OT_translate", GKEY, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_translate", EVT_TWEAK_S, KM_ANY, 0, 0);
	WM_keymap_add_item(keymap, "TRANSFORM_OT_rotate", RKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "TRANSFORM_OT_resize", SKEY, KM_PRESS, 0, 0);
}

void ED_keymap_paint(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	int i;
	
	keymap = WM_keymap_find(keyconf, "Paint Curve", 0, 0);
	keymap->poll = paint_curve_poll;

	paint_keymap_curve(keymap);

	/* Sculpt mode */
	keymap = WM_keymap_find(keyconf, "Sculpt", 0, 0);
	keymap->poll = sculpt_mode_poll;

	RNA_enum_set(WM_keymap_add_item(keymap, "SCULPT_OT_brush_stroke", LEFTMOUSE, KM_PRESS, 0,        0)->ptr, "mode", BRUSH_STROKE_NORMAL);
	RNA_enum_set(WM_keymap_add_item(keymap, "SCULPT_OT_brush_stroke", LEFTMOUSE, KM_PRESS, KM_CTRL,  0)->ptr, "mode", BRUSH_STROKE_INVERT);
	RNA_enum_set(WM_keymap_add_item(keymap, "SCULPT_OT_brush_stroke", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "mode", BRUSH_STROKE_SMOOTH);

	/* Partial visibility, sculpt-only for now */
	paint_partial_visibility_keys(keymap);

	for (i = 0; i <= 5; i++)
		RNA_int_set(WM_keymap_add_item(keymap, "OBJECT_OT_subdivision_set", ZEROKEY + i, KM_PRESS, KM_CTRL, 0)->ptr, "level", i);

	/* Clear mask */
	kmi = WM_keymap_add_item(keymap, "PAINT_OT_mask_flood_fill", MKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "mode", PAINT_MASK_FLOOD_VALUE);
	RNA_float_set(kmi->ptr, "value", 0);

	/* Invert mask */
	kmi = WM_keymap_add_item(keymap, "PAINT_OT_mask_flood_fill", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "mode", PAINT_MASK_INVERT);

	WM_keymap_add_item(keymap, "PAINT_OT_mask_lasso_gesture", LEFTMOUSE, KM_PRESS, KM_CTRL | KM_SHIFT, 0);

	/* Toggle dynamic topology */
	WM_keymap_add_item(keymap, "SCULPT_OT_dynamic_topology_toggle", DKEY, KM_PRESS, KM_CTRL, 0);

	/* Dynamic-topology detail size
	 * 
	 * This should be improved further, perhaps by showing a triangle
	 * grid rather than brush alpha */
	kmi = WM_keymap_add_item(keymap, "SCULPT_OT_set_detail_size", DKEY, KM_PRESS, KM_SHIFT, 0);

	/* multires switch */
	kmi = WM_keymap_add_item(keymap, "OBJECT_OT_subdivision_set", PAGEUPKEY, KM_PRESS, 0, 0);
	RNA_int_set(kmi->ptr, "level", 1);
	RNA_boolean_set(kmi->ptr, "relative", true);

	kmi = WM_keymap_add_item(keymap, "OBJECT_OT_subdivision_set", PAGEDOWNKEY, KM_PRESS, 0, 0);
	RNA_int_set(kmi->ptr, "level", -1);
	RNA_boolean_set(kmi->ptr, "relative", true);

	ed_keymap_paint_brush_switch(keymap, "sculpt");
	ed_keymap_paint_brush_size(keymap, "tool_settings.sculpt.brush.size");
	ed_keymap_paint_brush_radial_control(keymap, "sculpt", RC_ROTATION);

	ed_keymap_stencil(keymap);

	keymap_brush_select(keymap, OB_MODE_SCULPT, SCULPT_TOOL_DRAW, XKEY, 0);
	keymap_brush_select(keymap, OB_MODE_SCULPT, SCULPT_TOOL_SMOOTH, SKEY, 0);
	keymap_brush_select(keymap, OB_MODE_SCULPT, SCULPT_TOOL_PINCH, PKEY, 0);
	keymap_brush_select(keymap, OB_MODE_SCULPT, SCULPT_TOOL_INFLATE, IKEY, 0);
	keymap_brush_select(keymap, OB_MODE_SCULPT, SCULPT_TOOL_GRAB, GKEY, 0);
	keymap_brush_select(keymap, OB_MODE_SCULPT, SCULPT_TOOL_LAYER, LKEY, 0);
	keymap_brush_select(keymap, OB_MODE_SCULPT, SCULPT_TOOL_FLATTEN, TKEY, KM_SHIFT);
	keymap_brush_select(keymap, OB_MODE_SCULPT, SCULPT_TOOL_CLAY, CKEY, 0);
	keymap_brush_select(keymap, OB_MODE_SCULPT, SCULPT_TOOL_CREASE, CKEY, KM_SHIFT);
	keymap_brush_select(keymap, OB_MODE_SCULPT, SCULPT_TOOL_SNAKE_HOOK, KKEY, 0);
	kmi = keymap_brush_select(keymap, OB_MODE_SCULPT, SCULPT_TOOL_MASK, MKEY, 0);
	RNA_boolean_set(kmi->ptr, "toggle", 1);
	RNA_boolean_set(kmi->ptr, "create_missing", 1);

	/* */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_menu_enum", EKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.sculpt.brush.stroke_method");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", SKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.sculpt.brush.use_smooth_stroke");

	WM_keymap_add_menu(keymap, "VIEW3D_MT_angle_control", RKEY, KM_PRESS, 0, 0);

	/* Vertex Paint mode */
	keymap = WM_keymap_find(keyconf, "Vertex Paint", 0, 0);
	keymap->poll = vertex_paint_mode_poll;

	WM_keymap_verify_item(keymap, "PAINT_OT_vertex_paint", LEFTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "PAINT_OT_sample_color", SKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap,
	                   "PAINT_OT_vertex_color_set", KKEY, KM_PRESS, KM_SHIFT, 0);

	ed_keymap_paint_brush_switch(keymap, "vertex_paint");
	ed_keymap_paint_brush_size(keymap, "tool_settings.vertex_paint.brush.size");
	ed_keymap_paint_brush_radial_control(keymap, "vertex_paint", RC_COLOR | RC_COLOR_OVERRIDE | RC_ROTATION);

	ed_keymap_stencil(keymap);

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", MKEY, KM_PRESS, 0, 0); /* mask toggle */
	RNA_string_set(kmi->ptr, "data_path", "vertex_paint_object.data.use_paint_mask");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", SKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.vertex_paint.brush.use_smooth_stroke");

	WM_keymap_add_menu(keymap, "VIEW3D_MT_angle_control", RKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_menu_enum", EKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.vertex_paint.brush.stroke_method");

	/* Weight Paint mode */
	keymap = WM_keymap_find(keyconf, "Weight Paint", 0, 0);
	keymap->poll = weight_paint_mode_poll;

	WM_keymap_verify_item(keymap, "PAINT_OT_weight_paint", LEFTMOUSE, KM_PRESS, 0, 0);

	/* these keys are from 2.4x but could be changed */
	WM_keymap_verify_item(keymap, "PAINT_OT_weight_sample", ACTIONMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "PAINT_OT_weight_sample_group", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0);

	RNA_enum_set(WM_keymap_add_item(keymap, "PAINT_OT_weight_gradient", LEFTMOUSE, KM_PRESS, KM_ALT, 0)->ptr,           "type", WPAINT_GRADIENT_TYPE_LINEAR);
	RNA_enum_set(WM_keymap_add_item(keymap, "PAINT_OT_weight_gradient", LEFTMOUSE, KM_PRESS, KM_ALT | KM_CTRL, 0)->ptr, "type", WPAINT_GRADIENT_TYPE_RADIAL);

	WM_keymap_add_item(keymap,
	                   "PAINT_OT_weight_set", KKEY, KM_PRESS, KM_SHIFT, 0);

	ed_keymap_paint_brush_switch(keymap, "weight_paint");
	ed_keymap_paint_brush_size(keymap, "tool_settings.weight_paint.brush.size");
	ed_keymap_paint_brush_radial_control(keymap, "weight_paint", RC_WEIGHT);

	ed_keymap_stencil(keymap);

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_menu_enum", EKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.vertex_paint.brush.stroke_method");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", MKEY, KM_PRESS, 0, 0); /* face mask toggle */
	RNA_string_set(kmi->ptr, "data_path", "weight_paint_object.data.use_paint_mask");

	/* note, conflicts with vertex paint, but this is more useful */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", VKEY, KM_PRESS, 0, 0); /* vert mask toggle */
	RNA_string_set(kmi->ptr, "data_path", "weight_paint_object.data.use_paint_mask_vertex");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", SKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.weight_paint.brush.use_smooth_stroke");

	/*Weight paint's Vertex Selection Mode */
	keymap = WM_keymap_find(keyconf, "Weight Paint Vertex Selection", 0, 0);
	keymap->poll = vert_paint_poll;
	kmi = WM_keymap_add_item(keymap, "PAINT_OT_vert_select_all", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_TOGGLE);
	kmi = WM_keymap_add_item(keymap, "PAINT_OT_vert_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);
	WM_keymap_add_item(keymap, "VIEW3D_OT_select_border", BKEY, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_SHIFT | KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "deselect", true);
	WM_keymap_add_item(keymap, "VIEW3D_OT_select_circle", CKEY, KM_PRESS, 0, 0);

	/* Image/Texture Paint mode */
	keymap = WM_keymap_find(keyconf, "Image Paint", 0, 0);
	keymap->poll = image_texture_paint_poll;

	RNA_enum_set(WM_keymap_add_item(keymap, "PAINT_OT_image_paint", LEFTMOUSE, KM_PRESS, 0,        0)->ptr, "mode", BRUSH_STROKE_NORMAL);
	RNA_enum_set(WM_keymap_add_item(keymap, "PAINT_OT_image_paint", LEFTMOUSE, KM_PRESS, KM_CTRL,  0)->ptr, "mode", BRUSH_STROKE_INVERT);
	WM_keymap_add_item(keymap, "PAINT_OT_brush_colors_flip", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "PAINT_OT_grab_clone", RIGHTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "PAINT_OT_sample_color", SKEY, KM_PRESS, 0, 0);

	ed_keymap_paint_brush_switch(keymap, "image_paint");
	ed_keymap_paint_brush_size(keymap, "tool_settings.image_paint.brush.size");
	ed_keymap_paint_brush_radial_control(
	        keymap, "image_paint",
	        RC_COLOR | RC_COLOR_OVERRIDE | RC_ZOOM | RC_ROTATION | RC_SECONDARY_ROTATION);

	ed_keymap_stencil(keymap);

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", MKEY, KM_PRESS, 0, 0); /* mask toggle */
	RNA_string_set(kmi->ptr, "data_path", "image_paint_object.data.use_paint_mask");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", SKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.image_paint.brush.use_smooth_stroke");

	WM_keymap_add_menu(keymap, "VIEW3D_MT_angle_control", RKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_menu_enum", EKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.image_paint.brush.stroke_method");

	/* face-mask mode */
	keymap = WM_keymap_find(keyconf, "Face Mask", 0, 0);
	keymap->poll = facemask_paint_poll;

	kmi = WM_keymap_add_item(keymap, "PAINT_OT_face_select_all", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_TOGGLE);
	kmi = WM_keymap_add_item(keymap, "PAINT_OT_face_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);
	kmi = WM_keymap_add_item(keymap, "PAINT_OT_face_select_hide", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", false);
	kmi = WM_keymap_add_item(keymap, "PAINT_OT_face_select_hide", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", true);
	WM_keymap_add_item(keymap, "PAINT_OT_face_select_reveal", HKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "PAINT_OT_face_select_linked", LKEY, KM_PRESS, KM_CTRL, 0);
	kmi = WM_keymap_add_item(keymap, "PAINT_OT_face_select_linked_pick", LKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	kmi = WM_keymap_add_item(keymap, "PAINT_OT_face_select_linked_pick", LKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", true);

	keymap = WM_keymap_find(keyconf, "UV Sculpt", 0, 0);
	keymap->poll = uv_sculpt_keymap_poll;

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", QKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.use_uv_sculpt");

	RNA_enum_set(WM_keymap_add_item(keymap, "SCULPT_OT_uv_sculpt_stroke", LEFTMOUSE, KM_PRESS, 0,        0)->ptr, "mode", BRUSH_STROKE_NORMAL);
	RNA_enum_set(WM_keymap_add_item(keymap, "SCULPT_OT_uv_sculpt_stroke", LEFTMOUSE, KM_PRESS, KM_CTRL,  0)->ptr, "mode", BRUSH_STROKE_INVERT);
	RNA_enum_set(WM_keymap_add_item(keymap, "SCULPT_OT_uv_sculpt_stroke", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "mode", BRUSH_STROKE_SMOOTH);

	ed_keymap_paint_brush_size(keymap, "tool_settings.uv_sculpt.brush.size");
	ed_keymap_paint_brush_radial_control(keymap, "uv_sculpt", 0);

	RNA_enum_set(WM_keymap_add_item(keymap, "BRUSH_OT_uv_sculpt_tool_set", SKEY, KM_PRESS, 0, 0)->ptr, "tool", UV_SCULPT_TOOL_RELAX);
	RNA_enum_set(WM_keymap_add_item(keymap, "BRUSH_OT_uv_sculpt_tool_set", PKEY, KM_PRESS, 0, 0)->ptr, "tool", UV_SCULPT_TOOL_PINCH);
	RNA_enum_set(WM_keymap_add_item(keymap, "BRUSH_OT_uv_sculpt_tool_set", GKEY, KM_PRESS, 0, 0)->ptr, "tool", UV_SCULPT_TOOL_GRAB);

	/* paint stroke */
	keymap = paint_stroke_modal_keymap(keyconf);
	WM_modalkeymap_assign(keymap, "SCULPT_OT_brush_stroke");
}
