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

/** \file blender/draw/engines/gpencil/gpencil_cache_utils.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "ED_gpencil.h"
#include "ED_view3d.h"

#include "DNA_gpencil_types.h"
#include "DNA_view3d_types.h"

#include "BKE_gpencil.h"

#include "gpencil_engine.h"

#include "draw_cache_impl.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

 /* add a gpencil object to cache to defer drawing */
tGPencilObjectCache *gpencil_object_cache_add(
        tGPencilObjectCache *cache_array, Object *ob,
        int *gp_cache_size, int *gp_cache_used)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	tGPencilObjectCache *cache_elem = NULL;
	RegionView3D *rv3d = draw_ctx->rv3d;
	tGPencilObjectCache *p = NULL;

	/* By default a cache is created with one block with a predefined number of free slots,
	if the size is not enough, the cache is reallocated adding a new block of free slots.
	This is done in order to keep cache small */
	if (*gp_cache_used + 1 > *gp_cache_size) {
		if ((*gp_cache_size == 0) || (cache_array == NULL)) {
			p = MEM_callocN(sizeof(struct tGPencilObjectCache) * GP_CACHE_BLOCK_SIZE, "tGPencilObjectCache");
			*gp_cache_size = GP_CACHE_BLOCK_SIZE;
		}
		else {
			*gp_cache_size += GP_CACHE_BLOCK_SIZE;
			p = MEM_recallocN(cache_array, sizeof(struct tGPencilObjectCache) * *gp_cache_size);
		}
		cache_array = p;
	}
	/* zero out all pointers */
	cache_elem = &cache_array[*gp_cache_used];
	memset(cache_elem, 0, sizeof(*cache_elem));

	Object *ob_orig = (Object *)DEG_get_original_id(&ob->id);
	cache_elem->ob = ob_orig;
	cache_elem->gpd = (bGPdata *)ob_orig->data;
	copy_v3_v3(cache_elem->loc, ob->loc);
	copy_m4_m4(cache_elem->obmat, ob->obmat);
	cache_elem->idx = *gp_cache_used;

	/* object is duplicated (particle) */
	cache_elem->is_dup_ob = ob->base_flag & BASE_FROMDUPLI;

	/* save FXs */
	cache_elem->pixfactor = cache_elem->gpd->pixfactor;
	cache_elem->shader_fx = ob_orig->shader_fx;

	/* shgrp array */
	cache_elem->tot_layers = 0;
	int totgpl = BLI_listbase_count(&cache_elem->gpd->layers);
	if (totgpl > 0) {
		cache_elem->shgrp_array = MEM_callocN(sizeof(tGPencilObjectCache_shgrp) * totgpl, __func__);
	}

	/* calculate zdepth from point of view */
	float zdepth = 0.0;
	if (rv3d) {
		if (rv3d->is_persp) {
			zdepth = ED_view3d_calc_zfac(rv3d, ob->loc, NULL);
		}
		else {
			zdepth = -dot_v3v3(rv3d->viewinv[2], ob->loc);
		}
	}
	else {
		/* In render mode, rv3d is not available, so use the distance to camera.
		 * The real distance is not important, but the relative distance to the camera plane
		 * in order to sort by z_depth of the objects
		 */
		float vn[3] = { 0.0f, 0.0f, -1.0f }; /* always face down */
		float plane_cam[4];
		struct Object *camera = draw_ctx->scene->camera;
		if (camera) {
			mul_m4_v3(camera->obmat, vn);
			normalize_v3(vn);
			plane_from_point_normal_v3(plane_cam, camera->loc, vn);
			zdepth = dist_squared_to_plane_v3(ob->loc, plane_cam);
		}
	}
	cache_elem->zdepth = zdepth;
	/* increase slots used in cache */
	(*gp_cache_used)++;

	return cache_array;
}

/* add a shading group to the cache to create later */
GpencilBatchGroup *gpencil_group_cache_add(
	GpencilBatchGroup *cache_array,
	bGPDlayer *gpl, bGPDframe *gpf, bGPDstroke *gps,
	const short type, const bool onion,
	const int vertex_idx,
	int *grp_size, int *grp_used)
{
	GpencilBatchGroup *cache_elem = NULL;
	GpencilBatchGroup *p = NULL;

	/* By default a cache is created with one block with a predefined number of free slots,
	if the size is not enough, the cache is reallocated adding a new block of free slots.
	This is done in order to keep cache small */
	if (*grp_used + 1 > *grp_size) {
		if ((*grp_size == 0) || (cache_array == NULL)) {
			p = MEM_callocN(sizeof(struct GpencilBatchGroup) * GPENCIL_GROUPS_BLOCK_SIZE, "GpencilBatchGroup");
			*grp_size = GPENCIL_GROUPS_BLOCK_SIZE;
		}
		else {
			*grp_size += GPENCIL_GROUPS_BLOCK_SIZE;
			p = MEM_recallocN(cache_array, sizeof(struct GpencilBatchGroup) * *grp_size);
		}
		cache_array = p;
	}
	/* zero out all data */
	cache_elem = &cache_array[*grp_used];
	memset(cache_elem, 0, sizeof(*cache_elem));

	cache_elem->gpl = gpl;
	cache_elem->gpf = gpf;
	cache_elem->gps = gps;
	cache_elem->type = type;
	cache_elem->onion = onion;
	cache_elem->vertex_idx = vertex_idx;

	/* increase slots used in cache */
	(*grp_used)++;

	return cache_array;
}

/* get current cache data */
static GpencilBatchCache *gpencil_batch_get_element(Object *ob)
{
	Object *ob_orig = (Object *)DEG_get_original_id(&ob->id);
	return ob_orig->runtime.gpencil_cache;
}

/* verify if cache is valid */
static bool gpencil_batch_cache_valid(GpencilBatchCache *cache, bGPdata *gpd, int cfra)
{
	bool valid = true;
	if (cache == NULL) {
		return false;
	}

	cache->is_editmode = GPENCIL_ANY_EDIT_MODE(gpd);
	if (cfra != cache->cache_frame) {
		valid = false;
	}
	else if (gpd->flag & GP_DATA_CACHE_IS_DIRTY) {
		valid = false;
	}
	else if (gpd->flag & GP_DATA_SHOW_ONIONSKINS) {
		/* if onion, set as dirty always
		 * This reduces performance, but avoid any crash in the multiple
		 * overlay and multiwindow options and keep all windows working
		 */
		valid = false;
	}
	else if (cache->is_editmode) {
		valid = false;
	}
	else if (cache->is_dirty) {
		valid = false;
	}

	return valid;
}

/* cache init */
static GpencilBatchCache *gpencil_batch_cache_init(Object *ob, int cfra)
{
	Object *ob_orig = (Object *)DEG_get_original_id(&ob->id);
	bGPdata *gpd = (bGPdata *)ob_orig->data;

	GpencilBatchCache *cache = gpencil_batch_get_element(ob);

	if (!cache) {
		cache = MEM_callocN(sizeof(*cache), __func__);
		ob_orig->runtime.gpencil_cache = cache;
	}
	else {
		memset(cache, 0, sizeof(*cache));
	}

	cache->is_editmode = GPENCIL_ANY_EDIT_MODE(gpd);

	cache->is_dirty = true;

	cache->cache_frame = cfra;

	/* create array of derived frames equal to number of layers */
	cache->tot_layers = BLI_listbase_count(&gpd->layers);
	CLAMP_MIN(cache->tot_layers, 1);
	cache->derived_array = MEM_callocN(sizeof(struct bGPDframe) * cache->tot_layers, "Derived GPF");

	return cache;
}

/* clear cache */
static void gpencil_batch_cache_clear(GpencilBatchCache *cache)
{
	if (!cache) {
		return;
	}

	GPU_BATCH_DISCARD_SAFE(cache->b_stroke.batch);
	GPU_BATCH_DISCARD_SAFE(cache->b_point.batch);
	GPU_BATCH_DISCARD_SAFE(cache->b_fill.batch);
	GPU_BATCH_DISCARD_SAFE(cache->b_edit.batch);
	GPU_BATCH_DISCARD_SAFE(cache->b_edlin.batch);

	MEM_SAFE_FREE(cache->b_stroke.batch);
	MEM_SAFE_FREE(cache->b_point.batch);
	MEM_SAFE_FREE(cache->b_fill.batch);
	MEM_SAFE_FREE(cache->b_edit.batch);
	MEM_SAFE_FREE(cache->b_edlin.batch);

	MEM_SAFE_FREE(cache->grp_cache);
	cache->grp_size = 0;
	cache->grp_used = 0;

	/* clear all frames derived data */
	for (int i = 0; i < cache->tot_layers; i++) {
		bGPDframe *derived_gpf = &cache->derived_array[i];
		BKE_gpencil_free_frame_runtime_data(derived_gpf);
		derived_gpf = NULL;
	}
	cache->tot_layers = 0;
	MEM_SAFE_FREE(cache->derived_array);
}

/* get cache */
GpencilBatchCache *gpencil_batch_cache_get(Object *ob, int cfra)
{
	Object *ob_orig = (Object *)DEG_get_original_id(&ob->id);
	bGPdata *gpd = (bGPdata *)ob_orig->data;

	GpencilBatchCache *cache = gpencil_batch_get_element(ob);
	if (!gpencil_batch_cache_valid(cache, gpd, cfra)) {
		if (cache) {
			gpencil_batch_cache_clear(cache);
		}
		return gpencil_batch_cache_init(ob, cfra);
	}
	else {
		return cache;
	}
}

/* set cache as dirty */
void DRW_gpencil_batch_cache_dirty_tag(bGPdata *gpd)
{
	bGPdata *gpd_orig = (bGPdata *)DEG_get_original_id(&gpd->id);
	gpd_orig->flag |= GP_DATA_CACHE_IS_DIRTY;
}

/* free batch cache */
void DRW_gpencil_batch_cache_free(bGPdata *UNUSED(gpd))
{
	return;
}

/* wrapper to clear cache */
void DRW_gpencil_freecache(struct Object *ob)
{
	if ((ob) && (ob->type == OB_GPENCIL)) {
		gpencil_batch_cache_clear(ob->runtime.gpencil_cache);
		MEM_SAFE_FREE(ob->runtime.gpencil_cache);
		bGPdata *gpd = (bGPdata *)ob->data;
		if (gpd) {
			gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
		}
	}
}
