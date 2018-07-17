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

#include "BKE_object.h"

#include "GPU_shader.h"
#include "GPU_extensions.h"
#include "DRW_render.h"

#include "draw_mode_engines.h"

/* Structures */
typedef struct OVERLAY_StorageList {
	struct OVERLAY_PrivateData *g_data;
} OVERLAY_StorageList;

typedef struct OVERLAY_PassList {
	struct DRWPass *face_orientation_pass;
	struct DRWPass *face_wireframe_pass;
} OVERLAY_PassList;

typedef struct OVERLAY_Data {
	void *engine_type;
	DRWViewportEmptyList *fbl;
	DRWViewportEmptyList *txl;
	OVERLAY_PassList *psl;
	OVERLAY_StorageList *stl;
} OVERLAY_Data;

typedef struct OVERLAY_PrivateData {
	GPUShader *wire_sh; /* reference */
	DRWShadingGroup *face_orientation_shgrp;
	View3DOverlay overlay;
	float wire_step_param[2];
} OVERLAY_PrivateData; /* Transient data */

/* *********** STATIC *********** */
static struct {
	/* Face orientation shader */
	struct GPUShader *face_orientation_sh;
	/* Wireframe shader */
	struct GPUShader *face_wireframe_sh;
	struct GPUShader *face_wireframe_pretty_sh;
} e_data = {NULL};

/* Shaders */
extern char datatoc_overlay_face_orientation_frag_glsl[];
extern char datatoc_overlay_face_orientation_vert_glsl[];

extern char datatoc_overlay_face_wireframe_vert_glsl[];
extern char datatoc_overlay_face_wireframe_geom_glsl[];
extern char datatoc_overlay_face_wireframe_frag_glsl[];

extern struct GlobalsUboStorage ts; /* draw_common.c */

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
		e_data.face_orientation_sh = DRW_shader_create(
		        datatoc_overlay_face_orientation_vert_glsl, NULL,
		        datatoc_overlay_face_orientation_frag_glsl, NULL);
	}

	if (!e_data.face_wireframe_sh) {
		bool use_geom = GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY);

		e_data.face_wireframe_sh = DRW_shader_create(
		        datatoc_overlay_face_wireframe_vert_glsl,
		        use_geom ? datatoc_overlay_face_wireframe_geom_glsl : NULL,
		        datatoc_overlay_face_wireframe_frag_glsl,
		        use_geom ? "#define USE_GEOM_SHADER\n"
		                 : NULL);

		e_data.face_wireframe_pretty_sh = DRW_shader_create(
		        datatoc_overlay_face_wireframe_vert_glsl,
		        use_geom ? datatoc_overlay_face_wireframe_geom_glsl : NULL,
		        datatoc_overlay_face_wireframe_frag_glsl,
		        use_geom ? "#define USE_GEOM_SHADER\n"
		                   "#define LIGHT_EDGES\n"
		                 : "#define LIGHT_EDGES\n");
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
		stl->g_data->overlay = v3d->overlay;
	}
	else {
		memset(&stl->g_data->overlay, 0, sizeof(stl->g_data->overlay));
	}

	/* Face Orientation Pass */
	if (stl->g_data->overlay.flag & V3D_OVERLAY_FACE_ORIENTATION) {
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND;
		psl->face_orientation_pass = DRW_pass_create("Face Orientation", state);
		stl->g_data->face_orientation_shgrp = DRW_shgroup_create(
		        e_data.face_orientation_sh, psl->face_orientation_pass);
	}
	if (stl->g_data->overlay.flag & V3D_OVERLAY_WIREFRAMES) {
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND;
		psl->face_wireframe_pass = DRW_pass_create("Face Wires", state);
		/* Sticky uniforms (don't need to respecify each time since shader does not change). */
		stl->g_data->wire_sh = (stl->g_data->overlay.wireframe_threshold == 1.0f) ? e_data.face_wireframe_sh
		                                                                          : e_data.face_wireframe_pretty_sh;
		DRWShadingGroup *shgrp = DRW_shgroup_create(stl->g_data->wire_sh, psl->face_wireframe_pass);
		DRW_shgroup_uniform_vec2(shgrp, "viewportSize", DRW_viewport_size_get(), 1);
		DRW_shgroup_uniform_vec2(shgrp, "wireStepParam", stl->g_data->wire_step_param, 1);

		/**
		 * The wireframe threshold ranges from 0.0 to 1.0
		 * When 1.0 we show all the edges, when 0.5 we show as many as 2.7.
		 *
		 * If we wanted 0.0 to match 2.7, factor would need to be 0.003f.
		 * The range controls the falloff effect. If range was 0.0f we would get a hard cut (as in 2.7).
		 * That said we are using a different algorithm so the results will always differ.
		 */
		const float factor = 0.0045f;
		const float range = 0.00125f;
		stl->g_data->wire_step_param[1] = (1.0f - factor) + stl->g_data->overlay.wireframe_threshold * factor;
		stl->g_data->wire_step_param[0] = stl->g_data->wire_step_param[1] + range;
	}
}

static void overlay_cache_populate(void *vedata, Object *ob)
{
	OVERLAY_Data * data = (OVERLAY_Data *)vedata;
	OVERLAY_StorageList *stl = data->stl;
	OVERLAY_PrivateData *pd = stl->g_data;
	OVERLAY_PassList *psl = data->psl;
	const DRWContextState *draw_ctx = DRW_context_state_get();

	if (!DRW_object_is_renderable(ob))
		return;

	if (stl->g_data->overlay.flag & V3D_OVERLAY_FACE_ORIENTATION) {
		struct GPUBatch *geom = DRW_cache_object_surface_get(ob);
		if (geom) {
			DRW_shgroup_call_add(pd->face_orientation_shgrp, geom, ob->obmat);
		}
	}

	if (stl->g_data->overlay.flag & V3D_OVERLAY_WIREFRAMES) {
		/* Don't do that in edit mode. */
		if ((ob != draw_ctx->object_edit) && !BKE_object_is_in_editmode(ob)) {
			int tri_count;
			GPUTexture *verts = NULL, *faceids;
			DRW_cache_object_face_wireframe_get(ob, &verts, &faceids, &tri_count);
			if (verts) {
				float *rim_col = ts.colorWire;
				if ((ob->base_flag & BASE_SELECTED) != 0) {
					rim_col = (ob == draw_ctx->obact) ? ts.colorActive : ts.colorSelect;
				}
				DRWShadingGroup *shgrp = DRW_shgroup_create(stl->g_data->wire_sh, psl->face_wireframe_pass);
				DRW_shgroup_uniform_texture(shgrp, "vertData", verts);
				DRW_shgroup_uniform_texture(shgrp, "faceIds", faceids);
				DRW_shgroup_uniform_vec3(shgrp, "wireColor", ts.colorWire, 1);
				DRW_shgroup_uniform_vec3(shgrp, "rimColor", rim_col, 1);
				DRW_shgroup_call_object_procedural_triangles_culled_add(shgrp, tri_count, ob);
			}
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

	if (pd->overlay.flag & V3D_OVERLAY_FACE_ORIENTATION) {
		DRW_draw_pass(psl->face_orientation_pass);
	}
	if (pd->overlay.flag & V3D_OVERLAY_WIREFRAMES) {
		DRW_draw_pass(psl->face_wireframe_pass);
	}
}

static void overlay_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.face_orientation_sh);
	DRW_SHADER_FREE_SAFE(e_data.face_wireframe_sh);
	DRW_SHADER_FREE_SAFE(e_data.face_wireframe_pretty_sh);
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
