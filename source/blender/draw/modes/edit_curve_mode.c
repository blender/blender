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

/** \file blender/draw/modes/edit_curve_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_curve_types.h"
#include "DNA_view3d_types.h"

#include "BKE_object.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"
#include "GPU_batch.h"

#include "draw_common.h"

#include "draw_mode_engines.h"

/* If needed, contains all global/Theme colors
 * Add needed theme colors / values to DRW_globals_update() and update UBO
 * Not needed for constant color. */
extern struct GPUUniformBuffer *globals_ubo; /* draw_common.c */
extern struct GlobalsUboStorage ts; /* draw_common.c */

extern char datatoc_common_globals_lib_glsl[];
extern char datatoc_edit_curve_overlay_loosevert_vert_glsl[];
extern char datatoc_edit_curve_overlay_frag_glsl[];
extern char datatoc_edit_curve_overlay_handle_geom_glsl[];

extern char datatoc_gpu_shader_3D_vert_glsl[];
extern char datatoc_gpu_shader_uniform_color_frag_glsl[];
extern char datatoc_gpu_shader_point_uniform_color_frag_glsl[];
extern char datatoc_gpu_shader_flat_color_frag_glsl[];

/* *********** LISTS *********** */
/* All lists are per viewport specific datas.
 * They are all free when viewport changes engines
 * or is free itself. Use EDIT_CURVE_engine_init() to
 * initialize most of them and EDIT_CURVE_cache_init()
 * for EDIT_CURVE_PassList */

typedef struct EDIT_CURVE_PassList {
	/* Declare all passes here and init them in
	 * EDIT_CURVE_cache_init().
	 * Only contains (DRWPass *) */
	struct DRWPass *wire_pass;
	struct DRWPass *overlay_edge_pass;
	struct DRWPass *overlay_vert_pass;
} EDIT_CURVE_PassList;

typedef struct EDIT_CURVE_FramebufferList {
	/* Contains all framebuffer objects needed by this engine.
	 * Only contains (GPUFrameBuffer *) */
	struct GPUFrameBuffer *fb;
} EDIT_CURVE_FramebufferList;

typedef struct EDIT_CURVE_TextureList {
	/* Contains all framebuffer textures / utility textures
	 * needed by this engine. Only viewport specific textures
	 * (not per object). Only contains (GPUTexture *) */
	struct GPUTexture *texture;
} EDIT_CURVE_TextureList;

typedef struct EDIT_CURVE_StorageList {
	/* Contains any other memory block that the engine needs.
	 * Only directly MEM_(m/c)allocN'ed blocks because they are
	 * free with MEM_freeN() when viewport is freed.
	 * (not per object) */
	struct CustomStruct *block;
	struct EDIT_CURVE_PrivateData *g_data;
} EDIT_CURVE_StorageList;

typedef struct EDIT_CURVE_Data {
	/* Struct returned by DRW_viewport_engine_data_ensure.
	 * If you don't use one of these, just make it a (void *) */
	// void *fbl;
	void *engine_type; /* Required */
	EDIT_CURVE_FramebufferList *fbl;
	EDIT_CURVE_TextureList *txl;
	EDIT_CURVE_PassList *psl;
	EDIT_CURVE_StorageList *stl;
} EDIT_CURVE_Data;

/* *********** STATIC *********** */

static struct {
	/* Custom shaders :
	 * Add sources to source/blender/draw/modes/shaders
	 * init in EDIT_CURVE_engine_init();
	 * free in EDIT_CURVE_engine_free(); */

	GPUShader *wire_sh;

	GPUShader *overlay_edge_sh;  /* handles and nurbs control cage */
	GPUShader *overlay_vert_sh;

} e_data = {NULL}; /* Engine data */

typedef struct EDIT_CURVE_PrivateData {
	/* This keeps the references of the shading groups for
	 * easy access in EDIT_CURVE_cache_populate() */

	/* resulting curve as 'wire' for curves (and optionally normals) */
	DRWShadingGroup *wire_shgrp;

	DRWShadingGroup *overlay_edge_shgrp;
	DRWShadingGroup *overlay_vert_shgrp;
} EDIT_CURVE_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

/* Init Textures, Framebuffers, Storage and Shaders.
 * It is called for every frames.
 * (Optional) */
static void EDIT_CURVE_engine_init(void *vedata)
{
	EDIT_CURVE_TextureList *txl = ((EDIT_CURVE_Data *)vedata)->txl;
	EDIT_CURVE_FramebufferList *fbl = ((EDIT_CURVE_Data *)vedata)->fbl;
	EDIT_CURVE_StorageList *stl = ((EDIT_CURVE_Data *)vedata)->stl;

	UNUSED_VARS(txl, fbl, stl);

	/* Init Framebuffers like this: order is attachment order (for color texs) */
	/*
	 * DRWFboTexture tex[2] = {{&txl->depth, GPU_DEPTH_COMPONENT24, 0},
	 *                         {&txl->color, GPU_RGBA8, DRW_TEX_FILTER}};
	 */

	/* DRW_framebuffer_init takes care of checking if
	 * the framebuffer is valid and has the right size*/
	/*
	 * float *viewport_size = DRW_viewport_size_get();
	 * DRW_framebuffer_init(&fbl->occlude_wire_fb,
	 *                     (int)viewport_size[0], (int)viewport_size[1],
	 *                     tex, 2);
	 */

	if (!e_data.wire_sh) {
		e_data.wire_sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	}

	if (!e_data.overlay_edge_sh) {
		e_data.overlay_edge_sh = DRW_shader_create_with_lib(
		        datatoc_edit_curve_overlay_loosevert_vert_glsl,
		        datatoc_edit_curve_overlay_handle_geom_glsl,
		        datatoc_gpu_shader_flat_color_frag_glsl,
		        datatoc_common_globals_lib_glsl, NULL);
	}

	if (!e_data.overlay_vert_sh) {
		e_data.overlay_vert_sh = DRW_shader_create_with_lib(
		        datatoc_edit_curve_overlay_loosevert_vert_glsl, NULL,
		        datatoc_edit_curve_overlay_frag_glsl,
		        datatoc_common_globals_lib_glsl, NULL);
	}
}

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void EDIT_CURVE_cache_init(void *vedata)
{
	EDIT_CURVE_PassList *psl = ((EDIT_CURVE_Data *)vedata)->psl;
	EDIT_CURVE_StorageList *stl = ((EDIT_CURVE_Data *)vedata)->stl;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	{
		DRWShadingGroup *grp;

		/* Center-Line (wire) */
		psl->wire_pass = DRW_pass_create(
		        "Curve Wire",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WIRE);

		grp = DRW_shgroup_create(e_data.wire_sh, psl->wire_pass);
		DRW_shgroup_uniform_vec4(grp, "color", ts.colorWireEdit, 1);
		stl->g_data->wire_shgrp = grp;


		psl->overlay_edge_pass = DRW_pass_create(
		        "Curve Handle Overlay",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_WIRE);

		grp = DRW_shgroup_create(e_data.overlay_edge_sh, psl->overlay_edge_pass);
		DRW_shgroup_uniform_block(grp, "globalsBlock", globals_ubo);
		DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
		stl->g_data->overlay_edge_shgrp = grp;


		psl->overlay_vert_pass = DRW_pass_create(
		        "Curve Vert Overlay",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_POINT);

		grp = DRW_shgroup_create(e_data.overlay_vert_sh, psl->overlay_vert_pass);
		DRW_shgroup_uniform_block(grp, "globalsBlock", globals_ubo);
		DRW_shgroup_uniform_vec2(grp, "viewportSize", DRW_viewport_size_get(), 1);
		stl->g_data->overlay_vert_shgrp = grp;
	}

}

/* Add geometry to shadingGroups. Execute for each objects */
static void EDIT_CURVE_cache_populate(void *vedata, Object *ob)
{
	EDIT_CURVE_PassList *psl = ((EDIT_CURVE_Data *)vedata)->psl;
	EDIT_CURVE_StorageList *stl = ((EDIT_CURVE_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	View3D *v3d = draw_ctx->v3d;

	UNUSED_VARS(psl, stl);

	if (ob->type == OB_CURVE) {
#if 0
		if (ob == draw_ctx->object_edit)
#else
		if ((ob == draw_ctx->object_edit) || BKE_object_is_in_editmode(ob))
#endif
		{
			Curve *cu = ob->data;
			/* Get geometry cache */
			struct GPUBatch *geom;

			geom = DRW_cache_curve_edge_wire_get(ob);
			DRW_shgroup_call_add(stl->g_data->wire_shgrp, geom, ob->obmat);

			if ((cu->flag & CU_3D) && (cu->drawflag & CU_HIDE_NORMALS) == 0) {
				geom = DRW_cache_curve_edge_normal_get(ob, v3d->overlay.normals_length);
				DRW_shgroup_call_add(stl->g_data->wire_shgrp, geom, ob->obmat);
			}

			/* Add geom to a shading group */
			geom = DRW_cache_curve_edge_overlay_get(ob);
			if (geom) {
				DRW_shgroup_call_add(stl->g_data->overlay_edge_shgrp, geom, ob->obmat);
			}

			geom = DRW_cache_curve_vert_overlay_get(ob);
			DRW_shgroup_call_add(stl->g_data->overlay_vert_shgrp, geom, ob->obmat);
		}
	}
}

/* Optional: Post-cache_populate callback */
static void EDIT_CURVE_cache_finish(void *vedata)
{
	EDIT_CURVE_PassList *psl = ((EDIT_CURVE_Data *)vedata)->psl;
	EDIT_CURVE_StorageList *stl = ((EDIT_CURVE_Data *)vedata)->stl;

	/* Do something here! dependant on the objects gathered */
	UNUSED_VARS(psl, stl);
}

/* Draw time ! Control rendering pipeline from here */
static void EDIT_CURVE_draw_scene(void *vedata)
{
	EDIT_CURVE_PassList *psl = ((EDIT_CURVE_Data *)vedata)->psl;
	EDIT_CURVE_FramebufferList *fbl = ((EDIT_CURVE_Data *)vedata)->fbl;

	/* Default framebuffer and texture */
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	UNUSED_VARS(fbl);

	MULTISAMPLE_SYNC_ENABLE(dfbl, dtxl);

	/* Show / hide entire passes, swap framebuffers ... whatever you fancy */
	/*
	 * DRW_framebuffer_texture_detach(dtxl->depth);
	 * DRW_framebuffer_bind(fbl->custom_fb);
	 * DRW_draw_pass(psl->pass);
	 * DRW_framebuffer_texture_attach(dfbl->default_fb, dtxl->depth, 0, 0);
	 * DRW_framebuffer_bind(dfbl->default_fb);
	 */

	/* ... or just render passes on default framebuffer. */
	DRW_draw_pass(psl->wire_pass);
	DRW_draw_pass(psl->overlay_edge_pass);
	DRW_draw_pass(psl->overlay_vert_pass);

	MULTISAMPLE_SYNC_DISABLE(dfbl, dtxl)

	/* If you changed framebuffer, double check you rebind
	 * the default one with its textures attached before finishing */
}

/* Cleanup when destroying the engine.
 * This is not per viewport ! only when quitting blender.
 * Mostly used for freeing shaders */
static void EDIT_CURVE_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.overlay_edge_sh);
	DRW_SHADER_FREE_SAFE(e_data.overlay_vert_sh);
}

static const DrawEngineDataSize EDIT_CURVE_data_size = DRW_VIEWPORT_DATA_SIZE(EDIT_CURVE_Data);

DrawEngineType draw_engine_edit_curve_type = {
	NULL, NULL,
	N_("EditCurveMode"),
	&EDIT_CURVE_data_size,
	&EDIT_CURVE_engine_init,
	&EDIT_CURVE_engine_free,
	&EDIT_CURVE_cache_init,
	&EDIT_CURVE_cache_populate,
	&EDIT_CURVE_cache_finish,
	NULL, /* draw_background but not needed by mode engines */
	&EDIT_CURVE_draw_scene,
	NULL,
	NULL,
};
