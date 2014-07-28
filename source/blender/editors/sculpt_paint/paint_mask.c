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

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BIF_glutil.h"

#include "BLI_math_matrix.h"
#include "BLI_math_geom.h"
#include "BLI_utildefines.h"
#include "BLI_lasso.h"

#include "BKE_pbvh.h"
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
#include "ED_sculpt.h"
#include "ED_view3d.h"

#include "bmesh.h"

#include "paint_intern.h"
#include "sculpt_intern.h" /* for undo push */

#include <stdlib.h>

static EnumPropertyItem mode_items[] = {
	{PAINT_MASK_FLOOD_VALUE, "VALUE", 0, "Value", "Set mask to the level specified by the 'value' property"},
    {PAINT_MASK_FLOOD_VALUE_INVERSE, "VALUE_INVERSE", 0, "Value Inverted", "Set mask to the level specified by the inverted 'value' property"},
	{PAINT_MASK_INVERT, "INVERT", 0, "Invert", "Invert the mask"},
	{0}};


static void mask_flood_fill_set_elem(float *elem,
                                     PaintMaskFloodMode mode,
                                     float value)
{
	switch (mode) {
		case PAINT_MASK_FLOOD_VALUE:
			(*elem) = value;
			break;
		case PAINT_MASK_FLOOD_VALUE_INVERSE:
			(*elem) = 1.0f - value;
			break;
		case PAINT_MASK_INVERT:
			(*elem) = 1.0f - (*elem);
			break;
	}
}

static int mask_flood_fill_exec(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	struct Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	PaintMaskFloodMode mode;
	float value;
	PBVH *pbvh;
	PBVHNode **nodes;
	int totnode, i;
	bool multires;
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

	mode = RNA_enum_get(op->ptr, "mode");
	value = RNA_float_get(op->ptr, "value");

	BKE_sculpt_update_mesh_elements(scene, sd, ob, false, true);
	pbvh = ob->sculpt->pbvh;
	multires = (BKE_pbvh_type(pbvh) == PBVH_GRIDS);

	BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

	sculpt_undo_push_begin("Mask flood fill");

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
	for (i = 0; i < totnode; i++) {
		PBVHVertexIter vi;

		sculpt_undo_push_node(ob, nodes[i], SCULPT_UNDO_MASK);

		BKE_pbvh_vertex_iter_begin(pbvh, nodes[i], vi, PBVH_ITER_UNIQUE) {
			mask_flood_fill_set_elem(vi.mask, mode, value);
		} BKE_pbvh_vertex_iter_end;
		
		BKE_pbvh_node_mark_redraw(nodes[i]);
		if (multires)
			BKE_pbvh_node_mark_normals_update(nodes[i]);
	}

	if (multires)
		multires_mark_as_modified(ob, MULTIRES_COORDS_MODIFIED);

	sculpt_undo_push_end();

	if (nodes)
		MEM_freeN(nodes);

	ED_region_tag_redraw(ar);

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void PAINT_OT_mask_flood_fill(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Mask Flood Fill";
	ot->idname = "PAINT_OT_mask_flood_fill";
	ot->description = "Fill the whole mask with a given value, or invert its values";

	/* api callbacks */
	ot->exec = mask_flood_fill_exec;
	ot->poll = sculpt_mode_poll;

	ot->flag = OPTYPE_REGISTER;

	/* rna */
	RNA_def_enum(ot->srna, "mode", mode_items, PAINT_MASK_FLOOD_VALUE, "Mode", NULL);
	RNA_def_float(ot->srna, "value", 0, 0, 1, "Value",
	              "Mask level to use when mode is 'Value'; zero means no masking and one is fully masked", 0, 1);
}

/* Box select, operator is VIEW3D_OT_select_border, defined in view3d_select.c */

static bool is_effected(float planes[4][4], const float co[3])
{
	return isect_point_planes_v3(planes, 4, co);
}

static void flip_plane(float out[4], const float in[4], const char symm)
{
	if (symm & PAINT_SYMM_X)
		out[0] = -in[0];
	else
		out[0] = in[0];
	if (symm & PAINT_SYMM_Y)
		out[1] = -in[1];
	else
		out[1] = in[1];
	if (symm & PAINT_SYMM_Z)
		out[2] = -in[2];
	else
		out[2] = in[2];

	out[3] = in[3];
}

int ED_sculpt_mask_box_select(struct bContext *C, ViewContext *vc, const rcti *rect, bool select, bool UNUSED(extend))
{
	Sculpt *sd = vc->scene->toolsettings->sculpt;
	BoundBox bb;
	bglMats mats = {{0}};
	float clip_planes[4][4];
	float clip_planes_final[4][4];
	ARegion *ar = vc->ar;
	struct Scene *scene = vc->scene;
	Object *ob = vc->obact;
	PaintMaskFloodMode mode;
	float value;
	bool multires;
	PBVH *pbvh;
	PBVHNode **nodes;
	int totnode, i, symmpass;
	int symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;

	mode = PAINT_MASK_FLOOD_VALUE;
	value = select ? 1.0 : 0.0;

	/* transform the clip planes in object space */
	view3d_get_transformation(vc->ar, vc->rv3d, vc->obact, &mats);
	ED_view3d_clipping_calc(&bb, clip_planes, &mats, rect);
	negate_m4(clip_planes);

	BKE_sculpt_update_mesh_elements(scene, sd, ob, false, true);
	pbvh = ob->sculpt->pbvh;
	multires = (BKE_pbvh_type(pbvh) == PBVH_GRIDS);

	sculpt_undo_push_begin("Mask box fill");

	for (symmpass = 0; symmpass <= symm; ++symmpass) {
		if (symmpass == 0 ||
		    (symm & symmpass &&
		     (symm != 5 || symmpass != 3) &&
		     (symm != 6 || (symmpass != 3 && symmpass != 5))))
		{
			int j = 0;

			/* flip the planes symmetrically as needed */
			for (; j < 4; j++) {
				flip_plane(clip_planes_final[j], clip_planes[j], symmpass);
			}

			BKE_pbvh_search_gather(pbvh, BKE_pbvh_node_planes_contain_AABB, clip_planes_final, &nodes, &totnode);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
			for (i = 0; i < totnode; i++) {
				PBVHVertexIter vi;
				bool any_masked = false;

				BKE_pbvh_vertex_iter_begin(pbvh, nodes[i], vi, PBVH_ITER_UNIQUE) {
					if (is_effected(clip_planes_final, vi.co)) {
						if (!any_masked) {
							any_masked = true;

							sculpt_undo_push_node(ob, nodes[i], SCULPT_UNDO_MASK);

							BKE_pbvh_node_mark_redraw(nodes[i]);
							if (multires)
								BKE_pbvh_node_mark_normals_update(nodes[i]);
						}
						mask_flood_fill_set_elem(vi.mask, mode, value);
					}
				} BKE_pbvh_vertex_iter_end;
			}

			if (nodes)
				MEM_freeN(nodes);
		}
	}

	if (multires)
		multires_mark_as_modified(ob, MULTIRES_COORDS_MODIFIED);

	sculpt_undo_push_end();

	ED_region_tag_redraw(ar);

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

typedef struct LassoMaskData {
	struct ViewContext *vc;
	float projviewobjmat[4][4];
	bool *px;
	int width;
	rcti rect; /* bounding box for scanfilling */
	int symmpass;
} LassoMaskData;


/* Lasso select. This could be defined as part of VIEW3D_OT_select_lasso, still the shortcuts conflict,
 * so we will use a separate operator */

static bool is_effected_lasso(LassoMaskData *data, float co[3])
{
	float scr_co_f[2];
	short scr_co_s[2];
	float co_final[3];

	flip_v3_v3(co_final, co, data->symmpass);
	/* first project point to 2d space */
	ED_view3d_project_float_v2_m4(data->vc->ar, co_final, scr_co_f, data->projviewobjmat);

	scr_co_s[0] = scr_co_f[0];
	scr_co_s[1] = scr_co_f[1];

	/* clip against screen, because lasso is limited to screen only */
	if (scr_co_s[0] < data->rect.xmin || scr_co_s[1] < data->rect.ymin || scr_co_s[0] >= data->rect.xmax || scr_co_s[1] >= data->rect.ymax)
		return false;

	scr_co_s[0] -= data->rect.xmin;
	scr_co_s[1] -= data->rect.ymin;

	return data->px[scr_co_s[1] * data->width + scr_co_s[0]];
}

static void mask_lasso_px_cb(int x, int y, void *user_data)
{
	struct LassoMaskData *data = user_data;
	data->px[(y * data->width) + x] = true;
}

static int paint_mask_gesture_lasso_exec(bContext *C, wmOperator *op)
{
	int mcords_tot;
	int (*mcords)[2] = (int (*)[2])WM_gesture_lasso_path_to_array(C, op, &mcords_tot);

	if (mcords) {
		float clip_planes[4][4], clip_planes_final[4][4];
		BoundBox bb;
		bglMats mats = {{0}};
		Object *ob;
		ViewContext vc;
		LassoMaskData data;
		struct Scene *scene = CTX_data_scene(C);
		Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
		int symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
		PBVH *pbvh;
		PBVHNode **nodes;
		int totnode, i, symmpass;
		bool multires;
		PaintMaskFloodMode mode = RNA_enum_get(op->ptr, "mode");
		float value = RNA_float_get(op->ptr, "value");

		/* Calculations of individual vertices are done in 2D screen space to diminish the amount of
		 * calculations done. Bounding box PBVH collision is not computed against enclosing rectangle
		 * of lasso */
		view3d_set_viewcontext(C, &vc);
		view3d_get_transformation(vc.ar, vc.rv3d, vc.obact, &mats);

		/* lasso data calculations */
		data.vc = &vc;
		ob = vc.obact;
		ED_view3d_ob_project_mat_get(vc.rv3d, ob, data.projviewobjmat);

		BLI_lasso_boundbox(&data.rect, (const int (*)[2])mcords, mcords_tot);
		data.width = data.rect.xmax - data.rect.xmin;
		data.px = MEM_callocN(sizeof(*data.px) * data.width * (data.rect.ymax - data.rect.ymin), "lasso_mask_pixel_buffer");

		fill_poly_v2i_n(
		       data.rect.xmin, data.rect.ymin, data.rect.xmax, data.rect.ymax,
		       (const int (*)[2])mcords, mcords_tot,
		       mask_lasso_px_cb, &data);

		ED_view3d_clipping_calc(&bb, clip_planes, &mats, &data.rect);
		negate_m4(clip_planes);

		BKE_sculpt_update_mesh_elements(scene, sd, ob, false, true);
		pbvh = ob->sculpt->pbvh;
		multires = (BKE_pbvh_type(pbvh) == PBVH_GRIDS);

		sculpt_undo_push_begin("Mask lasso fill");

		for (symmpass = 0; symmpass <= symm; ++symmpass) {
			if ((symmpass == 0) ||
			    (symm & symmpass &&
			     (symm != 5 || symmpass != 3) &&
			     (symm != 6 || (symmpass != 3 && symmpass != 5))))
			{
				int j = 0;

				/* flip the planes symmetrically as needed */
				for (; j < 4; j++) {
					flip_plane(clip_planes_final[j], clip_planes[j], symmpass);
				}

				data.symmpass = symmpass;

				/* gather nodes inside lasso's enclosing rectangle (should greatly help with bigger meshes) */
				BKE_pbvh_search_gather(pbvh, BKE_pbvh_node_planes_contain_AABB, clip_planes_final, &nodes, &totnode);

#pragma omp parallel for schedule(guided) if ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_OMP_LIMIT)
				for (i = 0; i < totnode; i++) {
					PBVHVertexIter vi;
					bool any_masked = false;

					BKE_pbvh_vertex_iter_begin(pbvh, nodes[i], vi, PBVH_ITER_UNIQUE) {
						if (is_effected_lasso(&data, vi.co)) {
							if (!any_masked) {
								any_masked = true;

								sculpt_undo_push_node(ob, nodes[i], SCULPT_UNDO_MASK);

								BKE_pbvh_node_mark_redraw(nodes[i]);
								if (multires)
									BKE_pbvh_node_mark_normals_update(nodes[i]);
							}

							mask_flood_fill_set_elem(vi.mask, mode, value);
						}
					} BKE_pbvh_vertex_iter_end;
				}

				if (nodes)
					MEM_freeN(nodes);
			}
		}

		if (multires)
			multires_mark_as_modified(ob, MULTIRES_COORDS_MODIFIED);

		sculpt_undo_push_end();

		ED_region_tag_redraw(vc.ar);
		MEM_freeN((void *)mcords);
		MEM_freeN(data.px);

		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

		return OPERATOR_FINISHED;
	}
	return OPERATOR_PASS_THROUGH;
}

void PAINT_OT_mask_lasso_gesture(wmOperatorType *ot)
{
	PropertyRNA *prop;

	ot->name = "Mask Lasso Gesture";
	ot->idname = "PAINT_OT_mask_lasso_gesture";
	ot->description = "Add mask within the lasso as you move the pointer";

	ot->invoke = WM_gesture_lasso_invoke;
	ot->modal = WM_gesture_lasso_modal;
	ot->exec = paint_mask_gesture_lasso_exec;

	ot->poll = sculpt_mode_poll;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_property(ot->srna, "path", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_runtime(prop, &RNA_OperatorMousePath);

	RNA_def_enum(ot->srna, "mode", mode_items, PAINT_MASK_FLOOD_VALUE, "Mode", NULL);
	RNA_def_float(ot->srna, "value", 1.0, 0, 1.0, "Value",
	              "Mask level to use when mode is 'Value'; zero means no masking and one is fully masked", 0, 1);
}
