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

/** \file overlay_mode.c
 *  \ingroup draw_engine
 */

#include "DNA_view3d_types.h"

#include "GPU_shader.h"
#include "DRW_render.h"

#include "draw_mode_engines.h"

/* Structures */
typedef struct OVERLAY_StorageList {
	struct OVERLAY_PrivateData *g_data;
} OVERLAY_StorageList;

typedef struct OVERLAY_PassList {
	struct DRWPass *face_orientation_pass;
} OVERLAY_PassList;

typedef struct OVERLAY_Data {
	void *engine_type;
	DRWViewportEmptyList *fbl;
	DRWViewportEmptyList *txl;
	OVERLAY_PassList *psl;
	OVERLAY_StorageList *stl;
} OVERLAY_Data;

typedef struct OVERLAY_PrivateData {
	DRWShadingGroup *face_orientation_shgrp;
	int overlays;
} OVERLAY_PrivateData; /* Transient data */

/* *********** STATIC *********** */
static struct {
	/* Face orientation shader */
	struct GPUShader *face_orientation_sh;
} e_data = {NULL};

/* Shaders */
extern char datatoc_overlay_face_orientation_frag_glsl[];
extern char datatoc_overlay_face_orientation_vert_glsl[];


/* Functions */
static void overlay_engine_init(void *vedata)
{
	OVERLAY_Data * data = (OVERLAY_Data *)vedata;
	OVERLAY_StorageList *stl = data->stl;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	if (!e_data.face_orientation_sh) {
		/* Face orientation */
		e_data.face_orientation_sh = DRW_shader_create(datatoc_overlay_face_orientation_vert_glsl, NULL, datatoc_overlay_face_orientation_frag_glsl, "\n");
	}
}

static void overlay_cache_init(void *vedata)
{
	OVERLAY_Data * data = (OVERLAY_Data *)vedata;
	OVERLAY_PassList *psl = data->psl;
	OVERLAY_StorageList *stl = data->stl;

	const DRWContextState *DCS = DRW_context_state_get();

	View3D *v3d = DCS->v3d;
	if (v3d) {
		stl->g_data->overlays = v3d->overlays;
	}
	else {
		stl->g_data->overlays = 0;
	}

	/* Face Orientation Pass */
	if (stl->g_data->overlays & V3D_OVERLAY_FACE_ORIENTATION) {
		int state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND;
		psl->face_orientation_pass = DRW_pass_create("Face Orientation", state);
		stl->g_data->face_orientation_shgrp = DRW_shgroup_create(e_data.face_orientation_sh, psl->face_orientation_pass);
	}
}

static void overlay_cache_populate(void *vedata, Object *ob)
{
	OVERLAY_Data * data = (OVERLAY_Data *)vedata;
	OVERLAY_StorageList *stl = data->stl;
	OVERLAY_PrivateData *pd = stl->g_data;

	if (!DRW_object_is_renderable(ob))
		return;

	struct Gwn_Batch *geom = DRW_cache_object_surface_get(ob);
	if (geom) {
		/* Face Orientation */
		if (stl->g_data->overlays & V3D_OVERLAY_FACE_ORIENTATION) {
			DRW_shgroup_call_add(pd->face_orientation_shgrp, geom, ob->obmat);
		}
	}
}

static void overlay_cache_finish(void *UNUSED(vedata))
{
}

static void overlay_draw_scene(void *vedata)
{
	OVERLAY_Data * data = (OVERLAY_Data *)vedata;
	OVERLAY_PassList *psl = data->psl;
	OVERLAY_StorageList *stl = data->stl;
	OVERLAY_PrivateData *pd = stl->g_data;

	if (pd->overlays & V3D_OVERLAY_FACE_ORIENTATION) {
		DRW_draw_pass(psl->face_orientation_pass);
	}
}

static void overlay_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.face_orientation_sh);
}

static const DrawEngineDataSize overlay_data_size = DRW_VIEWPORT_DATA_SIZE(OVERLAY_Data);

DrawEngineType draw_engine_overlay_type = {
	NULL, NULL,
	N_("OverlayEngine"),
	&overlay_data_size,
	&overlay_engine_init,
	&overlay_engine_free,
	&overlay_cache_init,
	&overlay_cache_populate,
	&overlay_cache_finish,
	NULL,
	&overlay_draw_scene,
	NULL,
	NULL,
	NULL,
};

