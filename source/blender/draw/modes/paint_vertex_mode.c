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

/** \file blender/draw/modes/paint_vertex_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"

#include "draw_common.h"

#include "draw_mode_engines.h"

#include "DNA_mesh_types.h"
#include "DNA_view3d_types.h"

extern struct GPUUniformBuffer *globals_ubo; /* draw_common.c */
extern struct GlobalsUboStorage ts; /* draw_common.c */

extern char datatoc_paint_vertex_vert_glsl[];
extern char datatoc_paint_vertex_frag_glsl[];
extern char datatoc_paint_wire_vert_glsl[];
extern char datatoc_paint_wire_frag_glsl[];
extern char datatoc_common_globals_lib_glsl[];

/* *********** LISTS *********** */

typedef struct PAINT_VERTEX_PassList {
	struct DRWPass *vcolor_faces;
	struct DRWPass *wire_overlay;
	struct DRWPass *face_overlay;
} PAINT_VERTEX_PassList;

typedef struct PAINT_VERTEX_StorageList {
	struct PAINT_VERTEX_PrivateData *g_data;
} PAINT_VERTEX_StorageList;

typedef struct PAINT_VERTEX_Data {
	void *engine_type; /* Required */
	DRWViewportEmptyList *fbl;
	DRWViewportEmptyList *txl;
	PAINT_VERTEX_PassList *psl;
	PAINT_VERTEX_StorageList *stl;
} PAINT_VERTEX_Data;

/* *********** STATIC *********** */

static struct {
	struct GPUShader *vcolor_face_shader;
	struct GPUShader *wire_overlay_shader;
	struct GPUShader *face_overlay_shader;
} e_data = {NULL}; /* Engine data */

typedef struct PAINT_VERTEX_PrivateData {
	DRWShadingGroup *fvcolor_shgrp;
	DRWShadingGroup *lwire_shgrp;
	DRWShadingGroup *face_shgrp;
} PAINT_VERTEX_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

static void PAINT_VERTEX_engine_init(void *UNUSED(vedata))
{
	if (!e_data.vcolor_face_shader) {
		e_data.vcolor_face_shader = DRW_shader_create(
		        datatoc_paint_vertex_vert_glsl, NULL,
		        datatoc_paint_vertex_frag_glsl, NULL);

		e_data.wire_overlay_shader = DRW_shader_create_with_lib(
		        datatoc_paint_wire_vert_glsl, NULL,
		        datatoc_paint_wire_frag_glsl,
		        datatoc_common_globals_lib_glsl, "#define VERTEX_MODE\n");

		e_data.face_overlay_shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	}
}

static void PAINT_VERTEX_cache_init(void *vedata)
{
	PAINT_VERTEX_PassList *psl = ((PAINT_VERTEX_Data *)vedata)->psl;
	PAINT_VERTEX_StorageList *stl = ((PAINT_VERTEX_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	const View3D *v3d = draw_ctx->v3d;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	{
		/* Create a pass */
		psl->vcolor_faces = DRW_pass_create(
		        "Vert Color Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND);

		stl->g_data->fvcolor_shgrp = DRW_shgroup_create(e_data.vcolor_face_shader, psl->vcolor_faces);
		DRW_shgroup_uniform_float(stl->g_data->fvcolor_shgrp, "alpha", &v3d->overlay.vertex_paint_mode_opacity, 1);
	}

	{
		psl->wire_overlay = DRW_pass_create(
		        "Wire Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL);

		stl->g_data->lwire_shgrp = DRW_shgroup_create(e_data.wire_overlay_shader, psl->wire_overlay);
		DRW_shgroup_uniform_block(stl->g_data->lwire_shgrp, "globalsBlock", globals_ubo);
	}

	{
		psl->face_overlay = DRW_pass_create(
		        "Face Mask Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND);

		stl->g_data->face_shgrp = DRW_shgroup_create(e_data.face_overlay_shader, psl->face_overlay);

		static float col[4] = {1.0f, 1.0f, 1.0f, 0.2f};
		DRW_shgroup_uniform_vec4(stl->g_data->face_shgrp, "color", col, 1);
	}
}

static void PAINT_VERTEX_cache_populate(void *vedata, Object *ob)
{
	PAINT_VERTEX_StorageList *stl = ((PAINT_VERTEX_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	const View3D *v3d = draw_ctx->v3d;

	if ((ob->type == OB_MESH) && (ob == draw_ctx->obact)) {
		const Mesh *me = ob->data;
		const bool use_wire = (v3d->overlay.paint_flag & V3D_OVERLAY_PAINT_WIRE) != 0;
		const bool use_surface = v3d->overlay.vertex_paint_mode_opacity != 0.0f;
		const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
		struct GPUBatch *geom;

		if (use_surface) {
			geom = DRW_cache_mesh_surface_vert_colors_get(ob);
			DRW_shgroup_call_add(stl->g_data->fvcolor_shgrp, geom, ob->obmat);
		}

		if (use_face_sel || use_wire) {
			geom = DRW_cache_mesh_edges_paint_overlay_get(ob, use_wire, use_face_sel);
			DRW_shgroup_call_add(stl->g_data->lwire_shgrp, geom, ob->obmat);
		}

		if (use_face_sel) {
			geom = DRW_cache_mesh_faces_weight_overlay_get(ob);
			DRW_shgroup_call_add(stl->g_data->face_shgrp, geom, ob->obmat);
		}
	}
}

static void PAINT_VERTEX_draw_scene(void *vedata)
{
	PAINT_VERTEX_PassList *psl = ((PAINT_VERTEX_Data *)vedata)->psl;

	DRW_draw_pass(psl->vcolor_faces);
	DRW_draw_pass(psl->face_overlay);
	DRW_draw_pass(psl->wire_overlay);
}

static void PAINT_VERTEX_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.vcolor_face_shader);
	DRW_SHADER_FREE_SAFE(e_data.wire_overlay_shader);
}

static const DrawEngineDataSize PAINT_VERTEX_data_size = DRW_VIEWPORT_DATA_SIZE(PAINT_VERTEX_Data);

DrawEngineType draw_engine_paint_vertex_type = {
	NULL, NULL,
	N_("PaintVertexMode"),
	&PAINT_VERTEX_data_size,
	&PAINT_VERTEX_engine_init,
	&PAINT_VERTEX_engine_free,
	&PAINT_VERTEX_cache_init,
	&PAINT_VERTEX_cache_populate,
	NULL,
	NULL,
	&PAINT_VERTEX_draw_scene,
	NULL,
	NULL,
};
