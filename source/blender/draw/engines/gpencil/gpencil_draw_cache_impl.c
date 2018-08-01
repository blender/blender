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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file draw/engines/gpencil/gpencil_draw_cache_impl.c
 *  \ingroup draw
 */

#include "BLI_polyfill_2d.h"
#include "BLI_math_color.h"

#include "DNA_meshdata_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_gpencil.h"
#include "BKE_action.h"

#include "DRW_render.h"

#include "GPU_immediate.h"
#include "GPU_draw.h"

#include "ED_gpencil.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "gpencil_engine.h"

/* Helper to add stroke point to vbo */
static void gpencil_set_stroke_point(
        GPUVertBuf *vbo, float matrix[4][4], const bGPDspoint *pt, int idx,
        uint pos_id, uint color_id,
        uint thickness_id, uint uvdata_id, short thickness,
        const float ink[4])
{
	float viewfpt[3];

	float alpha = ink[3] * pt->strength;
	CLAMP(alpha, GPENCIL_STRENGTH_MIN, 1.0f);
	float col[4];
	ARRAY_SET_ITEMS(col, ink[0], ink[1], ink[2], alpha);

	GPU_vertbuf_attr_set(vbo, color_id, idx, col);

	/* transfer both values using the same shader variable */
	float uvdata[2] = { pt->uv_fac, pt->uv_rot };
	GPU_vertbuf_attr_set(vbo, uvdata_id, idx, uvdata);

	/* the thickness of the stroke must be affected by zoom, so a pixel scale is calculated */
	mul_v3_m4v3(viewfpt, matrix, &pt->x);
	float thick = max_ff(pt->pressure * thickness, 1.0f);
	GPU_vertbuf_attr_set(vbo, thickness_id, idx, &thick);

	GPU_vertbuf_attr_set(vbo, pos_id, idx, &pt->x);
}

/* Helper to add a new fill point and texture coordinates to vertex buffer */
static void gpencil_set_fill_point(
	GPUVertBuf *vbo, int idx, bGPDspoint *pt, const float fcolor[4], float uv[2],
	uint pos_id, uint color_id, uint text_id)
{
	GPU_vertbuf_attr_set(vbo, pos_id, idx, &pt->x);
	GPU_vertbuf_attr_set(vbo, color_id, idx, fcolor);
	GPU_vertbuf_attr_set(vbo, text_id, idx, uv);
}

/* create batch geometry data for points stroke shader */
GPUBatch *DRW_gpencil_get_point_geom(bGPDstroke *gps, short thickness, const float ink[4])
{
	static GPUVertFormat format = { 0 };
	static uint pos_id, color_id, size_id, uvdata_id;
	if (format.attr_len == 0) {
		pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
		size_id = GPU_vertformat_attr_add(&format, "thickness", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
		uvdata_id = GPU_vertformat_attr_add(&format, "uvdata", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	}

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, gps->totpoints);

	/* draw stroke curve */
	const bGPDspoint *pt = gps->points;
	int idx = 0;
	float alpha;
	float col[4];

	for (int i = 0; i < gps->totpoints; i++, pt++) {
		/* set point */
		alpha = ink[3] * pt->strength;
		CLAMP(alpha, GPENCIL_STRENGTH_MIN, 1.0f);
		ARRAY_SET_ITEMS(col, ink[0], ink[1], ink[2], alpha);

		float thick = max_ff(pt->pressure * thickness, 1.0f);

		GPU_vertbuf_attr_set(vbo, color_id, idx, col);
		GPU_vertbuf_attr_set(vbo, size_id, idx, &thick);

		/* transfer both values using the same shader variable */
		float uvdata[2] = { pt->uv_fac, pt->uv_rot };
		GPU_vertbuf_attr_set(vbo, uvdata_id, idx, uvdata);

		GPU_vertbuf_attr_set(vbo, pos_id, idx, &pt->x);
		idx++;
	}

	return GPU_batch_create_ex(GPU_PRIM_POINTS, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

/* create batch geometry data for stroke shader */
GPUBatch *DRW_gpencil_get_stroke_geom(bGPDframe *gpf, bGPDstroke *gps, short thickness, const float ink[4])
{
	bGPDspoint *points = gps->points;
	int totpoints = gps->totpoints;
	/* if cyclic needs more vertex */
	int cyclic_add = (gps->flag & GP_STROKE_CYCLIC) ? 1 : 0;

	static GPUVertFormat format = { 0 };
	static uint pos_id, color_id, thickness_id, uvdata_id;
	if (format.attr_len == 0) {
		pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
		thickness_id = GPU_vertformat_attr_add(&format, "thickness", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
		uvdata_id = GPU_vertformat_attr_add(&format, "uvdata", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	}

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, totpoints + cyclic_add + 2);

	/* draw stroke curve */
	const bGPDspoint *pt = points;
	int idx = 0;
	for (int i = 0; i < totpoints; i++, pt++) {
		/* first point for adjacency (not drawn) */
		if (i == 0) {
			if (gps->flag & GP_STROKE_CYCLIC && totpoints > 2) {
				gpencil_set_stroke_point(
				        vbo, gpf->runtime.viewmatrix, &points[totpoints - 1], idx,
				        pos_id, color_id, thickness_id, uvdata_id, thickness, ink);
				idx++;
			}
			else {
				gpencil_set_stroke_point(
				        vbo, gpf->runtime.viewmatrix, &points[1], idx,
				        pos_id, color_id, thickness_id, uvdata_id, thickness, ink);
				idx++;
			}
		}
		/* set point */
		gpencil_set_stroke_point(
		        vbo, gpf->runtime.viewmatrix, pt, idx,
		        pos_id, color_id, thickness_id, uvdata_id, thickness, ink);
		idx++;
	}

	if (gps->flag & GP_STROKE_CYCLIC && totpoints > 2) {
		/* draw line to first point to complete the cycle */
		gpencil_set_stroke_point(
		        vbo, gpf->runtime.viewmatrix, &points[0], idx,
		        pos_id, color_id, thickness_id, uvdata_id, thickness, ink);
		idx++;
		/* now add adjacency point (not drawn) */
		gpencil_set_stroke_point(
		        vbo, gpf->runtime.viewmatrix, &points[1], idx,
		        pos_id, color_id, thickness_id, uvdata_id, thickness, ink);
		idx++;
	}
	/* last adjacency point (not drawn) */
	else {
		gpencil_set_stroke_point(
		        vbo, gpf->runtime.viewmatrix, &points[totpoints - 2], idx,
		        pos_id, color_id, thickness_id, uvdata_id, thickness, ink);
	}

	return GPU_batch_create_ex(GPU_PRIM_LINE_STRIP_ADJ, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

/* create batch geometry data for current buffer stroke shader */
GPUBatch *DRW_gpencil_get_buffer_stroke_geom(bGPdata *gpd, float matrix[4][4], short thickness)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Scene *scene = draw_ctx->scene;
	View3D *v3d = draw_ctx->v3d;
	ARegion *ar = draw_ctx->ar;
	RegionView3D *rv3d = draw_ctx->rv3d;
	ToolSettings *ts = scene->toolsettings;
	Object *ob = draw_ctx->obact;

	tGPspoint *points = gpd->runtime.sbuffer;
	int totpoints = gpd->runtime.sbuffer_size;

	static GPUVertFormat format = { 0 };
	static uint pos_id, color_id, thickness_id, uvdata_id;
	if (format.attr_len == 0) {
		pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
		thickness_id = GPU_vertformat_attr_add(&format, "thickness", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
		uvdata_id = GPU_vertformat_attr_add(&format, "uvdata", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	}

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, totpoints + 2);

	/* draw stroke curve */
	const tGPspoint *tpt = points;
	bGPDspoint pt, pt2;
	int idx = 0;

	/* get origin to reproject point */
	float origin[3];
	bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
	ED_gp_get_drawing_reference(v3d, scene, ob, gpl, ts->gpencil_v3d_align, origin);

	for (int i = 0; i < totpoints; i++, tpt++) {
		ED_gpencil_tpoint_to_point(ar, origin, tpt, &pt);
		ED_gp_project_point_to_plane(ob, rv3d, origin, ts->gp_sculpt.lock_axis - 1, &pt);

		/* first point for adjacency (not drawn) */
		if (i == 0) {
			if (totpoints > 1) {
				ED_gpencil_tpoint_to_point(ar, origin, &points[1], &pt2);
				gpencil_set_stroke_point(
				        vbo, matrix, &pt2, idx,
				        pos_id, color_id, thickness_id, uvdata_id, thickness, gpd->runtime.scolor);
			}
			else {
				gpencil_set_stroke_point(
				        vbo, matrix, &pt, idx,
				        pos_id, color_id, thickness_id, uvdata_id, thickness, gpd->runtime.scolor);
			}
			idx++;
		}
		/* set point */
		gpencil_set_stroke_point(
		        vbo, matrix, &pt, idx,
		        pos_id, color_id, thickness_id, uvdata_id, thickness, gpd->runtime.scolor);
		idx++;
	}

	/* last adjacency point (not drawn) */
	if (totpoints > 2) {
		ED_gpencil_tpoint_to_point(ar, origin, &points[totpoints - 2], &pt2);
		gpencil_set_stroke_point(
		        vbo, matrix, &pt2, idx,
		        pos_id, color_id, thickness_id, uvdata_id, thickness, gpd->runtime.scolor);
	}
	else {
		gpencil_set_stroke_point(
		        vbo, matrix, &pt, idx,
		        pos_id, color_id, thickness_id, uvdata_id, thickness, gpd->runtime.scolor);
	}

	return GPU_batch_create_ex(GPU_PRIM_LINE_STRIP_ADJ, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

/* create batch geometry data for current buffer point shader */
GPUBatch *DRW_gpencil_get_buffer_point_geom(bGPdata *gpd, float matrix[4][4], short thickness)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Scene *scene = draw_ctx->scene;
	View3D *v3d = draw_ctx->v3d;
	ARegion *ar = draw_ctx->ar;
	RegionView3D *rv3d = draw_ctx->rv3d;
	ToolSettings *ts = scene->toolsettings;
	Object *ob = draw_ctx->obact;

	tGPspoint *points = gpd->runtime.sbuffer;
	int totpoints = gpd->runtime.sbuffer_size;

	static GPUVertFormat format = { 0 };
	static uint pos_id, color_id, thickness_id, uvdata_id;
	if (format.attr_len == 0) {
		pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
		thickness_id = GPU_vertformat_attr_add(&format, "thickness", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
		uvdata_id = GPU_vertformat_attr_add(&format, "uvdata", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	}

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, totpoints);

	/* draw stroke curve */
	const tGPspoint *tpt = points;
	bGPDspoint pt;
	int idx = 0;

	/* get origin to reproject point */
	float origin[3];
	bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
	ED_gp_get_drawing_reference(v3d, scene, ob, gpl, ts->gpencil_v3d_align, origin);

	for (int i = 0; i < totpoints; i++, tpt++) {
		ED_gpencil_tpoint_to_point(ar, origin, tpt, &pt);
		ED_gp_project_point_to_plane(ob, rv3d, origin, ts->gp_sculpt.lock_axis - 1, &pt);

		/* set point */
		gpencil_set_stroke_point(
		        vbo, matrix, &pt, idx,
		        pos_id, color_id, thickness_id, uvdata_id,
		        thickness, gpd->runtime.scolor);
		idx++;
	}

	return GPU_batch_create_ex(GPU_PRIM_POINTS, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

/* create batch geometry data for current buffer fill shader */
GPUBatch *DRW_gpencil_get_buffer_fill_geom(bGPdata *gpd)
{
	if (gpd == NULL) {
		return NULL;
	}

	const tGPspoint *points = gpd->runtime.sbuffer;
	int totpoints = gpd->runtime.sbuffer_size;
	if (totpoints < 3) {
		return NULL;
	}

	const DRWContextState *draw_ctx = DRW_context_state_get();
	Scene *scene = draw_ctx->scene;
	View3D *v3d = draw_ctx->v3d;
	ARegion *ar = draw_ctx->ar;
	ToolSettings *ts = scene->toolsettings;
	Object *ob = draw_ctx->obact;

	/* get origin to reproject point */
	float origin[3];
	bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
	ED_gp_get_drawing_reference(v3d, scene, ob, gpl, ts->gpencil_v3d_align, origin);

	int tot_triangles = totpoints - 2;
	/* allocate memory for temporary areas */
	uint (*tmp_triangles)[3] = MEM_mallocN(sizeof(*tmp_triangles) * tot_triangles, __func__);
	float (*points2d)[2] = MEM_mallocN(sizeof(*points2d) * totpoints, __func__);

	/* Convert points to array and triangulate
	* Here a cache is not used because while drawing the information changes all the time, so the cache
	* would be recalculated constantly, so it is better to do direct calculation for each function call
	*/
	for (int i = 0; i < totpoints; i++) {
		const tGPspoint *pt = &points[i];
		points2d[i][0] = pt->x;
		points2d[i][1] = pt->y;
	}
	BLI_polyfill_calc(points2d, (uint)totpoints, 0, tmp_triangles);

	static GPUVertFormat format = { 0 };
	static uint pos_id, color_id;
	if (format.attr_len == 0) {
		pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
	}

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);

	/* draw triangulation data */
	if (tot_triangles > 0) {
		GPU_vertbuf_data_alloc(vbo, tot_triangles * 3);

		const tGPspoint *tpt;
		bGPDspoint pt;

		int idx = 0;
		for (int i = 0; i < tot_triangles; i++) {
			for (int j = 0; j < 3; j++) {
				tpt = &points[tmp_triangles[i][j]];
				ED_gpencil_tpoint_to_point(ar, origin, tpt, &pt);
				GPU_vertbuf_attr_set(vbo, pos_id, idx, &pt.x);
				GPU_vertbuf_attr_set(vbo, color_id, idx, gpd->runtime.sfill);
				idx++;
			}
		}
	}

	/* clear memory */
	if (tmp_triangles) {
		MEM_freeN(tmp_triangles);
	}
	if (points2d) {
		MEM_freeN(points2d);
	}

	return GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

/* create batch geometry data for stroke shader */
GPUBatch *DRW_gpencil_get_fill_geom(Object *ob, bGPDstroke *gps, const float color[4])
{
	BLI_assert(gps->totpoints >= 3);

	/* Calculate triangles cache for filling area (must be done only after changes) */
	if ((gps->flag & GP_STROKE_RECALC_CACHES) || (gps->tot_triangles == 0) || (gps->triangles == NULL)) {
		DRW_gpencil_triangulate_stroke_fill(gps);
		ED_gpencil_calc_stroke_uv(ob, gps);
	}

	BLI_assert(gps->tot_triangles >= 1);

	static GPUVertFormat format = { 0 };
	static uint pos_id, color_id, text_id;
	if (format.attr_len == 0) {
		pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
		text_id = GPU_vertformat_attr_add(&format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	}

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, gps->tot_triangles * 3);

	/* Draw all triangles for filling the polygon (cache must be calculated before) */
	bGPDtriangle *stroke_triangle = gps->triangles;
	int idx = 0;
	for (int i = 0; i < gps->tot_triangles; i++, stroke_triangle++) {
		for (int j = 0; j < 3; j++) {
			gpencil_set_fill_point(
			        vbo, idx, &gps->points[stroke_triangle->verts[j]], color, stroke_triangle->uv[j],
			        pos_id, color_id, text_id);
			idx++;
		}
	}

	return GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

/* Draw selected verts for strokes being edited */
GPUBatch *DRW_gpencil_get_edit_geom(bGPDstroke *gps, float alpha, short dflag)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Object *ob = draw_ctx->obact;
	bGPdata *gpd = ob->data;
	bool is_weight_paint = (gpd) && (gpd->flag & GP_DATA_STROKE_WEIGHTMODE);

	int vgindex = ob->actdef - 1;
	if (!BLI_findlink(&ob->defbase, vgindex)) {
		vgindex = -1;
	}

	/* Get size of verts:
	* - The selected state needs to be larger than the unselected state so that
	*   they stand out more.
	* - We use the theme setting for size of the unselected verts
	*/
	float bsize = UI_GetThemeValuef(TH_GP_VERTEX_SIZE);
	float vsize;
	if ((int)bsize > 8) {
		vsize = 10.0f;
		bsize = 8.0f;
	}
	else {
		vsize = bsize + 2;
	}

	/* for now, we assume that the base color of the points is not too close to the real color */
	float selectColor[4];
	UI_GetThemeColor3fv(TH_GP_VERTEX_SELECT, selectColor);
	selectColor[3] = alpha;

	static GPUVertFormat format = { 0 };
	static uint pos_id, color_id, size_id;
	if (format.attr_len == 0) {
		pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
		size_id = GPU_vertformat_attr_add(&format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
	}

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, gps->totpoints);

	/* Draw start and end point differently if enabled stroke direction hint */
	bool show_direction_hint = (dflag & GP_DATA_SHOW_DIRECTION) && (gps->totpoints > 1);

	/* Draw all the stroke points (selected or not) */
	bGPDspoint *pt = gps->points;
	MDeformVert *dvert = gps->dvert;

	int idx = 0;
	float fcolor[4];
	float fsize = 0;
	for (int i = 0; i < gps->totpoints; i++, pt++, dvert++) {
		/* weight paint */
		if (is_weight_paint) {
			float weight = BKE_gpencil_vgroup_use_index(dvert, vgindex);
			CLAMP(weight, 0.0f, 1.0f);
			float hue = 2.0f * (1.0f - weight) / 3.0f;
			hsv_to_rgb(hue, 1.0f, 1.0f, &selectColor[0], &selectColor[1], &selectColor[2]);
			selectColor[3] = 1.0f;
			copy_v4_v4(fcolor, selectColor);
			fsize = vsize;
		}
		else {
			if (show_direction_hint && i == 0) {
				/* start point in green bigger */
				ARRAY_SET_ITEMS(fcolor, 0.0f, 1.0f, 0.0f, 1.0f);
				fsize = vsize + 4;
			}
			else if (show_direction_hint && (i == gps->totpoints - 1)) {
				/* end point in red smaller */
				ARRAY_SET_ITEMS(fcolor, 1.0f, 0.0f, 0.0f, 1.0f);
				fsize = vsize + 1;
			}
			else if (pt->flag & GP_SPOINT_SELECT) {
				copy_v4_v4(fcolor, selectColor);
				fsize = vsize;
			}
			else {
				copy_v4_v4(fcolor, gps->runtime.tmp_stroke_rgba);
				fsize = bsize;
			}
		}

		GPU_vertbuf_attr_set(vbo, color_id, idx, fcolor);
		GPU_vertbuf_attr_set(vbo, size_id, idx, &fsize);
		GPU_vertbuf_attr_set(vbo, pos_id, idx, &pt->x);
		idx++;
	}

	return GPU_batch_create_ex(GPU_PRIM_POINTS, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

/* Draw lines for strokes being edited */
GPUBatch *DRW_gpencil_get_edlin_geom(bGPDstroke *gps, float alpha, short UNUSED(dflag))
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Object *ob = draw_ctx->obact;
	bGPdata *gpd = ob->data;
	bool is_weight_paint = (gpd) && (gpd->flag & GP_DATA_STROKE_WEIGHTMODE);

	int vgindex = ob->actdef - 1;
	if (!BLI_findlink(&ob->defbase, vgindex)) {
		vgindex = -1;
	}

	float selectColor[4];
	UI_GetThemeColor3fv(TH_GP_VERTEX_SELECT, selectColor);
	selectColor[3] = alpha;
	float linecolor[4];
	copy_v4_v4(linecolor, gpd->line_color);

	static GPUVertFormat format = { 0 };
	static uint pos_id, color_id;
	if (format.attr_len == 0) {
		pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
	}

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, gps->totpoints);

	/* Draw all the stroke lines (selected or not) */
	bGPDspoint *pt = gps->points;

	/* GPXX: for some converted files, this struct could be null
	 * maybe we can remove this and move to versioning code after
	 * merge */
	if (gps->dvert == NULL) {
		gps->dvert = MEM_callocN(sizeof(MDeformVert) * gps->totpoints, "gp_stroke_weights");
	}

	MDeformVert *dvert = gps->dvert;

	int idx = 0;
	float fcolor[4];
	for (int i = 0; i < gps->totpoints; i++, pt++, dvert++) {
		/* weight paint */
		if (is_weight_paint) {
			float weight = BKE_gpencil_vgroup_use_index(dvert, vgindex);
			CLAMP(weight, 0.0f, 1.0f);
			float hue = 2.0f * (1.0f - weight) / 3.0f;
			hsv_to_rgb(hue, 1.0f, 1.0f, &selectColor[0], &selectColor[1], &selectColor[2]);
			selectColor[3] = 1.0f;
			copy_v4_v4(fcolor, selectColor);
		}
		else {
			if (pt->flag & GP_SPOINT_SELECT) {
				copy_v4_v4(fcolor, selectColor);
			}
			else {
				copy_v4_v4(fcolor, linecolor);
			}
		}

		GPU_vertbuf_attr_set(vbo, color_id, idx, fcolor);
		GPU_vertbuf_attr_set(vbo, pos_id, idx, &pt->x);
		idx++;
	}

	return GPU_batch_create_ex(GPU_PRIM_LINE_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

static void set_grid_point(
        GPUVertBuf *vbo, int idx, float col_grid[4],
        uint pos_id, uint color_id,
        float v1, float v2, int axis)
{
	GPU_vertbuf_attr_set(vbo, color_id, idx, col_grid);

	float pos[3];
	/* Set the grid in the selected axis (default is always Y axis) */
	if (axis & V3D_GP_GRID_AXIS_X) {
		pos[0] = 0.0f;
		pos[1] = v1;
		pos[2] = v2;
	}
	else if (axis & V3D_GP_GRID_AXIS_Z) {
		pos[0] = v1;
		pos[1] = v2;
		pos[2] = 0.0f;
	}
	else {
		pos[0] = v1;
		pos[1] = 0.0f;
		pos[2] = v2;
	}



	GPU_vertbuf_attr_set(vbo, pos_id, idx, pos);
}

/* Draw grid lines */
GPUBatch *DRW_gpencil_get_grid(void)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Scene *scene = draw_ctx->scene;
	ToolSettings *ts = scene->toolsettings;
	View3D *v3d = draw_ctx->v3d;

	float col_grid[4];

	/* verify we have something to draw and valid values */
	if (v3d->overlay.gpencil_grid_lines < 1) {
		v3d->overlay.gpencil_grid_lines = GP_DEFAULT_GRID_LINES;
	}

	if (v3d->overlay.gpencil_grid_scale == 0.0f) {
		v3d->overlay.gpencil_grid_scale = 1.0f;
	}

	if (v3d->overlay.gpencil_grid_opacity < 0.1f) {
		v3d->overlay.gpencil_grid_opacity = 0.1f;
	}

	UI_GetThemeColor3fv(TH_GRID, col_grid);
	col_grid[3] = v3d->overlay.gpencil_grid_opacity;

	/* if use locked axis, copy value */
	int axis = v3d->overlay.gpencil_grid_axis;
	if ((v3d->overlay.gpencil_grid_axis & V3D_GP_GRID_AXIS_LOCK) == 0) {

		axis = v3d->overlay.gpencil_grid_axis;
	}
	else {
		switch (ts->gp_sculpt.lock_axis) {
			case GP_LOCKAXIS_X:
			{
				axis = V3D_GP_GRID_AXIS_X;
				break;
			}
			case GP_LOCKAXIS_NONE:
			case GP_LOCKAXIS_Y:
			{
				axis = V3D_GP_GRID_AXIS_Y;
				break;
			}
			case GP_LOCKAXIS_Z:
			{
				axis = V3D_GP_GRID_AXIS_Z;
				break;
			}
		}
	}

	const char *grid_unit = NULL;
	const int gridlines = v3d->overlay.gpencil_grid_lines;
	const float grid_scale = v3d->overlay.gpencil_grid_scale * ED_scene_grid_scale(scene, &grid_unit);
	const float grid = grid_scale;
	const float space = (grid_scale / gridlines);

	const uint vertex_len = 2 * (gridlines * 4 + 2);

	static GPUVertFormat format = { 0 };
	static uint pos_id, color_id;
	if (format.attr_len == 0) {
		pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
	}

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, vertex_len);

	int idx = 0;

	for (int a = 1; a <= gridlines; a++) {
		const float line = a * space;

		set_grid_point(vbo, idx, col_grid, pos_id, color_id, -grid, -line, axis);
		idx++;
		set_grid_point(vbo, idx, col_grid, pos_id, color_id, +grid, -line, axis);
		idx++;
		set_grid_point(vbo, idx, col_grid, pos_id, color_id, -grid, +line, axis);
		idx++;
		set_grid_point(vbo, idx, col_grid, pos_id, color_id, +grid, +line, axis);
		idx++;

		set_grid_point(vbo, idx, col_grid, pos_id, color_id, -line, -grid, axis);
		idx++;
		set_grid_point(vbo, idx, col_grid, pos_id, color_id, -line, +grid, axis);
		idx++;
		set_grid_point(vbo, idx, col_grid, pos_id, color_id, +line, -grid, axis);
		idx++;
		set_grid_point(vbo, idx, col_grid, pos_id, color_id, +line, +grid, axis);
		idx++;
	}
	/* center lines */
	set_grid_point(vbo, idx, col_grid, pos_id, color_id, -grid, 0.0f, axis);
	idx++;
	set_grid_point(vbo, idx, col_grid, pos_id, color_id, +grid, 0.0f, axis);
	idx++;

	set_grid_point(vbo, idx, col_grid, pos_id, color_id, 0.0f, -grid, axis);
	idx++;
	set_grid_point(vbo, idx, col_grid, pos_id, color_id, 0.0f, +grid, axis);
	idx++;

	return GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
}
