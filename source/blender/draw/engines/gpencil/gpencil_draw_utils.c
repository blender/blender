/*
 * Copyright 2017, Blender Foundation.
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
 * Contributor(s): Antonio Vazquez
 *
 */

/** \file blender/draw/engines/gpencil/gpencil_draw_utils.c
 *  \ingroup draw
 */

#include "BLI_polyfill_2d.h"

#include "DRW_engine.h"
#include "DRW_render.h"

#include "BKE_brush.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_image.h"
#include "BKE_material.h"
#include "BKE_paint.h"

#include "ED_gpencil.h"
#include "ED_view3d.h"

#include "DNA_gpencil_types.h"
#include "DNA_material_types.h"
#include "DNA_view3d_types.h"
#include "DNA_gpencil_modifier_types.h"

 /* If builtin shaders are needed */
#include "GPU_shader.h"
#include "GPU_texture.h"

/* For EvaluationContext... */
#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "IMB_imbuf_types.h"

#include "gpencil_engine.h"

/* fill type to communicate to shader */
#define SOLID 0
#define GRADIENT 1
#define RADIAL 2
#define CHESS 3
#define TEXTURE 4
#define PATTERN 5

#define GP_SET_SRC_GPS(src_gps) if (src_gps) src_gps = src_gps->next

/* Helper for doing all the checks on whether a stroke can be drawn */
static bool gpencil_can_draw_stroke(
        struct MaterialGPencilStyle *gp_style, const bGPDstroke *gps,
        const bool onion, const bool is_mat_preview)
{
	/* skip stroke if it doesn't have any valid data */
	if ((gps->points == NULL) || (gps->totpoints < 1) || (gp_style == NULL))
		return false;

	/* if mat preview render always visible */
	if (is_mat_preview) {
		return true;
	}

	/* check if the color is visible */
	if ((gp_style == NULL) ||
	    (gp_style->flag & GP_STYLE_COLOR_HIDE) ||
	    (onion && (gp_style->flag & GP_STYLE_COLOR_ONIONSKIN)))
	{
		return false;
	}

	/* stroke can be drawn */
	return true;
}

/* calc bounding box in 2d using flat projection data */
static void gpencil_calc_2d_bounding_box(
        const float(*points2d)[2], int totpoints, float minv[2], float maxv[2])
{
	minv[0] = points2d[0][0];
	minv[1] = points2d[0][1];
	maxv[0] = points2d[0][0];
	maxv[1] = points2d[0][1];

	for (int i = 1; i < totpoints; i++) {
		/* min */
		if (points2d[i][0] < minv[0]) {
			minv[0] = points2d[i][0];
		}
		if (points2d[i][1] < minv[1]) {
			minv[1] = points2d[i][1];
		}
		/* max */
		if (points2d[i][0] > maxv[0]) {
			maxv[0] = points2d[i][0];
		}
		if (points2d[i][1] > maxv[1]) {
			maxv[1] = points2d[i][1];
		}
	}
	/* use a perfect square */
	if (maxv[0] > maxv[1]) {
		maxv[1] = maxv[0];
	}
	else {
		maxv[0] = maxv[1];
	}
}

/* calc texture coordinates using flat projected points */
static void gpencil_calc_stroke_fill_uv(
        const float(*points2d)[2], int totpoints, float minv[2], float maxv[2], float(*r_uv)[2])
{
	float d[2];
	d[0] = maxv[0] - minv[0];
	d[1] = maxv[1] - minv[1];
	for (int i = 0; i < totpoints; i++) {
		r_uv[i][0] = (points2d[i][0] - minv[0]) / d[0];
		r_uv[i][1] = (points2d[i][1] - minv[1]) / d[1];
	}
}

/* Get points of stroke always flat to view not affected by camera view or view position */
static void gpencil_stroke_2d_flat(const bGPDspoint *points, int totpoints, float(*points2d)[2], int *r_direction)
{
	const bGPDspoint *pt0 = &points[0];
	const bGPDspoint *pt1 = &points[1];
	const bGPDspoint *pt3 = &points[(int)(totpoints * 0.75)];

	float locx[3];
	float locy[3];
	float loc3[3];
	float normal[3];

	/* local X axis (p0 -> p1) */
	sub_v3_v3v3(locx, &pt1->x, &pt0->x);

	/* point vector at 3/4 */
	sub_v3_v3v3(loc3, &pt3->x, &pt0->x);

	/* vector orthogonal to polygon plane */
	cross_v3_v3v3(normal, locx, loc3);

	/* local Y axis (cross to normal/x axis) */
	cross_v3_v3v3(locy, normal, locx);

	/* Normalize vectors */
	normalize_v3(locx);
	normalize_v3(locy);

	/* Get all points in local space */
	for (int i = 0; i < totpoints; i++) {
		const bGPDspoint *pt = &points[i];
		float loc[3];

		/* Get local space using first point as origin */
		sub_v3_v3v3(loc, &pt->x, &pt0->x);

		points2d[i][0] = dot_v3v3(loc, locx);
		points2d[i][1] = dot_v3v3(loc, locy);
	}

	/* Concave (-1), Convex (1), or Autodetect (0)? */
	*r_direction = (int)locy[2];
}

/* Triangulate stroke for high quality fill (this is done only if cache is null or stroke was modified) */
void DRW_gpencil_triangulate_stroke_fill(Object *ob, bGPDstroke *gps)
{
	BLI_assert(gps->totpoints >= 3);

	bGPdata *gpd = (bGPdata *)ob->data;

	/* allocate memory for temporary areas */
	gps->tot_triangles = gps->totpoints - 2;
	uint(*tmp_triangles)[3] = MEM_mallocN(sizeof(*tmp_triangles) * gps->tot_triangles, "GP Stroke temp triangulation");
	float(*points2d)[2] = MEM_mallocN(sizeof(*points2d) * gps->totpoints, "GP Stroke temp 2d points");
	float(*uv)[2] = MEM_mallocN(sizeof(*uv) * gps->totpoints, "GP Stroke temp 2d uv data");

	int direction = 0;

	/* convert to 2d and triangulate */
	gpencil_stroke_2d_flat(gps->points, gps->totpoints, points2d, &direction);
	BLI_polyfill_calc(points2d, (uint)gps->totpoints, direction, tmp_triangles);

	/* calc texture coordinates automatically */
	float minv[2];
	float maxv[2];
	/* first needs bounding box data */
	if (gpd->flag & GP_DATA_UV_ADAPTATIVE) {
		gpencil_calc_2d_bounding_box(points2d, gps->totpoints, minv, maxv);
	}
	else {
		ARRAY_SET_ITEMS(minv, -1.0f, -1.0f);
		ARRAY_SET_ITEMS(maxv, 1.0f, 1.0f);
	}

	/* calc uv data */
	gpencil_calc_stroke_fill_uv(points2d, gps->totpoints, minv, maxv, uv);

	/* Number of triangles */
	gps->tot_triangles = gps->totpoints - 2;
	/* save triangulation data in stroke cache */
	if (gps->tot_triangles > 0) {
		if (gps->triangles == NULL) {
			gps->triangles = MEM_callocN(sizeof(*gps->triangles) * gps->tot_triangles, "GP Stroke triangulation");
		}
		else {
			gps->triangles = MEM_recallocN(gps->triangles, sizeof(*gps->triangles) * gps->tot_triangles);
		}

		for (int i = 0; i < gps->tot_triangles; i++) {
			bGPDtriangle *stroke_triangle = &gps->triangles[i];
			memcpy(gps->triangles[i].verts, tmp_triangles[i], sizeof(uint[3]));
			/* copy texture coordinates */
			copy_v2_v2(stroke_triangle->uv[0], uv[tmp_triangles[i][0]]);
			copy_v2_v2(stroke_triangle->uv[1], uv[tmp_triangles[i][1]]);
			copy_v2_v2(stroke_triangle->uv[2], uv[tmp_triangles[i][2]]);
		}
	}
	else {
		/* No triangles needed - Free anything allocated previously */
		if (gps->triangles)
			MEM_freeN(gps->triangles);

		gps->triangles = NULL;
	}

	/* disable recalculation flag */
	if (gps->flag & GP_STROKE_RECALC_CACHES) {
		gps->flag &= ~GP_STROKE_RECALC_CACHES;
	}

	/* clear memory */
	MEM_SAFE_FREE(tmp_triangles);
	MEM_SAFE_FREE(points2d);
	MEM_SAFE_FREE(uv);
}

/* recalc the internal geometry caches for fill and uvs */
static void DRW_gpencil_recalc_geometry_caches(Object *ob, MaterialGPencilStyle *gp_style, bGPDstroke *gps)
{
	if (gps->flag & GP_STROKE_RECALC_CACHES) {
		/* Calculate triangles cache for filling area (must be done only after changes) */
		if ((gps->tot_triangles == 0) || (gps->triangles == NULL)) {
			if ((gps->totpoints > 2) &&
			    ((gp_style->fill_rgba[3] > GPENCIL_ALPHA_OPACITY_THRESH) || (gp_style->fill_style > 0)))
			{
				DRW_gpencil_triangulate_stroke_fill(ob, gps);
			}
		}

		/* calc uv data along the stroke */
		ED_gpencil_calc_stroke_uv(ob, gps);

		/* clear flag */
		gps->flag &= ~GP_STROKE_RECALC_CACHES;
	}
}

/* create shading group for filling */
static DRWShadingGroup *DRW_gpencil_shgroup_fill_create(
        GPENCIL_e_data *e_data, GPENCIL_Data *vedata, DRWPass *pass,
        GPUShader *shader, bGPdata *gpd, bGPDlayer *gpl,
		MaterialGPencilStyle *gp_style, int id)
{
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;

	/* e_data.gpencil_fill_sh */
	DRWShadingGroup *grp = DRW_shgroup_create(shader, pass);

	DRW_shgroup_uniform_vec4(grp, "color2", gp_style->mix_rgba, 1);

	/* set style type */
	switch (gp_style->fill_style) {
		case GP_STYLE_FILL_STYLE_SOLID:
			stl->shgroups[id].fill_style = SOLID;
			break;
		case GP_STYLE_FILL_STYLE_GRADIENT:
			if (gp_style->gradient_type == GP_STYLE_GRADIENT_LINEAR) {
				stl->shgroups[id].fill_style = GRADIENT;
			}
			else {
				stl->shgroups[id].fill_style = RADIAL;
			}
			break;
		case GP_STYLE_FILL_STYLE_CHESSBOARD:
			stl->shgroups[id].fill_style = CHESS;
			break;
		case GP_STYLE_FILL_STYLE_TEXTURE:
			if (gp_style->flag & GP_STYLE_FILL_PATTERN) {
				stl->shgroups[id].fill_style = PATTERN;
			}
			else {
				stl->shgroups[id].fill_style = TEXTURE;
			}
			break;
		default:
			stl->shgroups[id].fill_style = GP_STYLE_FILL_STYLE_SOLID;
			break;
	}
	DRW_shgroup_uniform_int(grp, "fill_type", &stl->shgroups[id].fill_style, 1);

	DRW_shgroup_uniform_float(grp, "mix_factor", &gp_style->mix_factor, 1);

	DRW_shgroup_uniform_float(grp, "gradient_angle", &gp_style->gradient_angle, 1);
	DRW_shgroup_uniform_float(grp, "gradient_radius", &gp_style->gradient_radius, 1);
	DRW_shgroup_uniform_float(grp, "pattern_gridsize", &gp_style->pattern_gridsize, 1);
	DRW_shgroup_uniform_vec2(grp, "gradient_scale", gp_style->gradient_scale, 1);
	DRW_shgroup_uniform_vec2(grp, "gradient_shift", gp_style->gradient_shift, 1);

	DRW_shgroup_uniform_float(grp, "texture_angle", &gp_style->texture_angle, 1);
	DRW_shgroup_uniform_vec2(grp, "texture_scale", gp_style->texture_scale, 1);
	DRW_shgroup_uniform_vec2(grp, "texture_offset", gp_style->texture_offset, 1);
	DRW_shgroup_uniform_float(grp, "texture_opacity", &gp_style->texture_opacity, 1);
	DRW_shgroup_uniform_float(grp, "layer_opacity", &gpl->opacity, 1);

	stl->shgroups[id].texture_mix = gp_style->flag & GP_STYLE_COLOR_TEX_MIX ? 1 : 0;
	DRW_shgroup_uniform_int(grp, "texture_mix", &stl->shgroups[id].texture_mix, 1);

	stl->shgroups[id].texture_flip = gp_style->flag & GP_STYLE_COLOR_FLIP_FILL ? 1 : 0;
	DRW_shgroup_uniform_int(grp, "texture_flip", &stl->shgroups[id].texture_flip, 1);

	DRW_shgroup_uniform_int(grp, "xraymode", (const int *) &gpd->xray_mode, 1);
	/* image texture */
	if ((gp_style->flag & GP_STYLE_COLOR_TEX_MIX) ||
	    (gp_style->fill_style & GP_STYLE_FILL_STYLE_TEXTURE))
	{
		ImBuf *ibuf;
		Image *image = gp_style->ima;
		ImageUser iuser = { NULL };
		void *lock;

		iuser.ok = true;

		ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

		if (ibuf == NULL || ibuf->rect == NULL) {
			BKE_image_release_ibuf(image, ibuf, NULL);
		}
		else {
			GPUTexture *texture = GPU_texture_from_blender(gp_style->ima, &iuser, GL_TEXTURE_2D, true, 0.0);
			DRW_shgroup_uniform_texture(grp, "myTexture", texture);

			stl->shgroups[id].texture_clamp = gp_style->flag & GP_STYLE_COLOR_TEX_CLAMP ? 1 : 0;
			DRW_shgroup_uniform_int(grp, "texture_clamp", &stl->shgroups[id].texture_clamp, 1);

			BKE_image_release_ibuf(image, ibuf, NULL);
		}
	}
	else {
		/* if no texture defined, need a blank texture to avoid errors in draw manager */
		DRW_shgroup_uniform_texture(grp, "myTexture", e_data->gpencil_blank_texture);
		stl->shgroups[id].texture_clamp = 0;
		DRW_shgroup_uniform_int(grp, "texture_clamp", &stl->shgroups[id].texture_clamp, 1);
	}

	return grp;
}

/* create shading group for strokes */
DRWShadingGroup *DRW_gpencil_shgroup_stroke_create(
        GPENCIL_e_data *e_data, GPENCIL_Data *vedata, DRWPass *pass, GPUShader *shader, Object *ob,
        bGPdata *gpd, MaterialGPencilStyle *gp_style, int id, bool onion)
{
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	const float *viewport_size = DRW_viewport_size_get();

	/* e_data.gpencil_stroke_sh */
	DRWShadingGroup *grp = DRW_shgroup_create(shader, pass);

	DRW_shgroup_uniform_vec2(grp, "Viewport", viewport_size, 1);

	DRW_shgroup_uniform_float(grp, "pixsize", stl->storage->pixsize, 1);

	/* avoid wrong values */
	if ((gpd) && (gpd->pixfactor == 0)) {
		gpd->pixfactor = GP_DEFAULT_PIX_FACTOR;
	}

	/* object scale and depth */
	if ((ob) && (id > -1)) {
		stl->shgroups[id].obj_scale = mat4_to_scale(ob->obmat);
		DRW_shgroup_uniform_float(grp, "objscale", &stl->shgroups[id].obj_scale, 1);
		stl->shgroups[id].keep_size = (int)((gpd) && (gpd->flag & GP_DATA_STROKE_KEEPTHICKNESS));
		DRW_shgroup_uniform_int(grp, "keep_size", &stl->shgroups[id].keep_size, 1);

		stl->shgroups[id].stroke_style = gp_style->stroke_style;
		stl->shgroups[id].color_type = GPENCIL_COLOR_SOLID;
		if ((gp_style->stroke_style == GP_STYLE_STROKE_STYLE_TEXTURE) && (!onion)) {
			stl->shgroups[id].color_type = GPENCIL_COLOR_TEXTURE;
			if (gp_style->flag & GP_STYLE_STROKE_PATTERN) {
				stl->shgroups[id].color_type = GPENCIL_COLOR_PATTERN;
			}
		}
		DRW_shgroup_uniform_int(grp, "color_type", &stl->shgroups[id].color_type, 1);
		DRW_shgroup_uniform_float(grp, "pixfactor", &gpd->pixfactor, 1);
	}
	else {
		stl->storage->obj_scale = 1.0f;
		stl->storage->keep_size = 0;
		stl->storage->pixfactor = GP_DEFAULT_PIX_FACTOR;
		DRW_shgroup_uniform_float(grp, "objscale", &stl->storage->obj_scale, 1);
		DRW_shgroup_uniform_int(grp, "keep_size", &stl->storage->keep_size, 1);
		DRW_shgroup_uniform_int(grp, "color_type", &stl->storage->color_type, 1);
		if (gpd) {
			DRW_shgroup_uniform_float(grp, "pixfactor", &gpd->pixfactor, 1);
		}
		else {
			DRW_shgroup_uniform_float(grp, "pixfactor", &stl->storage->pixfactor, 1);
		}
	}

	if ((gpd) && (id > -1)) {
		DRW_shgroup_uniform_int(grp, "xraymode", (const int *) &gpd->xray_mode, 1);
	}
	else {
		/* for drawing always on front */
		DRW_shgroup_uniform_int(grp, "xraymode", &stl->storage->xray, 1);
	}

	/* image texture for pattern */
	if ((gp_style) && (gp_style->stroke_style == GP_STYLE_STROKE_STYLE_TEXTURE) && (!onion)) {
		ImBuf *ibuf;
		Image *image = gp_style->sima;
		ImageUser iuser = { NULL };
		void *lock;

		iuser.ok = true;

		ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

		if (ibuf == NULL || ibuf->rect == NULL) {
			BKE_image_release_ibuf(image, ibuf, NULL);
		}
		else {
			GPUTexture *texture = GPU_texture_from_blender(gp_style->sima, &iuser, GL_TEXTURE_2D, true, 0.0f);
			DRW_shgroup_uniform_texture(grp, "myTexture", texture);

			BKE_image_release_ibuf(image, ibuf, NULL);
		}
	}
	else {
		/* if no texture defined, need a blank texture to avoid errors in draw manager */
		DRW_shgroup_uniform_texture(grp, "myTexture", e_data->gpencil_blank_texture);
	}

	return grp;
}

/* create shading group for volumetrics */
static DRWShadingGroup *DRW_gpencil_shgroup_point_create(
        GPENCIL_e_data *e_data, GPENCIL_Data *vedata, DRWPass *pass, GPUShader *shader, Object *ob,
        bGPdata *gpd, MaterialGPencilStyle *gp_style, int id, bool onion)
{
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	const float *viewport_size = DRW_viewport_size_get();

	/* e_data.gpencil_stroke_sh */
	DRWShadingGroup *grp = DRW_shgroup_create(shader, pass);

	DRW_shgroup_uniform_vec2(grp, "Viewport", viewport_size, 1);
	DRW_shgroup_uniform_float(grp, "pixsize", stl->storage->pixsize, 1);

	/* avoid wrong values */
	if ((gpd) && (gpd->pixfactor == 0)) {
		gpd->pixfactor = GP_DEFAULT_PIX_FACTOR;
	}

	/* object scale and depth */
	if ((ob) && (id > -1)) {
		stl->shgroups[id].obj_scale = mat4_to_scale(ob->obmat);
		DRW_shgroup_uniform_float(grp, "objscale", &stl->shgroups[id].obj_scale, 1);
		stl->shgroups[id].keep_size = (int)((gpd) && (gpd->flag & GP_DATA_STROKE_KEEPTHICKNESS));
		DRW_shgroup_uniform_int(grp, "keep_size", &stl->shgroups[id].keep_size, 1);

		stl->shgroups[id].mode = gp_style->mode;
		stl->shgroups[id].stroke_style = gp_style->stroke_style;
		stl->shgroups[id].color_type = GPENCIL_COLOR_SOLID;
		if ((gp_style->stroke_style == GP_STYLE_STROKE_STYLE_TEXTURE) && (!onion)) {
			stl->shgroups[id].color_type = GPENCIL_COLOR_TEXTURE;
			if (gp_style->flag & GP_STYLE_STROKE_PATTERN) {
				stl->shgroups[id].color_type = GPENCIL_COLOR_PATTERN;
			}
		}
		DRW_shgroup_uniform_int(grp, "color_type", &stl->shgroups[id].color_type, 1);
		DRW_shgroup_uniform_int(grp, "mode", &stl->shgroups[id].mode, 1);
		DRW_shgroup_uniform_float(grp, "pixfactor", &gpd->pixfactor, 1);
	}
	else {
		stl->storage->obj_scale = 1.0f;
		stl->storage->keep_size = 0;
		stl->storage->pixfactor = GP_DEFAULT_PIX_FACTOR;
		stl->storage->mode = gp_style->mode;
		DRW_shgroup_uniform_float(grp, "objscale", &stl->storage->obj_scale, 1);
		DRW_shgroup_uniform_int(grp, "keep_size", &stl->storage->keep_size, 1);
		DRW_shgroup_uniform_int(grp, "color_type", &stl->storage->color_type, 1);
		DRW_shgroup_uniform_int(grp, "mode", &stl->storage->mode, 1);
		if (gpd) {
			DRW_shgroup_uniform_float(grp, "pixfactor", &gpd->pixfactor, 1);
		}
		else {
			DRW_shgroup_uniform_float(grp, "pixfactor", &stl->storage->pixfactor, 1);
		}
	}

	if (gpd) {
		DRW_shgroup_uniform_int(grp, "xraymode", (const int *)&gpd->xray_mode, 1);
	}
	else {
		/* for drawing always on front */
		DRW_shgroup_uniform_int(grp, "xraymode", &stl->storage->xray, 1);
	}

	/* image texture */
	if ((gp_style) && (gp_style->stroke_style == GP_STYLE_STROKE_STYLE_TEXTURE) && (!onion)) {
		ImBuf *ibuf;
		Image *image = gp_style->sima;
		ImageUser iuser = { NULL };
		void *lock;

		iuser.ok = true;

		ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

		if (ibuf == NULL || ibuf->rect == NULL) {
			BKE_image_release_ibuf(image, ibuf, NULL);
		}
		else {
			GPUTexture *texture = GPU_texture_from_blender(gp_style->sima, &iuser, GL_TEXTURE_2D, true, 0.0f);
			DRW_shgroup_uniform_texture(grp, "myTexture", texture);

			BKE_image_release_ibuf(image, ibuf, NULL);
		}
	}
	else {
		/* if no texture defined, need a blank texture to avoid errors in draw manager */
		DRW_shgroup_uniform_texture(grp, "myTexture", e_data->gpencil_blank_texture);
	}

	return grp;
}

/* add fill shading group to pass */
static void gpencil_add_fill_shgroup(
        GpencilBatchCache *cache, DRWShadingGroup *fillgrp,
        Object *ob, bGPDframe *gpf, bGPDstroke *gps,
        float opacity, const float tintcolor[4], const bool onion, const bool custonion)
{
	MaterialGPencilStyle *gp_style = BKE_material_gpencil_settings_get(ob, gps->mat_nr + 1);
	if (gps->totpoints >= 3) {
		float tfill[4];
		/* set color using material, tint color and opacity */
		interp_v3_v3v3(tfill, gps->runtime.tmp_fill_rgba, tintcolor, tintcolor[3]);
		tfill[3] = gps->runtime.tmp_fill_rgba[3] * opacity;
		if ((tfill[3] > GPENCIL_ALPHA_OPACITY_THRESH) || (gp_style->fill_style > 0)) {
			if (cache->is_dirty) {
				const float *color;
				if (!onion) {
					color = tfill;
				}
				else {
					if (custonion) {
						color = tintcolor;
					}
					else {
						ARRAY_SET_ITEMS(tfill, UNPACK3(gps->runtime.tmp_fill_rgba), tintcolor[3]);
						color = tfill;
					}
				}
				gpencil_batch_cache_check_free_slots(ob);
				cache->batch_fill[cache->cache_idx] = DRW_gpencil_get_fill_geom(ob, gps, color);
			}

			DRW_shgroup_call_add(fillgrp, cache->batch_fill[cache->cache_idx], gpf->runtime.viewmatrix);
		}
	}
}

/* add stroke shading group to pass */
static void gpencil_add_stroke_shgroup(GpencilBatchCache *cache, DRWShadingGroup *strokegrp,
	Object *ob, bGPDlayer *gpl, bGPDframe *gpf, bGPDstroke *gps,
	const float opacity, const float tintcolor[4], const bool onion,
	const bool custonion)
{
	float tcolor[4];
	float ink[4];
	short sthickness;
	MaterialGPencilStyle *gp_style = BKE_material_gpencil_settings_get(ob, gps->mat_nr + 1);

	/* set color using base color, tint color and opacity */
	if (cache->is_dirty) {
		if (!onion) {
			/* if special stroke, use fill color as stroke color */
			if (gps->flag & GP_STROKE_NOFILL) {
				interp_v3_v3v3(tcolor, gps->runtime.tmp_fill_rgba, tintcolor, tintcolor[3]);
				tcolor[3] = gps->runtime.tmp_fill_rgba[3] * opacity;
			}
			else {
				interp_v3_v3v3(tcolor, gps->runtime.tmp_stroke_rgba, tintcolor, tintcolor[3]);
				tcolor[3] = gps->runtime.tmp_stroke_rgba[3] * opacity;
			}
			copy_v4_v4(ink, tcolor);
		}
		else {
			if (custonion) {
				copy_v4_v4(ink, tintcolor);
			}
			else {
				ARRAY_SET_ITEMS(tcolor, UNPACK3(gps->runtime.tmp_stroke_rgba), opacity);
				copy_v4_v4(ink, tcolor);
			}
		}

		sthickness = gps->thickness + gpl->line_change;
		CLAMP_MIN(sthickness, 1);
		gpencil_batch_cache_check_free_slots(ob);
		if ((gps->totpoints > 1) && (gp_style->mode == GP_STYLE_MODE_LINE)) {
			cache->batch_stroke[cache->cache_idx] = DRW_gpencil_get_stroke_geom(gps, sthickness, ink);
		}
		else {
			cache->batch_stroke[cache->cache_idx] = DRW_gpencil_get_point_geom(gps, sthickness, ink);
		}
	}
	DRW_shgroup_call_add(strokegrp, cache->batch_stroke[cache->cache_idx], gpf->runtime.viewmatrix);
}

/* add edit points shading group to pass */
static void gpencil_add_editpoints_shgroup(
        GPENCIL_StorageList *stl, GpencilBatchCache *cache, ToolSettings *UNUSED(ts), Object *ob,
        bGPdata *gpd, bGPDlayer *gpl, bGPDframe *gpf, bGPDstroke *gps)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	View3D *v3d = draw_ctx->v3d;
	MaterialGPencilStyle *gp_style = BKE_material_gpencil_settings_get(ob, gps->mat_nr + 1);

	/* alpha factor for edit points/line to make them more subtle */
	float edit_alpha = v3d->vertex_opacity;

	if (GPENCIL_ANY_EDIT_MODE(gpd)) {
		Object *obact = DRW_context_state_get()->obact;
		if ((!obact) || (obact->type != OB_GPENCIL)) {
			return;
		}
		const bool is_weight_paint = (gpd) && (gpd->flag & GP_DATA_STROKE_WEIGHTMODE);

		/* line of the original stroke */
		if (cache->is_dirty) {
			gpencil_batch_cache_check_free_slots(ob);
			cache->batch_edlin[cache->cache_idx] = DRW_gpencil_get_edlin_geom(gps, edit_alpha, gpd->flag);
		}
		if (cache->batch_edlin[cache->cache_idx]) {
			if ((obact) && (obact == ob) &&
			    ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) &&
			    (v3d->gp_flag & V3D_GP_SHOW_EDIT_LINES))
			{
				DRW_shgroup_call_add(
				        stl->g_data->shgrps_edit_line,
				        cache->batch_edlin[cache->cache_idx],
				        gpf->runtime.viewmatrix);
			}
		}
		/* edit points */
		if ((gps->flag & GP_STROKE_SELECT) || (is_weight_paint)) {
			if ((gpl->flag & GP_LAYER_UNLOCK_COLOR) || ((gp_style->flag & GP_STYLE_COLOR_LOCKED) == 0)) {
				if (cache->is_dirty) {
					gpencil_batch_cache_check_free_slots(ob);
					cache->batch_edit[cache->cache_idx] = DRW_gpencil_get_edit_geom(gps, edit_alpha, gpd->flag);
				}
				if (cache->batch_edit[cache->cache_idx]) {
					if ((obact) && (obact == ob)) {
						/* edit pass */
						DRW_shgroup_call_add(
						        stl->g_data->shgrps_edit_point,
						        cache->batch_edit[cache->cache_idx],
						        gpf->runtime.viewmatrix);
					}
				}
			}
		}
	}
}

/* function to draw strokes for onion only */
static void gpencil_draw_onion_strokes(
        GpencilBatchCache *cache, GPENCIL_e_data *e_data, void *vedata, Object *ob,
        bGPdata *gpd, bGPDlayer *gpl, bGPDframe *gpf,
        const float opacity, const float tintcolor[4], const bool custonion)
{
	GPENCIL_PassList *psl = ((GPENCIL_Data *)vedata)->psl;
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	Depsgraph *depsgraph = DRW_context_state_get()->depsgraph;

	float viewmatrix[4][4];

	/* get parent matrix and save as static data */
	ED_gpencil_parent_location(depsgraph, ob, gpd, gpl, viewmatrix);
	copy_m4_m4(gpf->runtime.viewmatrix, viewmatrix);

	for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
		MaterialGPencilStyle *gp_style = BKE_material_gpencil_settings_get(ob, gps->mat_nr + 1);
		copy_v4_v4(gps->runtime.tmp_stroke_rgba, gp_style->stroke_rgba);
		copy_v4_v4(gps->runtime.tmp_fill_rgba, gp_style->fill_rgba);

		int id = stl->storage->shgroup_id;
		/* check if stroke can be drawn */
		if (gpencil_can_draw_stroke(gp_style, gps, true, false) == false) {
			continue;
		}
		/* limit the number of shading groups */
		if (id >= GPENCIL_MAX_SHGROUPS) {
			continue;
		}

		stl->shgroups[id].shgrps_fill = NULL;
		if ((gps->totpoints > 1) && (gp_style->mode == GP_STYLE_MODE_LINE)) {
			stl->shgroups[id].shgrps_stroke = DRW_gpencil_shgroup_stroke_create(
			        e_data, vedata, psl->stroke_pass, e_data->gpencil_stroke_sh, ob, gpd, gp_style, id, true);
		}
		else {
			stl->shgroups[id].shgrps_stroke = DRW_gpencil_shgroup_point_create(
			        e_data, vedata, psl->stroke_pass, e_data->gpencil_point_sh, ob, gpd, gp_style, id, true);
		}

		/* stroke */
		gpencil_add_stroke_shgroup(
		        cache, stl->shgroups[id].shgrps_stroke, ob, gpl, gpf, gps, opacity, tintcolor, true, custonion);

		stl->storage->shgroup_id++;
		cache->cache_idx++;
	}
}


/* main function to draw strokes */
static void gpencil_draw_strokes(
        GpencilBatchCache *cache, GPENCIL_e_data *e_data, void *vedata, ToolSettings *ts, Object *ob,
        bGPdata *gpd, bGPDlayer *gpl, bGPDframe *src_gpf, bGPDframe *derived_gpf,
        const float opacity, const float tintcolor[4],
        const bool custonion, tGPencilObjectCache *cache_ob)
{
	GPENCIL_PassList *psl = ((GPENCIL_Data *)vedata)->psl;
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Scene *scene = draw_ctx->scene;
	View3D *v3d = draw_ctx->v3d;
	bGPDstroke *gps, *src_gps;
	DRWShadingGroup *fillgrp;
	DRWShadingGroup *strokegrp;
	float viewmatrix[4][4];
	const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
	const bool playing = stl->storage->is_playing;
	const bool is_render = (bool)stl->storage->is_render;
	const bool is_mat_preview = (bool)stl->storage->is_mat_preview;
	const bool overlay_multiedit = v3d != NULL ? (v3d->gp_flag & V3D_GP_SHOW_MULTIEDIT_LINES) : true;

	/* Get evaluation context */
	/* NOTE: We must check if C is valid, otherwise we get crashes when trying to save files
	 * (i.e. the thumbnail offscreen rendering fails)
	 */
	Depsgraph *depsgraph = DRW_context_state_get()->depsgraph;

	/* get parent matrix and save as static data */
	ED_gpencil_parent_location(depsgraph, ob, gpd, gpl, viewmatrix);
	copy_m4_m4(derived_gpf->runtime.viewmatrix, viewmatrix);

	if ((cache_ob != NULL) && (cache_ob->is_dup_ob)) {
		copy_m4_m4(derived_gpf->runtime.viewmatrix, cache_ob->obmat);
	}

	/* apply geometry modifiers */
	if ((cache->is_dirty) && (ob->greasepencil_modifiers.first) && (!is_multiedit)) {
		if (!stl->storage->simplify_modif) {
			if (BKE_gpencil_has_geometry_modifiers(ob)) {
				BKE_gpencil_geometry_modifiers(depsgraph, ob, gpl, derived_gpf, stl->storage->is_render);
			}
		}
	}

	if (src_gpf) {
		src_gps = src_gpf->strokes.first;
	}
	else {
		src_gps = NULL;
	}

	for (gps = derived_gpf->strokes.first; gps; gps = gps->next) {
		MaterialGPencilStyle *gp_style = BKE_material_gpencil_settings_get(ob, gps->mat_nr + 1);

		/* check if stroke can be drawn */
		if (gpencil_can_draw_stroke(gp_style, gps, false, is_mat_preview) == false) {
			GP_SET_SRC_GPS(src_gps);
			continue;
		}
		/* limit the number of shading groups */
		if (stl->storage->shgroup_id >= GPENCIL_MAX_SHGROUPS) {
			GP_SET_SRC_GPS(src_gps);
			continue;
		}

		/* be sure recalc all cache in source stroke to avoid recalculation when frame change
		 * and improve fps */
		if (src_gps) {
			DRW_gpencil_recalc_geometry_caches(ob, gp_style, src_gps);
		}

		/* if the fill has any value, it's considered a fill and is not drawn if simplify fill is enabled */
		if ((stl->storage->simplify_fill) && (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_REMOVE_FILL_LINE)) {
			if ((gp_style->fill_rgba[3] > GPENCIL_ALPHA_OPACITY_THRESH) ||
			    (gp_style->fill_style > GP_STYLE_FILL_STYLE_SOLID))
			{
				GP_SET_SRC_GPS(src_gps);
				continue;
			}
		}

		if ((gpl->actframe->framenum == derived_gpf->framenum) ||
		    (!is_multiedit) || (overlay_multiedit))
		{
			int id = stl->storage->shgroup_id;
			if (gps->totpoints > 0) {
				if ((gps->totpoints > 2) && (!stl->storage->simplify_fill) &&
				    ((gp_style->fill_rgba[3] > GPENCIL_ALPHA_OPACITY_THRESH) || (gp_style->fill_style > 0)) &&
				    ((gps->flag & GP_STROKE_NOFILL) == 0) &&
				    (gp_style->flag & GP_STYLE_FILL_SHOW))
				{
					stl->shgroups[id].shgrps_fill = DRW_gpencil_shgroup_fill_create(
					        e_data, vedata, psl->stroke_pass, e_data->gpencil_fill_sh,
							gpd, gpl, gp_style, id);
				}
				else {
					stl->shgroups[id].shgrps_fill = NULL;
				}
				if ((gp_style->mode == GP_STYLE_MODE_LINE) && (gps->totpoints > 1)) {
					stl->shgroups[id].shgrps_stroke = DRW_gpencil_shgroup_stroke_create(
					        e_data, vedata, psl->stroke_pass, e_data->gpencil_stroke_sh, ob, gpd, gp_style, id, false);
				}
				else {
					stl->shgroups[id].shgrps_stroke = DRW_gpencil_shgroup_point_create(
					        e_data, vedata, psl->stroke_pass, e_data->gpencil_point_sh, ob, gpd, gp_style, id, false);
				}
			}
			else {
				stl->shgroups[id].shgrps_fill = NULL;
				stl->shgroups[id].shgrps_stroke = NULL;
			}
			stl->storage->shgroup_id++;

			fillgrp = stl->shgroups[id].shgrps_fill;
			strokegrp = stl->shgroups[id].shgrps_stroke;

			/* copy color to temp fields to apply temporal changes in the stroke */
			copy_v4_v4(gps->runtime.tmp_stroke_rgba, gp_style->stroke_rgba);
			copy_v4_v4(gps->runtime.tmp_fill_rgba, gp_style->fill_rgba);

			/* apply modifiers (only modify geometry, but not create ) */
			if ((cache->is_dirty) && (ob->greasepencil_modifiers.first) && (!is_multiedit)) {
				if (!stl->storage->simplify_modif) {
					BKE_gpencil_stroke_modifiers(depsgraph, ob, gpl, derived_gpf, gps, stl->storage->is_render);
				}
			}

			/* fill */
			if ((fillgrp) && (!stl->storage->simplify_fill)) {
				gpencil_add_fill_shgroup(
				        cache, fillgrp, ob, derived_gpf, gps,
				        opacity, tintcolor, false, custonion);
			}
			/* stroke */
			if (strokegrp) {
				const float nop = ((gp_style->flag & GP_STYLE_STROKE_SHOW) == 0) || (gp_style->stroke_rgba[3] < GPENCIL_ALPHA_OPACITY_THRESH) ? 0.0f : opacity;
				gpencil_add_stroke_shgroup(
				        cache, strokegrp, ob, gpl, derived_gpf, gps,
				        nop, tintcolor, false, custonion);
			}
		}

		/* edit points (only in edit mode and not play animation not render) */
		if ((draw_ctx->obact == ob) && (src_gps) &&
		    (!playing) && (!is_render) && (!cache_ob->is_dup_ob))
		{
			if ((gpl->flag & GP_LAYER_LOCKED) == 0) {
				if (!stl->g_data->shgrps_edit_line) {
					stl->g_data->shgrps_edit_line = DRW_shgroup_create(e_data->gpencil_line_sh, psl->edit_pass);
				}
				if (!stl->g_data->shgrps_edit_point) {
					stl->g_data->shgrps_edit_point = DRW_shgroup_create(e_data->gpencil_edit_point_sh, psl->edit_pass);
					const float *viewport_size = DRW_viewport_size_get();
					DRW_shgroup_uniform_vec2(stl->g_data->shgrps_edit_point, "Viewport", viewport_size, 1);
				}

				gpencil_add_editpoints_shgroup(stl, cache, ts, ob, gpd, gpl, derived_gpf, src_gps);
			}
			else {
				gpencil_batch_cache_check_free_slots(ob);
			}
		}

		GP_SET_SRC_GPS(src_gps);

		cache->cache_idx++;
	}
}

 /* draw stroke in drawing buffer */
void DRW_gpencil_populate_buffer_strokes(GPENCIL_e_data *e_data, void *vedata, ToolSettings *ts, Object *ob)
{
	GPENCIL_PassList *psl = ((GPENCIL_Data *)vedata)->psl;
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	Brush *brush = BKE_paint_brush(&ts->gp_paint->paint);
	bGPdata *gpd_eval = ob->data;
	/* need the original to avoid cow overhead while drawing */
	bGPdata *gpd = (bGPdata *)DEG_get_original_id(&gpd_eval->id);

	MaterialGPencilStyle *gp_style = NULL;

	float obscale = mat4_to_scale(ob->obmat);

	/* use the brush material */
	Material *ma = BKE_gpencil_get_material_from_brush(brush);
	if (ma != NULL) {
		gp_style = ma->gp_style;
	}
	/* this is not common, but avoid any special situations when brush could be without material */
	if (gp_style == NULL) {
		gp_style = BKE_material_gpencil_settings_get(ob, ob->actcol);
	}

	/* drawing strokes */
	/* Check if may need to draw the active stroke cache, only if this layer is the active layer
	 * that is being edited. (Stroke buffer is currently stored in gp-data)
	 */
	if (ED_gpencil_session_active() && (gpd->runtime.sbuffer_size > 0)) {
		if ((gpd->runtime.sbuffer_sflag & GP_STROKE_ERASER) == 0) {
			/* It should also be noted that sbuffer contains temporary point types
			 * i.e. tGPspoints NOT bGPDspoints
			 */
			short lthick = brush->size * obscale;
			/* if only one point, don't need to draw buffer because the user has no time to see it */
			if (gpd->runtime.sbuffer_size > 1) {
				if ((gp_style) && (gp_style->mode == GP_STYLE_MODE_LINE)) {
					stl->g_data->shgrps_drawing_stroke = DRW_gpencil_shgroup_stroke_create(
					        e_data, vedata, psl->drawing_pass, e_data->gpencil_stroke_sh, NULL, gpd, gp_style, -1, false);
				}
				else {
					stl->g_data->shgrps_drawing_stroke = DRW_gpencil_shgroup_point_create(
					        e_data, vedata, psl->drawing_pass, e_data->gpencil_point_sh, NULL, gpd, gp_style, -1, false);
				}

				/* clean previous version of the batch */
				if (stl->storage->buffer_stroke) {
					GPU_BATCH_DISCARD_SAFE(e_data->batch_buffer_stroke);
					MEM_SAFE_FREE(e_data->batch_buffer_stroke);
					stl->storage->buffer_stroke = false;
				}

				/* use unit matrix because the buffer is in screen space and does not need conversion */
				if (gpd->runtime.mode == GP_STYLE_MODE_LINE) {
					e_data->batch_buffer_stroke = DRW_gpencil_get_buffer_stroke_geom(
					        gpd, lthick);
				}
				else {
					e_data->batch_buffer_stroke = DRW_gpencil_get_buffer_point_geom(
					        gpd, lthick);
				}

				if (gp_style->flag & GP_STYLE_STROKE_SHOW) {
					DRW_shgroup_call_add(
						stl->g_data->shgrps_drawing_stroke,
						e_data->batch_buffer_stroke,
						stl->storage->unit_matrix);
				}

				if ((gpd->runtime.sbuffer_size >= 3) &&
				    (gpd->runtime.sfill[3] > GPENCIL_ALPHA_OPACITY_THRESH) &&
				    ((gpd->runtime.sbuffer_sflag & GP_STROKE_NOFILL) == 0) &&
				    ((brush->gpencil_settings->flag & GP_BRUSH_DISSABLE_LASSO) == 0) &&
				    (gp_style->flag & GP_STYLE_FILL_SHOW))
				{
					/* if not solid, fill is simulated with solid color */
					if (gpd->runtime.bfill_style > 0) {
						gpd->runtime.sfill[3] = 0.5f;
					}
					stl->g_data->shgrps_drawing_fill = DRW_shgroup_create(
					        e_data->gpencil_drawing_fill_sh, psl->drawing_pass);

					/* clean previous version of the batch */
					if (stl->storage->buffer_fill) {
						GPU_BATCH_DISCARD_SAFE(e_data->batch_buffer_fill);
						MEM_SAFE_FREE(e_data->batch_buffer_fill);
						stl->storage->buffer_fill = false;
					}

					e_data->batch_buffer_fill = DRW_gpencil_get_buffer_fill_geom(gpd);
					DRW_shgroup_call_add(
					        stl->g_data->shgrps_drawing_fill,
					        e_data->batch_buffer_fill,
					        stl->storage->unit_matrix);
					stl->storage->buffer_fill = true;
				}
				stl->storage->buffer_stroke = true;
			}
		}
	}
}

/* get alpha factor for onion strokes */
static void gpencil_get_onion_alpha(float color[4], bGPdata *gpd)
{
#define MIN_ALPHA_VALUE 0.01f

	/* if fade is disabled, opacity is equal in all frames */
	if ((gpd->onion_flag & GP_ONION_FADE) == 0) {
		color[3] = gpd->onion_factor;
	}
	else {
		/* add override opacity factor */
		color[3] += gpd->onion_factor - 0.5f;
	}

	CLAMP(color[3], MIN_ALPHA_VALUE, 1.0f);
}

/* draw onion-skinning for a layer */
static void gpencil_draw_onionskins(
	GpencilBatchCache *cache, GPENCIL_e_data *e_data, void *vedata,
	Object *ob, bGPdata *gpd, bGPDlayer *gpl, bGPDframe *gpf)
{

	const float default_color[3] = { UNPACK3(U.gpencil_new_layer_col) };
	const float alpha = 1.0f;
	float color[4];
	int idx;
	float fac = 1.0f;
	int step = 0;
	int mode = 0;
	bool colflag = false;
	bGPDframe *gpf_loop = NULL;
	int last = gpf->framenum;

	colflag = (bool)gpd->onion_flag & GP_ONION_GHOST_PREVCOL;


	/* -------------------------------
	 * 1) Draw Previous Frames First
	 * ------------------------------- */
	step = gpd->gstep;
	mode = gpd->onion_mode;

	if (gpd->onion_flag & GP_ONION_GHOST_PREVCOL) {
		copy_v3_v3(color, gpd->gcolor_prev);
	}
	else {
		copy_v3_v3(color, default_color);
	}

	idx = 0;
	for (bGPDframe *gf = gpf->prev; gf; gf = gf->prev) {
		/* only selected frames */
		if ((mode == GP_ONION_MODE_SELECTED) && ((gf->flag & GP_FRAME_SELECT) == 0)) {
			continue;
		}
		/* absolute range */
		if (mode == GP_ONION_MODE_ABSOLUTE) {
			if ((gpf->framenum - gf->framenum) > step) {
				break;
			}
		}
		/* relative range */
		if (mode == GP_ONION_MODE_RELATIVE) {
			idx++;
			if (idx > step) {
				break;
			}

		}
		/* alpha decreases with distance from curframe index */
		if (mode != GP_ONION_MODE_SELECTED) {
			if (mode == GP_ONION_MODE_ABSOLUTE) {
				fac = 1.0f - ((float)(gpf->framenum - gf->framenum) / (float)(step + 1));
			}
			else {
				fac = 1.0f - ((float)idx / (float)(step + 1));
			}
			color[3] = alpha * fac * 0.66f;
		}
		else {
			idx++;
			fac = alpha - ((1.1f - (1.0f / (float)idx)) * 0.66f);
			color[3] = fac;
		}

		/* if loop option, save the frame to use later */
		if ((mode != GP_ONION_MODE_ABSOLUTE) && (gpd->onion_flag & GP_ONION_LOOP)) {
			gpf_loop = gf;
		}

		gpencil_get_onion_alpha(color, gpd);
		gpencil_draw_onion_strokes(cache, e_data, vedata, ob, gpd, gpl, gf, color[3], color, colflag);
	}
	/* -------------------------------
	 * 2) Now draw next frames
	 * ------------------------------- */
	step = gpd->gstep_next;
	mode = gpd->onion_mode;

	if (gpd->onion_flag & GP_ONION_GHOST_NEXTCOL) {
		copy_v3_v3(color, gpd->gcolor_next);
	}
	else {
		copy_v3_v3(color, default_color);
	}

	idx = 0;
	for (bGPDframe *gf = gpf->next; gf; gf = gf->next) {
		/* only selected frames */
		if ((mode == GP_ONION_MODE_SELECTED) && ((gf->flag & GP_FRAME_SELECT) == 0)) {
			continue;
		}
		/* absolute range */
		if (mode == GP_ONION_MODE_ABSOLUTE) {
			if ((gf->framenum - gpf->framenum) > step) {
				break;
			}
		}
		/* relative range */
		if (mode == GP_ONION_MODE_RELATIVE) {
			idx++;
			if (idx > step) {
				break;
			}

		}
		/* alpha decreases with distance from curframe index */
		if (mode != GP_ONION_MODE_SELECTED) {
			if (mode == GP_ONION_MODE_ABSOLUTE) {
				fac = 1.0f - ((float)(gf->framenum - gpf->framenum) / (float)(step + 1));
			}
			else {
				fac = 1.0f - ((float)idx / (float)(step + 1));
			}
			color[3] = alpha * fac * 0.66f;
		}
		else {
			idx++;
			fac = alpha - ((1.1f - (1.0f / (float)idx)) * 0.66f);
			color[3] = fac;
		}

		gpencil_get_onion_alpha(color, gpd);
		gpencil_draw_onion_strokes(cache, e_data, vedata, ob, gpd, gpl, gf, color[3], color, colflag);
		if (last < gf->framenum) {
			last = gf->framenum;
		}
	}

	/* Draw first frame in blue for loop mode */
	if ((gpd->onion_flag & GP_ONION_LOOP) && (gpf_loop != NULL)) {
		if ((last == gpf->framenum) || (gpf->next == NULL)) {
			gpencil_get_onion_alpha(color, gpd);
			gpencil_draw_onion_strokes(
				cache, e_data, vedata, ob, gpd, gpl,
				gpf_loop, color[3], color, colflag);
		}
	}
}

/* populate a datablock for multiedit (no onions, no modifiers) */
void DRW_gpencil_populate_multiedit(
        GPENCIL_e_data *e_data, void *vedata, Scene *scene, Object *ob,
        tGPencilObjectCache *cache_ob)
{
	bGPdata *gpd = (bGPdata *)ob->data;
	bGPDframe *gpf = NULL;

	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	int cfra_eval = (int)DEG_get_ctime(draw_ctx->depsgraph);
	GpencilBatchCache *cache = gpencil_batch_cache_get(ob, cfra_eval);
	ToolSettings *ts = scene->toolsettings;
	cache->cache_idx = 0;

	/* check if playing animation */
	bool playing = stl->storage->is_playing;

	/* draw strokes */
	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		/* don't draw layer if hidden */
		if (gpl->flag & GP_LAYER_HIDE)
			continue;

		/* list of frames to draw */
		if (!playing) {
			for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
				if ((gpf == gpl->actframe) || (gpf->flag & GP_FRAME_SELECT)) {
					gpencil_draw_strokes(
					        cache, e_data, vedata, ts, ob, gpd, gpl, gpf, gpf,
					        gpl->opacity, gpl->tintcolor, false, cache_ob);
				}
			}
		}
		else {
			gpf = BKE_gpencil_layer_getframe(gpl, cfra_eval, GP_GETFRAME_USE_PREV);
			if (gpf) {
				gpencil_draw_strokes(
				        cache, e_data, vedata, ts, ob, gpd, gpl, gpf, gpf,
				        gpl->opacity, gpl->tintcolor, false, cache_ob);
			}
		}

	}

	cache->is_dirty = false;
}

static void gpencil_copy_frame(bGPDframe *gpf, bGPDframe *derived_gpf)
{
	derived_gpf->prev = gpf->prev;
	derived_gpf->next = gpf->next;
	derived_gpf->framenum = gpf->framenum;
	derived_gpf->flag = gpf->flag;
	derived_gpf->key_type = gpf->key_type;
	derived_gpf->runtime = gpf->runtime;

	/* copy strokes */
	BLI_listbase_clear(&derived_gpf->strokes);
	for (bGPDstroke *gps_src = gpf->strokes.first; gps_src; gps_src = gps_src->next) {
		/* make copy of source stroke */
		bGPDstroke *gps_dst = BKE_gpencil_stroke_duplicate(gps_src);
		BLI_addtail(&derived_gpf->strokes, gps_dst);
	}
}

/* helper for populate a complete grease pencil datablock */
void DRW_gpencil_populate_datablock(
        GPENCIL_e_data *e_data, void *vedata,
        Scene *scene, Object *ob,
        tGPencilObjectCache *cache_ob)
{
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	const ViewLayer *view_layer = DEG_get_evaluated_view_layer(draw_ctx->depsgraph);

	bGPdata *gpd_eval = (bGPdata *)ob->data;
	bGPdata *gpd = (bGPdata *)DEG_get_original_id(&gpd_eval->id);

	View3D *v3d = draw_ctx->v3d;
	int cfra_eval = (int)DEG_get_ctime(draw_ctx->depsgraph);
	ToolSettings *ts = scene->toolsettings;

	bGPDframe *derived_gpf = NULL;
	const bool main_onion = v3d != NULL ? (v3d->gp_flag & V3D_GP_SHOW_ONION_SKIN) : true;
	const bool do_onion = (bool)((gpd->flag & GP_DATA_STROKE_WEIGHTMODE) == 0) && main_onion;
	const bool overlay = v3d != NULL ? (bool)((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) : true;
	const bool time_remap = BKE_gpencil_has_time_modifiers(ob);

	float opacity;
	bGPDframe *p = NULL;
	bGPDframe *gpf = NULL;
	bGPDlayer *gpl_active = BKE_gpencil_layer_getactive(gpd);

	/* check if playing animation */
	bool playing = stl->storage->is_playing;

	GpencilBatchCache *cache = gpencil_batch_cache_get(ob, cfra_eval);
	cache->cache_idx = 0;

	/* init general modifiers data */
	if (!stl->storage->simplify_modif) {
		if ((cache->is_dirty) && (ob->greasepencil_modifiers.first)) {
			BKE_gpencil_lattice_init(ob);
		}
	}
	/* draw normal strokes */
	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		/* don't draw layer if hidden */
		if (gpl->flag & GP_LAYER_HIDE)
			continue;

		/* filter view layer to gp layers in the same view layer (for compo) */
		if ((stl->storage->is_render) && (gpl->viewlayername[0] != '\0')) {
			if (!STREQ(view_layer->name, gpl->viewlayername)) {
				continue;
			}
		}

		if ((!time_remap) || (stl->storage->simplify_modif)) {
			gpf = BKE_gpencil_layer_getframe(gpl, cfra_eval, GP_GETFRAME_USE_PREV);
		}
		else {
			int remap_cfra = BKE_gpencil_time_modifier(
			        draw_ctx->depsgraph, scene, ob, gpl, cfra_eval,
			        stl->storage->is_render);

			gpf = BKE_gpencil_layer_getframe(gpl, remap_cfra, GP_GETFRAME_USE_PREV);
		}
		if (gpf == NULL)
			continue;

		opacity = gpl->opacity;
		/* if pose mode, maybe the overlay to fade geometry is enabled */
		if ((draw_ctx->obact) && (draw_ctx->object_mode == OB_MODE_POSE) &&
		    (v3d->overlay.flag & V3D_OVERLAY_BONE_SELECT))
		{
			opacity = opacity * v3d->overlay.bone_select_alpha;
		}
		/* fade no active layers */
		if ((overlay) && (draw_ctx->object_mode == OB_MODE_GPENCIL_PAINT) &&
		    (v3d->gp_flag & V3D_GP_FADE_NOACTIVE_LAYERS) &&
		    (draw_ctx->obact) && (draw_ctx->obact == ob) &&
		    (gpl != gpl_active))
		{
			opacity = opacity * v3d->overlay.gpencil_fade_layer;
		}

		/* create derived array data or expand */
		if (cache_ob->data_idx + 1 > gpl->runtime.len_derived) {
			if ((gpl->runtime.len_derived == 0) ||
			    (gpl->runtime.derived_array == NULL))
			{
				p = MEM_callocN(sizeof(struct bGPDframe), "bGPDframe array");
				gpl->runtime.len_derived = 1;
			}
			else {
				gpl->runtime.len_derived++;
				p = MEM_recallocN(gpl->runtime.derived_array, sizeof(struct bGPDframe) * gpl->runtime.len_derived);
			}
			gpl->runtime.derived_array = p;

			derived_gpf = &gpl->runtime.derived_array[cache_ob->data_idx];
		}

		derived_gpf = &gpl->runtime.derived_array[cache_ob->data_idx];

		/* if no derived frame or dirty cache, create a new one */
		if ((derived_gpf == NULL) || (cache->is_dirty)) {
			if (derived_gpf != NULL) {
				/* first clear temp data */
				BKE_gpencil_free_frame_runtime_data(derived_gpf);
			}
			/* create new data (do not assign new memory)*/
			gpencil_copy_frame(gpf, derived_gpf);
		}

		/* draw onion skins */
		if (!ID_IS_LINKED(&gpd->id)) {
			if ((!cache_ob->is_dup_data) &&
			    (gpd->flag & GP_DATA_SHOW_ONIONSKINS) &&
			    (do_onion) && (gpl->onion_flag & GP_LAYER_ONIONSKIN) &&
			    ((!playing) || (gpd->onion_flag & GP_ONION_GHOST_ALWAYS)) &&
			    (!cache_ob->is_dup_ob) && (gpd->id.us <= 1))
			{
				if (((!stl->storage->is_render) && (overlay)) ||
				    ((stl->storage->is_render) && (gpd->onion_flag & GP_ONION_GHOST_ALWAYS)))
				{
					gpencil_draw_onionskins(cache, e_data, vedata, ob, gpd, gpl, gpf);
				}
			}
		}
		/* draw normal strokes */
		if (!cache_ob->is_dup_ob) {
			/* save batch index */
			gpl->runtime.batch_index = cache->cache_idx;
		}
		else {
			cache->cache_idx = gpl->runtime.batch_index;
		}

		gpencil_draw_strokes(
			cache, e_data, vedata, ts, ob, gpd, gpl, gpf, derived_gpf,
			opacity, gpl->tintcolor, false, cache_ob);

	}

	/* clear any lattice data */
	if ((cache->is_dirty) && (ob->greasepencil_modifiers.first)) {
		BKE_gpencil_lattice_clear(ob);
	}

	cache->is_dirty = false;
}
