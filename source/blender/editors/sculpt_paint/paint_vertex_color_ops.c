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

/** \file blender/editors/sculpt_paint/paint_vertex_color_ops.c
 *  \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_deform.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"

#include "paint_intern.h"  /* own include */


static int vertex_weight_paint_mode_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me = BKE_mesh_from_object(ob);
	return (ob && (ob->mode == OB_MODE_VERTEX_PAINT || ob->mode == OB_MODE_WEIGHT_PAINT)) &&
	       (me && me->totpoly && me->dvert);
}

static bool vertex_paint_from_weight(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me;
	const MPoly *mp;
	int vgroup_active;

	if (((me = BKE_mesh_from_object(ob)) == NULL ||
	    (ED_mesh_color_ensure(me, NULL)) == false))
	{
		return false;
	}

	/* TODO: respect selection. */
	mp = me->mpoly;
	vgroup_active = ob->actdef - 1;
	for (int i = 0; i < me->totpoly; i++, mp++) {
		MLoopCol *lcol = &me->mloopcol[mp->loopstart];
		uint j = 0;
		do{
			uint vidx = me->mloop[mp->loopstart + j].v;
			const float weight = defvert_find_weight(&me->dvert[vidx], vgroup_active);
			const uchar grayscale = weight * 255;
			lcol->r = grayscale;
			lcol->b = grayscale;
			lcol->g = grayscale;
			lcol++;
			j++;
		} while (j < mp->totloop);
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	return true;
}

static int vertex_paint_from_weight_exec(bContext *C, wmOperator *UNUSED(op))
{
	if (vertex_paint_from_weight(C)) {
		return OPERATOR_FINISHED;
	}
	return OPERATOR_CANCELLED;
}

void PAINT_OT_vertex_color_from_weight(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Vertex Color from Weight";
	ot->idname = "PAINT_OT_vertex_color_from_weight";
	ot->description = "Converts active weight into greyscale vertex colors";

	/* api callback */
	ot->exec = vertex_paint_from_weight_exec;
	ot->poll = vertex_weight_paint_mode_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* TODO: invert, alpha */
}
