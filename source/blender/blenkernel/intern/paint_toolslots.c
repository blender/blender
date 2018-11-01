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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/paint_toolslots.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "BLI_utildefines.h"

#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_paint.h"

void BKE_paint_toolslots_len_ensure(Paint *paint, int len)
{
	if (paint->tool_slots_len < len) {
		paint->tool_slots = MEM_recallocN(paint->tool_slots, sizeof(*paint->tool_slots) * len);
		paint->tool_slots_len = len;
	}
}

typedef bool (*BrushCompatFn)(const Brush *brush);
typedef char (*BrushToolFn)(const Brush *brush);

static void paint_toolslots_init_paint(
        Main *bmain,
        Paint *paint,
        BrushCompatFn brush_compat_fn, BrushToolFn brush_tool_fn)
{
	for (Brush *brush = bmain->brush.first; brush; brush = brush->id.next) {
		if (brush_compat_fn(brush)) {
			uint slot_index = brush_tool_fn(brush);
			BKE_paint_toolslots_len_ensure(paint, slot_index + 1);
			if (paint->tool_slots[slot_index].brush == NULL) {
				paint->tool_slots[slot_index].brush = brush;
				id_us_plus(&brush->id);
			}
		}
	}
}

/* Image paint. */
static bool brush_compat_from_imagepaint(const Brush *brush) { return brush->ob_mode & OB_MODE_TEXTURE_PAINT; }
static char brush_tool_from_imagepaint(const Brush *brush) { return brush->imagepaint_tool; }
/* Sculpt. */
static bool brush_compat_from_sculpt(const Brush *brush) { return brush->ob_mode & OB_MODE_SCULPT; }
static char brush_tool_from_sculpt(const Brush *brush) { return brush->sculpt_tool; }
/* Vertex Paint. */
static bool brush_compat_from_vertexpaint(const Brush *brush) { return brush->ob_mode & OB_MODE_VERTEX_PAINT; }
static char brush_tool_from_vertexpaint(const Brush *brush) { return brush->vertexpaint_tool; }
/* Weight Paint. */
static bool brush_compat_from_weightpaint(const Brush *brush) { return brush->ob_mode & OB_MODE_WEIGHT_PAINT; }
static char brush_tool_from_weightpaint(const Brush *brush) { return brush->vertexpaint_tool; }
/* Grease Pencil. */
static bool brush_compat_from_gpencil(const Brush *brush) { return brush->ob_mode & OB_MODE_GPENCIL_PAINT; }
static char brush_tool_from_gpencil(const Brush *brush) { return brush->gpencil_tool; }

void BKE_paint_toolslots_init_from_main(struct Main *bmain)
{
	for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
		ToolSettings *ts = scene->toolsettings;
		paint_toolslots_init_paint(bmain, &ts->imapaint.paint, brush_compat_from_imagepaint, brush_tool_from_imagepaint);
		paint_toolslots_init_paint(bmain, &ts->sculpt->paint, brush_compat_from_sculpt, brush_tool_from_sculpt);
		paint_toolslots_init_paint(bmain, &ts->vpaint->paint, brush_compat_from_vertexpaint, brush_tool_from_vertexpaint);
		paint_toolslots_init_paint(bmain, &ts->wpaint->paint, brush_compat_from_weightpaint, brush_tool_from_weightpaint);
		paint_toolslots_init_paint(bmain, &ts->gp_paint->paint, brush_compat_from_gpencil, brush_tool_from_gpencil);
	}
}


void BKE_paint_toolslots_brush_update_ex(Scene *scene, Paint *paint, Brush *brush)
{
	ToolSettings *ts = scene->toolsettings;
	int slot_index;
	if (paint == &ts->imapaint.paint) {
		slot_index = brush->imagepaint_tool;
	}
	else if (paint == &ts->sculpt->paint) {
		slot_index = brush->sculpt_tool;
	}
	else if (paint == &ts->vpaint->paint) {
		slot_index = brush->vertexpaint_tool;
	}
	else if (paint == &ts->wpaint->paint) {
		slot_index = brush->vertexpaint_tool;
	}
	else if (paint == &ts->gp_paint->paint) {
		slot_index = brush->gpencil_tool;
	}
	BKE_paint_toolslots_len_ensure(paint, slot_index + 1);
	PaintToolSlot *tslot = &paint->tool_slots[slot_index];
	id_us_plus(&brush->id);
	id_us_min(&tslot->brush->id);
	tslot->brush = brush;
}

void BKE_paint_toolslots_brush_update(Scene *scene, Paint *paint)
{
	if (paint->brush == NULL) {
		return;
	}
	BKE_paint_toolslots_brush_update_ex(scene, paint, paint->brush);
}
