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
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_pbvh.h"
#include "BKE_ccg.h"
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

#include "bmesh.h"

#include "paint_intern.h"
#include "sculpt_intern.h" /* for undo push */

#include <assert.h>

/* return true if the element should be hidden/shown */
static bool is_effected(PartialVisArea area,
                        float planes[4][4],
                        const float co[3],
                        const float mask)
{
	if (area == PARTIALVIS_ALL)
		return 1;
	else if (area == PARTIALVIS_MASKED) {
		return mask > 0.5f;
	}
	else {
		bool inside = isect_point_planes_v3(planes, 4, co);
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
	Mesh *me = ob->data;
	MVert *mvert;
	const float *paint_mask;
	int *vert_indices;
	int totvert, i;
	bool any_changed = false, any_visible = false;
			
	BKE_pbvh_node_num_verts(pbvh, node, NULL, &totvert);
	BKE_pbvh_node_get_verts(pbvh, node, &vert_indices, &mvert);
	paint_mask = CustomData_get_layer(&me->vdata, CD_PAINT_MASK);

	sculpt_undo_push_node(ob, node, SCULPT_UNDO_HIDDEN);

	for (i = 0; i < totvert; i++) {
		MVert *v = &mvert[vert_indices[i]];
		float vmask = paint_mask ? paint_mask[vert_indices[i]] : 0;

		/* hide vertex if in the hide volume */
		if (is_effected(area, planes, v->co, vmask)) {
			if (action == PARTIALVIS_HIDE)
				v->flag |= ME_HIDE;
			else
				v->flag &= ~ME_HIDE;
			any_changed = true;
		}

		if (!(v->flag & ME_HIDE))
			any_visible = true;
	}

	if (any_changed) {
		BKE_pbvh_node_mark_rebuild_draw(node);
		BKE_pbvh_node_fully_hidden_set(node, !any_visible);
	}
}

/* Hide or show elements in multires grids with a special GridFlags
 * customdata layer. */
static void partialvis_update_grids(Object *ob,
                                    PBVH *pbvh,
                                    PBVHNode *node,
                                    PartialVisAction action,
                                    PartialVisArea area,
                                    float planes[4][4])
{
	CCGElem **grids;
	CCGKey key;
	BLI_bitmap **grid_hidden;
	int *grid_indices, totgrid, i;
	bool any_changed = false, any_visible = false;


	/* get PBVH data */
	BKE_pbvh_node_get_grids(pbvh, node,
	                        &grid_indices, &totgrid, NULL, NULL,
	                        &grids, NULL);
	grid_hidden = BKE_pbvh_grid_hidden(pbvh);
	BKE_pbvh_get_grid_key(pbvh, &key);
	
	sculpt_undo_push_node(ob, node, SCULPT_UNDO_HIDDEN);

	for (i = 0; i < totgrid; i++) {
		int any_hidden = 0;
		int g = grid_indices[i], x, y;
		BLI_bitmap *gh = grid_hidden[g];

		if (!gh) {
			switch (action) {
				case PARTIALVIS_HIDE:
					/* create grid flags data */
					gh = grid_hidden[g] = BLI_BITMAP_NEW(key.grid_area,
					                                     "partialvis_update_grids");
					break;
				case PARTIALVIS_SHOW:
					/* entire grid is visible, nothing to show */
					continue;
			}
		}
		else if (action == PARTIALVIS_SHOW && area == PARTIALVIS_ALL) {
			/* special case if we're showing all, just free the
			 * grid */
			MEM_freeN(gh);
			grid_hidden[g] = NULL;
			any_changed = true;
			any_visible = true;
			continue;
		}

		for (y = 0; y < key.grid_size; y++) {
			for (x = 0; x < key.grid_size; x++) {
				CCGElem *elem = CCG_grid_elem(&key, grids[g], x, y);
				const float *co = CCG_elem_co(&key, elem);
				float mask = key.has_mask ? *CCG_elem_mask(&key, elem) : 0.0f;

				/* skip grid element if not in the effected area */
				if (is_effected(area, planes, co, mask)) {
					/* set or clear the hide flag */
					BLI_BITMAP_SET(gh, y * key.grid_size + x,
					                  action == PARTIALVIS_HIDE);

					any_changed = true;
				}

				/* keep track of whether any elements are still hidden */
				if (BLI_BITMAP_TEST(gh, y * key.grid_size + x))
					any_hidden = true;
				else
					any_visible = true;
			}
		}

		/* if everything in the grid is now visible, free the grid
		 * flags */
		if (!any_hidden) {
			MEM_freeN(gh);
			grid_hidden[g] = NULL;
		}
	}

	/* mark updates if anything was hidden/shown */
	if (any_changed) {
		BKE_pbvh_node_mark_rebuild_draw(node);
		BKE_pbvh_node_fully_hidden_set(node, !any_visible);
		multires_mark_as_modified(ob, MULTIRES_HIDDEN_MODIFIED);
	}
}

static void partialvis_update_bmesh_verts(BMesh *bm,
                                          GSet *verts,
                                          PartialVisAction action,
                                          PartialVisArea area,
                                          float planes[4][4],
                                          bool *any_changed,
                                          bool *any_visible)
{
	GSetIterator gs_iter;

	GSET_ITER (gs_iter, verts) {
		BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
		float *vmask = CustomData_bmesh_get(&bm->vdata,
		                                    v->head.data,
		                                    CD_PAINT_MASK);

		/* hide vertex if in the hide volume */
		if (is_effected(area, planes, v->co, *vmask)) {
			if (action == PARTIALVIS_HIDE)
				BM_elem_flag_enable(v, BM_ELEM_HIDDEN);
			else
				BM_elem_flag_disable(v, BM_ELEM_HIDDEN);
			(*any_changed) = true;
		}

		if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN))
			(*any_visible) = true;
	}
}

static void partialvis_update_bmesh_faces(GSet *faces)
{
	GSetIterator gs_iter;

	GSET_ITER (gs_iter, faces) {
		BMFace *f = BLI_gsetIterator_getKey(&gs_iter);

		if (paint_is_bmesh_face_hidden(f))
			BM_elem_flag_enable(f, BM_ELEM_HIDDEN);
		else
			BM_elem_flag_disable(f, BM_ELEM_HIDDEN);
	}
}

static void partialvis_update_bmesh(Object *ob,
                                    PBVH *pbvh,
                                    PBVHNode *node,
                                    PartialVisAction action,
                                    PartialVisArea area,
                                    float planes[4][4])
{
	BMesh *bm;
	GSet *unique, *other, *faces;
	bool any_changed = false, any_visible = false;

	bm = BKE_pbvh_get_bmesh(pbvh);
	unique = BKE_pbvh_bmesh_node_unique_verts(node);
	other = BKE_pbvh_bmesh_node_other_verts(node);
	faces = BKE_pbvh_bmesh_node_faces(node);

	sculpt_undo_push_node(ob, node, SCULPT_UNDO_HIDDEN);

	partialvis_update_bmesh_verts(bm,
	                              unique,
	                              action,
	                              area,
	                              planes,
	                              &any_changed,
	                              &any_visible);

	partialvis_update_bmesh_verts(bm,
	                              other,
	                              action,
	                              area,
	                              planes,
	                              &any_changed,
	                              &any_visible);

	/* finally loop over node faces and tag the ones that are fully hidden */
	partialvis_update_bmesh_faces(faces);

	if (any_changed) {
		BKE_pbvh_node_mark_rebuild_draw(node);
		BKE_pbvh_node_fully_hidden_set(node, !any_visible);
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
	ED_view3d_clipping_calc(&bb, clip_planes, &mats, rect);
	negate_m4(clip_planes);
}

/* If mode is inside, get all PBVH nodes that lie at least partially
 * inside the clip_planes volume. If mode is outside, get all nodes
 * that lie at least partially outside the volume. If showing all, get
 * all nodes. */
static void get_pbvh_nodes(PBVH *pbvh,
                           PBVHNode ***nodes,
                           int *totnode,
                           float clip_planes[4][4],
                           PartialVisArea mode)
{
	BKE_pbvh_SearchCallback cb = NULL;

	/* select search callback */
	switch (mode) {
		case PARTIALVIS_INSIDE:
			cb = BKE_pbvh_node_planes_contain_AABB;
			break;
		case PARTIALVIS_OUTSIDE:
			cb = BKE_pbvh_node_planes_exclude_AABB;
			break;
		case PARTIALVIS_ALL:
		case PARTIALVIS_MASKED:
			break;
	}
	
	BKE_pbvh_search_gather(pbvh, cb, clip_planes, nodes, totnode);
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
	pbvh_type = BKE_pbvh_type(pbvh);

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
			case PBVH_BMESH:
				partialvis_update_bmesh(ob, pbvh, nodes[i], action, area, clip_planes);
				break;
		}
	}

	if (nodes)
		MEM_freeN(nodes);
	
	/* end undo */
	sculpt_undo_push_end();

	/* ensure that edges and faces get hidden as well (not used by
	 * sculpt but it looks wrong when entering editmode otherwise) */
	if (pbvh_type == PBVH_FACES) {
		BKE_mesh_flush_hidden_from_verts(me);
	}

	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

static int hide_show_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	PartialVisArea area = RNA_enum_get(op->ptr, "area");

	if (!ELEM(area, PARTIALVIS_ALL, PARTIALVIS_MASKED))
		return WM_border_select_invoke(C, op, event);
	else
		return op->type->exec(C, op);
}

void PAINT_OT_hide_show(struct wmOperatorType *ot)
{
	static EnumPropertyItem action_items[] = {
		{PARTIALVIS_HIDE, "HIDE", 0, "Hide", "Hide vertices"},
		{PARTIALVIS_SHOW, "SHOW", 0, "Show", "Show vertices"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem area_items[] = {
		{PARTIALVIS_OUTSIDE, "OUTSIDE", 0, "Outside", "Hide or show vertices outside the selection"},
		{PARTIALVIS_INSIDE, "INSIDE", 0, "Inside", "Hide or show vertices inside the selection"},
		{PARTIALVIS_ALL, "ALL", 0, "All", "Hide or show all vertices"},
		{PARTIALVIS_MASKED, "MASKED", 0, "Masked", "Hide or show vertices that are masked (minimum mask value of 0.5)"},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name = "Hide/Show";
	ot->idname = "PAINT_OT_hide_show";
	ot->description = "Hide/show some vertices";

	/* api callbacks */
	ot->invoke = hide_show_invoke;
	ot->modal = WM_border_select_modal;
	ot->exec = hide_show_exec;
	/* sculpt-only for now */
	ot->poll = sculpt_mode_poll_view3d;

	ot->flag = OPTYPE_REGISTER;

	/* rna */
	RNA_def_enum(ot->srna, "action", action_items, PARTIALVIS_HIDE,
	             "Action", "Whether to hide or show vertices");
	RNA_def_enum(ot->srna, "area", area_items, PARTIALVIS_INSIDE,
	             "Area", "Which vertices to hide or show");
	
	WM_operator_properties_border(ot);
}
