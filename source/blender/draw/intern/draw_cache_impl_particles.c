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

#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"

#include "BKE_particle.h"
#include "BKE_DerivedMesh.h"

#include "GPU_batch.h"

#include "DEG_depsgraph_query.h"

#include "draw_cache_impl.h"  /* own include */

static void particle_batch_cache_clear(ParticleSystem *psys);

/* ---------------------------------------------------------------------- */
/* Particle Gwn_Batch Cache */

typedef struct ParticleBatchCache {
	Gwn_VertBuf *pos;
	Gwn_IndexBuf *indices;

	Gwn_Batch *hairs;

	int elems_count;
	int point_count;

	/* settings to determine if cache is invalid */
	bool is_dirty;
} ParticleBatchCache;

/* Gwn_Batch cache management. */

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

	GWN_BATCH_DISCARD_SAFE(cache->hairs);

	GWN_VERTBUF_DISCARD_SAFE(cache->pos);
	GWN_INDEXBUF_DISCARD_SAFE(cache->indices);
}

void DRW_particle_batch_cache_free(ParticleSystem *psys)
{
	particle_batch_cache_clear(psys);
	MEM_SAFE_FREE(psys->batch_cache);
}

static void ensure_seg_pt_count(ParticleSystem *psys, ParticleBatchCache *cache)
{
	if (cache->pos == NULL || cache->indices == NULL) {
		cache->elems_count = 0;
		cache->point_count = 0;

		if (psys->pathcache && (!psys->childcache || (psys->part->draw & PART_DRAW_PARENT))) {
			for (int i = 0; i < psys->totpart; i++) {
				ParticleCacheKey *path = psys->pathcache[i];

				if (path->segments > 0) {
					cache->elems_count += path->segments + 2;
					cache->point_count += path->segments + 1;
				}
			}
		}

		if (psys->childcache) {
			int child_count = psys->totchild * psys->part->disp / 100;

			for (int i = 0; i < child_count; i++) {
				ParticleCacheKey *path = psys->childcache[i];

				if (path->segments > 0) {
					cache->elems_count += path->segments + 2;
					cache->point_count += path->segments + 1;
				}
			}
		}
	}
}

/* Gwn_Batch cache usage. */
static void particle_batch_cache_ensure_pos_and_seg(ParticleSystem *psys, ModifierData *md, ParticleBatchCache *cache)
{
	if (cache->pos != NULL && cache->indices != NULL) {
		return;
	}

	int curr_point = 0;
	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;

	GWN_VERTBUF_DISCARD_SAFE(cache->pos);
	GWN_INDEXBUF_DISCARD_SAFE(cache->indices);

	static Gwn_VertFormat format = { 0 };
	static struct { uint pos, tan, ind; } attr_id;
	unsigned int *uv_id = NULL;
	int uv_layers = 0;
	MTFace **mtfaces = NULL;
	float (**parent_uvs)[2] = NULL;
	bool simple = psys->part->childtype == PART_CHILD_PARTICLES;

	if (psmd) {
		if (CustomData_has_layer(&psmd->dm_final->loopData, CD_MLOOPUV)) {
			uv_layers = CustomData_number_of_layers(&psmd->dm_final->loopData, CD_MLOOPUV);
		}
	}

	GWN_vertformat_clear(&format);

	/* initialize vertex format */
	attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	attr_id.tan = GWN_vertformat_attr_add(&format, "nor", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	attr_id.ind = GWN_vertformat_attr_add(&format, "ind", GWN_COMP_I32, 1, GWN_FETCH_INT);

	if (psmd) {
		uv_id = MEM_mallocN(sizeof(*uv_id) * uv_layers, "UV attrib format");

		for (int i = 0; i < uv_layers; i++) {
			const char *name = CustomData_get_layer_name(&psmd->dm_final->loopData, CD_MLOOPUV, i);
			char uuid[32];

			BLI_snprintf(uuid, sizeof(uuid), "u%u", BLI_ghashutil_strhash_p(name));
			uv_id[i] = GWN_vertformat_attr_add(&format, uuid, GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		}
	}

	cache->pos = GWN_vertbuf_create_with_format(&format);
	GWN_vertbuf_data_alloc(cache->pos, cache->point_count);

	Gwn_IndexBufBuilder elb;
	GWN_indexbuf_init_ex(&elb, GWN_PRIM_LINE_STRIP, cache->elems_count, cache->point_count, true);

	if (uv_layers) {
		DM_ensure_tessface(psmd->dm_final);

		mtfaces = MEM_mallocN(sizeof(*mtfaces) * uv_layers, "Faces UV layers");

		for (int i = 0; i < uv_layers; i++) {
			mtfaces[i] = (MTFace *)CustomData_get_layer_n(&psmd->dm_final->faceData, CD_MTFACE, i);
		}
	}

	if (psys->pathcache && (!psys->childcache || (psys->part->draw & PART_DRAW_PARENT))) {
		if (simple) {
			parent_uvs = MEM_callocN(sizeof(*parent_uvs) * psys->totpart, "Parent particle UVs");
		}

		for (int i = 0; i < psys->totpart; i++) {
			ParticleCacheKey *path = psys->pathcache[i];

			if (path->segments > 0) {
				float tangent[3];
				int from = psmd ? psmd->psys->part->from : 0;
				float (*uv)[2] = NULL;

				if (psmd) {
					uv = MEM_callocN(sizeof(*uv) * uv_layers, "Particle UVs");

					if (simple) {
						parent_uvs[i] = uv;
					}
				}

				if (ELEM(from, PART_FROM_FACE, PART_FROM_VOLUME)) {
					ParticleData *particle = &psys->particles[i];
					int num = particle->num_dmcache;

					if (num == DMCACHE_NOTFOUND) {
						if (particle->num < psmd->dm_final->getNumTessFaces(psmd->dm_final)) {
							num = particle->num;
						}
					}

					if (num != DMCACHE_NOTFOUND) {
						MFace *mface = psmd->dm_final->getTessFaceData(psmd->dm_final, num, CD_MFACE);

						for (int j = 0; j < uv_layers; j++) {
							psys_interpolate_uvs(mtfaces[j] + num, mface->v4, particle->fuv, uv[j]);
						}
					}
				}

				for (int j = 0; j < path->segments; j++) {
					if (j == 0) {
						sub_v3_v3v3(tangent, path[j + 1].co, path[j].co);
					}
					else {
						sub_v3_v3v3(tangent, path[j + 1].co, path[j - 1].co);
					}

					GWN_vertbuf_attr_set(cache->pos, attr_id.pos, curr_point, path[j].co);
					GWN_vertbuf_attr_set(cache->pos, attr_id.tan, curr_point, tangent);
					GWN_vertbuf_attr_set(cache->pos, attr_id.ind, curr_point, &i);

					if (psmd) {
						for (int k = 0; k < uv_layers; k++) {
							GWN_vertbuf_attr_set(cache->pos, uv_id[k], curr_point, uv[k]);
						}
					}

					GWN_indexbuf_add_generic_vert(&elb, curr_point);

					curr_point++;
				}

				sub_v3_v3v3(tangent, path[path->segments].co, path[path->segments - 1].co);

				GWN_vertbuf_attr_set(cache->pos, attr_id.pos, curr_point, path[path->segments].co);
				GWN_vertbuf_attr_set(cache->pos, attr_id.tan, curr_point, tangent);
				GWN_vertbuf_attr_set(cache->pos, attr_id.ind, curr_point, &i);

				if (psmd) {
					for (int k = 0; k < uv_layers; k++) {
						GWN_vertbuf_attr_set(cache->pos, uv_id[k], curr_point, uv[k]);
					}

					if (!simple) {
						MEM_freeN(uv);
					}
				}

				/* finish the segment and add restart primitive */
				GWN_indexbuf_add_generic_vert(&elb, curr_point);
				GWN_indexbuf_add_primitive_restart(&elb);

				curr_point++;
			}
		}
	}

	if (psys->childcache) {
		int child_count = psys->totchild * psys->part->disp / 100;

		if (simple && !parent_uvs) {
			parent_uvs = MEM_callocN(sizeof(*parent_uvs) * psys->totpart, "Parent particle UVs");
		}

		for (int i = 0, x = psys->totpart; i < child_count; i++, x++) {
			ParticleCacheKey *path = psys->childcache[i];
			float tangent[3];

			if (path->segments > 0) {
				int from = psmd ? psmd->psys->part->from : 0;
				float (*uv)[2] = NULL;

				if (!simple) {
					if (psmd) {
						uv = MEM_callocN(sizeof(*uv) * uv_layers, "Particle UVs");
					}

					if (ELEM(from, PART_FROM_FACE, PART_FROM_VOLUME)) {
						ChildParticle *particle = &psys->child[i];
						int num = particle->num;

						if (num != DMCACHE_NOTFOUND) {
							MFace *mface = psmd->dm_final->getTessFaceData(psmd->dm_final, num, CD_MFACE);

							for (int j = 0; j < uv_layers; j++) {
								psys_interpolate_uvs(mtfaces[j] + num, mface->v4, particle->fuv, uv[j]);
							}
						}
					}
				}
				else if (!parent_uvs[psys->child[i].parent]) {
					if (psmd) {
						parent_uvs[psys->child[i].parent] = MEM_callocN(sizeof(*uv) * uv_layers, "Particle UVs");
					}

					if (ELEM(from, PART_FROM_FACE, PART_FROM_VOLUME)) {
						ParticleData *particle = &psys->particles[psys->child[i].parent];
						int num = particle->num_dmcache;

						if (num == DMCACHE_NOTFOUND) {
							if (particle->num < psmd->dm_final->getNumTessFaces(psmd->dm_final)) {
								num = particle->num;
							}
						}

						if (num != DMCACHE_NOTFOUND) {
							MFace *mface = psmd->dm_final->getTessFaceData(psmd->dm_final, num, CD_MFACE);

							for (int j = 0; j < uv_layers; j++) {
								psys_interpolate_uvs(mtfaces[j] + num, mface->v4, particle->fuv, parent_uvs[psys->child[i].parent][j]);
							}
						}
					}
				}

				for (int j = 0; j < path->segments; j++) {
					if (j == 0) {
						sub_v3_v3v3(tangent, path[j + 1].co, path[j].co);
					}
					else {
						sub_v3_v3v3(tangent, path[j + 1].co, path[j - 1].co);
					}

					GWN_vertbuf_attr_set(cache->pos, attr_id.pos, curr_point, path[j].co);
					GWN_vertbuf_attr_set(cache->pos, attr_id.tan, curr_point, tangent);
					GWN_vertbuf_attr_set(cache->pos, attr_id.ind, curr_point, &x);

					if (psmd) {
						for (int k = 0; k < uv_layers; k++) {
							GWN_vertbuf_attr_set(cache->pos, uv_id[k], curr_point,
							                     simple ? parent_uvs[psys->child[i].parent][k] : uv[k]);
						}
					}

					GWN_indexbuf_add_generic_vert(&elb, curr_point);

					curr_point++;
				}

				sub_v3_v3v3(tangent, path[path->segments].co, path[path->segments - 1].co);

				GWN_vertbuf_attr_set(cache->pos, attr_id.pos, curr_point, path[path->segments].co);
				GWN_vertbuf_attr_set(cache->pos, attr_id.tan, curr_point, tangent);
				GWN_vertbuf_attr_set(cache->pos, attr_id.ind, curr_point, &x);

				if (psmd) {
					for (int k = 0; k < uv_layers; k++) {
						GWN_vertbuf_attr_set(cache->pos, uv_id[k], curr_point,
						                     simple ? parent_uvs[psys->child[i].parent][k] : uv[k]);
					}

					if (!simple) {
						MEM_freeN(uv);
					}
				}

				/* finish the segment and add restart primitive */
				GWN_indexbuf_add_generic_vert(&elb, curr_point);
				GWN_indexbuf_add_primitive_restart(&elb);

				curr_point++;
			}
		}
	}

	if (parent_uvs) {
		for (int i = 0; i < psys->totpart; i++) {
			MEM_SAFE_FREE(parent_uvs[i]);
		}

		MEM_freeN(parent_uvs);
	}

	if (uv_layers) {
		MEM_freeN(mtfaces);
	}

	if (psmd) {
		MEM_freeN(uv_id);
	}

	cache->indices = GWN_indexbuf_build(&elb);
}

static void particle_batch_cache_ensure_pos(Object *object, ParticleSystem *psys, ParticleBatchCache *cache)
{
	if (cache->pos != NULL) {
		return;
	}

	static Gwn_VertFormat format = { 0 };
	static unsigned pos_id, rot_id, val_id;
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

	GWN_VERTBUF_DISCARD_SAFE(cache->pos);
	GWN_INDEXBUF_DISCARD_SAFE(cache->indices);

	if (format.attrib_ct == 0) {
		/* initialize vertex format */
		pos_id = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		rot_id = GWN_vertformat_attr_add(&format, "rot", GWN_COMP_F32, 4, GWN_FETCH_FLOAT);
		val_id = GWN_vertformat_attr_add(&format, "val", GWN_COMP_F32, 1, GWN_FETCH_FLOAT);
	}

	cache->pos = GWN_vertbuf_create_with_format(&format);
	GWN_vertbuf_data_alloc(cache->pos, psys->totpart);

	for (curr_point = 0, i = 0, pa = psys->particles; i < psys->totpart; i++, pa++) {
		state.time = DEG_get_ctime(draw_ctx->depsgraph);
		if (!psys_get_particle_state(&sim, curr_point, &state, 0)) {
			continue;
		}

		float val;

		GWN_vertbuf_attr_set(cache->pos, pos_id, curr_point, pa->state.co);
		GWN_vertbuf_attr_set(cache->pos, rot_id, curr_point, pa->state.rot);

		switch (psys->part->draw_col) {
			case PART_DRAW_COL_VEL:
				val = len_v3(pa->state.vel) / psys->part->color_vec_max;
				break;
			case PART_DRAW_COL_ACC:
				val = len_v3v3(pa->state.vel, pa->prev_state.vel) / ((pa->state.time - pa->prev_state.time) * psys->part->color_vec_max);
				break;
			default:
				val = -1.0f;
				break;
		}

		GWN_vertbuf_attr_set(cache->pos, val_id, curr_point, &val);

		curr_point++;
	}

	if (curr_point != psys->totpart) {
		GWN_vertbuf_data_resize(cache->pos, curr_point);
	}
}

Gwn_Batch *DRW_particles_batch_cache_get_hair(ParticleSystem *psys, ModifierData *md)
{
	ParticleBatchCache *cache = particle_batch_cache_get(psys);

	if (cache->hairs == NULL) {
		ensure_seg_pt_count(psys, cache);
		particle_batch_cache_ensure_pos_and_seg(psys, md, cache);
		cache->hairs = GWN_batch_create(GWN_PRIM_LINE_STRIP, cache->pos, cache->indices);
	}

	return cache->hairs;
}

Gwn_Batch *DRW_particles_batch_cache_get_dots(Object *object, ParticleSystem *psys)
{
	ParticleBatchCache *cache = particle_batch_cache_get(psys);

	if (cache->hairs == NULL) {
		particle_batch_cache_ensure_pos(object, psys, cache);
		cache->hairs = GWN_batch_create(GWN_PRIM_POINTS, cache->pos, NULL);
	}

	return cache->hairs;
}
