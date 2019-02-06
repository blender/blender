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

/** \file \ingroup draw_engine
 */

#include "DNA_mesh_types.h"
#include "DNA_view3d_types.h"

#include "BKE_editmesh.h"
#include "BKE_object.h"
#include "BKE_global.h"

#include "GPU_shader.h"
#include "DRW_render.h"


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
	DRWShadingGroup *face_orientation_shgrp;
	DRWShadingGroup *face_wires_shgrp;
	DRWShadingGroup *flat_wires_shgrp;
	DRWShadingGroup *sculpt_wires_shgrp;
	View3DOverlay overlay;
	float wire_step_param[2];
	bool ghost_stencil_test;
	bool show_overlays;
} OVERLAY_PrivateData; /* Transient data */

typedef struct OVERLAY_Shaders {
	/* Face orientation shader */
	struct GPUShader *face_orientation;
	/* Wireframe shader */
	struct GPUShader *select_wireframe;
	struct GPUShader *face_wireframe;
	struct GPUShader *face_wireframe_sculpt;
} OVERLAY_Shaders;

/* *********** STATIC *********** */
static struct {
	OVERLAY_Shaders sh_data[GPU_SHADER_CFG_LEN];
} e_data = {NULL};

extern char datatoc_gpu_shader_cfg_world_clip_lib_glsl[];

/* Shaders */
extern char datatoc_overlay_face_orientation_frag_glsl[];
extern char datatoc_overlay_face_orientation_vert_glsl[];

extern char datatoc_overlay_face_wireframe_vert_glsl[];
extern char datatoc_overlay_face_wireframe_geom_glsl[];
extern char datatoc_overlay_face_wireframe_frag_glsl[];
extern char datatoc_gpu_shader_depth_only_frag_glsl[];

/* Functions */
static void overlay_engine_init(void *vedata)
{
	OVERLAY_Data *data = vedata;
	OVERLAY_StorageList *stl = data->stl;

	const DRWContextState *draw_ctx = DRW_context_state_get();
	OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->shader_cfg];
	const bool is_clip = (draw_ctx->rv3d->rflag & RV3D_CLIPPING) != 0;

	if (is_clip) {
		DRW_state_clip_planes_set_from_rv3d(draw_ctx->rv3d);
	}

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}
	stl->g_data->ghost_stencil_test = false;

	const char *world_clip_lib_or_empty = is_clip ? datatoc_gpu_shader_cfg_world_clip_lib_glsl : "";
	const char *world_clip_def_or_empty = is_clip ? "#define USE_WORLD_CLIP_PLANES\n" : "";

	if (!sh_data->face_orientation) {
		/* Face orientation */
		sh_data->face_orientation = GPU_shader_create_from_arrays({
		        .vert = (const char *[]){world_clip_lib_or_empty, datatoc_overlay_face_orientation_vert_glsl, NULL},
		        .frag = (const char *[]){datatoc_overlay_face_orientation_frag_glsl, NULL},
		        .defs = (const char *[]){world_clip_def_or_empty, NULL},
		});
	}

	if (!sh_data->face_wireframe) {
		sh_data->select_wireframe = GPU_shader_create_from_arrays({
		        .vert = (const char *[]){world_clip_lib_or_empty, datatoc_overlay_face_wireframe_vert_glsl, NULL},
		        .geom = (const char *[]){world_clip_lib_or_empty, datatoc_overlay_face_wireframe_geom_glsl, NULL},
		        .frag = (const char *[]){datatoc_gpu_shader_depth_only_frag_glsl, NULL},
		        .defs = (const char *[]){world_clip_def_or_empty, "#define SELECT_EDGES\n", NULL},
		});
		sh_data->face_wireframe = GPU_shader_create_from_arrays({
		        .vert = (const char *[]){world_clip_lib_or_empty, datatoc_overlay_face_wireframe_vert_glsl, NULL},
		        .frag = (const char *[]){datatoc_overlay_face_wireframe_frag_glsl, NULL},
		        .defs = (const char *[]){world_clip_def_or_empty, NULL},
		});
		sh_data->face_wireframe_sculpt = GPU_shader_create_from_arrays({
		        .vert = (const char *[]){world_clip_lib_or_empty, datatoc_overlay_face_wireframe_vert_glsl, NULL},
		        .geom = (const char *[]){world_clip_lib_or_empty, datatoc_overlay_face_wireframe_geom_glsl, NULL},
		        .frag = (const char *[]){datatoc_overlay_face_wireframe_frag_glsl, NULL},
		        .defs = (const char *[]){world_clip_def_or_empty, "#define USE_SCULPT\n", NULL},
		});
	}
}

static void overlay_cache_init(void *vedata)
{
	OVERLAY_Data *data = vedata;
	OVERLAY_PassList *psl = data->psl;
	OVERLAY_StorageList *stl = data->stl;
	OVERLAY_PrivateData *g_data = stl->g_data;

	const DRWContextState *draw_ctx = DRW_context_state_get();
	RegionView3D *rv3d = draw_ctx->rv3d;
	OVERLAY_Shaders *sh_data = &e_data.sh_data[draw_ctx->shader_cfg];

	const DRWContextState *DCS = DRW_context_state_get();

	View3D *v3d = DCS->v3d;
	if (v3d) {
		g_data->overlay = v3d->overlay;
		g_data->show_overlays = (v3d->flag2 & V3D_RENDER_OVERRIDE) == 0;
	}
	else {
		memset(&g_data->overlay, 0, sizeof(g_data->overlay));
		g_data->show_overlays = false;
	}

	if (g_data->show_overlays == false) {
		g_data->overlay.flag = 0;
	}

	if (v3d->shading.type == OB_WIRE) {
		g_data->overlay.flag |= V3D_OVERLAY_WIREFRAMES;
		g_data->show_overlays = true;
	}

	{
		/* Face Orientation Pass */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND;
		psl->face_orientation_pass = DRW_pass_create("Face Orientation", state);
		g_data->face_orientation_shgrp = DRW_shgroup_create(
		        sh_data->face_orientation, psl->face_orientation_pass);
		if (rv3d->rflag & RV3D_CLIPPING) {
			DRW_shgroup_world_clip_planes_from_rv3d(g_data->face_orientation_shgrp, rv3d);
		}
	}

	{
		/* Wireframe */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND | DRW_STATE_FIRST_VERTEX_CONVENTION;
		float wire_size = max_ff(0.0f, U.pixelsize - 1.0f) * 0.5f;

		const bool use_select = (DRW_state_is_select() || DRW_state_is_depth());
		GPUShader *sculpt_wire_sh = use_select ? sh_data->select_wireframe : sh_data->face_wireframe_sculpt;
		GPUShader *face_wires_sh = use_select ? sh_data->select_wireframe : sh_data->face_wireframe;
		GPUShader *flat_wires_sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);

		psl->face_wireframe_pass = DRW_pass_create("Face Wires", state);

		g_data->flat_wires_shgrp = DRW_shgroup_create(flat_wires_sh, psl->face_wireframe_pass);
		if (rv3d->rflag & RV3D_CLIPPING) {
			DRW_shgroup_world_clip_planes_from_rv3d(g_data->flat_wires_shgrp, rv3d);
		}

		g_data->sculpt_wires_shgrp = DRW_shgroup_create(sculpt_wire_sh, psl->face_wireframe_pass);
		if (rv3d->rflag & RV3D_CLIPPING) {
			DRW_shgroup_world_clip_planes_from_rv3d(g_data->sculpt_wires_shgrp, rv3d);
		}

		g_data->face_wires_shgrp = DRW_shgroup_create(face_wires_sh, psl->face_wireframe_pass);
		DRW_shgroup_uniform_vec2(g_data->face_wires_shgrp, "wireStepParam", g_data->wire_step_param, 1);
		if (rv3d->rflag & RV3D_CLIPPING) {
			DRW_shgroup_world_clip_planes_from_rv3d(g_data->face_wires_shgrp, rv3d);
		}

		if (!use_select) {
			DRW_shgroup_uniform_float_copy(g_data->sculpt_wires_shgrp, "wireSize", wire_size);
			DRW_shgroup_uniform_float_copy(g_data->face_wires_shgrp, "wireSize", wire_size);
		}

		/* Control aspect of the falloff. */
		const float sharpness = 4.0f;
		/* Scale and bias: Adjust with wiredata encoding. (see mesh_batch_cache_create_edges_wireframe_data) */
		const float decompress = (0xFF / (float)(0xFF - 0x20));
		g_data->wire_step_param[0] = -sharpness * decompress;
		g_data->wire_step_param[1] = decompress + sharpness * stl->g_data->overlay.wireframe_threshold;


	}
}

static void overlay_cache_populate(void *vedata, Object *ob)
{
	OVERLAY_Data *data = vedata;
	OVERLAY_StorageList *stl = data->stl;
	OVERLAY_PrivateData *pd = stl->g_data;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	RegionView3D *rv3d = draw_ctx->rv3d;
	View3D *v3d = draw_ctx->v3d;

	if ((!pd->show_overlays) ||
	    (ob->dt < OB_WIRE) ||
	    (!DRW_object_is_renderable(ob) && (ob->dt != OB_WIRE)))
	{
		return;
	}

	if (DRW_object_is_renderable(ob) && pd->overlay.flag & V3D_OVERLAY_FACE_ORIENTATION) {
		struct GPUBatch *geom = DRW_cache_object_surface_get(ob);
		if (geom) {
			DRW_shgroup_call_add(pd->face_orientation_shgrp, geom, ob->obmat);
		}
	}

	if ((pd->overlay.flag & V3D_OVERLAY_WIREFRAMES) ||
	    (v3d->shading.type == OB_WIRE) ||
	    (ob->dtx & OB_DRAWWIRE) ||
	    (ob->dt == OB_WIRE))
	{
		const bool is_edit_mode = BKE_object_is_in_editmode(ob);
		bool has_edit_mesh_cage = false;
		if (ob->type == OB_MESH) {
			/* TODO: Should be its own function. */
			Mesh *me = (Mesh *)ob->data;
			BMEditMesh *embm = me->edit_btmesh;
			if (embm) {
				has_edit_mesh_cage = embm->mesh_eval_cage && (embm->mesh_eval_cage != embm->mesh_eval_final);
			}
		}

		/* Don't do that in edit Mesh mode, unless there is a modifier preview. */
		if ((((ob != draw_ctx->object_edit) && !is_edit_mode) || has_edit_mesh_cage) ||
		    ob->type != OB_MESH)
		{
			const bool is_active = (ob == draw_ctx->obact);
			const bool is_sculpt_mode = is_active && (draw_ctx->object_mode & OB_MODE_SCULPT) != 0;
			const bool all_wires = (pd->overlay.wireframe_threshold == 1.0f) ||
			                       (ob->dtx & OB_DRAW_ALL_EDGES);
			const bool is_wire = (ob->dt < OB_SOLID);
			const int stencil_mask = (ob->dtx & OB_DRAWXRAY) ? 0x00 : 0xFF;
			DRWShadingGroup *shgrp = NULL;

			const float *rim_col = NULL;
			const float *wire_col = NULL;
			if (UNLIKELY(ob->base_flag & BASE_FROM_SET)) {
				rim_col = G_draw.block.colorDupli;
				wire_col = G_draw.block.colorDupli;
			}
			else if (UNLIKELY(ob->base_flag & BASE_FROM_DUPLI)) {
				if (ob->base_flag & BASE_SELECTED) {
					if (G.moving & G_TRANSFORM_OBJ) {
						rim_col = G_draw.block.colorTransform;
					}
					else {
						rim_col = G_draw.block.colorDupliSelect;
					}
				}
				else {
					rim_col = G_draw.block.colorDupli;
				}
				wire_col = G_draw.block.colorDupli;
			}
			else if ((ob->base_flag & BASE_SELECTED) &&
			         (!is_edit_mode && !is_sculpt_mode && !has_edit_mesh_cage))
			{
				if (G.moving & G_TRANSFORM_OBJ) {
					rim_col = G_draw.block.colorTransform;
				}
				else if (ob == draw_ctx->obact) {
					rim_col = G_draw.block.colorActive;
				}
				else {
					rim_col = G_draw.block.colorSelect;
				}
				wire_col = G_draw.block.colorWire;
			}
			else {
				rim_col = G_draw.block.colorWire;
				wire_col = G_draw.block.colorWire;
			}
			BLI_assert(rim_col && wire_col);

			/* This fixes only the biggest case which is a plane in ortho view. */
			int flat_axis = 0;
			bool is_flat_object_viewed_from_side = (
			        (rv3d->persp == RV3D_ORTHO) &&
			        DRW_object_is_flat(ob, &flat_axis) &&
			        DRW_object_axis_orthogonal_to_view(ob, flat_axis));

			if (is_flat_object_viewed_from_side && !is_sculpt_mode) {
				/* Avoid losing flat objects when in ortho views (see T56549) */
				struct GPUBatch *geom = DRW_cache_object_all_edges_get(ob);
				if (geom) {
					shgrp = pd->flat_wires_shgrp;
					shgrp = DRW_shgroup_create_sub(shgrp);
					DRW_shgroup_stencil_mask(shgrp, stencil_mask);
					DRW_shgroup_call_object_add(shgrp, geom, ob);
					DRW_shgroup_uniform_vec4(shgrp, "color", rim_col, 1);
				}
			}
			else {
				struct GPUBatch *geom = DRW_cache_object_face_wireframe_get(ob);
				if (geom || is_sculpt_mode) {
					shgrp = (is_sculpt_mode) ? pd->sculpt_wires_shgrp : pd->face_wires_shgrp;
					shgrp = DRW_shgroup_create_sub(shgrp);

					static float all_wires_params[2] = {0.0f, 10.0f}; /* Parameters for all wires */
					DRW_shgroup_uniform_vec2(
					        shgrp, "wireStepParam", (all_wires) ?
					        all_wires_params : pd->wire_step_param, 1);

					if (!(DRW_state_is_select() || DRW_state_is_depth())) {
						DRW_shgroup_stencil_mask(shgrp, stencil_mask);
						DRW_shgroup_uniform_vec3(shgrp, "wireColor", wire_col, 1);
						DRW_shgroup_uniform_vec3(shgrp, "rimColor", rim_col, 1);
					}

					if (is_sculpt_mode) {
						DRW_shgroup_call_sculpt_add(shgrp, ob, ob->obmat);
					}
					else {
						DRW_shgroup_call_add(shgrp, geom, ob->obmat);
					}
				}
			}
			if (is_wire && shgrp != NULL) {
				/* If object is wireframe, don't try to use stencil test. */
				DRW_shgroup_state_disable(shgrp, DRW_STATE_STENCIL_EQUAL);

				if (ob->dtx & OB_DRAWXRAY) {
					DRW_shgroup_state_disable(shgrp, DRW_STATE_DEPTH_LESS_EQUAL);
				}
			}
			else if ((ob->dtx & OB_DRAWXRAY) && shgrp != NULL) {
				pd->ghost_stencil_test = true;
			}
		}
	}
}

static void overlay_cache_finish(void *vedata)
{
	OVERLAY_Data *data = vedata;
	OVERLAY_PassList *psl = data->psl;
	OVERLAY_StorageList *stl = data->stl;

	const DRWContextState *ctx = DRW_context_state_get();
	View3D *v3d = ctx->v3d;

	/* only in solid mode */
	if (v3d->shading.type == OB_SOLID && (v3d->shading.flag & XRAY_FLAG(v3d)) == 0) {
		if (stl->g_data->ghost_stencil_test) {
			DRW_pass_state_add(psl->face_wireframe_pass, DRW_STATE_STENCIL_EQUAL);
		}
	}
}

static void overlay_draw_scene(void *vedata)
{
	OVERLAY_Data *data = vedata;
	OVERLAY_PassList *psl = data->psl;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

	if (DRW_state_is_fbo()) {
		GPU_framebuffer_bind(dfbl->default_fb);
	}
	DRW_draw_pass(psl->face_orientation_pass);
	DRW_draw_pass(psl->face_wireframe_pass);
}

static void overlay_engine_free(void)
{
	for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
		OVERLAY_Shaders *sh_data = &e_data.sh_data[sh_data_index];
		GPUShader **sh_data_as_array = (GPUShader **)sh_data;
		for (int i = 0; i < (sizeof(OVERLAY_Shaders) / sizeof(GPUShader *)); i++) {
			DRW_SHADER_FREE_SAFE(sh_data_as_array[i]);
		}
	}
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
