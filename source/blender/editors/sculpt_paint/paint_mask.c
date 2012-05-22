/*
 * $Id$
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2012 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/editors/sculpt_paint/paint_mask.c
 *  \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_pbvh.h"

#include "BKE_ccg.h"
#include "BKE_context.h"
#include "BKE_DerivedMesh.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_subsurf.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "paint_intern.h"
#include "sculpt_intern.h" /* for undo push */

#include <stdlib.h>

static void mask_flood_fill_set_elem(float *elem,
									 PaintMaskFloodMode mode,
									 float value)
{
	switch(mode) {
	case PAINT_MASK_FLOOD_VALUE:
		(*elem) = value;
		break;
	case PAINT_MASK_INVERT:
		(*elem) = 1.0f - (*elem);
		break;
	}
}

static int mask_flood_fill_exec(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	Object *ob = CTX_data_active_object(C);
	PaintMaskFloodMode mode;
	float value;
	DerivedMesh *dm;
	PBVH *pbvh;
	PBVHNode **nodes;
	int totnode, i;

	mode = RNA_enum_get(op->ptr, "mode");
	value = RNA_float_get(op->ptr, "value");

	dm = mesh_get_derived_final(CTX_data_scene(C), ob, CD_MASK_BAREMESH);
	pbvh = dm->getPBVH(ob, dm);
	ob->sculpt->pbvh = pbvh;

	BLI_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

	sculpt_undo_push_begin("Mask flood fill");

	for(i = 0; i < totnode; i++) {
		PBVHVertexIter vi;

		sculpt_undo_push_node(ob, nodes[i], SCULPT_UNDO_MASK);

		BLI_pbvh_vertex_iter_begin(pbvh, nodes[i], vi, PBVH_ITER_UNIQUE) {
			mask_flood_fill_set_elem(vi.mask, mode, value);
		} BLI_pbvh_vertex_iter_end;
		
		BLI_pbvh_node_mark_update(nodes[i]);
		if (BLI_pbvh_type(pbvh) == PBVH_GRIDS)
			multires_mark_as_modified(ob, MULTIRES_COORDS_MODIFIED);
	}
	
	sculpt_undo_push_end();

	if (nodes)
		MEM_freeN(nodes);

	ED_region_tag_redraw(ar);

	return OPERATOR_FINISHED;
}

void PAINT_OT_mask_flood_fill(struct wmOperatorType *ot)
{
	static EnumPropertyItem mode_items[] = {
		{PAINT_MASK_FLOOD_VALUE, "VALUE", 0, "Value", "Set mask to the level specified by the \"value\" property"},
		{PAINT_MASK_INVERT, "INVERT", 0, "Invert", "Invert the mask"},
		{0}};

	/* identifiers */
	ot->name = "Mask Flood Fill";
	ot->idname = "PAINT_OT_mask_flood_fill";

	/* api callbacks */
	ot->exec = mask_flood_fill_exec;
	ot->poll = sculpt_mode_poll;

	ot->flag = OPTYPE_REGISTER;

	/* rna */
	RNA_def_enum(ot->srna, "mode", mode_items, PAINT_MASK_FLOOD_VALUE, "Mode", NULL);
	RNA_def_float(ot->srna, "value", 0, 0, 1, "Value", "Mask level to use when mode is \"Value\"; zero means no masking and one is fully masked", 0, 1);
}
