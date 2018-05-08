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

extern char datatoc_particle_vert_glsl[];
extern char datatoc_particle_strand_frag_glsl[];

/* *********** LISTS *********** */

typedef struct PARTICLE_PassList {
	struct DRWPass *hair_pass;
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
	struct GPUShader *hair_shader;
} e_data = {NULL}; /* Engine data */

typedef struct PARTICLE_PrivateData {
	DRWShadingGroup *hair_group;
} PARTICLE_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

static void particle_engine_init(void *UNUSED(vedata))
{
	if (!e_data.hair_shader) {
		e_data.hair_shader = DRW_shader_create(
		        datatoc_particle_vert_glsl,
		        NULL,
		        datatoc_particle_strand_frag_glsl,
		        "#define MAX_MATERIAL 1\n" );
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
	psl->hair_pass = DRW_pass_create("Hair Pass", (DRW_STATE_WRITE_COLOR |
	                                               DRW_STATE_WRITE_DEPTH |
	                                               DRW_STATE_DEPTH_LESS |
	                                               DRW_STATE_WIRE));

	stl->g_data->hair_group = DRW_shgroup_create(e_data.hair_shader,
	                                             psl->hair_pass);
}

static void particle_cache_populate(void *vedata, Object *object)
{
	PARTICLE_StorageList *stl = ((PARTICLE_Data *)vedata)->stl;
	for (ParticleSystem *psys = object->particlesystem.first;
	     psys != NULL;
	     psys = psys->next)
	{
		if (!psys_check_enabled(object, psys, false)) {
			continue;
		}
		if (PE_get_current_from_psys(psys) == NULL) {
			continue;
		}
		/* NOTE: Particle edit mode visualizes particles as strands. */
		struct Gwn_Batch *hair = DRW_cache_particles_get_hair(psys, NULL);
		DRW_shgroup_call_add(stl->g_data->hair_group, hair, NULL);
		break;
	}
}

/* Optional: Post-cache_populate callback */
static void particle_cache_finish(void *UNUSED(vedata))
{
}

/* Draw time ! Control rendering pipeline from here */
static void particle_draw_scene(void *vedata)
{

	PARTICLE_PassList *psl = ((PARTICLE_Data *)vedata)->psl;

	DRW_draw_pass(psl->hair_pass);
}

static void particle_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.hair_shader);
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
