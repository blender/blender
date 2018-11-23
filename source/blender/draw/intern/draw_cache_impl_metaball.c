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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file draw_cache_impl_metaball.c
 *  \ingroup draw
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

#include "DRW_render.h"

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
		GPUVertBuf *elem_vbo;
		GPUTexture *elem_tx;
		GPUTexture *verts_tx;
		int tri_count;
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
	cache->face_wire.elem_vbo = NULL;
	cache->face_wire.elem_tx = NULL;
	cache->face_wire.verts_tx = NULL;
	cache->face_wire.tri_count = 0;
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

	GPU_VERTBUF_DISCARD_SAFE(cache->face_wire.elem_vbo);
	DRW_TEXTURE_FREE_SAFE(cache->face_wire.elem_tx);
	DRW_TEXTURE_FREE_SAFE(cache->face_wire.verts_tx);

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
		cache->pos_nor_in_order = DRW_displist_vertbuf_calc_pos_with_normals(lb);
	}
	return cache->pos_nor_in_order;
}

static GPUTexture *mball_batch_cache_get_edges_overlay_texture_buf(Object *ob, MetaBallBatchCache *cache)
{
	if (cache->face_wire.elem_tx != NULL) {
		return cache->face_wire.elem_tx;
	}

	ListBase *lb = &ob->runtime.curve_cache->disp;

	/* We need a special index buffer. */
	GPUVertBuf *vbo = cache->face_wire.elem_vbo = DRW_displist_create_edges_overlay_texture_buf(lb);

	/* Upload data early because we need to create the texture for it. */
	GPU_vertbuf_use(vbo);
	cache->face_wire.elem_tx = GPU_texture_create_from_vertbuf(vbo);
	cache->face_wire.tri_count = vbo->vertex_alloc / 3;

	return cache->face_wire.elem_tx;
}

static GPUTexture *mball_batch_cache_get_vert_pos_and_nor_in_order_buf(Object *ob, MetaBallBatchCache *cache)
{
	if (cache->face_wire.verts_tx == NULL) {
		GPUVertBuf *vbo = mball_batch_cache_get_pos_and_normals(ob, cache);
		GPU_vertbuf_use(vbo); /* Upload early for buffer texture creation. */
		cache->face_wire.verts_tx = GPU_texture_create_buffer(GPU_R32F, vbo->vbo_id);
	}

	return cache->face_wire.verts_tx;
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
		cache->batch = GPU_batch_create_ex(
		        GPU_PRIM_TRIS,
		        mball_batch_cache_get_pos_and_normals(ob, cache),
		        DRW_displist_indexbuf_calc_triangles_in_order(lb),
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

void DRW_metaball_batch_cache_get_wireframes_face_texbuf(
        Object *ob, struct GPUTexture **verts_data, struct GPUTexture **face_indices, int *tri_count)
{
	if (!BKE_mball_is_basis(ob)) {
		*verts_data = NULL;
		*face_indices = NULL;
		*tri_count = 0;
		return;
	}

	MetaBall *mb = ob->data;
	MetaBallBatchCache *cache = metaball_batch_cache_get(mb);

	if (cache->face_wire.verts_tx == NULL) {
		*verts_data = mball_batch_cache_get_vert_pos_and_nor_in_order_buf(ob, cache);
		*face_indices = mball_batch_cache_get_edges_overlay_texture_buf(ob, cache);
	}
	else {
		*verts_data = cache->face_wire.verts_tx;
		*face_indices = cache->face_wire.elem_tx;
	}
	*tri_count = cache->face_wire.tri_count;
}