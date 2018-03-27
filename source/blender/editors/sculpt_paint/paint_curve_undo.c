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

/** \file blender/editors/sculpt_paint/paint_curve_undo.c
 *  \ingroup edsculpt
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_space_types.h"

#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_paint.h"

#include "ED_paint.h"

#include "WM_api.h"
#include "WM_types.h"

#include "paint_intern.h"

typedef struct UndoCurve {
	struct UndoImageTile *next, *prev;

	PaintCurvePoint *points; /* points of curve */
	int tot_points;
	int active_point;

	char idname[MAX_ID_NAME];  /* name instead of pointer*/
} UndoCurve;

static void paintcurve_undo_restore(bContext *C, ListBase *lb)
{
	Paint *p = BKE_paint_get_active_from_context(C);
	UndoCurve *uc;
	PaintCurve *pc = NULL;

	if (p->brush) {
		pc = p->brush->paint_curve;
	}

	if (!pc) {
		return;
	}

	uc = (UndoCurve *)lb->first;

	if (STREQLEN(uc->idname, pc->id.name, BLI_strnlen(uc->idname, sizeof(uc->idname)))) {
		SWAP(PaintCurvePoint *, pc->points, uc->points);
		SWAP(int, pc->tot_points, uc->tot_points);
		SWAP(int, pc->add_index, uc->active_point);
	}
}

static void paintcurve_undo_delete(ListBase *lb)
{
	UndoCurve *uc;
	uc = (UndoCurve *)lb->first;

	if (uc->points)
		MEM_freeN(uc->points);
	uc->points = NULL;
}

/**
 * \note This is called before executing steps (not after).
 */
void ED_paintcurve_undo_push(bContext *C, wmOperator *op, PaintCurve *pc)
{
	ePaintMode mode = BKE_paintmode_get_active_from_context(C);
	ListBase *lb = NULL;
	int undo_stack_id;
	UndoCurve *uc;

	switch (mode) {
		case ePaintTexture2D:
		case ePaintTextureProjective:
			undo_stack_id = UNDO_PAINT_IMAGE;
			break;

		case ePaintSculpt:
			undo_stack_id = UNDO_PAINT_MESH;
			break;

		default:
			/* do nothing, undo is handled by global */
			return;
	}


	ED_undo_paint_push_begin(undo_stack_id, op->type->name,
	                         paintcurve_undo_restore, paintcurve_undo_delete, NULL);
	lb = undo_paint_push_get_list(undo_stack_id);

	uc = MEM_callocN(sizeof(*uc), "Undo_curve");

	lb->first = uc;

	BLI_strncpy(uc->idname, pc->id.name, sizeof(uc->idname));
	uc->tot_points = pc->tot_points;
	uc->active_point = pc->add_index;
	uc->points = MEM_dupallocN(pc->points);

	undo_paint_push_count_alloc(undo_stack_id, sizeof(*uc) + sizeof(*pc->points) * pc->tot_points);

	ED_undo_paint_push_end(undo_stack_id);
}
