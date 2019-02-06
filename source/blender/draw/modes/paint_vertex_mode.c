/*
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
 * Copyright 2016, Blender Foundation.
 */

/** \file \ingroup draw
 */

#include "DRW_render.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"

#include "draw_common.h"


#include "DNA_mesh_types.h"
#include "DNA_view3d_types.h"

#include "DEG_depsgraph_query.h"

extern char datatoc_gpu_shader_cfg_world_clip_lib_glsl[];

extern char datatoc_paint_vertex_vert_glsl[];
extern char datatoc_paint_vertex_frag_glsl[];
extern char datatoc_paint_wire_vert_glsl[];
extern char datatoc_paint_wire_frag_glsl[];
extern char datatoc_paint_face_vert_glsl[];
extern char datatoc_paint_vert_frag_glsl[];
extern char datatoc_common_globals_lib_glsl[];

extern char datatoc_gpu_shader_uniform_color_frag_glsl[];

/* *********** LISTS *********** */

typedef struct PAINT_VERTEX_PassList {
	struct DRWPass *vcolor_faces;
	struct DRWPass *wire_overlay;
	struct DRWPass *face_overlay;
	struct DRWPass *vert_overlay;
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

typedef struct PAINT_VERTEX_Shaders {
	struct GPUShader *vcolor_face;
	struct GPUShader *wire_overlay;
	struct GPUShader *face_overlay;
	struct GPUShader *vert_overlay;
} PAINT_VERTEX_Shaders;

/* *********** STATIC *********** */

static struct {
	PAINT_VERTEX_Shaders sh_data[GPU_SHADER_CFG_LEN];
} e_data = {{{NULL}}}; /* Engine data */

typedef struct PAINT_VERTEX_PrivateData {
	DRWShadingGroup *fvcolor_shgrp;
	DRWShadingGroup *lwire_shgrp;
	DRWShadingGroup *face_shgrp;
	DRWShadingGroup *vert_shgrp;
} PAINT_VERTEX_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

static void PAINT_VERTEX_engine_init(void *UNUSED(vedata))
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	PAINT_VERTEX_Shaders *sh_data = &e_data.sh_data[draw_ctx->shader_cfg];
	const bool is_clip = (draw_ctx->rv3d->rflag & RV3D_CLIPPING) != 0;

	if (is_clip) {
		DRW_state_clip_planes_set_from_rv3d(draw_ctx->rv3d);
	}

	if (!sh_data->vcolor_face) {
		const char *world_clip_lib_or_empty = is_clip ? datatoc_gpu_shader_cfg_world_clip_lib_glsl : "";
		const char *world_clip_def_or_empty = is_clip ? "#define USE_WORLD_CLIP_PLANES\n" : "";

		sh_data->vcolor_face = GPU_shader_create_from_arrays({
		        .vert = (const char *[]){world_clip_lib_or_empty, datatoc_paint_vertex_vert_glsl, NULL},
		        .frag = (const char *[]){datatoc_paint_vertex_frag_glsl, NULL},
		        .defs = (const char *[]){world_clip_def_or_empty, NULL},
		});
		sh_data->wire_overlay = GPU_shader_create_from_arrays({
		        .vert = (const char *[]){world_clip_lib_or_empty, datatoc_common_globals_lib_glsl, datatoc_paint_wire_vert_glsl, NULL},
		        .frag = (const char *[]){datatoc_paint_wire_frag_glsl, NULL},
		        .defs = (const char *[]){world_clip_def_or_empty, "#define VERTEX_MODE\n", NULL},
		});
		sh_data->face_overlay = GPU_shader_create_from_arrays({
		        .vert = (const char *[]){world_clip_lib_or_empty, datatoc_paint_face_vert_glsl, NULL},
		        .frag = (const char *[]){datatoc_gpu_shader_uniform_color_frag_glsl, NULL},
		        .defs = (const char *[]){world_clip_def_or_empty, NULL},
		});
		sh_data->vert_overlay = GPU_shader_create_from_arrays({
		        .vert = (const char *[]){world_clip_lib_or_empty, datatoc_common_globals_lib_glsl, datatoc_paint_wire_vert_glsl, NULL},
		        .frag = (const char *[]){datatoc_paint_vert_frag_glsl, NULL},
		        .defs = (const char *[]){world_clip_def_or_empty, NULL},
		});
	}
}

static void PAINT_VERTEX_cache_init(void *vedata)
{
	PAINT_VERTEX_PassList *psl = ((PAINT_VERTEX_Data *)vedata)->psl;
	PAINT_VERTEX_StorageList *stl = ((PAINT_VERTEX_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	const View3D *v3d = draw_ctx->v3d;
	const RegionView3D *rv3d = draw_ctx->rv3d;
	PAINT_VERTEX_Shaders *sh_data = &e_data.sh_data[draw_ctx->shader_cfg];

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	{
		/* Create a pass */
		psl->vcolor_faces = DRW_pass_create(
		        "Vert Color Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_MULTIPLY);

		stl->g_data->fvcolor_shgrp = DRW_shgroup_create(sh_data->vcolor_face, psl->vcolor_faces);
		DRW_shgroup_uniform_float_copy(stl->g_data->fvcolor_shgrp, "white_factor", 1.0f - v3d->overlay.vertex_paint_mode_opacity);
		if (rv3d->rflag & RV3D_CLIPPING) {
			DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->fvcolor_shgrp, rv3d);
		}
	}

	{
		psl->wire_overlay = DRW_pass_create(
		        "Wire Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_OFFSET_NEGATIVE);

		stl->g_data->lwire_shgrp = DRW_shgroup_create(sh_data->wire_overlay, psl->wire_overlay);
		DRW_shgroup_uniform_block(stl->g_data->lwire_shgrp, "globalsBlock", G_draw.block_ubo);
		if (rv3d->rflag & RV3D_CLIPPING) {
			DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->lwire_shgrp, rv3d);
		}
	}

	{
		psl->face_overlay = DRW_pass_create(
		        "Face Mask Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND);

		stl->g_data->face_shgrp = DRW_shgroup_create(sh_data->face_overlay, psl->face_overlay);
		static float col[4] = {1.0f, 1.0f, 1.0f, 0.2f};
		DRW_shgroup_uniform_vec4(stl->g_data->face_shgrp, "color", col, 1);
		if (rv3d->rflag & RV3D_CLIPPING) {
			DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->face_shgrp, rv3d);
		}
	}

	{
		psl->vert_overlay = DRW_pass_create(
		        "Vert Mask Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_OFFSET_NEGATIVE);

		stl->g_data->vert_shgrp = DRW_shgroup_create(sh_data->vert_overlay, psl->vert_overlay);
		DRW_shgroup_uniform_block(stl->g_data->vert_shgrp, "globalsBlock", G_draw.block_ubo);
		if (rv3d->rflag & RV3D_CLIPPING) {
			DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->vert_shgrp, rv3d);
		}
	}
}

static void PAINT_VERTEX_cache_populate(void *vedata, Object *ob)
{
	PAINT_VERTEX_StorageList *stl = ((PAINT_VERTEX_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	const View3D *v3d = draw_ctx->v3d;

	if ((ob->type == OB_MESH) && (ob == draw_ctx->obact)) {
		const Mesh *me = ob->data;
		const Mesh *me_orig = DEG_get_original_object(ob)->data;
		const bool use_wire = (v3d->overlay.paint_flag & V3D_OVERLAY_PAINT_WIRE) != 0;
		const bool use_surface = v3d->overlay.vertex_paint_mode_opacity != 0.0f;
		const bool use_face_sel = (me_orig->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
		const bool use_vert_sel = (me_orig->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
		struct GPUBatch *geom;

		if (me->mloopcol == NULL) {
			return;
		}

		if (use_surface) {
			geom = DRW_cache_mesh_surface_vertpaint_get(ob);
			DRW_shgroup_call_add(stl->g_data->fvcolor_shgrp, geom, ob->obmat);
		}

		if (use_face_sel || use_wire) {
			geom = DRW_cache_mesh_surface_edges_get(ob);
			DRW_shgroup_call_add(stl->g_data->lwire_shgrp, geom, ob->obmat);
		}

		if (use_face_sel) {
			geom = DRW_cache_mesh_surface_get(ob);
			DRW_shgroup_call_add(stl->g_data->face_shgrp, geom, ob->obmat);
		}

		if (use_vert_sel) {
			geom = DRW_cache_mesh_all_verts_get(ob);
			DRW_shgroup_call_add(stl->g_data->vert_shgrp, geom, ob->obmat);
		}
	}
}

static void PAINT_VERTEX_draw_scene(void *vedata)
{
	PAINT_VERTEX_PassList *psl = ((PAINT_VERTEX_Data *)vedata)->psl;

	DRW_draw_pass(psl->vcolor_faces);
	DRW_draw_pass(psl->face_overlay);
	DRW_draw_pass(psl->wire_overlay);
	DRW_draw_pass(psl->vert_overlay);
}

static void PAINT_VERTEX_engine_free(void)
{
	for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
		PAINT_VERTEX_Shaders *sh_data = &e_data.sh_data[sh_data_index];
		GPUShader **sh_data_as_array = (GPUShader **)sh_data;
		for (int i = 0; i < (sizeof(PAINT_VERTEX_Shaders) / sizeof(GPUShader *)); i++) {
			DRW_SHADER_FREE_SAFE(sh_data_as_array[i]);
		}
	}
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
