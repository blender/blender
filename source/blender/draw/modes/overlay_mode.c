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
typedef struct OVERLAY_FramebufferList {
	struct GPUFrameBuffer *object_data_fb;
} OVERLAY_FramebufferList;

typedef struct OVERLAY_StorageList {
	struct OVERLAY_PrivateData *g_data;
} OVERLAY_StorageList;

typedef struct OVERLAY_PassList {
	struct DRWPass *face_orientation_pass;
	struct DRWPass *object_data_pass;
	struct DRWPass *object_overlap_pass;
} OVERLAY_PassList;

typedef struct OVERLAY_Data {
	void *engine_type;
	OVERLAY_FramebufferList *fbl;
	DRWViewportEmptyList *txl;
	OVERLAY_PassList *psl;
	OVERLAY_StorageList *stl;
} OVERLAY_Data;

typedef struct OVERLAY_PrivateData {
	DRWShadingGroup *face_orientation_shgrp;
	int overlays;
	int next_object_id;
	ListBase materials;


} OVERLAY_PrivateData; /* Transient data */

typedef struct OVERLAY_MaterialData {
	struct Link *next, *prev;
	DRWShadingGroup *object_data_shgrp;
	int object_id;
} OVERLAY_MaterialData;

typedef struct OVERLAY_ObjectData {
	struct ObjectEngineData *next, *prev;
	struct DrawEngineType *engine_type;
	/* Only nested data, NOT the engine data itself. */
	ObjectEngineDataFreeCb free;
	/* Accumulated recalc flags, which corresponds to ID->recalc flags. */
	int recalc;
} OVERLAY_ObjectData;

/* *********** STATIC *********** */
static struct {
	/* Face orientation shader */
	struct GPUShader *face_orientation_sh;
	struct GPUShader *object_data_sh;
	struct GPUShader *object_overlap_sh;

	struct GPUTexture *object_id_tx; /* ref only, not alloced */
} e_data = {NULL};

/* Shaders */
extern char datatoc_overlay_face_orientation_frag_glsl[];
extern char datatoc_overlay_face_orientation_vert_glsl[];
extern char datatoc_overlay_object_data_frag_glsl[];
extern char datatoc_overlay_object_data_vert_glsl[];
extern char datatoc_overlay_object_overlap_frag_glsl[];


/* Functions */
static void overlay_engine_init(void *vedata)
{
	OVERLAY_Data * data = (OVERLAY_Data *)vedata;
	OVERLAY_FramebufferList *fbl = data->fbl;
	OVERLAY_StorageList *stl = data->stl;
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
		BLI_listbase_clear(&stl->g_data->materials);
	}

	if (!e_data.face_orientation_sh) {
		/* Face orientation */
		e_data.face_orientation_sh = DRW_shader_create(datatoc_overlay_face_orientation_vert_glsl, NULL, datatoc_overlay_face_orientation_frag_glsl, "\n");
		e_data.object_data_sh = DRW_shader_create(datatoc_overlay_object_data_vert_glsl, NULL, datatoc_overlay_object_data_frag_glsl, "\n");
		e_data.object_overlap_sh = DRW_shader_create_fullscreen(datatoc_overlay_object_overlap_frag_glsl, NULL);
	}

	{
		const float *viewport_size = DRW_viewport_size_get();
		const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};

		e_data.object_id_tx = DRW_texture_pool_query_2D(size[0], size[1], DRW_TEX_R_32U, &draw_engine_overlay_type);
		GPU_framebuffer_ensure_config(&fbl->object_data_fb, {
			GPU_ATTACHMENT_TEXTURE(dtxl->depth),
			GPU_ATTACHMENT_TEXTURE(e_data.object_id_tx)
		});
	}
}

static void overlay_cache_init(void *vedata)
{

	OVERLAY_Data * data = (OVERLAY_Data *)vedata;
	OVERLAY_PassList *psl = data->psl;
	OVERLAY_StorageList *stl = data->stl;

	const DRWContextState *DCS = DRW_context_state_get();
	DRWShadingGroup *grp;


	View3D *v3d = DCS->v3d;
	if (v3d) {
		stl->g_data->overlays = v3d->overlays;
	}
	else {
		stl->g_data->overlays = 0;
	}
	stl->g_data->next_object_id = 0;

	/* Face Orientation Pass */
	if (stl->g_data->overlays & V3D_OVERLAY_FACE_ORIENTATION) {
		int state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND;
		psl->face_orientation_pass = DRW_pass_create("Face Orientation", state);
		stl->g_data->face_orientation_shgrp = DRW_shgroup_create(e_data.face_orientation_sh, psl->face_orientation_pass);
	}
	if (stl->g_data->overlays & V3D_OVERLAY_OBJECT_OVERLAP) {

		int state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;
		psl->object_data_pass = DRW_pass_create("Object Data", state);

		psl->object_overlap_pass = DRW_pass_create("Overlap", DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND);
		grp = DRW_shgroup_create(e_data.object_overlap_sh, psl->object_overlap_pass);
		DRW_shgroup_uniform_texture_ref(grp, "objectId", &e_data.object_id_tx);
		DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);
	}
}

static void overlay_cache_populate(void *vedata, Object *ob)
{
	OVERLAY_Data * data = (OVERLAY_Data *)vedata;
	OVERLAY_PassList *psl = data->psl;
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

		if (stl->g_data->overlays & V3D_OVERLAY_OBJECT_OVERLAP) {
			OVERLAY_MaterialData *material_data = (OVERLAY_MaterialData*)MEM_mallocN(sizeof(OVERLAY_MaterialData), __func__);
			material_data->object_data_shgrp = DRW_shgroup_create(e_data.object_data_sh, psl->object_data_pass);
			material_data->object_id = pd->next_object_id ++;
			DRW_shgroup_uniform_int(material_data->object_data_shgrp, "object_id", &material_data->object_id, 1);
			DRW_shgroup_call_add(material_data->object_data_shgrp, geom, ob->obmat);
			BLI_addhead(&pd->materials, material_data);
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
	OVERLAY_FramebufferList *fbl = data->fbl;
	OVERLAY_PrivateData *pd = stl->g_data;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

	if (pd->overlays & V3D_OVERLAY_FACE_ORIENTATION) {
		DRW_draw_pass(psl->face_orientation_pass);
	}

	if (pd->overlays & V3D_OVERLAY_OBJECT_OVERLAP) {
		const float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};

		GPU_framebuffer_bind(fbl->object_data_fb);
		GPU_framebuffer_clear_color(fbl->object_data_fb, clear_col);
		DRW_draw_pass(psl->object_data_pass);

		GPU_framebuffer_bind(dfbl->color_only_fb);
		DRW_draw_pass(psl->object_overlap_pass);
	}

	BLI_freelistN(&pd->materials);
}

static void overlay_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.face_orientation_sh);
	DRW_SHADER_FREE_SAFE(e_data.object_data_sh);
	DRW_SHADER_FREE_SAFE(e_data.object_overlap_sh);
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

