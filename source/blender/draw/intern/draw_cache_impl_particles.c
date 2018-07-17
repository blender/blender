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
 * Contributor(s): Blender Foundation, Mike Erwin, Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file draw_cache_impl_particles.c
 *  \ingroup draw
 *
 * \brief Particle API for render engines
 */

#include "DRW_render.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_ghash.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_customdata_types.h"

#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"

#include "ED_particle.h"

#include "GPU_batch.h"

#include "DEG_depsgraph_query.h"

#include "draw_cache_impl.h"  /* own include */
#include "draw_hair_private.h"

static void particle_batch_cache_clear(ParticleSystem *psys);

/* ---------------------------------------------------------------------- */
/* Particle GPUBatch Cache */

typedef struct ParticlePointCache {
	GPUVertBuf *pos;
	GPUBatch *points;
	int elems_len;
	int point_len;
} ParticlePointCache;

typedef struct ParticleBatchCache {
	/* Object mode strands for hair and points for particle,
	 * strands for paths when in edit mode.
	 */
	ParticleHairCache hair;    /* Used for hair strands */
	ParticlePointCache point;  /* Used for particle points. */

	/* Control points when in edit mode. */
	ParticleHairCache edit_hair;

	GPUVertBuf *edit_inner_pos;
	GPUBatch *edit_inner_points;
	int edit_inner_point_len;

	GPUVertBuf *edit_tip_pos;
	GPUBatch *edit_tip_points;
	int edit_tip_point_len;

	/* Settings to determine if cache is invalid. */
	bool is_dirty;
} ParticleBatchCache;

/* GPUBatch cache management. */

typedef struct HairAttributeID {
	uint pos;
	uint tan;
	uint ind;
} HairAttributeID;

static bool particle_batch_cache_valid(ParticleSystem *psys)
{
	ParticleBatchCache *cache = psys->batch_cache;

	if (cache == NULL) {
		return false;
	}

	if (cache->is_dirty == false) {
		return true;
	}
	else {
		return false;
	}

	return true;
}

static void particle_batch_cache_init(ParticleSystem *psys)
{
	ParticleBatchCache *cache = psys->batch_cache;

	if (!cache) {
		cache = psys->batch_cache = MEM_callocN(sizeof(*cache), __func__);
	}
	else {
		memset(cache, 0, sizeof(*cache));
	}

	cache->is_dirty = false;
}

static ParticleBatchCache *particle_batch_cache_get(ParticleSystem *psys)
{
	if (!particle_batch_cache_valid(psys)) {
		particle_batch_cache_clear(psys);
		particle_batch_cache_init(psys);
	}
	return psys->batch_cache;
}

void DRW_particle_batch_cache_dirty(ParticleSystem *psys, int mode)
{
	ParticleBatchCache *cache = psys->batch_cache;
	if (cache == NULL) {
		return;
	}
	switch (mode) {
		case BKE_PARTICLE_BATCH_DIRTY_ALL:
			cache->is_dirty = true;
			break;
		default:
			BLI_assert(0);
	}
}

static void particle_batch_cache_clear_point(ParticlePointCache *point_cache)
{
	GPU_BATCH_DISCARD_SAFE(point_cache->points);
	GPU_VERTBUF_DISCARD_SAFE(point_cache->pos);
}

static void particle_batch_cache_clear_hair(ParticleHairCache *hair_cache)
{
	/* TODO more granular update tagging. */
	GPU_VERTBUF_DISCARD_SAFE(hair_cache->proc_point_buf);
	DRW_TEXTURE_FREE_SAFE(hair_cache->point_tex);

	GPU_VERTBUF_DISCARD_SAFE(hair_cache->proc_strand_buf);
	DRW_TEXTURE_FREE_SAFE(hair_cache->strand_tex);

	for (int i = 0; i < MAX_MTFACE; ++i) {
		GPU_VERTBUF_DISCARD_SAFE(hair_cache->proc_uv_buf[i]);
		DRW_TEXTURE_FREE_SAFE(hair_cache->uv_tex[i]);
	}
	for (int i = 0; i < MAX_MCOL; ++i) {
		GPU_VERTBUF_DISCARD_SAFE(hair_cache->proc_col_buf[i]);
		DRW_TEXTURE_FREE_SAFE(hair_cache->col_tex[i]);
	}
	for (int i = 0; i < MAX_HAIR_SUBDIV; ++i) {
		GPU_VERTBUF_DISCARD_SAFE(hair_cache->final[i].proc_buf);
		DRW_TEXTURE_FREE_SAFE(hair_cache->final[i].proc_tex);
		for (int j = 0; j < MAX_THICKRES; ++j) {
			GPU_BATCH_DISCARD_SAFE(hair_cache->final[i].proc_hairs[j]);
		}
	}

	/* "Normal" legacy hairs */
	GPU_BATCH_DISCARD_SAFE(hair_cache->hairs);
	GPU_VERTBUF_DISCARD_SAFE(hair_cache->pos);
	GPU_INDEXBUF_DISCARD_SAFE(hair_cache->indices);
}

static void particle_batch_cache_clear(ParticleSystem *psys)
{
	ParticleBatchCache *cache = psys->batch_cache;
	if (!cache) {
		return;
	}

	particle_batch_cache_clear_point(&cache->point);
	particle_batch_cache_clear_hair(&cache->hair);

	particle_batch_cache_clear_hair(&cache->edit_hair);

	GPU_BATCH_DISCARD_SAFE(cache->edit_inner_points);
	GPU_VERTBUF_DISCARD_SAFE(cache->edit_inner_pos);
	GPU_BATCH_DISCARD_SAFE(cache->edit_tip_points);
	GPU_VERTBUF_DISCARD_SAFE(cache->edit_tip_pos);
}

void DRW_particle_batch_cache_free(ParticleSystem *psys)
{
	particle_batch_cache_clear(psys);
	MEM_SAFE_FREE(psys->batch_cache);
}

static void count_cache_segment_keys(
        ParticleCacheKey **pathcache,
        const int num_path_cache_keys,
        ParticleHairCache *hair_cache)
{
	for (int i = 0; i < num_path_cache_keys; i++) {
		ParticleCacheKey *path = pathcache[i];
		if (path->segments > 0) {
			hair_cache->strands_len++;
			hair_cache->elems_len += path->segments + 2;
			hair_cache->point_len += path->segments + 1;
		}
	}
}

static void ensure_seg_pt_count(
        PTCacheEdit *edit,
        ParticleSystem *psys,
        ParticleHairCache *hair_cache)
{
	if ((hair_cache->pos != NULL && hair_cache->indices != NULL) ||
	    (hair_cache->proc_point_buf != NULL))
	{
		return;
	}

	hair_cache->strands_len = 0;
	hair_cache->elems_len = 0;
	hair_cache->point_len = 0;

	if (edit != NULL && edit->pathcache != NULL) {
		count_cache_segment_keys(edit->pathcache, edit->totcached, hair_cache);
	}
	else {
		if (psys->pathcache &&
		    (!psys->childcache || (psys->part->draw & PART_DRAW_PARENT)))
		{
			count_cache_segment_keys(psys->pathcache, psys->totpart, hair_cache);
		}
		if (psys->childcache) {
			const int child_count = psys->totchild * psys->part->disp / 100;
			count_cache_segment_keys(psys->childcache, child_count, hair_cache);
		}
	}
}

static void particle_pack_mcol(MCol *mcol, ushort r_scol[3])
{
	/* Convert to linear ushort and swizzle */
	r_scol[0] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol->b]);
	r_scol[1] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol->g]);
	r_scol[2] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol->r]);
}

/* Used by parent particles and simple children. */
static void particle_calculate_parent_uvs(
        ParticleSystem *psys,
        ParticleSystemModifierData *psmd,
        const int num_uv_layers,
        const int parent_index,
        /*const*/ MTFace **mtfaces,
        float (*r_uv)[2])
{
	if (psmd == NULL) {
		return;
	}
	const int emit_from = psmd->psys->part->from;
	if (!ELEM(emit_from, PART_FROM_FACE, PART_FROM_VOLUME)) {
		return;
	}
	ParticleData *particle = &psys->particles[parent_index];
	int num = particle->num_dmcache;
	if (num == DMCACHE_NOTFOUND || num == DMCACHE_ISCHILD) {
		if (particle->num < psmd->mesh_final->totface) {
			num = particle->num;
		}
	}
	if (num != DMCACHE_NOTFOUND && num != DMCACHE_ISCHILD) {
		MFace *mface = &psmd->mesh_final->mface[num];
		for (int j = 0; j < num_uv_layers; j++) {
			psys_interpolate_uvs(
			        mtfaces[j] + num,
			        mface->v4,
			        particle->fuv,
			        r_uv[j]);
		}
	}
}

static void particle_calculate_parent_mcol(
        ParticleSystem *psys,
        ParticleSystemModifierData *psmd,
        const int num_uv_layers,
        const int parent_index,
        /*const*/ MCol **mcols,
        MCol *r_mcol)
{
	if (psmd == NULL) {
		return;
	}
	const int emit_from = psmd->psys->part->from;
	if (!ELEM(emit_from, PART_FROM_FACE, PART_FROM_VOLUME)) {
		return;
	}
	ParticleData *particle = &psys->particles[parent_index];
	int num = particle->num_dmcache;
	if (num == DMCACHE_NOTFOUND || num == DMCACHE_ISCHILD) {
		if (particle->num < psmd->mesh_final->totface) {
			num = particle->num;
		}
	}
	if (num != DMCACHE_NOTFOUND && num != DMCACHE_ISCHILD) {
		MFace *mface = &psmd->mesh_final->mface[num];
		for (int j = 0; j < num_uv_layers; j++) {
			psys_interpolate_mcol(
			        mcols[j] + num,
			        mface->v4,
			        particle->fuv,
			        &r_mcol[j]);
		}
	}
}

/* Used by interpolated children. */
static void particle_interpolate_children_uvs(
        ParticleSystem *psys,
        ParticleSystemModifierData *psmd,
        const int num_uv_layers,
        const int child_index,
        /*const*/ MTFace **mtfaces,
        float (*r_uv)[2])
{
	if (psmd == NULL) {
		return;
	}
	const int emit_from = psmd->psys->part->from;
	if (!ELEM(emit_from, PART_FROM_FACE, PART_FROM_VOLUME)) {
		return;
	}
	ChildParticle *particle = &psys->child[child_index];
	int num = particle->num;
	if (num != DMCACHE_NOTFOUND) {
		MFace *mface = &psmd->mesh_final->mface[num];
		for (int j = 0; j < num_uv_layers; j++) {
			psys_interpolate_uvs(
			        mtfaces[j] + num,
			        mface->v4,
			        particle->fuv,
			        r_uv[j]);
		}
	}
}

static void particle_interpolate_children_mcol(
        ParticleSystem *psys,
        ParticleSystemModifierData *psmd,
        const int num_col_layers,
        const int child_index,
        /*const*/ MCol **mcols,
        MCol *r_mcol)
{
	if (psmd == NULL) {
		return;
	}
	const int emit_from = psmd->psys->part->from;
	if (!ELEM(emit_from, PART_FROM_FACE, PART_FROM_VOLUME)) {
		return;
	}
	ChildParticle *particle = &psys->child[child_index];
	int num = particle->num;
	if (num != DMCACHE_NOTFOUND) {
		MFace *mface = &psmd->mesh_final->mface[num];
		for (int j = 0; j < num_col_layers; j++) {
			psys_interpolate_mcol(
			        mcols[j] + num,
			        mface->v4,
			        particle->fuv,
			        &r_mcol[j]);
		}
	}
}

static void particle_calculate_uvs(
        ParticleSystem *psys,
        ParticleSystemModifierData *psmd,
        const bool is_simple,
        const int num_uv_layers,
        const int parent_index,
        const int child_index,
        /*const*/ MTFace **mtfaces,
        float (**r_parent_uvs)[2],
        float (**r_uv)[2])
{
	if (psmd == NULL) {
		return;
	}
	if (is_simple) {
		if (r_parent_uvs[parent_index] != NULL) {
			*r_uv = r_parent_uvs[parent_index];
		}
		else {
			*r_uv = MEM_callocN(sizeof(**r_uv) * num_uv_layers, "Particle UVs");
		}
	}
	else {
		*r_uv = MEM_callocN(sizeof(**r_uv) * num_uv_layers, "Particle UVs");
	}
	if (child_index == -1) {
		/* Calculate UVs for parent particles. */
		if (is_simple) {
			r_parent_uvs[parent_index] = *r_uv;
		}
		particle_calculate_parent_uvs(
		        psys, psmd, num_uv_layers, parent_index, mtfaces, *r_uv);
	}
	else {
		/* Calculate UVs for child particles. */
		if (!is_simple) {
			particle_interpolate_children_uvs(
			        psys, psmd, num_uv_layers, child_index, mtfaces, *r_uv);
		}
		else if (!r_parent_uvs[psys->child[child_index].parent]) {
			r_parent_uvs[psys->child[child_index].parent] = *r_uv;
			particle_calculate_parent_uvs(
			        psys, psmd, num_uv_layers, parent_index, mtfaces, *r_uv);
		}
	}
}

static void particle_calculate_mcol(
        ParticleSystem *psys,
        ParticleSystemModifierData *psmd,
        const bool is_simple,
        const int num_col_layers,
        const int parent_index,
        const int child_index,
        /*const*/ MCol **mcols,
        MCol **r_parent_mcol,
        MCol **r_mcol)
{
	if (psmd == NULL) {
		return;
	}
	if (is_simple) {
		if (r_parent_mcol[parent_index] != NULL) {
			*r_mcol = r_parent_mcol[parent_index];
		}
		else {
			*r_mcol = MEM_callocN(sizeof(**r_mcol) * num_col_layers, "Particle MCol");
		}
	}
	else {
		*r_mcol = MEM_callocN(sizeof(**r_mcol) * num_col_layers, "Particle MCol");
	}
	if (child_index == -1) {
		/* Calculate MCols for parent particles. */
		if (is_simple) {
			r_parent_mcol[parent_index] = *r_mcol;
		}
		particle_calculate_parent_mcol(
		        psys, psmd, num_col_layers, parent_index, mcols, *r_mcol);
	}
	else {
		/* Calculate MCols for child particles. */
		if (!is_simple) {
			particle_interpolate_children_mcol(
			        psys, psmd, num_col_layers, child_index, mcols, *r_mcol);
		}
		else if (!r_parent_mcol[psys->child[child_index].parent]) {
			r_parent_mcol[psys->child[child_index].parent] = *r_mcol;
			particle_calculate_parent_mcol(
			        psys, psmd, num_col_layers, parent_index, mcols, *r_mcol);
		}
	}
}

/* Will return last filled index. */
typedef enum ParticleSource {
	PARTICLE_SOURCE_PARENT,
	PARTICLE_SOURCE_CHILDREN,
} ParticleSource;
static int particle_batch_cache_fill_segments(
        ParticleSystem *psys,
        ParticleSystemModifierData *psmd,
        ParticleCacheKey **path_cache,
        const ParticleSource particle_source,
        const int global_offset,
        const int start_index,
        const int num_path_keys,
        const int num_uv_layers,
        const int num_col_layers,
        /*const*/ MTFace **mtfaces,
        /*const*/ MCol **mcols,
        uint *uv_id,
        uint *col_id,
        float (***r_parent_uvs)[2],
        MCol ***r_parent_mcol,
        GPUIndexBufBuilder *elb,
        HairAttributeID *attr_id,
        ParticleHairCache *hair_cache)
{
	const bool is_simple = (psys->part->childtype == PART_CHILD_PARTICLES);
	const bool is_child = (particle_source == PARTICLE_SOURCE_CHILDREN);
	if (is_simple && *r_parent_uvs == NULL) {
		/* TODO(sergey): For edit mode it should be edit->totcached. */
		*r_parent_uvs = MEM_callocN(
		        sizeof(*r_parent_uvs) * psys->totpart,
		        "Parent particle UVs");
	}
	if (is_simple && *r_parent_mcol == NULL) {
		*r_parent_mcol = MEM_callocN(
		        sizeof(*r_parent_mcol) * psys->totpart,
		        "Parent particle MCol");
	}
	int curr_point = start_index;
	for (int i = 0; i < num_path_keys; i++) {
		ParticleCacheKey *path = path_cache[i];
		if (path->segments <= 0) {
			continue;
		}
		float tangent[3];
		float (*uv)[2] = NULL;
		MCol *mcol = NULL;
		particle_calculate_mcol(
		        psys, psmd,
		        is_simple, num_col_layers,
		        is_child ? psys->child[i].parent : i,
		        is_child ? i : -1,
		        mcols,
		        *r_parent_mcol, &mcol);
		particle_calculate_uvs(
		        psys, psmd,
		        is_simple, num_uv_layers,
		        is_child ? psys->child[i].parent : i,
		        is_child ? i : -1,
		        mtfaces,
		        *r_parent_uvs, &uv);
		for (int j = 0; j < path->segments; j++) {
			if (j == 0) {
				sub_v3_v3v3(tangent, path[j + 1].co, path[j].co);
			}
			else {
				sub_v3_v3v3(tangent, path[j + 1].co, path[j - 1].co);
			}
			GPU_vertbuf_attr_set(hair_cache->pos, attr_id->pos, curr_point, path[j].co);
			GPU_vertbuf_attr_set(hair_cache->pos, attr_id->tan, curr_point, tangent);
			GPU_vertbuf_attr_set(hair_cache->pos, attr_id->ind, curr_point, &i);
			if (psmd != NULL) {
				for (int k = 0; k < num_uv_layers; k++) {
					GPU_vertbuf_attr_set(
					        hair_cache->pos, uv_id[k], curr_point,
					        (is_simple && is_child) ?
					        (*r_parent_uvs)[psys->child[i].parent][k] : uv[k]);
				}
				for (int k = 0; k < num_col_layers; k++) {
					/* TODO Put the conversion outside the loop */
					ushort scol[4];
					particle_pack_mcol(
					        (is_simple && is_child) ?
					        &(*r_parent_mcol)[psys->child[i].parent][k] : &mcol[k],
					        scol);
					GPU_vertbuf_attr_set(hair_cache->pos, col_id[k], curr_point, scol);
				}
			}
			GPU_indexbuf_add_generic_vert(elb, curr_point);
			curr_point++;
		}
		sub_v3_v3v3(tangent, path[path->segments].co, path[path->segments - 1].co);

		int global_index = i + global_offset;
		GPU_vertbuf_attr_set(hair_cache->pos, attr_id->pos, curr_point, path[path->segments].co);
		GPU_vertbuf_attr_set(hair_cache->pos, attr_id->tan, curr_point, tangent);
		GPU_vertbuf_attr_set(hair_cache->pos, attr_id->ind, curr_point, &global_index);

		if (psmd != NULL) {
			for (int k = 0; k < num_uv_layers; k++) {
				GPU_vertbuf_attr_set(
				        hair_cache->pos, uv_id[k], curr_point,
				        (is_simple && is_child) ?
				        (*r_parent_uvs)[psys->child[i].parent][k] : uv[k]);
			}
			for (int k = 0; k < num_col_layers; k++) {
				/* TODO Put the conversion outside the loop */
				ushort scol[4];
				particle_pack_mcol(
				        (is_simple && is_child) ?
				        &(*r_parent_mcol)[psys->child[i].parent][k] : &mcol[k],
				        scol);
				GPU_vertbuf_attr_set(hair_cache->pos, col_id[k], curr_point, scol);
			}
			if (!is_simple) {
				MEM_freeN(uv);
				MEM_freeN(mcol);
			}
		}
		/* Finish the segment and add restart primitive. */
		GPU_indexbuf_add_generic_vert(elb, curr_point);
		GPU_indexbuf_add_primitive_restart(elb);
		curr_point++;
	}
	return curr_point;
}

static void particle_batch_cache_fill_segments_proc_pos(
        ParticleCacheKey **path_cache,
        const int num_path_keys,
        GPUVertBufRaw *attr_step)
{
	for (int i = 0; i < num_path_keys; i++) {
		ParticleCacheKey *path = path_cache[i];
		if (path->segments <= 0) {
			continue;
		}
		float total_len = 0.0f;
		float *co_prev = NULL, *seg_data_first;
		for (int j = 0; j <= path->segments; j++) {
			float *seg_data = (float *)GPU_vertbuf_raw_step(attr_step);
			copy_v3_v3(seg_data, path[j].co);
			if (co_prev) {
				total_len += len_v3v3(co_prev, path[j].co);
			}
			else {
				seg_data_first = seg_data;
			}
			seg_data[3] = total_len;
			co_prev = path[j].co;
		}
		if (total_len > 0.0f) {
			/* Divide by total length to have a [0-1] number. */
			for (int j = 0; j <= path->segments; j++, seg_data_first += 4) {
				seg_data_first[3] /= total_len;
			}
		}
	}
}

static int particle_batch_cache_fill_segments_indices(
        ParticleCacheKey **path_cache,
        const int start_index,
        const int num_path_keys,
        const int res,
        GPUIndexBufBuilder *elb)
{
	int curr_point = start_index;
	for (int i = 0; i < num_path_keys; i++) {
		ParticleCacheKey *path = path_cache[i];
		if (path->segments <= 0) {
			continue;
		}
		for (int k = 0; k < res; k++) {
			GPU_indexbuf_add_generic_vert(elb, curr_point++);
		}
		GPU_indexbuf_add_primitive_restart(elb);
	}
	return curr_point;
}

static int particle_batch_cache_fill_strands_data(
        ParticleSystem *psys,
        ParticleSystemModifierData *psmd,
        ParticleCacheKey **path_cache,
        const ParticleSource particle_source,
        const int start_index,
        const int num_path_keys,
        GPUVertBufRaw *data_step,
        float (***r_parent_uvs)[2], GPUVertBufRaw *uv_step, MTFace **mtfaces, int num_uv_layers,
        MCol ***r_parent_mcol, GPUVertBufRaw *col_step, MCol **mcols, int num_col_layers)
{
	const bool is_simple = (psys->part->childtype == PART_CHILD_PARTICLES);
	const bool is_child = (particle_source == PARTICLE_SOURCE_CHILDREN);
	if (is_simple && *r_parent_uvs == NULL) {
		/* TODO(sergey): For edit mode it should be edit->totcached. */
		*r_parent_uvs = MEM_callocN(
		        sizeof(*r_parent_uvs) * psys->totpart,
		        "Parent particle UVs");
	}
	if (is_simple && *r_parent_mcol == NULL) {
		*r_parent_mcol = MEM_callocN(
		        sizeof(*r_parent_mcol) * psys->totpart,
		        "Parent particle MCol");
	}
	int curr_point = start_index;
	for (int i = 0; i < num_path_keys; i++) {
		ParticleCacheKey *path = path_cache[i];
		if (path->segments <= 0) {
			continue;
		}

		uint *seg_data = (uint *)GPU_vertbuf_raw_step(data_step);
		*seg_data = (curr_point & 0xFFFFFF) | (path->segments << 24);
		curr_point += path->segments + 1;

		if (psmd != NULL) {
			float (*uv)[2] = NULL;
			MCol *mcol = NULL;

			particle_calculate_uvs(
			        psys, psmd,
			        is_simple, num_uv_layers,
			        is_child ? psys->child[i].parent : i,
			        is_child ? i : -1,
			        mtfaces,
			        *r_parent_uvs, &uv);

			particle_calculate_mcol(
			        psys, psmd,
			        is_simple, num_col_layers,
			        is_child ? psys->child[i].parent : i,
			        is_child ? i : -1,
			        mcols,
			        *r_parent_mcol, &mcol);

			for (int k = 0; k < num_uv_layers; k++) {
				float *t_uv = (float *)GPU_vertbuf_raw_step(uv_step + k);
				copy_v2_v2(t_uv, uv[k]);
			}
			for (int k = 0; k < num_col_layers; k++) {
				ushort *scol = (ushort *)GPU_vertbuf_raw_step(col_step + k);
				particle_pack_mcol(
				        (is_simple && is_child) ?
				        &(*r_parent_mcol)[psys->child[i].parent][k] : &mcol[k],
				        scol);
			}
			if (!is_simple) {
				MEM_freeN(uv);
				MEM_freeN(mcol);
			}
		}
	}
	return curr_point;
}

static void particle_batch_cache_ensure_procedural_final_points(
        ParticleHairCache *cache,
        int subdiv)
{
	/* Same format as point_tex. */
	GPUVertFormat format = { 0 };
	GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

	cache->final[subdiv].proc_buf = GPU_vertbuf_create_with_format(&format);

	/* Create a destination buffer for the tranform feedback. Sized appropriately */
	/* Thoses are points! not line segments. */
	GPU_vertbuf_data_alloc(cache->final[subdiv].proc_buf, cache->final[subdiv].strands_res * cache->strands_len);

	/* Create vbo immediatly to bind to texture buffer. */
	GPU_vertbuf_use(cache->final[subdiv].proc_buf);

	cache->final[subdiv].proc_tex = GPU_texture_create_from_vertbuf(cache->final[subdiv].proc_buf);
}

static void particle_batch_cache_ensure_procedural_strand_data(
        PTCacheEdit *edit,
        ParticleSystem *psys,
        ModifierData *md,
        ParticleHairCache *cache)
{
	int active_uv = 0;
	int active_col = 0;

	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;

	if (psmd != NULL && psmd->mesh_final != NULL) {
		if (CustomData_has_layer(&psmd->mesh_final->ldata, CD_MLOOPUV)) {
			cache->num_uv_layers = CustomData_number_of_layers(&psmd->mesh_final->ldata, CD_MLOOPUV);
			active_uv = CustomData_get_active_layer(&psmd->mesh_final->ldata, CD_MLOOPUV);
		}
		if (CustomData_has_layer(&psmd->mesh_final->ldata, CD_MLOOPCOL)) {
			cache->num_col_layers = CustomData_number_of_layers(&psmd->mesh_final->ldata, CD_MLOOPCOL);
			active_col = CustomData_get_active_layer(&psmd->mesh_final->ldata, CD_MLOOPCOL);
		}
	}

	GPUVertBufRaw data_step;
	GPUVertBufRaw uv_step[MAX_MTFACE];
	GPUVertBufRaw col_step[MAX_MCOL];

	MTFace *mtfaces[MAX_MTFACE] = {NULL};
	MCol *mcols[MAX_MCOL] = {NULL};
	float (**parent_uvs)[2] = NULL;
	MCol **parent_mcol = NULL;

	GPUVertFormat format_data = {0};
	uint data_id = GPU_vertformat_attr_add(&format_data, "data", GPU_COMP_U32, 1, GPU_FETCH_INT);

	GPUVertFormat format_uv = {0};
	uint uv_id = GPU_vertformat_attr_add(&format_uv, "uv", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	GPUVertFormat format_col = {0};
	uint col_id = GPU_vertformat_attr_add(&format_col, "col", GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

	memset(cache->uv_layer_names, 0, sizeof(cache->uv_layer_names));
	memset(cache->col_layer_names, 0, sizeof(cache->col_layer_names));

	/* Strand Data */
	cache->proc_strand_buf = GPU_vertbuf_create_with_format(&format_data);
	GPU_vertbuf_data_alloc(cache->proc_strand_buf, cache->strands_len);
	GPU_vertbuf_attr_get_raw_data(cache->proc_strand_buf, data_id, &data_step);

	/* UV layers */
	for (int i = 0; i < cache->num_uv_layers; i++) {
		cache->proc_uv_buf[i] = GPU_vertbuf_create_with_format(&format_uv);
		GPU_vertbuf_data_alloc(cache->proc_uv_buf[i], cache->strands_len);
		GPU_vertbuf_attr_get_raw_data(cache->proc_uv_buf[i], uv_id, &uv_step[i]);

		const char *name = CustomData_get_layer_name(&psmd->mesh_final->ldata, CD_MLOOPUV, i);
		uint hash = BLI_ghashutil_strhash_p(name);
		int n = 0;
		BLI_snprintf(cache->uv_layer_names[i][n++], MAX_LAYER_NAME_LEN, "u%u", hash);
		BLI_snprintf(cache->uv_layer_names[i][n++], MAX_LAYER_NAME_LEN, "a%u", hash);

		if (i == active_uv) {
			BLI_snprintf(cache->uv_layer_names[i][n], MAX_LAYER_NAME_LEN, "u");
		}
	}
	/* Vertex colors */
	for (int i = 0; i < cache->num_col_layers; i++) {
		cache->proc_col_buf[i] = GPU_vertbuf_create_with_format(&format_col);
		GPU_vertbuf_data_alloc(cache->proc_col_buf[i], cache->strands_len);
		GPU_vertbuf_attr_get_raw_data(cache->proc_col_buf[i], col_id, &col_step[i]);

		const char *name = CustomData_get_layer_name(&psmd->mesh_final->ldata, CD_MLOOPCOL, i);
		uint hash = BLI_ghashutil_strhash_p(name);
		int n = 0;
		BLI_snprintf(cache->col_layer_names[i][n++], MAX_LAYER_NAME_LEN, "c%u", hash);

		/* We only do vcols auto name that are not overridden by uvs */
		if (CustomData_get_named_layer_index(&psmd->mesh_final->ldata, CD_MLOOPUV, name) == -1) {
			BLI_snprintf(cache->col_layer_names[i][n++], MAX_LAYER_NAME_LEN, "a%u", hash);
		}

		if (i == active_col) {
			BLI_snprintf(cache->col_layer_names[i][n], MAX_LAYER_NAME_LEN, "c");
		}
	}

	if (cache->num_uv_layers || cache->num_col_layers) {
		BKE_mesh_tessface_ensure(psmd->mesh_final);
		if (cache->num_uv_layers) {
			for (int j = 0; j < cache->num_uv_layers; j++) {
				mtfaces[j] = (MTFace *)CustomData_get_layer_n(&psmd->mesh_final->fdata, CD_MTFACE, j);
			}
		}
		if (cache->num_col_layers) {
			for (int j = 0; j < cache->num_col_layers; j++) {
				mcols[j] = (MCol *)CustomData_get_layer_n(&psmd->mesh_final->fdata, CD_MCOL, j);
			}
		}
	}

	if (edit != NULL && edit->pathcache != NULL) {
		particle_batch_cache_fill_strands_data(
		        psys, psmd, edit->pathcache, PARTICLE_SOURCE_PARENT,
		        0, edit->totcached,
		        &data_step,
		        &parent_uvs, uv_step, (MTFace **)mtfaces, cache->num_uv_layers,
		        &parent_mcol, col_step, (MCol **)mcols, cache->num_col_layers);
	}
	else {
		int curr_point = 0;
		if ((psys->pathcache != NULL) &&
		    (!psys->childcache || (psys->part->draw & PART_DRAW_PARENT)))
		{
			curr_point = particle_batch_cache_fill_strands_data(
			        psys, psmd, psys->pathcache, PARTICLE_SOURCE_PARENT,
			        0, psys->totpart,
			        &data_step,
			        &parent_uvs, uv_step, (MTFace **)mtfaces, cache->num_uv_layers,
			        &parent_mcol, col_step, (MCol **)mcols, cache->num_col_layers);
		}
		if (psys->childcache) {
			const int child_count = psys->totchild * psys->part->disp / 100;
			curr_point = particle_batch_cache_fill_strands_data(
			        psys, psmd, psys->childcache, PARTICLE_SOURCE_CHILDREN,
			        curr_point, child_count,
			        &data_step,
			        &parent_uvs, uv_step, (MTFace **)mtfaces, cache->num_uv_layers,
			        &parent_mcol, col_step, (MCol **)mcols, cache->num_col_layers);
		}
	}
	/* Cleanup. */
	if (parent_uvs != NULL) {
		/* TODO(sergey): For edit mode it should be edit->totcached. */
		for (int i = 0; i < psys->totpart; i++) {
			MEM_SAFE_FREE(parent_uvs[i]);
		}
		MEM_freeN(parent_uvs);
	}
	if (parent_mcol != NULL) {
		for (int i = 0; i < psys->totpart; i++) {
			MEM_SAFE_FREE(parent_mcol[i]);
		}
		MEM_freeN(parent_mcol);
	}

	/* Create vbo immediatly to bind to texture buffer. */
	GPU_vertbuf_use(cache->proc_strand_buf);
	cache->strand_tex = GPU_texture_create_from_vertbuf(cache->proc_strand_buf);

	for (int i = 0; i < cache->num_uv_layers; i++) {
		GPU_vertbuf_use(cache->proc_uv_buf[i]);
		cache->uv_tex[i] = GPU_texture_create_from_vertbuf(cache->proc_uv_buf[i]);
	}
	for (int i = 0; i < cache->num_col_layers; i++) {
		GPU_vertbuf_use(cache->proc_col_buf[i]);
		cache->col_tex[i] = GPU_texture_create_from_vertbuf(cache->proc_col_buf[i]);
	}
}

static void particle_batch_cache_ensure_procedural_indices(
        PTCacheEdit *edit,
        ParticleSystem *psys,
        ParticleHairCache *cache,
        int thickness_res,
        int subdiv)
{
	BLI_assert(thickness_res <= MAX_THICKRES); /* Cylinder strip not currently supported. */

	if (cache->final[subdiv].proc_hairs[thickness_res - 1] != NULL) {
		return;
	}

	int verts_per_hair = cache->final[subdiv].strands_res * thickness_res;
	/* +1 for primitive restart */
	int element_count = (verts_per_hair + 1) * cache->strands_len;
	GPUPrimType prim_type = (thickness_res == 1) ? GPU_PRIM_LINE_STRIP : GPU_PRIM_TRI_STRIP;

	static GPUVertFormat format = { 0 };
	GPU_vertformat_clear(&format);

	/* initialize vertex format */
	GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);

	GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(vbo, 1);

	GPUIndexBufBuilder elb;
	GPU_indexbuf_init_ex(&elb, prim_type, element_count, element_count, true);

	if (edit != NULL && edit->pathcache != NULL) {
		particle_batch_cache_fill_segments_indices(
		        edit->pathcache, 0, edit->totcached, verts_per_hair, &elb);
	}
	else {
		int curr_point = 0;
		if ((psys->pathcache != NULL) &&
		    (!psys->childcache || (psys->part->draw & PART_DRAW_PARENT)))
		{
			curr_point = particle_batch_cache_fill_segments_indices(
			        psys->pathcache, 0, psys->totpart, verts_per_hair, &elb);
		}
		if (psys->childcache) {
			const int child_count = psys->totchild * psys->part->disp / 100;
			curr_point = particle_batch_cache_fill_segments_indices(
			        psys->childcache, curr_point, child_count, verts_per_hair, &elb);
		}
	}

	cache->final[subdiv].proc_hairs[thickness_res - 1] = GPU_batch_create_ex(
	        prim_type,
	        vbo,
	        GPU_indexbuf_build(&elb),
	        GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
}

static void particle_batch_cache_ensure_procedural_pos(
        PTCacheEdit *edit,
        ParticleSystem *psys,
        ParticleHairCache *cache)
{
	if (cache->proc_point_buf != NULL) {
		return;
	}

	/* initialize vertex format */
	GPUVertFormat format = {0};
	uint pos_id = GPU_vertformat_attr_add(&format, "posTime", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

	cache->proc_point_buf = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(cache->proc_point_buf, cache->point_len);

	GPUVertBufRaw pos_step;
	GPU_vertbuf_attr_get_raw_data(cache->proc_point_buf, pos_id, &pos_step);

	if (edit != NULL && edit->pathcache != NULL) {
		particle_batch_cache_fill_segments_proc_pos(
		        edit->pathcache,
		        edit->totcached,
		        &pos_step);
	}
	else {
		if ((psys->pathcache != NULL) &&
		    (!psys->childcache || (psys->part->draw & PART_DRAW_PARENT)))
		{
			particle_batch_cache_fill_segments_proc_pos(
			        psys->pathcache,
			        psys->totpart,
			        &pos_step);
		}
		if (psys->childcache) {
			const int child_count = psys->totchild * psys->part->disp / 100;
			particle_batch_cache_fill_segments_proc_pos(
			        psys->childcache,
			        child_count,
			        &pos_step);
		}
	}

	/* Create vbo immediatly to bind to texture buffer. */
	GPU_vertbuf_use(cache->proc_point_buf);

	cache->point_tex = GPU_texture_create_from_vertbuf(cache->proc_point_buf);
}

static void particle_batch_cache_ensure_pos_and_seg(
        PTCacheEdit *edit,
        ParticleSystem *psys,
        ModifierData *md,
        ParticleHairCache *hair_cache)
{
	if (hair_cache->pos != NULL && hair_cache->indices != NULL) {
		return;
	}

	int curr_point = 0;
	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;

	GPU_VERTBUF_DISCARD_SAFE(hair_cache->pos);
	GPU_INDEXBUF_DISCARD_SAFE(hair_cache->indices);

	static GPUVertFormat format = { 0 };
	HairAttributeID attr_id;
	uint *uv_id = NULL;
	uint *col_id = NULL;
	int num_uv_layers = 0;
	int num_col_layers = 0;
	int active_uv = 0;
	int active_col = 0;
	MTFace **mtfaces = NULL;
	MCol **mcols = NULL;
	float (**parent_uvs)[2] = NULL;
	MCol **parent_mcol = NULL;

	if (psmd != NULL) {
		if (CustomData_has_layer(&psmd->mesh_final->ldata, CD_MLOOPUV)) {
			num_uv_layers = CustomData_number_of_layers(&psmd->mesh_final->ldata, CD_MLOOPUV);
			active_uv = CustomData_get_active_layer(&psmd->mesh_final->ldata, CD_MLOOPUV);
		}
		if (CustomData_has_layer(&psmd->mesh_final->ldata, CD_MLOOPCOL)) {
			num_col_layers = CustomData_number_of_layers(&psmd->mesh_final->ldata, CD_MLOOPCOL);
			active_col = CustomData_get_active_layer(&psmd->mesh_final->ldata, CD_MLOOPCOL);
		}
	}

	GPU_vertformat_clear(&format);

	/* initialize vertex format */
	attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	attr_id.tan = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	attr_id.ind = GPU_vertformat_attr_add(&format, "ind", GPU_COMP_I32, 1, GPU_FETCH_INT);

	if (psmd) {
		uv_id = MEM_mallocN(sizeof(*uv_id) * num_uv_layers, "UV attrib format");
		col_id = MEM_mallocN(sizeof(*col_id) * num_col_layers, "Col attrib format");

		for (int i = 0; i < num_uv_layers; i++) {
			const char *name = CustomData_get_layer_name(&psmd->mesh_final->ldata, CD_MLOOPUV, i);
			char uuid[32];

			BLI_snprintf(uuid, sizeof(uuid), "u%u", BLI_ghashutil_strhash_p(name));
			uv_id[i] = GPU_vertformat_attr_add(&format, uuid, GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

			if (i == active_uv) {
				GPU_vertformat_alias_add(&format, "u");
			}
		}

		for (int i = 0; i < num_uv_layers; i++) {
			const char *name = CustomData_get_layer_name(&psmd->mesh_final->ldata, CD_MLOOPUV, i);
			char uuid[32];

			BLI_snprintf(uuid, sizeof(uuid), "c%u", BLI_ghashutil_strhash_p(name));
			col_id[i] = GPU_vertformat_attr_add(&format, uuid, GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

			if (i == active_col) {
				GPU_vertformat_alias_add(&format, "c");
			}
		}
	}

	hair_cache->pos = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(hair_cache->pos, hair_cache->point_len);

	GPUIndexBufBuilder elb;
	GPU_indexbuf_init_ex(
	        &elb,
	        GPU_PRIM_LINE_STRIP,
	        hair_cache->elems_len, hair_cache->point_len,
	        true);

	if (num_uv_layers || num_col_layers) {
		BKE_mesh_tessface_ensure(psmd->mesh_final);
		if (num_uv_layers) {
			mtfaces = MEM_mallocN(sizeof(*mtfaces) * num_uv_layers, "Faces UV layers");
			for (int i = 0; i < num_uv_layers; i++) {
				mtfaces[i] = (MTFace *)CustomData_get_layer_n(&psmd->mesh_final->fdata, CD_MTFACE, i);
			}
		}
		if (num_col_layers) {
			mcols = MEM_mallocN(sizeof(*mcols) * num_col_layers, "Color layers");
			for (int i = 0; i < num_col_layers; i++) {
				mcols[i] = (MCol *)CustomData_get_layer_n(&psmd->mesh_final->fdata, CD_MCOL, i);
			}
		}
	}

	if (edit != NULL && edit->pathcache != NULL) {
		curr_point = particle_batch_cache_fill_segments(
		        psys, psmd, edit->pathcache, PARTICLE_SOURCE_PARENT,
		        0, 0, edit->totcached,
		        num_uv_layers, num_col_layers, mtfaces, mcols, uv_id, col_id, &parent_uvs, &parent_mcol,
		        &elb, &attr_id, hair_cache);
	}
	else {
		if ((psys->pathcache != NULL) &&
		    (!psys->childcache || (psys->part->draw & PART_DRAW_PARENT)))
		{
			curr_point = particle_batch_cache_fill_segments(
			        psys, psmd, psys->pathcache, PARTICLE_SOURCE_PARENT,
			        0, 0, psys->totpart,
			        num_uv_layers, num_col_layers, mtfaces, mcols, uv_id, col_id, &parent_uvs, &parent_mcol,
			        &elb, &attr_id, hair_cache);
		}
		if (psys->childcache != NULL) {
			const int child_count = psys->totchild * psys->part->disp / 100;
			curr_point = particle_batch_cache_fill_segments(
			        psys, psmd, psys->childcache, PARTICLE_SOURCE_CHILDREN,
			        psys->totpart, curr_point, child_count,
			        num_uv_layers, num_col_layers, mtfaces, mcols, uv_id, col_id, &parent_uvs, &parent_mcol,
			        &elb, &attr_id, hair_cache);
		}
	}
	/* Cleanup. */
	if (parent_uvs != NULL) {
		/* TODO(sergey): For edit mode it should be edit->totcached. */
		for (int i = 0; i < psys->totpart; i++) {
			MEM_SAFE_FREE(parent_uvs[i]);
		}
		MEM_freeN(parent_uvs);
	}
	if (parent_mcol != NULL) {
		for (int i = 0; i < psys->totpart; i++) {
			MEM_SAFE_FREE(parent_mcol[i]);
		}
		MEM_freeN(parent_mcol);
	}
	if (num_uv_layers) {
		MEM_freeN(mtfaces);
	}
	if (num_col_layers) {
		MEM_freeN(mcols);
	}
	if (psmd != NULL) {
		MEM_freeN(uv_id);
	}
	hair_cache->indices = GPU_indexbuf_build(&elb);
}

static void particle_batch_cache_ensure_pos(
        Object *object,
        ParticleSystem *psys,
        ParticlePointCache *point_cache)
{
	if (point_cache->pos != NULL) {
		return;
	}

	static GPUVertFormat format = { 0 };
	static uint pos_id, rot_id, val_id;
	int i, curr_point;
	ParticleData *pa;
	ParticleKey state;
	ParticleSimulationData sim = {NULL};
	const DRWContextState *draw_ctx = DRW_context_state_get();

	sim.depsgraph = draw_ctx->depsgraph;
	sim.scene = draw_ctx->scene;
	sim.ob = object;
	sim.psys = psys;
	sim.psmd = psys_get_modifier(object, psys);

	if (psys->part->phystype == PART_PHYS_KEYED) {
		if (psys->flag & PSYS_KEYED) {
			psys_count_keyed_targets(&sim);
			if (psys->totkeyed == 0)
				return;
		}
	}

	GPU_VERTBUF_DISCARD_SAFE(point_cache->pos);

	if (format.attr_len == 0) {
		/* initialize vertex format */
		pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
		rot_id = GPU_vertformat_attr_add(&format, "rot", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
		val_id = GPU_vertformat_attr_add(&format, "val", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
	}

	point_cache->pos = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(point_cache->pos, psys->totpart);

	for (curr_point = 0, i = 0, pa = psys->particles; i < psys->totpart; i++, pa++) {
		state.time = DEG_get_ctime(draw_ctx->depsgraph);
		if (!psys_get_particle_state(&sim, i, &state, 0)) {
			continue;
		}

		float val;

		GPU_vertbuf_attr_set(point_cache->pos, pos_id, curr_point, pa->state.co);
		GPU_vertbuf_attr_set(point_cache->pos, rot_id, curr_point, pa->state.rot);

		switch (psys->part->draw_col) {
			case PART_DRAW_COL_VEL:
				val = len_v3(pa->state.vel) / psys->part->color_vec_max;
				break;
			case PART_DRAW_COL_ACC:
				val = len_v3v3(
				        pa->state.vel,
				        pa->prev_state.vel) / ((pa->state.time - pa->prev_state.time) * psys->part->color_vec_max);
				break;
			default:
				val = -1.0f;
				break;
		}

		GPU_vertbuf_attr_set(point_cache->pos, val_id, curr_point, &val);

		curr_point++;
	}

	if (curr_point != psys->totpart) {
		GPU_vertbuf_data_resize(point_cache->pos, curr_point);
	}
}

static void drw_particle_update_ptcache_edit(
        Object *object_eval,
        ParticleSystem *psys,
        PTCacheEdit *edit)
{
	if (edit->psys == NULL) {
		return;
	}
	/* NOTE: Get flag from particle system coming from drawing object.
	 * this is where depsgraph will be setting flags to.
	 */
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Scene *scene_orig = (Scene *)DEG_get_original_id(&draw_ctx->scene->id);
	Object *object_orig = DEG_get_original_object(object_eval);
	if (psys->flag & PSYS_HAIR_UPDATED) {
		PE_update_object(draw_ctx->depsgraph, scene_orig, object_orig, 0);
		psys->flag &= ~PSYS_HAIR_UPDATED;
	}
	if (edit->pathcache == NULL) {
		Depsgraph *depsgraph = draw_ctx->depsgraph;
		psys_cache_edit_paths(
		        depsgraph,
		        scene_orig, object_orig,
		        edit,
		        DEG_get_ctime(depsgraph),
		        DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
	}
}

static void drw_particle_update_ptcache(
        Object *object_eval,
        ParticleSystem *psys)
{
	if ((object_eval->mode & OB_MODE_PARTICLE_EDIT) == 0) {
		return;
	}
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Scene *scene_orig = (Scene *)DEG_get_original_id(&draw_ctx->scene->id);
	Object *object_orig = DEG_get_original_object(object_eval);
	PTCacheEdit *edit = PE_create_current(
	        draw_ctx->depsgraph, scene_orig, object_orig);
	if (edit != NULL) {
		drw_particle_update_ptcache_edit(object_eval, psys, edit);
	}
}

typedef struct ParticleDrawSource {
	Object *object;
	ParticleSystem *psys;
	ModifierData *md;
	PTCacheEdit *edit;
} ParticleDrawSource;

static void drw_particle_get_hair_source(
        Object *object,
        ParticleSystem *psys,
        ModifierData *md,
        PTCacheEdit *edit,
        ParticleDrawSource *r_draw_source)
{
	r_draw_source->object = object;
	r_draw_source->psys = psys;
	r_draw_source->md = md;
	r_draw_source->edit = edit;
	if ((object->mode & OB_MODE_PARTICLE_EDIT) != 0) {
		r_draw_source->object = DEG_get_original_object(object);
		r_draw_source->psys = psys_orig_get(psys);
	}
}

GPUBatch *DRW_particles_batch_cache_get_hair(
        Object *object,
        ParticleSystem *psys,
        ModifierData *md)
{
	ParticleBatchCache *cache = particle_batch_cache_get(psys);
	if (cache->hair.hairs == NULL) {
		drw_particle_update_ptcache(object, psys);
		ParticleDrawSource source;
		drw_particle_get_hair_source(object, psys, md, NULL, &source);
		ensure_seg_pt_count(source.edit, source.psys, &cache->hair);
		particle_batch_cache_ensure_pos_and_seg(source.edit, source.psys, source.md, &cache->hair);
		cache->hair.hairs = GPU_batch_create(
		        GPU_PRIM_LINE_STRIP,
		        cache->hair.pos,
		        cache->hair.indices);
	}
	return cache->hair.hairs;
}

GPUBatch *DRW_particles_batch_cache_get_dots(Object *object, ParticleSystem *psys)
{
	ParticleBatchCache *cache = particle_batch_cache_get(psys);

	if (cache->point.points == NULL) {
		particle_batch_cache_ensure_pos(object, psys, &cache->point);
		cache->point.points = GPU_batch_create(GPU_PRIM_POINTS, cache->point.pos, NULL);
	}

	return cache->point.points;
}

GPUBatch *DRW_particles_batch_cache_get_edit_strands(
        Object *object,
        ParticleSystem *psys,
        PTCacheEdit *edit)
{
	ParticleBatchCache *cache = particle_batch_cache_get(psys);
	if (cache->edit_hair.hairs != NULL) {
		return cache->edit_hair.hairs;
	}
	drw_particle_update_ptcache_edit(object, psys, edit);
	ensure_seg_pt_count(edit, psys, &cache->edit_hair);
	particle_batch_cache_ensure_pos_and_seg(edit, psys, NULL, &cache->edit_hair);
	cache->edit_hair.hairs = GPU_batch_create(
	        GPU_PRIM_LINE_STRIP,
	        cache->edit_hair.pos,
	        cache->edit_hair.indices);
	return cache->edit_hair.hairs;
}

static void ensure_edit_inner_points_count(
        const PTCacheEdit *edit,
        ParticleBatchCache *cache)
{
	if (cache->edit_inner_pos != NULL) {
		return;
	}
	cache->edit_inner_point_len = 0;
	for (int point_index = 0; point_index < edit->totpoint; point_index++) {
		const PTCacheEditPoint *point = &edit->points[point_index];
		BLI_assert(point->totkey >= 1);
		cache->edit_inner_point_len += (point->totkey - 1);
	}
}

static void edit_colors_get(
        PTCacheEdit *edit,
        float selected_color[4],
        float normal_color[4])
{
	rgb_uchar_to_float(selected_color, edit->sel_col);
	rgb_uchar_to_float(normal_color, edit->nosel_col);
	selected_color[3] = 1.0f;
	normal_color[3] = 1.0f;
}

static void particle_batch_cache_ensure_edit_inner_pos(
        PTCacheEdit *edit,
        ParticleBatchCache *cache)
{
	if (cache->edit_inner_pos != NULL) {
		return;
	}

	static GPUVertFormat format = { 0 };
	static uint pos_id, color_id;

	GPU_VERTBUF_DISCARD_SAFE(cache->edit_inner_pos);

	if (format.attr_len == 0) {
		/* initialize vertex format */
		pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
	}

	cache->edit_inner_pos = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(cache->edit_inner_pos, cache->edit_inner_point_len);

	float selected_color[4], normal_color[4];
	edit_colors_get(edit, selected_color, normal_color);

	int global_key_index = 0;
	for (int point_index = 0; point_index < edit->totpoint; point_index++) {
		const PTCacheEditPoint *point = &edit->points[point_index];
		for (int key_index = 0; key_index < point->totkey - 1; key_index++) {
			PTCacheEditKey *key = &point->keys[key_index];
			GPU_vertbuf_attr_set(cache->edit_inner_pos, pos_id, global_key_index, key->world_co);
			if (key->flag & PEK_SELECT) {
				GPU_vertbuf_attr_set(cache->edit_inner_pos, color_id, global_key_index, selected_color);
			}
			else {
				GPU_vertbuf_attr_set(cache->edit_inner_pos, color_id, global_key_index, normal_color);
			}
			global_key_index++;
		}
	}
}

GPUBatch *DRW_particles_batch_cache_get_edit_inner_points(
        Object *object,
        ParticleSystem *psys,
        PTCacheEdit *edit)
{
	ParticleBatchCache *cache = particle_batch_cache_get(psys);
	if (cache->edit_inner_points != NULL) {
		return cache->edit_inner_points;
	}
	drw_particle_update_ptcache_edit(object, psys, edit);
	ensure_edit_inner_points_count(edit, cache);
	particle_batch_cache_ensure_edit_inner_pos(edit, cache);
	cache->edit_inner_points = GPU_batch_create(
	        GPU_PRIM_POINTS,
	        cache->edit_inner_pos,
	        NULL);
	return cache->edit_inner_points;
}

static void ensure_edit_tip_points_count(
        const PTCacheEdit *edit,
        ParticleBatchCache *cache)
{
	if (cache->edit_tip_pos != NULL) {
		return;
	}
	cache->edit_tip_point_len = edit->totpoint;
}

static void particle_batch_cache_ensure_edit_tip_pos(
        PTCacheEdit *edit,
        ParticleBatchCache *cache)
{
	if (cache->edit_tip_pos != NULL) {
		return;
	}

	static GPUVertFormat format = { 0 };
	static uint pos_id, color_id;

	GPU_VERTBUF_DISCARD_SAFE(cache->edit_tip_pos);

	if (format.attr_len == 0) {
		/* initialize vertex format */
		pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
	}

	cache->edit_tip_pos = GPU_vertbuf_create_with_format(&format);
	GPU_vertbuf_data_alloc(cache->edit_tip_pos, cache->edit_tip_point_len);

	float selected_color[4], normal_color[4];
	edit_colors_get(edit, selected_color, normal_color);

	for (int point_index = 0; point_index < edit->totpoint; point_index++) {
		const PTCacheEditPoint *point = &edit->points[point_index];
		PTCacheEditKey *key = &point->keys[point->totkey - 1];
		GPU_vertbuf_attr_set(cache->edit_tip_pos, pos_id, point_index, key->world_co);
		if (key->flag & PEK_SELECT) {
			GPU_vertbuf_attr_set(cache->edit_tip_pos, color_id, point_index, selected_color);
		}
		else {
			GPU_vertbuf_attr_set(cache->edit_tip_pos, color_id, point_index, normal_color);
		}
	}
}

GPUBatch *DRW_particles_batch_cache_get_edit_tip_points(
        Object *object,
        ParticleSystem *psys,
        PTCacheEdit *edit)
{
	ParticleBatchCache *cache = particle_batch_cache_get(psys);
	if (cache->edit_tip_points != NULL) {
		return cache->edit_tip_points;
	}
	drw_particle_update_ptcache_edit(object, psys, edit);
	ensure_edit_tip_points_count(edit, cache);
	particle_batch_cache_ensure_edit_tip_pos(edit, cache);
	cache->edit_tip_points = GPU_batch_create(
	        GPU_PRIM_POINTS,
	        cache->edit_tip_pos,
	        NULL);
	return cache->edit_tip_points;
}

/* Ensure all textures and buffers needed for GPU accelerated drawing. */
bool particles_ensure_procedural_data(
        Object *object,
        ParticleSystem *psys,
        ModifierData *md,
        ParticleHairCache **r_hair_cache,
        int subdiv,
        int thickness_res)
{
	bool need_ft_update = false;

	drw_particle_update_ptcache(object, psys);

	ParticleDrawSource source;
	drw_particle_get_hair_source(object, psys, md, NULL, &source);

	ParticleSettings *part = source.psys->part;
	ParticleBatchCache *cache = particle_batch_cache_get(source.psys);
	*r_hair_cache = &cache->hair;

	(*r_hair_cache)->final[subdiv].strands_res = 1 << (part->draw_step + subdiv);

	/* Refreshed on combing and simulation. */
	if ((*r_hair_cache)->proc_point_buf == NULL) {
		ensure_seg_pt_count(source.edit, source.psys, &cache->hair);
		particle_batch_cache_ensure_procedural_pos(source.edit, source.psys, &cache->hair);
		need_ft_update = true;
	}

	/* Refreshed if active layer or custom data changes. */
	if ((*r_hair_cache)->strand_tex == NULL) {
		particle_batch_cache_ensure_procedural_strand_data(source.edit, source.psys, source.md, &cache->hair);
	}

	/* Refreshed only on subdiv count change. */
	if ((*r_hair_cache)->final[subdiv].proc_buf == NULL) {
		particle_batch_cache_ensure_procedural_final_points(&cache->hair, subdiv);
		need_ft_update = true;
	}
	if ((*r_hair_cache)->final[subdiv].proc_hairs[thickness_res - 1] == NULL) {
		particle_batch_cache_ensure_procedural_indices(source.edit, source.psys, &cache->hair, thickness_res, subdiv);
	}


	return need_ft_update;
}
