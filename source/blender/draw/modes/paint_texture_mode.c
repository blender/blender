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

/** \file blender/draw/modes/paint_texture_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "BIF_gl.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"
#include "GPU_texture.h"

#include "draw_common.h"

#include "draw_mode_engines.h"

#include "DNA_mesh_types.h"

extern char datatoc_common_globals_lib_glsl[];
extern char datatoc_paint_texture_vert_glsl[];
extern char datatoc_paint_texture_frag_glsl[];
extern char datatoc_paint_wire_vert_glsl[];
extern char datatoc_paint_wire_frag_glsl[];

/* If needed, contains all global/Theme colors
 * Add needed theme colors / values to DRW_globals_update() and update UBO
 * Not needed for constant color. */
extern struct GPUUniformBuffer *globals_ubo; /* draw_common.c */
extern struct GlobalsUboStorage ts; /* draw_common.c */

/* *********** LISTS *********** */
/* All lists are per viewport specific datas.
 * They are all free when viewport changes engines
 * or is free itself. Use PAINT_TEXTURE_engine_init() to
 * initialize most of them and PAINT_TEXTURE_cache_init()
 * for PAINT_TEXTURE_PassList */

typedef struct PAINT_TEXTURE_PassList {
	/* Declare all passes here and init them in
	 * PAINT_TEXTURE_cache_init().
	 * Only contains (DRWPass *) */
	struct DRWPass *image_faces;

	struct DRWPass *wire_overlay;
	struct DRWPass *face_overlay;
} PAINT_TEXTURE_PassList;

typedef struct PAINT_TEXTURE_FramebufferList {
	/* Contains all framebuffer objects needed by this engine.
	 * Only contains (GPUFrameBuffer *) */
	struct GPUFrameBuffer *fb;
} PAINT_TEXTURE_FramebufferList;

typedef struct PAINT_TEXTURE_TextureList {
	/* Contains all framebuffer textures / utility textures
	 * needed by this engine. Only viewport specific textures
	 * (not per object). Only contains (GPUTexture *) */
	struct GPUTexture *texture;
} PAINT_TEXTURE_TextureList;

typedef struct PAINT_TEXTURE_StorageList {
	/* Contains any other memory block that the engine needs.
	 * Only directly MEM_(m/c)allocN'ed blocks because they are
	 * free with MEM_freeN() when viewport is freed.
	 * (not per object) */
	struct CustomStruct *block;
	struct PAINT_TEXTURE_PrivateData *g_data;
} PAINT_TEXTURE_StorageList;

typedef struct PAINT_TEXTURE_Data {
	/* Struct returned by DRW_viewport_engine_data_ensure.
	 * If you don't use one of these, just make it a (void *) */
	// void *fbl;
	void *engine_type; /* Required */
	PAINT_TEXTURE_FramebufferList *fbl;
	PAINT_TEXTURE_TextureList *txl;
	PAINT_TEXTURE_PassList *psl;
	PAINT_TEXTURE_StorageList *stl;
} PAINT_TEXTURE_Data;

/* *********** STATIC *********** */

static struct {
	/* Custom shaders :
	 * Add sources to source/blender/draw/modes/shaders
	 * init in PAINT_TEXTURE_engine_init();
	 * free in PAINT_TEXTURE_engine_free(); */
	struct GPUShader *fallback_sh;
	struct GPUShader *image_sh;

	struct GPUShader *wire_overlay_shader;
	struct GPUShader *face_overlay_shader;
} e_data = {NULL}; /* Engine data */

typedef struct PAINT_TEXTURE_PrivateData {
	/* This keeps the references of the shading groups for
	 * easy access in PAINT_TEXTURE_cache_populate() */
	DRWShadingGroup *shgroup_fallback;
	DRWShadingGroup **shgroup_image_array;

	/* face-mask  */
	DRWShadingGroup *lwire_shgrp;
	DRWShadingGroup *face_shgrp;
} PAINT_TEXTURE_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

/* Init Textures, Framebuffers, Storage and Shaders.
 * It is called for every frames.
 * (Optional) */
static void PAINT_TEXTURE_engine_init(void *vedata)
{
	PAINT_TEXTURE_TextureList *txl = ((PAINT_TEXTURE_Data *)vedata)->txl;
	PAINT_TEXTURE_FramebufferList *fbl = ((PAINT_TEXTURE_Data *)vedata)->fbl;
	PAINT_TEXTURE_StorageList *stl = ((PAINT_TEXTURE_Data *)vedata)->stl;

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

	if (!e_data.fallback_sh) {
		e_data.fallback_sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	}
	if (!e_data.image_sh) {
		e_data.image_sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);

		e_data.image_sh = DRW_shader_create_with_lib(
		        datatoc_paint_texture_vert_glsl, NULL,
		        datatoc_paint_texture_frag_glsl,
		        datatoc_common_globals_lib_glsl, NULL);

	}

	if (!e_data.wire_overlay_shader) {
		e_data.wire_overlay_shader = DRW_shader_create_with_lib(
		        datatoc_paint_wire_vert_glsl, NULL,
		        datatoc_paint_wire_frag_glsl,
		        datatoc_common_globals_lib_glsl,
		        "#define VERTEX_MODE\n");
	}

	if (!e_data.face_overlay_shader) {
		e_data.face_overlay_shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	}
}

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void PAINT_TEXTURE_cache_init(void *vedata)
{
	PAINT_TEXTURE_PassList *psl = ((PAINT_TEXTURE_Data *)vedata)->psl;
	PAINT_TEXTURE_StorageList *stl = ((PAINT_TEXTURE_Data *)vedata)->stl;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
		stl->g_data->shgroup_image_array = NULL;
	}

	{
		/* Create a pass */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND;
		psl->image_faces = DRW_pass_create("Image Color Pass", state);

		stl->g_data->shgroup_fallback = DRW_shgroup_create(e_data.fallback_sh, psl->image_faces);

		/* Uniforms need a pointer to it's value so be sure it's accessible at
		 * any given time (i.e. use static vars) */
		static float color[4] = {1.0f, 0.0f, 1.0f, 1.0};
		DRW_shgroup_uniform_vec4(stl->g_data->shgroup_fallback, "color", color, 1);

		MEM_SAFE_FREE(stl->g_data->shgroup_image_array);

		const DRWContextState *draw_ctx = DRW_context_state_get();
		Object *ob = draw_ctx->obact;
		if (ob && ob->type == OB_MESH) {
			Scene *scene = draw_ctx->scene;
			const bool use_material_slots = (scene->toolsettings->imapaint.mode == IMAGEPAINT_MODE_MATERIAL);
			const Mesh *me = ob->data;

			stl->g_data->shgroup_image_array = MEM_mallocN(
			        sizeof(*stl->g_data->shgroup_image_array) * (use_material_slots ? me->totcol : 1), __func__);

			if (use_material_slots) {
				for (int i = 0; i < me->totcol; i++) {
					Material *ma = give_current_material(ob, i + 1);
					Image *ima = (ma && ma->texpaintslot) ? ma->texpaintslot[ma->paint_active_slot].ima : NULL;
					GPUTexture *tex = ima ?
					        GPU_texture_from_blender(ima, NULL, GL_TEXTURE_2D, false, 0.0f) : NULL;

					if (tex) {
						DRWShadingGroup *grp = DRW_shgroup_create(e_data.image_sh, psl->image_faces);
						DRW_shgroup_uniform_texture(grp, "image", tex);
						DRW_shgroup_uniform_float(grp, "alpha", &draw_ctx->v3d->overlay.texture_paint_mode_opacity, 1);
						DRW_shgroup_uniform_block(grp, "globalsBlock", globals_ubo);
						stl->g_data->shgroup_image_array[i] = grp;
					}
					else {
						stl->g_data->shgroup_image_array[i] = NULL;
					}
				}
			}
			else {
				Image *ima = scene->toolsettings->imapaint.canvas;
				GPUTexture *tex = ima ?
				        GPU_texture_from_blender(ima, NULL, GL_TEXTURE_2D, false, 0.0f) : NULL;

				if (tex) {
					DRWShadingGroup *grp = DRW_shgroup_create(e_data.image_sh, psl->image_faces);
					DRW_shgroup_uniform_texture(grp, "image", tex);
					DRW_shgroup_uniform_float(grp, "alpha", &draw_ctx->v3d->overlay.texture_paint_mode_opacity, 1);
					stl->g_data->shgroup_image_array[0] = grp;
				}
				else {
					stl->g_data->shgroup_image_array[0] = NULL;
				}
			}
		}
	}

	/* Face Mask */
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

/* Add geometry to shadingGroups. Execute for each objects */
static void PAINT_TEXTURE_cache_populate(void *vedata, Object *ob)
{
	PAINT_TEXTURE_PassList *psl = ((PAINT_TEXTURE_Data *)vedata)->psl;
	PAINT_TEXTURE_StorageList *stl = ((PAINT_TEXTURE_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();

	UNUSED_VARS(psl, stl);

	if ((ob->type == OB_MESH) && (draw_ctx->obact == ob)) {
		/* Get geometry cache */
		const Mesh *me = ob->data;
		Scene *scene = draw_ctx->scene;
		const bool use_surface = draw_ctx->v3d->overlay.texture_paint_mode_opacity != 0.0; //DRW_object_is_mode_shade(ob) == true;
		const bool use_material_slots = (scene->toolsettings->imapaint.mode == IMAGEPAINT_MODE_MATERIAL);
		bool ok = false;

		if (use_surface) {
			if (me->mloopuv != NULL) {
				if (use_material_slots) {
					struct GPUBatch **geom_array = me->totcol ? DRW_cache_mesh_surface_texpaint_get(ob) : NULL;
					if ((me->totcol == 0) || (geom_array == NULL)) {
						struct GPUBatch *geom = DRW_cache_mesh_surface_get(ob);
						DRW_shgroup_call_add(stl->g_data->shgroup_fallback, geom, ob->obmat);
						ok = true;
					}
					else {
						for (int i = 0; i < me->totcol; i++) {
							if (stl->g_data->shgroup_image_array[i]) {
								DRW_shgroup_call_add(stl->g_data->shgroup_image_array[i], geom_array[i], ob->obmat);
							}
							else {
								DRW_shgroup_call_add(stl->g_data->shgroup_fallback, geom_array[i], ob->obmat);
							}
							ok = true;
						}
					}
				}
				else {
					struct GPUBatch *geom = DRW_cache_mesh_surface_texpaint_single_get(ob);
					if (geom && stl->g_data->shgroup_image_array[0]) {
						DRW_shgroup_call_add(stl->g_data->shgroup_image_array[0], geom, ob->obmat);
						ok = true;
					}
				}
			}

			if (!ok) {
				struct GPUBatch *geom = DRW_cache_mesh_surface_get(ob);
				DRW_shgroup_call_add(stl->g_data->shgroup_fallback, geom, ob->obmat);
			}
		}

		/* Face Mask */
		const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
		if (use_face_sel) {
			struct GPUBatch *geom;
			/* Note: ideally selected faces wouldn't show interior wire. */
			const bool use_wire = true;
			geom = DRW_cache_mesh_edges_paint_overlay_get(ob, use_wire, use_face_sel);
			DRW_shgroup_call_add(stl->g_data->lwire_shgrp, geom, ob->obmat);

			geom = DRW_cache_mesh_faces_weight_overlay_get(ob);
			DRW_shgroup_call_add(stl->g_data->face_shgrp, geom, ob->obmat);
		}
	}
}

/* Optional: Post-cache_populate callback */
static void PAINT_TEXTURE_cache_finish(void *vedata)
{
	PAINT_TEXTURE_PassList *psl = ((PAINT_TEXTURE_Data *)vedata)->psl;
	PAINT_TEXTURE_StorageList *stl = ((PAINT_TEXTURE_Data *)vedata)->stl;

	/* Do something here! dependant on the objects gathered */
	UNUSED_VARS(psl);

	MEM_SAFE_FREE(stl->g_data->shgroup_image_array);
}

/* Draw time ! Control rendering pipeline from here */
static void PAINT_TEXTURE_draw_scene(void *vedata)
{
	PAINT_TEXTURE_PassList *psl = ((PAINT_TEXTURE_Data *)vedata)->psl;
	PAINT_TEXTURE_FramebufferList *fbl = ((PAINT_TEXTURE_Data *)vedata)->fbl;

	/* Default framebuffer and texture */
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	UNUSED_VARS(fbl, dfbl, dtxl);

	DRW_draw_pass(psl->image_faces);

	DRW_draw_pass(psl->face_overlay);
	DRW_draw_pass(psl->wire_overlay);
}

/* Cleanup when destroying the engine.
 * This is not per viewport ! only when quitting blender.
 * Mostly used for freeing shaders */
static void PAINT_TEXTURE_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.image_sh);
	DRW_SHADER_FREE_SAFE(e_data.wire_overlay_shader);
}

static const DrawEngineDataSize PAINT_TEXTURE_data_size = DRW_VIEWPORT_DATA_SIZE(PAINT_TEXTURE_Data);

DrawEngineType draw_engine_paint_texture_type = {
	NULL, NULL,
	N_("PaintTextureMode"),
	&PAINT_TEXTURE_data_size,
	&PAINT_TEXTURE_engine_init,
	&PAINT_TEXTURE_engine_free,
	&PAINT_TEXTURE_cache_init,
	&PAINT_TEXTURE_cache_populate,
	&PAINT_TEXTURE_cache_finish,
	NULL, /* draw_background but not needed by mode engines */
	&PAINT_TEXTURE_draw_scene,
	NULL,
	NULL,
};
