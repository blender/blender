/*
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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 */

/** \file \ingroup draw
 *
 * \brief MetaBall API for render engines
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_meta_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_mball.h"

#include "GPU_batch.h"


#include "draw_cache_impl.h"  /* own include */


static void metaball_batch_cache_clear(MetaBall *mb);

/* ---------------------------------------------------------------------- */
/* MetaBall GPUBatch Cache */

typedef struct MetaBallBatchCache {
	GPUBatch *batch;
	GPUBatch **shaded_triangles;
	int mat_len;

	/* Shared */
	GPUVertBuf *pos_nor_in_order;

	/* Wireframe */
	struct {
		GPUBatch *batch;
	} face_wire;

	/* settings to determine if cache is invalid */
	bool is_dirty;
} MetaBallBatchCache;

/* GPUBatch cache management. */

static bool metaball_batch_cache_valid(MetaBall *mb)
{
	MetaBallBatchCache *cache = mb->batch_cache;

	if (cache == NULL) {
		return false;
	}

	return cache->is_dirty == false;
}

static void metaball_batch_cache_init(MetaBall *mb)
{
	MetaBallBatchCache *cache = mb->batch_cache;

	if (!cache) {
		cache = mb->batch_cache = MEM_mallocN(sizeof(*cache), __func__);
	}
	cache->batch = NULL;
	cache->mat_len = 0;
	cache->shaded_triangles = NULL;
	cache->is_dirty = false;
	cache->pos_nor_in_order = NULL;
	cache->face_wire.batch = NULL;
}

static MetaBallBatchCache *metaball_batch_cache_get(MetaBall *mb)
{
	if (!metaball_batch_cache_valid(mb)) {
		metaball_batch_cache_clear(mb);
		metaball_batch_cache_init(mb);
	}
	return mb->batch_cache;
}

void DRW_mball_batch_cache_dirty_tag(MetaBall *mb, int mode)
{
	MetaBallBatchCache *cache = mb->batch_cache;
	if (cache == NULL) {
		return;
	}
	switch (mode) {
		case BKE_MBALL_BATCH_DIRTY_ALL:
			cache->is_dirty = true;
			break;
		default:
			BLI_assert(0);
	}
}

static void metaball_batch_cache_clear(MetaBall *mb)
{
	MetaBallBatchCache *cache = mb->batch_cache;
	if (!cache) {
		return;
	}

	GPU_BATCH_DISCARD_SAFE(cache->face_wire.batch);
	GPU_BATCH_DISCARD_SAFE(cache->batch);
	GPU_VERTBUF_DISCARD_SAFE(cache->pos_nor_in_order);
	/* Note: shaded_triangles[0] is already freed by cache->batch */
	MEM_SAFE_FREE(cache->shaded_triangles);
	cache->mat_len = 0;
}

void DRW_mball_batch_cache_free(MetaBall *mb)
{
	metaball_batch_cache_clear(mb);
	MEM_SAFE_FREE(mb->batch_cache);
}

static GPUVertBuf *mball_batch_cache_get_pos_and_normals(Object *ob, MetaBallBatchCache *cache)
{
	if (cache->pos_nor_in_order == NULL) {
		ListBase *lb = &ob->runtime.curve_cache->disp;
		cache->pos_nor_in_order = MEM_callocN(sizeof(GPUVertBuf), __func__);
		DRW_displist_vertbuf_create_pos_and_nor(lb, cache->pos_nor_in_order);
	}
	return cache->pos_nor_in_order;
}

/* -------------------------------------------------------------------- */
/** \name Public Object/MetaBall API
 * \{ */

GPUBatch *DRW_metaball_batch_cache_get_triangles_with_normals(Object *ob)
{
	if (!BKE_mball_is_basis(ob)) {
		return NULL;
	}

	MetaBall *mb = ob->data;
	MetaBallBatchCache *cache = metaball_batch_cache_get(mb);

	if (cache->batch == NULL) {
		ListBase *lb = &ob->runtime.curve_cache->disp;
		GPUIndexBuf *ibo = MEM_callocN(sizeof(GPUIndexBuf), __func__);
		DRW_displist_indexbuf_create_triangles_in_order(lb, ibo);
		cache->batch = GPU_batch_create_ex(
		        GPU_PRIM_TRIS,
		        mball_batch_cache_get_pos_and_normals(ob, cache),
		        ibo,
		        GPU_BATCH_OWNS_INDEX);
	}

	return cache->batch;
}

GPUBatch **DRW_metaball_batch_cache_get_surface_shaded(Object *ob, MetaBall *mb, struct GPUMaterial **UNUSED(gpumat_array), uint gpumat_array_len)
{
	if (!BKE_mball_is_basis(ob)) {
		return NULL;
	}

	MetaBallBatchCache *cache = metaball_batch_cache_get(mb);
	if (cache->shaded_triangles == NULL) {
		cache->mat_len = gpumat_array_len;
		cache->shaded_triangles = MEM_callocN(sizeof(*cache->shaded_triangles) * cache->mat_len, __func__);
		cache->shaded_triangles[0] = DRW_metaball_batch_cache_get_triangles_with_normals(ob);
		for (int i = 1; i < cache->mat_len; ++i) {
			cache->shaded_triangles[i] = NULL;
		}
	}
	return cache->shaded_triangles;

}

GPUBatch *DRW_metaball_batch_cache_get_wireframes_face(Object *ob)
{
	if (!BKE_mball_is_basis(ob)) {
		return NULL;
	}

	MetaBall *mb = ob->data;
	MetaBallBatchCache *cache = metaball_batch_cache_get(mb);

	if (cache->face_wire.batch == NULL) {
		ListBase *lb = &ob->runtime.curve_cache->disp;

		GPUVertBuf *vbo_pos_nor = MEM_callocN(sizeof(GPUVertBuf), __func__);
		GPUVertBuf *vbo_wireframe_data = MEM_callocN(sizeof(GPUVertBuf), __func__);

		DRW_displist_vertbuf_create_pos_and_nor_and_uv_tess(lb, vbo_pos_nor, NULL);
		DRW_displist_vertbuf_create_wireframe_data_tess(lb, vbo_wireframe_data);

		cache->face_wire.batch = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo_pos_nor, NULL, GPU_BATCH_OWNS_VBO);
		GPU_batch_vertbuf_add_ex(cache->face_wire.batch, vbo_wireframe_data, true);
	}

	return cache->face_wire.batch;
}
