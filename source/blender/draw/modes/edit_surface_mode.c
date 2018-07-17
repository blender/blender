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

/** \file blender/draw/modes/edit_surface_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"

#include "draw_common.h"

#include "draw_mode_engines.h"

/* If needed, contains all global/Theme colors
 * Add needed theme colors / values to DRW_globals_update() and update UBO
 * Not needed for constant color. */
extern struct GPUUniformBuffer *globals_ubo; /* draw_common.c */
extern struct GlobalsUboStorage ts; /* draw_common.c */

/* *********** LISTS *********** */
/* All lists are per viewport specific datas.
 * They are all free when viewport changes engines
 * or is free itself. Use EDIT_SURFACE_engine_init() to
 * initialize most of them and EDIT_SURFACE_cache_init()
 * for EDIT_SURFACE_PassList */

typedef struct EDIT_SURFACE_PassList {
	/* Declare all passes here and init them in
	 * EDIT_SURFACE_cache_init().
	 * Only contains (DRWPass *) */
	struct DRWPass *pass;
} EDIT_SURFACE_PassList;

typedef struct EDIT_SURFACE_FramebufferList {
	/* Contains all framebuffer objects needed by this engine.
	 * Only contains (GPUFrameBuffer *) */
	struct GPUFrameBuffer *fb;
} EDIT_SURFACE_FramebufferList;

typedef struct EDIT_SURFACE_TextureList {
	/* Contains all framebuffer textures / utility textures
	 * needed by this engine. Only viewport specific textures
	 * (not per object). Only contains (GPUTexture *) */
	struct GPUTexture *texture;
} EDIT_SURFACE_TextureList;

typedef struct EDIT_SURFACE_StorageList {
	/* Contains any other memory block that the engine needs.
	 * Only directly MEM_(m/c)allocN'ed blocks because they are
	 * free with MEM_freeN() when viewport is freed.
	 * (not per object) */
	struct CustomStruct *block;
	struct EDIT_SURFACE_PrivateData *g_data;
} EDIT_SURFACE_StorageList;

typedef struct EDIT_SURFACE_Data {
	/* Struct returned by DRW_viewport_engine_data_ensure.
	 * If you don't use one of these, just make it a (void *) */
	// void *fbl;
	void *engine_type; /* Required */
	EDIT_SURFACE_FramebufferList *fbl;
	EDIT_SURFACE_TextureList *txl;
	EDIT_SURFACE_PassList *psl;
	EDIT_SURFACE_StorageList *stl;
} EDIT_SURFACE_Data;

/* *********** STATIC *********** */

static struct {
	/* Custom shaders :
	 * Add sources to source/blender/draw/modes/shaders
	 * init in EDIT_SURFACE_engine_init();
	 * free in EDIT_SURFACE_engine_free(); */
	struct GPUShader *custom_shader;
} e_data = {NULL}; /* Engine data */

typedef struct EDIT_SURFACE_PrivateData {
	/* This keeps the references of the shading groups for
	 * easy access in EDIT_SURFACE_cache_populate() */
	DRWShadingGroup *group;
} EDIT_SURFACE_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

/* Init Textures, Framebuffers, Storage and Shaders.
 * It is called for every frames.
 * (Optional) */
static void EDIT_SURFACE_engine_init(void *vedata)
{
	EDIT_SURFACE_TextureList *txl = ((EDIT_SURFACE_Data *)vedata)->txl;
	EDIT_SURFACE_FramebufferList *fbl = ((EDIT_SURFACE_Data *)vedata)->fbl;
	EDIT_SURFACE_StorageList *stl = ((EDIT_SURFACE_Data *)vedata)->stl;

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

	if (!e_data.custom_shader) {
		e_data.custom_shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	}
}

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void EDIT_SURFACE_cache_init(void *vedata)
{
	EDIT_SURFACE_PassList *psl = ((EDIT_SURFACE_Data *)vedata)->psl;
	EDIT_SURFACE_StorageList *stl = ((EDIT_SURFACE_Data *)vedata)->stl;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	{
		/* Create a pass */
		DRWState state = (
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
		        DRW_STATE_BLEND | DRW_STATE_WIRE);
		psl->pass = DRW_pass_create("My Pass", state);

		/* Create a shadingGroup using a function in draw_common.c or custom one */
		/*
		 * stl->g_data->group = shgroup_dynlines_uniform_color(psl->pass, ts.colorWire);
		 * -- or --
		 * stl->g_data->group = DRW_shgroup_create(e_data.custom_shader, psl->pass);
		 */
		stl->g_data->group = DRW_shgroup_create(e_data.custom_shader, psl->pass);

		/* Uniforms need a pointer to it's value so be sure it's accessible at
		 * any given time (i.e. use static vars) */
		static float color[4] = {0.0f, 0.0f, 1.0f, 1.0};
		DRW_shgroup_uniform_vec4(stl->g_data->group, "color", color, 1);
	}

}

/* Add geometry to shadingGroups. Execute for each objects */
static void EDIT_SURFACE_cache_populate(void *vedata, Object *ob)
{
	EDIT_SURFACE_PassList *psl = ((EDIT_SURFACE_Data *)vedata)->psl;
	EDIT_SURFACE_StorageList *stl = ((EDIT_SURFACE_Data *)vedata)->stl;

	UNUSED_VARS(psl, stl);

	if (ob->type == OB_MESH) {
		/* Get geometry cache */
		struct GPUBatch *geom = DRW_cache_mesh_surface_get(ob);

		/* Add geom to a shading group */
		DRW_shgroup_call_add(stl->g_data->group, geom, ob->obmat);
	}
}

/* Optional: Post-cache_populate callback */
static void EDIT_SURFACE_cache_finish(void *vedata)
{
	EDIT_SURFACE_PassList *psl = ((EDIT_SURFACE_Data *)vedata)->psl;
	EDIT_SURFACE_StorageList *stl = ((EDIT_SURFACE_Data *)vedata)->stl;

	/* Do something here! dependant on the objects gathered */
	UNUSED_VARS(psl, stl);
}

/* Draw time ! Control rendering pipeline from here */
static void EDIT_SURFACE_draw_scene(void *vedata)
{
	EDIT_SURFACE_PassList *psl = ((EDIT_SURFACE_Data *)vedata)->psl;
	EDIT_SURFACE_FramebufferList *fbl = ((EDIT_SURFACE_Data *)vedata)->fbl;

	/* Default framebuffer and texture */
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	UNUSED_VARS(fbl, dfbl, dtxl);

	/* Show / hide entire passes, swap framebuffers ... whatever you fancy */
	/*
	 * DRW_framebuffer_texture_detach(dtxl->depth);
	 * DRW_framebuffer_bind(fbl->custom_fb);
	 * DRW_draw_pass(psl->pass);
	 * DRW_framebuffer_texture_attach(dfbl->default_fb, dtxl->depth, 0, 0);
	 * DRW_framebuffer_bind(dfbl->default_fb);
	 */

	/* ... or just render passes on default framebuffer. */
	DRW_draw_pass(psl->pass);

	/* If you changed framebuffer, double check you rebind
	 * the default one with its textures attached before finishing */
}

/* Cleanup when destroying the engine.
 * This is not per viewport ! only when quitting blender.
 * Mostly used for freeing shaders */
static void EDIT_SURFACE_engine_free(void)
{
	// DRW_SHADER_FREE_SAFE(custom_shader);
}

static const DrawEngineDataSize EDIT_SURFACE_data_size = DRW_VIEWPORT_DATA_SIZE(EDIT_SURFACE_Data);

DrawEngineType draw_engine_edit_surface_type = {
	NULL, NULL,
	N_("EditSurfaceMode"),
	&EDIT_SURFACE_data_size,
	&EDIT_SURFACE_engine_init,
	&EDIT_SURFACE_engine_free,
	&EDIT_SURFACE_cache_init,
	&EDIT_SURFACE_cache_populate,
	&EDIT_SURFACE_cache_finish,
	NULL, /* draw_background but not needed by mode engines */
	&EDIT_SURFACE_draw_scene,
	NULL,
	NULL,
};
