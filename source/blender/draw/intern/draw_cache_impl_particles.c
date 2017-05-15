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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

#include "DNA_particle_types.h"

#include "BKE_particle.h"

#include "GPU_batch.h"

#include "draw_cache_impl.h"  /* own include */

static void particle_batch_cache_clear(ParticleSystem *psys);

/* ---------------------------------------------------------------------- */
/* Particle Batch Cache */

typedef struct ParticleBatchCache {
	VertexBuffer *pos;
	ElementList *segments;

	Batch *hairs;

	int segment_count;
	int point_count;

	/* settings to determine if cache is invalid */
	bool is_dirty;
} ParticleBatchCache;

/* Batch cache management. */

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

static void particle_batch_cache_clear(ParticleSystem *psys)
{
	ParticleBatchCache *cache = psys->batch_cache;
	if (!cache) {
		return;
	}

	BATCH_DISCARD_SAFE(cache->hairs);

	VERTEXBUFFER_DISCARD_SAFE(cache->pos);
	ELEMENTLIST_DISCARD_SAFE(cache->segments);
}

void DRW_particle_batch_cache_free(ParticleSystem *psys)
{
	particle_batch_cache_clear(psys);
	MEM_SAFE_FREE(psys->batch_cache);
}

static void ensure_seg_pt_count(ParticleSystem *psys, ParticleBatchCache *cache)
{
	if (cache->pos == NULL || cache->segments == NULL) {
		cache->segment_count = 0;
		cache->point_count = 0;

		if (psys->pathcache && (!psys->childcache || (psys->part->draw & PART_DRAW_PARENT))) {
			for (int i = 0; i < psys->totpart; i++) {
				ParticleCacheKey *path = psys->pathcache[i];

				if (path->segments) {
					cache->segment_count += path->segments;
					cache->point_count += path->segments + 1;
				}
			}
		}

		if (psys->childcache) {
			int child_count = psys->totchild * psys->part->disp / 100;

			for (int i = 0; i < child_count; i++) {
				ParticleCacheKey *path = psys->childcache[i];

				if (path->segments) {
					cache->segment_count += path->segments;
					cache->point_count += path->segments + 1;
				}
			}
		}
	}
}

/* Batch cache usage. */
static void particle_batch_cache_ensure_pos_and_seg(ParticleSystem *psys, ParticleBatchCache *cache)
{
	if (cache->pos == NULL || cache->segments == NULL) {
		static VertexFormat format = { 0 };
		static unsigned pos_id, tan_id, ind_id;
		int curr_point = 0;

		VERTEXBUFFER_DISCARD_SAFE(cache->pos);
		ELEMENTLIST_DISCARD_SAFE(cache->segments);

		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
			tan_id = VertexFormat_add_attrib(&format, "tang", COMP_F32, 3, KEEP_FLOAT);
			ind_id = VertexFormat_add_attrib(&format, "ind", COMP_I32, 1, KEEP_INT);
		}

		cache->pos = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(cache->pos, cache->point_count);

		ElementListBuilder elb;
		ElementListBuilder_init(&elb, PRIM_LINES, cache->segment_count, cache->point_count);

		if (psys->pathcache && (!psys->childcache || (psys->part->draw & PART_DRAW_PARENT))) {
			for (int i = 0; i < psys->totpart; i++) {
				ParticleCacheKey *path = psys->pathcache[i];

				if (path->segments) {
					float tangent[3];

					for (int j = 0; j < path->segments; j++) {
						if (j == 0) {
							sub_v3_v3v3(tangent, path[j + 1].co, path[j].co);
						}
						else {
							sub_v3_v3v3(tangent, path[j + 1].co, path[j - 1].co);
						}

						VertexBuffer_set_attrib(cache->pos, pos_id, curr_point, path[j].co);
						VertexBuffer_set_attrib(cache->pos, tan_id, curr_point, tangent);
						VertexBuffer_set_attrib(cache->pos, ind_id, curr_point, &i);

						add_line_vertices(&elb, curr_point, curr_point + 1);

						curr_point++;
					}

					sub_v3_v3v3(tangent, path[path->segments].co, path[path->segments - 1].co);

					VertexBuffer_set_attrib(cache->pos, pos_id, curr_point, path[path->segments].co);
					VertexBuffer_set_attrib(cache->pos, tan_id, curr_point, tangent);
					VertexBuffer_set_attrib(cache->pos, ind_id, curr_point, &i);

					curr_point++;
				}
			}
		}

		if (psys->childcache) {
			int child_count = psys->totchild * psys->part->disp / 100;

			for (int i = 0, x = psys->totpart; i < child_count; i++, x++) {
				ParticleCacheKey *path = psys->childcache[i];
				float tangent[3];

				if (path->segments) {
					for (int j = 0; j < path->segments; j++) {
						if (j == 0) {
							sub_v3_v3v3(tangent, path[j + 1].co, path[j].co);
						}
						else {
							sub_v3_v3v3(tangent, path[j + 1].co, path[j - 1].co);
						}

						VertexBuffer_set_attrib(cache->pos, pos_id, curr_point, path[j].co);
						VertexBuffer_set_attrib(cache->pos, tan_id, curr_point, tangent);
						VertexBuffer_set_attrib(cache->pos, ind_id, curr_point, &x);

						add_line_vertices(&elb, curr_point, curr_point + 1);

						curr_point++;
					}

					sub_v3_v3v3(tangent, path[path->segments].co, path[path->segments - 1].co);

					VertexBuffer_set_attrib(cache->pos, pos_id, curr_point, path[path->segments].co);
					VertexBuffer_set_attrib(cache->pos, tan_id, curr_point, tangent);
					VertexBuffer_set_attrib(cache->pos, ind_id, curr_point, &x);

					curr_point++;
				}
			}
		}

		cache->segments = ElementList_build(&elb);
	}
}

Batch *DRW_particles_batch_cache_get_hair(ParticleSystem *psys)
{
	ParticleBatchCache *cache = particle_batch_cache_get(psys);

	if (cache->hairs == NULL) {
		ensure_seg_pt_count(psys, cache);
		particle_batch_cache_ensure_pos_and_seg(psys, cache);
		cache->hairs = Batch_create(PRIM_LINES, cache->pos, cache->segments);
	}

	return cache->hairs;
}
