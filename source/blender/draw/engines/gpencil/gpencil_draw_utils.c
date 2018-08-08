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
        const float(*points2d)[2], int totpoints, float minv[2], float maxv[2], bool expand)
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
	/* If not expanded, use a perfect square */
	if (expand == false) {
		if (maxv[0] > maxv[1]) {
			maxv[1] = maxv[0];
		}
		else {
			maxv[0] = maxv[1];
		}
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
void DRW_gpencil_triangulate_stroke_fill(bGPDstroke *gps)
{
	BLI_assert(gps->totpoints >= 3);

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
	gpencil_calc_2d_bounding_box(points2d, gps->totpoints, minv, maxv, false);
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
				DRW_gpencil_triangulate_stroke_fill(gps);
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
        GPUShader *shader, bGPdata *gpd, MaterialGPencilStyle *gp_style, int id)
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
	DRW_shgroup_uniform_float(grp, "pixelsize", &U.pixelsize, 1);

	/* avoid wrong values */
	if ((gpd) && (gpd->pixfactor == 0)) {
		gpd->pixfactor = GP_DEFAULT_PIX_FACTOR;
	}

	/* object scale and depth */
	if ((ob) && (id > -1)) {
		stl->shgroups[id].obj_scale = (ob->size[0] + ob->size[1] + ob->size[2]) / 3.0f;
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
	DRW_shgroup_uniform_float(grp, "pixelsize", &U.pixelsize, 1);

	/* avoid wrong values */
	if ((gpd) && (gpd->pixfactor == 0)) {
		gpd->pixfactor = GP_DEFAULT_PIX_FACTOR;
	}

	/* object scale and depth */
	if ((ob) && (id > -1)) {
		stl->shgroups[id].obj_scale = (ob->size[0] + ob->size[1] + ob->size[2]) / 3.0f;
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
        Object *ob, bGPDlayer *gpl, bGPDframe *gpf, bGPDstroke *gps,
        const float tintcolor[4], const bool onion, const bool custonion)
{
	MaterialGPencilStyle *gp_style = BKE_material_gpencil_settings_get(ob, gps->mat_nr + 1);
	if (gps->totpoints >= 3) {
		float tfill[4];
		/* set color using material, tint color and opacity */
		interp_v3_v3v3(tfill, gps->runtime.tmp_fill_rgba, tintcolor, tintcolor[3]);
		tfill[3] = gps->runtime.tmp_fill_rgba[3] * gpl->opacity;
		if ((tfill[3] > GPENCIL_ALPHA_OPACITY_THRESH) || (gp_style->fill_style > 0)) {
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
			if (cache->is_dirty) {
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
	const float opacity, const float tintcolor[4], const bool onion, const bool custonion)
{
	float tcolor[4];
	float ink[4];
	short sthickness;
	MaterialGPencilStyle *gp_style = BKE_material_gpencil_settings_get(ob, gps->mat_nr + 1);

	/* set color using base color, tint color and opacity */
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
	if (cache->is_dirty) {
		gpencil_batch_cache_check_free_slots(ob);
		if ((gps->totpoints > 1) && (gp_style->mode == GP_STYLE_MODE_LINE)) {
			cache->batch_stroke[cache->cache_idx] = DRW_gpencil_get_stroke_geom(gpf, gps, sthickness, ink);
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
        const float opacity, const float tintcolor[4], const bool custonion)
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
			continue;
		}
		/* limit the number of shading groups */
		if (stl->storage->shgroup_id >= GPENCIL_MAX_SHGROUPS) {
			continue;
		}

		/* be sure recalc all chache in source stroke to avoid recalculation when frame change
		 * and improve fps */
		if (src_gps) {
			DRW_gpencil_recalc_geometry_caches(ob, gp_style, src_gps);
		}

		/* if the fill has any value, it's considered a fill and is not drawn if simplify fill is enabled */
		if ((stl->storage->simplify_fill) && (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_REMOVE_FILL_LINE)) {
			if ((gp_style->fill_rgba[3] > GPENCIL_ALPHA_OPACITY_THRESH) ||
			    (gp_style->fill_style > GP_STYLE_FILL_STYLE_SOLID))
			{
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
				    ((gps->flag & GP_STROKE_NOFILL) == 0))
				{
					stl->shgroups[id].shgrps_fill = DRW_gpencil_shgroup_fill_create(
					        e_data, vedata, psl->stroke_pass, e_data->gpencil_fill_sh, gpd, gp_style, id);
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
				        cache, fillgrp, ob, gpl, derived_gpf, gps, tintcolor, false, custonion);
			}
			/* stroke */
			if (strokegrp) {
				gpencil_add_stroke_shgroup(
				        cache, strokegrp, ob, gpl, derived_gpf, gps, opacity, tintcolor, false, custonion);
			}
		}

		/* edit points (only in edit mode and not play animation not render) */
		if ((src_gps) && (!playing) && (!is_render)) {
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

		if (src_gps) {
			src_gps = src_gps->next;
		}

		cache->cache_idx++;
	}
}

 /* draw stroke in drawing buffer */
void DRW_gpencil_populate_buffer_strokes(GPENCIL_e_data *e_data, void *vedata, ToolSettings *ts, Object *ob)
{
	GPENCIL_PassList *psl = ((GPENCIL_Data *)vedata)->psl;
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	Brush *brush = BKE_brush_getactive_gpencil(ts);
	bGPdata *gpd = ob->data;
	MaterialGPencilStyle *gp_style = NULL;

	float obscale = (ob->size[0] + ob->size[1] + ob->size[2]) / 3.0f;

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

				/* use unit matrix because the buffer is in screen space and does not need conversion */
				if (gpd->runtime.mode == GP_STYLE_MODE_LINE) {
					stl->g_data->batch_buffer_stroke = DRW_gpencil_get_buffer_stroke_geom(
					        gpd, stl->storage->unit_matrix, lthick);
				}
				else {
					stl->g_data->batch_buffer_stroke = DRW_gpencil_get_buffer_point_geom(
					        gpd, stl->storage->unit_matrix, lthick);
				}

				DRW_shgroup_call_add(
				        stl->g_data->shgrps_drawing_stroke,
				        stl->g_data->batch_buffer_stroke,
				        stl->storage->unit_matrix);

				if ((gpd->runtime.sbuffer_size >= 3) && (gpd->runtime.sfill[3] > GPENCIL_ALPHA_OPACITY_THRESH) &&
				    ((gpd->runtime.sbuffer_sflag & GP_STROKE_NOFILL) == 0))
				{
					/* if not solid, fill is simulated with solid color */
					if (gpd->runtime.bfill_style > 0) {
						gpd->runtime.sfill[3] = 0.5f;
					}
					stl->g_data->shgrps_drawing_fill = DRW_shgroup_create(
					        e_data->gpencil_drawing_fill_sh, psl->drawing_pass);
					stl->g_data->batch_buffer_fill = DRW_gpencil_get_buffer_fill_geom(gpd);
					DRW_shgroup_call_add(
					        stl->g_data->shgrps_drawing_fill,
					        stl->g_data->batch_buffer_fill,
					        stl->storage->unit_matrix);
				}
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
void DRW_gpencil_populate_multiedit(GPENCIL_e_data *e_data, void *vedata, Scene *scene, Object *ob, bGPdata *gpd)
{
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
					        gpl->opacity, gpl->tintcolor, false);
				}
			}
		}
		else {
			gpf = BKE_gpencil_layer_getframe(gpl, cfra_eval, 0);
			if (gpf) {
				gpencil_draw_strokes(
				        cache, e_data, vedata, ts, ob, gpd, gpl, gpf, gpf,
				        gpl->opacity, gpl->tintcolor, false);
			}
		}

	}

	cache->is_dirty = false;
}

/* helper for populate a complete grease pencil datablock */
void DRW_gpencil_populate_datablock(GPENCIL_e_data *e_data, void *vedata, Scene *scene, Object *ob, bGPdata *gpd)
{
	GPENCIL_StorageList *stl = ((GPENCIL_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	View3D *v3d = draw_ctx->v3d;
	int cfra_eval = (int)DEG_get_ctime(draw_ctx->depsgraph);
	ToolSettings *ts = scene->toolsettings;
	bGPDframe *derived_gpf = NULL;
	const bool main_onion = v3d != NULL ? ((v3d->gp_flag & V3D_GP_SHOW_ONION_SKIN) == 0) : true;
	const bool no_onion = (bool)(gpd->flag & GP_DATA_STROKE_WEIGHTMODE) || main_onion;
	const bool overlay = v3d != NULL ? (bool)((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) : true;

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

		bGPDframe *gpf = BKE_gpencil_layer_getframe(gpl, cfra_eval, 0);
		if (gpf == NULL)
			continue;

		/* create GHash if need */
		if (gpl->runtime.derived_data == NULL) {
			gpl->runtime.derived_data = (GHash *)BLI_ghash_str_new(gpl->info);
		}

		derived_gpf = BLI_ghash_lookup(gpl->runtime.derived_data, ob->id.name);
		if (derived_gpf == NULL) {
			cache->is_dirty = true;
		}
		if (cache->is_dirty) {
			if (derived_gpf != NULL) {
				/* first clear temp data */
				BKE_gpencil_free_frame_runtime_data(derived_gpf);
				BLI_ghash_remove(gpl->runtime.derived_data, ob->id.name, NULL, NULL);
			}
			/* create new data */
			derived_gpf = BKE_gpencil_frame_duplicate(gpf);
			BLI_ghash_insert(gpl->runtime.derived_data, ob->id.name, derived_gpf);
		}

		/* draw onion skins */
		if ((gpd->flag & GP_DATA_SHOW_ONIONSKINS) &&
		    (!no_onion) && (overlay) &&
		    (gpl->onion_flag & GP_LAYER_ONIONSKIN) &&
		    ((!playing) || (gpd->onion_flag & GP_ONION_GHOST_ALWAYS)))
		{
			if ((!stl->storage->is_render) ||
			    ((stl->storage->is_render) && (gpd->onion_flag & GP_ONION_GHOST_ALWAYS)))
			{
				gpencil_draw_onionskins(cache, e_data, vedata, ob, gpd, gpl, gpf);
			}
		}

		/* draw normal strokes */
		gpencil_draw_strokes(
		        cache, e_data, vedata, ts, ob, gpd, gpl, gpf, derived_gpf,
		        gpl->opacity, gpl->tintcolor, false);

	}

	/* clear any lattice data */
	if ((cache->is_dirty) && (ob->greasepencil_modifiers.first)) {
		BKE_gpencil_lattice_clear(ob);
	}

	cache->is_dirty = false;
}

/* Helper for gpencil_instance_modifiers()
 * See also MOD_gpencilinstance.c -> bakeModifier()
 */
static void gp_instance_modifier_make_instances(GPENCIL_StorageList *stl, Object *ob, InstanceGpencilModifierData *mmd)
{
	/* reset random */
	mmd->rnd[0] = 1;
	char buf[8];
	int e = 0;

	/* Generate instances */
	for (int x = 0; x < mmd->count[0]; x++) {
		for (int y = 0; y < mmd->count[1]; y++) {
			for (int z = 0; z < mmd->count[2]; z++) {
				Object *newob;

				const int elem_idx[3] = {x, y, z};
				float mat[4][4];
				int sh;

				/* original strokes are at index = 0,0,0 */
				if ((x == 0) && (y == 0) && (z == 0)) {
					continue;
				}

				/* compute transform for instance */
				BKE_gpencil_instance_modifier_instance_tfm(mmd, elem_idx, mat);

				/* add object to cache */
				newob = MEM_dupallocN(ob);

				/* create a unique name or the object hash used in draw will fail.
				 * the name must be unique in the hash, not in the scene because
				 * the object never is linked to scene.
				 */
				sprintf(buf, "___%d", e++);
				strncat(newob->id.name, buf, sizeof(newob->id.name));

				mul_m4_m4m4(newob->obmat, ob->obmat, mat);

				/* apply scale */
				ARRAY_SET_ITEMS(newob->size, mat[0][0], mat[1][1], mat[2][2]);

				/* apply shift */
				sh = x;
				if (mmd->lock_axis == GP_LOCKAXIS_Y) {
					sh = y;
				}
				if (mmd->lock_axis == GP_LOCKAXIS_Z) {
					sh = z;
				}
				madd_v3_v3fl(newob->obmat[3], mmd->shift, sh);

				/* add temp object to cache */
				stl->g_data->gp_object_cache = gpencil_object_cache_add(
				        stl->g_data->gp_object_cache, newob, true,
				        &stl->g_data->gp_cache_size, &stl->g_data->gp_cache_used);
			}
		}
	}
}

/* create instances using instance modifiers */
void gpencil_instance_modifiers(GPENCIL_StorageList *stl, Object *ob)
{
	if ((ob) && (ob->data)) {
		bGPdata *gpd = ob->data;
		if (GPENCIL_ANY_EDIT_MODE(gpd)) {
			return;
		}
	}

	for (GpencilModifierData *md = ob->greasepencil_modifiers.first; md; md = md->next) {
		if (((md->mode & eGpencilModifierMode_Realtime) && (stl->storage->is_render == false)) ||
		    ((md->mode & eGpencilModifierMode_Render) && (stl->storage->is_render == true)))
		{
			if (md->type == eGpencilModifierType_Instance) {
				InstanceGpencilModifierData *mmd = (InstanceGpencilModifierData *)md;

				/* Only add instances if the "Make Objects" flag is set
				 * FIXME: This is a workaround for z-ordering weirdness when all instances are in the same object
				 */
				if (mmd->flag & GP_INSTANCE_MAKE_OBJECTS) {
					gp_instance_modifier_make_instances(stl, ob, mmd);
				}
			}
		}
	}
}
