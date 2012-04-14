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
 * The Original Code is Copyright (C) 2010 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implements the PBVH node hiding operator
 *
 */

/** \file blender/editors/sculpt_paint/paint_hide.c
 *  \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_pbvh.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_subsurf.h"

#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "paint_intern.h"
#include "sculpt_intern.h" /* for undo push */

#include <assert.h>

static int planes_contain_v3(float (*planes)[4], int totplane, const float p[3])
{
	int i;

	for (i = 0; i < totplane; i++) {
		if (dot_v3v3(planes[i], p) + planes[i][3] > 0)
			return 0;
	}

	return 1;
}

/* return true if the element should be hidden/shown */
static int is_effected(PartialVisArea area,
                       float planes[4][4],
                       const float co[3])
{
	if (area == PARTIALVIS_ALL)
		return 1;
	else {
		int inside = planes_contain_v3(planes, 4, co);
		return ((inside && area == PARTIALVIS_INSIDE) ||
		        (!inside && area == PARTIALVIS_OUTSIDE));
	}
}

static void partialvis_update_mesh(Object *ob,
                                   PBVH *pbvh,
                                   PBVHNode *node,
                                   PartialVisAction action,
                                   PartialVisArea area,
                                   float planes[4][4])
{
	MVert *mvert;
	int *vert_indices;
	int any_changed = 0, any_visible = 0, totvert, i;
			
	BLI_pbvh_node_num_verts(pbvh, node, NULL, &totvert);
	BLI_pbvh_node_get_verts(pbvh, node, &vert_indices, &mvert);

	sculpt_undo_push_node(ob, node, SCULPT_UNDO_HIDDEN);

	for (i = 0; i < totvert; i++) {
		MVert *v = &mvert[vert_indices[i]];

		/* hide vertex if in the hide volume */
		if (is_effected(area, planes, v->co)) {
			if (action == PARTIALVIS_HIDE)
				v->flag |= ME_HIDE;
			else
				v->flag &= ~ME_HIDE;
			any_changed = 1;
		}

		if (!(v->flag & ME_HIDE))
			any_visible = 1;
	}

	if (any_changed) {
		BLI_pbvh_node_mark_rebuild_draw(node);
		BLI_pbvh_node_fully_hidden_set(node, !any_visible);
	}
}

/* Hide or show elements in multires grids with a special GridFlags
   customdata layer. */
static void partialvis_update_grids(Object *ob,
                                    PBVH *pbvh,
                                    PBVHNode *node,
                                    PartialVisAction action,
                                    PartialVisArea area,
                                    float planes[4][4])
{
	DMGridData **grids;
	BLI_bitmap *grid_hidden;
	int any_visible = 0;
	int *grid_indices, gridsize, totgrid, any_changed, i;

	/* get PBVH data */
	BLI_pbvh_node_get_grids(pbvh, node,
	                        &grid_indices, &totgrid, NULL, &gridsize,
	                        &grids, NULL);
	grid_hidden = BLI_pbvh_grid_hidden(pbvh);
	
	sculpt_undo_push_node(ob, node, SCULPT_UNDO_HIDDEN);
	
	any_changed = 0;
	for (i = 0; i < totgrid; i++) {
		int any_hidden = 0;
		int g = grid_indices[i], x, y;
		BLI_bitmap gh = grid_hidden[g];

		if (!gh) {
			switch (action) {
				case PARTIALVIS_HIDE:
					/* create grid flags data */
					gh = grid_hidden[g] = BLI_BITMAP_NEW(gridsize * gridsize,
					                                     "partialvis_update_grids");
					break;
				case PARTIALVIS_SHOW:
					/* entire grid is visible, nothing to show */
					continue;
			}
		}
		else if (action == PARTIALVIS_SHOW && area == PARTIALVIS_ALL) {
			/* special case if we're showing all, just free the
			   grid */
			MEM_freeN(gh);
			grid_hidden[g] = NULL;
			any_changed = 1;
			any_visible = 1;
			continue;
		}

		for (y = 0; y < gridsize; y++) {
			for (x = 0; x < gridsize; x++) {
				const float *co = grids[g][y * gridsize + x].co;

				/* skip grid element if not in the effected area */
				if (is_effected(area, planes, co)) {
					/* set or clear the hide flag */
					BLI_BITMAP_MODIFY(gh, y * gridsize + x,
					                  action == PARTIALVIS_HIDE);

					any_changed = 1;
				}

				/* keep track of whether any elements are still hidden */
				if (BLI_BITMAP_GET(gh, y * gridsize + x))
					any_hidden = 1;
				else
					any_visible = 1;
			}
		}

		/* if everything in the grid is now visible, free the grid
		   flags */
		if (!any_hidden) {
			MEM_freeN(gh);
			grid_hidden[g] = NULL;
		}
	}

	/* mark updates if anything was hidden/shown */
	if (any_changed) {
		BLI_pbvh_node_mark_rebuild_draw(node);
		BLI_pbvh_node_fully_hidden_set(node, !any_visible);
		multires_mark_as_modified(ob, MULTIRES_HIDDEN_MODIFIED);
	}
}

static void rect_from_props(rcti *rect, PointerRNA *ptr)
{
	rect->xmin = RNA_int_get(ptr, "xmin");
	rect->ymin = RNA_int_get(ptr, "ymin");
	rect->xmax = RNA_int_get(ptr, "xmax");
	rect->ymax = RNA_int_get(ptr, "ymax");
}

static void clip_planes_from_rect(bContext *C,
                                  float clip_planes[4][4],
                                  const rcti *rect)
{
	ViewContext vc;
	BoundBox bb;
	bglMats mats = {{0}};
	
	view3d_operator_needs_opengl(C);
	view3d_set_viewcontext(C, &vc);
	view3d_get_transformation(vc.ar, vc.rv3d, vc.obact, &mats);
	ED_view3d_calc_clipping(&bb, clip_planes, &mats, rect);
	mul_m4_fl(clip_planes, -1.0f);
}

/* If mode is inside, get all PBVH nodes that lie at least partially
   inside the clip_planes volume. If mode is outside, get all nodes
   that lie at least partially outside the volume. If showing all, get
   all nodes. */
static void get_pbvh_nodes(PBVH *pbvh,
                           PBVHNode ***nodes,
                           int *totnode,
                           float clip_planes[4][4],
                           PartialVisArea mode)
{
	BLI_pbvh_SearchCallback cb;

	/* select search callback */
	switch (mode) {
		case PARTIALVIS_INSIDE:
			cb = BLI_pbvh_node_planes_contain_AABB;
			break;
		case PARTIALVIS_OUTSIDE:
			cb = BLI_pbvh_node_planes_exclude_AABB;
			break;
		case PARTIALVIS_ALL:
			cb = NULL;
	}
	
	BLI_pbvh_search_gather(pbvh, cb, clip_planes, nodes, totnode);
}

static int hide_show_exec(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;
	PartialVisAction action;
	PartialVisArea area;
	PBVH *pbvh;
	PBVHNode **nodes;
	DerivedMesh *dm;
	PBVHType pbvh_type;
	float clip_planes[4][4];
	rcti rect;
	int totnode, i;

	/* read operator properties */
	action = RNA_enum_get(op->ptr, "action");
	area = RNA_enum_get(op->ptr, "area");
	rect_from_props(&rect, op->ptr);

	clip_planes_from_rect(C, clip_planes, &rect);

	dm = mesh_get_derived_final(CTX_data_scene(C), ob, CD_MASK_BAREMESH);
	pbvh = dm->getPBVH(ob, dm);
	ob->sculpt->pbvh = pbvh;

	get_pbvh_nodes(pbvh, &nodes, &totnode, clip_planes, area);
	pbvh_type = BLI_pbvh_type(pbvh);

	/* start undo */
	switch (action) {
		case PARTIALVIS_HIDE:
			sculpt_undo_push_begin("Hide area");
			break;
		case PARTIALVIS_SHOW:
			sculpt_undo_push_begin("Show area");
			break;
	}

	for (i = 0; i < totnode; i++) {
		switch (pbvh_type) {
			case PBVH_FACES:
				partialvis_update_mesh(ob, pbvh, nodes[i], action, area, clip_planes);
				break;
			case PBVH_GRIDS:
				partialvis_update_grids(ob, pbvh, nodes[i], action, area, clip_planes);
				break;
		}
	}

	if (nodes)
		MEM_freeN(nodes);
	
	/* end undo */
	sculpt_undo_push_end();

	/* ensure that edges and faces get hidden as well (not used by
	   sculpt but it looks wrong when entering editmode otherwise) */
	if (pbvh_type == PBVH_FACES) {
		mesh_flush_hidden_from_verts(me->mvert, me->mloop,
		                             me->medge, me->totedge,
		                             me->mpoly, me->totpoly);
	}

	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

static int hide_show_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	PartialVisArea area = RNA_enum_get(op->ptr, "area");

	if (area != PARTIALVIS_ALL)
		return WM_border_select_invoke(C, op, event);
	else
		return op->type->exec(C, op);
}

void PAINT_OT_hide_show(struct wmOperatorType *ot)
{
	static EnumPropertyItem action_items[] = {
		{PARTIALVIS_HIDE, "HIDE", 0, "Hide", "Hide vertices"},
		{PARTIALVIS_SHOW, "SHOW", 0, "Show", "Show vertices"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem area_items[] = {
		{PARTIALVIS_OUTSIDE, "OUTSIDE", 0, "Outside", "Hide or show vertices outside the selection"},
		{PARTIALVIS_INSIDE, "INSIDE", 0, "Inside", "Hide or show vertices inside the selection"},
		{PARTIALVIS_ALL, "ALL", 0, "All", "Hide or show all vertices"},
		{0, NULL, 0, NULL, NULL}};
	
	/* identifiers */
	ot->name = "Hide/Show";
	ot->idname = "PAINT_OT_hide_show";

	/* api callbacks */
	ot->invoke = hide_show_invoke;
	ot->modal = WM_border_select_modal;
	ot->exec = hide_show_exec;
	/* sculpt-only for now */
	ot->poll = sculpt_mode_poll;

	ot->flag = OPTYPE_REGISTER;

	/* rna */
	RNA_def_enum(ot->srna, "action", action_items, PARTIALVIS_HIDE,
	             "Action", "Whether to hide or show vertices");
	RNA_def_enum(ot->srna, "area", area_items, PARTIALVIS_INSIDE,
	             "Area", "Which vertices to hide or show");
	
	RNA_def_int(ot->srna, "xmin", 0, INT_MIN, INT_MAX, "X Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmax", 0, INT_MIN, INT_MAX, "X Max", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymin", 0, INT_MIN, INT_MAX, "Y Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymax", 0, INT_MIN, INT_MAX, "Y Max", "", INT_MIN, INT_MAX);
}
