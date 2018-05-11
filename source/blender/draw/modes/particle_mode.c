/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/modes/particle_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_particle.h"
#include "BKE_pointcache.h"

#include "GPU_shader.h"
#include "GPU_batch.h"

#include "draw_common.h"

#include "draw_mode_engines.h"

#include "ED_particle.h"

#include "DEG_depsgraph_query.h"

#include "draw_cache_impl.h"

extern char datatoc_particle_strand_vert_glsl[];
extern char datatoc_particle_strand_frag_glsl[];

/* *********** LISTS *********** */

typedef struct PARTICLE_PassList {
	struct DRWPass *psys_edit_pass;
} PARTICLE_PassList;

typedef struct PARTICLE_FramebufferList {
	struct GPUFrameBuffer *fb;
} PARTICLE_FramebufferList;

typedef struct PARTICLE_TextureList {
	struct GPUTexture *texture;
} PARTICLE_TextureList;

typedef struct PARTICLE_StorageList {
	struct CustomStruct *block;
	struct PARTICLE_PrivateData *g_data;
} PARTICLE_StorageList;

typedef struct PARTICLE_Data {
	void *engine_type; /* Required */
	PARTICLE_FramebufferList *fbl;
	PARTICLE_TextureList *txl;
	PARTICLE_PassList *psl;
	PARTICLE_StorageList *stl;
} PARTICLE_Data;

/* *********** STATIC *********** */

static struct {
	struct GPUShader *strands_shader;
	struct GPUShader *points_shader;
} e_data = {NULL}; /* Engine data */

typedef struct PARTICLE_PrivateData {
	DRWShadingGroup *strands_group;
	DRWShadingGroup *inner_points_group;
	DRWShadingGroup *tip_points_group;
} PARTICLE_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

static void particle_engine_init(void *UNUSED(vedata))
{
	if (!e_data.strands_shader) {
		e_data.strands_shader = DRW_shader_create(
		        datatoc_particle_strand_vert_glsl,
		        NULL,
		        datatoc_particle_strand_frag_glsl,
		        "");
	}
	if (!e_data.points_shader) {
		e_data.points_shader = GPU_shader_get_builtin_shader(
		        GPU_SHADER_3D_POINT_FIXED_SIZE_VARYING_COLOR);
	}
}

static void particle_cache_init(void *vedata)
{
	PARTICLE_PassList *psl = ((PARTICLE_Data *)vedata)->psl;
	PARTICLE_StorageList *stl = ((PARTICLE_Data *)vedata)->stl;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	/* Create a pass */
	psl->psys_edit_pass = DRW_pass_create("PSys Edit Pass",
	                                      (DRW_STATE_WRITE_COLOR |
	                                       DRW_STATE_WRITE_DEPTH |
	                                       DRW_STATE_DEPTH_LESS |
	                                       DRW_STATE_WIRE |
	                                       DRW_STATE_POINT));

	stl->g_data->strands_group = DRW_shgroup_create(
	        e_data.strands_shader, psl->psys_edit_pass);
	stl->g_data->inner_points_group = DRW_shgroup_create(
	        e_data.points_shader, psl->psys_edit_pass);
	stl->g_data->tip_points_group = DRW_shgroup_create(
	        e_data.points_shader, psl->psys_edit_pass);

	static float size = 5.0f;
	static float outline_width = 1.0f;
	DRW_shgroup_uniform_float(stl->g_data->inner_points_group, "size", &size, 1);
	DRW_shgroup_uniform_float(stl->g_data->inner_points_group, "outlineWidth", &outline_width, 1);
	DRW_shgroup_uniform_float(stl->g_data->tip_points_group, "size", &size, 1);
	DRW_shgroup_uniform_float(stl->g_data->tip_points_group, "outlineWidth", &outline_width, 1);
}

/* TODO(sergey): Avoid linear lookup. */
static void draw_update_ptcache_edit(Object *object_eval, PTCacheEdit *edit)
{
	if (edit->psys == NULL) {
		return;
	}
	ParticleSystem *psys_eval;
	for (psys_eval = object_eval->particlesystem.first;
	     psys_eval != NULL;
	     psys_eval = psys_eval->next)
	{
		if (STREQ(edit->psys->name, psys_eval->name)) {
			break;
		}
	}
	if (psys_eval->flag & PSYS_HAIR_UPDATED) {
		const DRWContextState *draw_ctx = DRW_context_state_get();
		Scene *scene_orig = (Scene *)DEG_get_original_id(&draw_ctx->scene->id);
		Object *object_orig = DEG_get_original_object(object_eval);
		PE_update_object(draw_ctx->depsgraph, scene_orig, object_orig, 0);
	}
	BLI_assert(edit->pathcache != NULL);
}

static void particle_edit_cache_populate(void *vedata,
                                         Object *object,
                                         PTCacheEdit *edit)
{
	PARTICLE_StorageList *stl = ((PARTICLE_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	draw_update_ptcache_edit(object, edit);
	ParticleEditSettings *pset = PE_settings(draw_ctx->scene);
	{
		struct Gwn_Batch *strands =
		        DRW_cache_particles_get_edit_strands(object, edit);
		DRW_shgroup_call_add(stl->g_data->strands_group, strands, NULL);
	}
	if (pset->selectmode == SCE_SELECT_POINT) {
		struct Gwn_Batch *points =
		        DRW_cache_particles_get_edit_inner_points(object, edit);
		DRW_shgroup_call_add(stl->g_data->inner_points_group, points, NULL);
	}
	if (ELEM(pset->selectmode, SCE_SELECT_POINT, SCE_SELECT_END)) {
		struct Gwn_Batch *points =
		        DRW_cache_particles_get_edit_tip_points(object, edit);
		DRW_shgroup_call_add(stl->g_data->tip_points_group, points, NULL);
	}
}

static void particle_cache_populate(void *vedata, Object *object)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Object *object_orig = DEG_get_original_object(object);
	PTCacheEdit *edit = PE_get_current(draw_ctx->scene, object_orig);
	if (edit == NULL) {
		printf("Particle edit struct is NULL, not supposed to happen.\n");
		return;
	}
	particle_edit_cache_populate(vedata, object, edit);
}

/* Optional: Post-cache_populate callback */
static void particle_cache_finish(void *UNUSED(vedata))
{
}

/* Draw time ! Control rendering pipeline from here */
static void particle_draw_scene(void *vedata)
{

	PARTICLE_PassList *psl = ((PARTICLE_Data *)vedata)->psl;

	DRW_draw_pass(psl->psys_edit_pass);
}

static void particle_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.strands_shader);
}

static const DrawEngineDataSize particle_data_size =
      DRW_VIEWPORT_DATA_SIZE(PARTICLE_Data);

DrawEngineType draw_engine_particle_type = {
	NULL, NULL,
	N_("Particle Mode"),
	&particle_data_size,
	&particle_engine_init,
	&particle_engine_free,
	&particle_cache_init,
	&particle_cache_populate,
	&particle_cache_finish,
	NULL, /* draw_background but not needed by mode engines */
	&particle_draw_scene,
	NULL,
	NULL,
};
