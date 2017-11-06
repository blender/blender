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

/* Gather all screen space effects technique such as Bloom, Motion Blur, DoF, SSAO, SSR, ...
 */

/** \file eevee_effects.c
 *  \ingroup draw_engine
 */

#include "DRW_render.h"

#include "BKE_global.h" /* for G.debug_value */

#include "eevee_private.h"
#include "GPU_texture.h"

static struct {
	/* Downsample Depth */
	struct GPUShader *minz_downlevel_sh;
	struct GPUShader *maxz_downlevel_sh;
	struct GPUShader *minz_downdepth_sh;
	struct GPUShader *maxz_downdepth_sh;
	struct GPUShader *minz_downdepth_layer_sh;
	struct GPUShader *maxz_downdepth_layer_sh;
	struct GPUShader *minz_copydepth_sh;
	struct GPUShader *maxz_copydepth_sh;

	/* Simple Downsample */
	struct GPUShader *downsample_sh;
	struct GPUShader *downsample_cube_sh;

	/* Theses are just references, not actually allocated */
	struct GPUTexture *depth_src;
	struct GPUTexture *color_src;

	int depth_src_layer;
	float cube_texel_size;
} e_data = {NULL}; /* Engine data */

extern char datatoc_effect_minmaxz_frag_glsl[];
extern char datatoc_effect_downsample_frag_glsl[];
extern char datatoc_effect_downsample_cube_frag_glsl[];
extern char datatoc_lightprobe_vert_glsl[];
extern char datatoc_lightprobe_geom_glsl[];

static void eevee_create_shader_downsample(void)
{
	e_data.downsample_sh = DRW_shader_create_fullscreen(datatoc_effect_downsample_frag_glsl, NULL);
	e_data.downsample_cube_sh = DRW_shader_create(datatoc_lightprobe_vert_glsl,
	                                              datatoc_lightprobe_geom_glsl,
	                                              datatoc_effect_downsample_cube_frag_glsl, NULL);

	e_data.minz_downlevel_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MIN_PASS\n");
	e_data.maxz_downlevel_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MAX_PASS\n");
	e_data.minz_downdepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MIN_PASS\n"
	                                                                                          "#define INPUT_DEPTH\n");
	e_data.maxz_downdepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MAX_PASS\n"
	                                                                                          "#define INPUT_DEPTH\n");
	e_data.minz_downdepth_layer_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MIN_PASS\n"
	                                                                                                "#define LAYERED\n"
	                                                                                                "#define INPUT_DEPTH\n");
	e_data.maxz_downdepth_layer_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MAX_PASS\n"
	                                                                                                "#define LAYERED\n"
	                                                                                                "#define INPUT_DEPTH\n");
	e_data.minz_copydepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MIN_PASS\n"
	                                                                                          "#define INPUT_DEPTH\n"
	                                                                                          "#define COPY_DEPTH\n");
	e_data.maxz_copydepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl, "#define MAX_PASS\n"
	                                                                                          "#define INPUT_DEPTH\n"
	                                                                                          "#define COPY_DEPTH\n");
}

void EEVEE_effects_init(EEVEE_SceneLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects;

	const float *viewport_size = DRW_viewport_size_get();

	/* Shaders */
	if (!e_data.downsample_sh) {
		eevee_create_shader_downsample();
	}

	if (!stl->effects) {
		stl->effects = MEM_callocN(sizeof(EEVEE_EffectsInfo), "EEVEE_EffectsInfo");
	}

	effects = stl->effects;

	effects->enabled_effects = 0;
	effects->enabled_effects |= EEVEE_motion_blur_init(sldata, vedata);
	effects->enabled_effects |= EEVEE_bloom_init(sldata, vedata);
	effects->enabled_effects |= EEVEE_depth_of_field_init(sldata, vedata);
	effects->enabled_effects |= EEVEE_temporal_sampling_init(sldata, vedata);
	effects->enabled_effects |= EEVEE_occlusion_init(sldata, vedata);
	effects->enabled_effects |= EEVEE_screen_raytrace_init(sldata, vedata);
	effects->enabled_effects |= EEVEE_volumes_init(sldata, vedata);

	/**
	 * Ping Pong buffer
	 */
	if ((effects->enabled_effects & EFFECT_POST_BUFFER) != 0) {
		DRWFboTexture tex = {&txl->color_post, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP};
		DRW_framebuffer_init(&fbl->effect_fb, &draw_engine_eevee_type,
		                    (int)viewport_size[0], (int)viewport_size[1],
		                    &tex, 1);
	}
	else {
		/* Cleanup to release memory */
		DRW_TEXTURE_FREE_SAFE(txl->color_post);
		DRW_FRAMEBUFFER_FREE_SAFE(fbl->effect_fb);
	}

	/**
	 * MinMax Pyramid
	 */
	DRWFboTexture texmax = {&txl->maxzbuffer, DRW_TEX_DEPTH_24, DRW_TEX_MIPMAP};
	DRW_framebuffer_init(&fbl->downsample_fb, &draw_engine_eevee_type,
	                    (int)viewport_size[0] / 2, (int)viewport_size[1] / 2,
	                    &texmax, 1);

	/**
	 * Compute Mipmap texel alignement.
	 */
	for (int i = 0; i < 10; ++i) {
		float mip_size[2] = {viewport_size[0], viewport_size[1]};
		for (int j = 0; j < i; ++j) {
			mip_size[0] = floorf(fmaxf(1.0f, mip_size[0] / 2.0f));
			mip_size[1] = floorf(fmaxf(1.0f, mip_size[1] / 2.0f));
		}
		stl->g_data->mip_ratio[i][0] = viewport_size[0] / (mip_size[0] * powf(2.0f, floorf(log2f(floorf(viewport_size[0] / mip_size[0])))));
		stl->g_data->mip_ratio[i][1] = viewport_size[1] / (mip_size[1] * powf(2.0f, floorf(log2f(floorf(viewport_size[1] / mip_size[1])))));
	}


	/**
	 * Normal buffer for deferred passes.
	 */
	if ((effects->enabled_effects & EFFECT_NORMAL_BUFFER) != 0)	{
		if (txl->ssr_normal_input == NULL) {
			DRWTextureFormat nor_format = DRW_TEX_RG_16;
			txl->ssr_normal_input = DRW_texture_create_2D((int)viewport_size[0], (int)viewport_size[1], nor_format, 0, NULL);
		}

		/* Reattach textures to the right buffer (because we are alternating between buffers) */
		/* TODO multiple FBO per texture!!!! */
		DRW_framebuffer_texture_detach(txl->ssr_normal_input);
		DRW_framebuffer_texture_attach(fbl->main, txl->ssr_normal_input, 1, 0);
	}
	else {
		/* Cleanup to release memory */
		DRW_TEXTURE_FREE_SAFE(txl->ssr_normal_input);
	}

	/**
	 * Setup double buffer so we can access last frame as it was before post processes.
	 */
	if ((effects->enabled_effects & EFFECT_DOUBLE_BUFFER) != 0) {
		DRWFboTexture tex_double_buffer = {&txl->color_double_buffer, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP};
		DRW_framebuffer_init(&fbl->double_buffer, &draw_engine_eevee_type,
		                    (int)viewport_size[0], (int)viewport_size[1],
		                    &tex_double_buffer, 1);
	}
	else {
		/* Cleanup to release memory */
		DRW_TEXTURE_FREE_SAFE(txl->color_double_buffer);
		DRW_FRAMEBUFFER_FREE_SAFE(fbl->double_buffer);
	}
}

void EEVEE_effects_cache_init(EEVEE_SceneLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects = stl->effects;

	struct Gwn_Batch *quad = DRW_cache_fullscreen_quad_get();

	{
		psl->color_downsample_ps = DRW_pass_create("Downsample", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.downsample_sh, psl->color_downsample_ps);
		DRW_shgroup_uniform_buffer(grp, "source", &e_data.color_src);
		DRW_shgroup_uniform_float(grp, "fireflyFactor", &effects->ssr_firefly_fac, 1);
		DRW_shgroup_call_add(grp, quad, NULL);
	}

	{
		static int zero = 0;
		psl->color_downsample_cube_ps = DRW_pass_create("Downsample Cube", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = DRW_shgroup_instance_create(e_data.downsample_cube_sh, psl->color_downsample_cube_ps, quad);
		DRW_shgroup_uniform_buffer(grp, "source", &e_data.color_src);
		DRW_shgroup_uniform_float(grp, "texelSize", &e_data.cube_texel_size, 1);
		DRW_shgroup_uniform_int(grp, "Layer", &zero, 1);
		DRW_shgroup_set_instance_count(grp, 6);
	}

	{
		/* Perform min/max downsample */
		DRWShadingGroup *grp;

#if 0 /* Not used for now */
		psl->minz_downlevel_ps = DRW_pass_create("HiZ Min Down Level", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.minz_downlevel_sh, psl->minz_downlevel_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &stl->g_data->minzbuffer);
		DRW_shgroup_call_add(grp, quad, NULL);
#endif

		psl->maxz_downlevel_ps = DRW_pass_create("HiZ Max Down Level", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.maxz_downlevel_sh, psl->maxz_downlevel_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &txl->maxzbuffer);
		DRW_shgroup_call_add(grp, quad, NULL);

		/* Copy depth buffer to halfres top level of HiZ */
#if 0 /* Not used for now */
		psl->minz_downdepth_ps = DRW_pass_create("HiZ Min Copy Depth Halfres", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.minz_downdepth_sh, psl->minz_downdepth_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_call_add(grp, quad, NULL);
#endif

		psl->maxz_downdepth_ps = DRW_pass_create("HiZ Max Copy Depth Halfres", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.maxz_downdepth_sh, psl->maxz_downdepth_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_call_add(grp, quad, NULL);

#if 0 /* Not used for now */
		psl->minz_downdepth_layer_ps = DRW_pass_create("HiZ Min Copy DepthLayer Halfres", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.minz_downdepth_layer_sh, psl->minz_downdepth_layer_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_uniform_int(grp, "depthLayer", &e_data.depth_src_layer, 1);
		DRW_shgroup_call_add(grp, quad, NULL);
#endif

		psl->maxz_downdepth_layer_ps = DRW_pass_create("HiZ Max Copy DepthLayer Halfres", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.maxz_downdepth_layer_sh, psl->maxz_downdepth_layer_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_uniform_int(grp, "depthLayer", &e_data.depth_src_layer, 1);
		DRW_shgroup_call_add(grp, quad, NULL);

		/* Copy depth buffer to halfres top level of HiZ */
#if 0 /* Not used for now */
		psl->minz_copydepth_ps = DRW_pass_create("HiZ Min Copy Depth Fullres", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.minz_copydepth_sh, psl->minz_copydepth_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_call_add(grp, quad, NULL);
#endif

		psl->maxz_copydepth_ps = DRW_pass_create("HiZ Max Copy Depth Fullres", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
		grp = DRW_shgroup_create(e_data.maxz_copydepth_sh, psl->maxz_copydepth_ps);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_call_add(grp, quad, NULL);
	}
}

#if 0 /* Not required for now */
static void min_downsample_cb(void *vedata, int UNUSED(level))
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	DRW_draw_pass(psl->minz_downlevel_ps);
}
#endif

static void max_downsample_cb(void *vedata, int UNUSED(level))
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	DRW_draw_pass(psl->maxz_downlevel_ps);
}

static void simple_downsample_cb(void *vedata, int UNUSED(level))
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	DRW_draw_pass(psl->color_downsample_ps);
}

static void simple_downsample_cube_cb(void *vedata, int level)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	e_data.cube_texel_size = (float)(1 << level) / (float)GPU_texture_width(e_data.color_src);
	DRW_draw_pass(psl->color_downsample_cube_ps);
}

void EEVEE_create_minmax_buffer(EEVEE_Data *vedata, GPUTexture *depth_src, int layer)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;

	e_data.depth_src = depth_src;
	e_data.depth_src_layer = layer;

#if 0 /* Not required for now */
	DRW_stats_group_start("Min buffer");
	/* Copy depth buffer to min texture top level */
	DRW_framebuffer_texture_attach(fbl->downsample_fb, stl->g_data->minzbuffer, 0, 0);
	DRW_framebuffer_bind(fbl->downsample_fb);
	if (layer >= 0) {
		DRW_draw_pass(psl->minz_downdepth_layer_ps);
	}
	else {
		DRW_draw_pass(psl->minz_downdepth_ps);
	}
	DRW_framebuffer_texture_detach(stl->g_data->minzbuffer);

	/* Create lower levels */
	DRW_framebuffer_recursive_downsample(fbl->downsample_fb, stl->g_data->minzbuffer, 8, &min_downsample_cb, vedata);
	DRW_stats_group_end();
#endif

	DRW_stats_group_start("Max buffer");
	/* Copy depth buffer to max texture top level */
	DRW_framebuffer_texture_attach(fbl->downsample_fb, txl->maxzbuffer, 0, 0);
	DRW_framebuffer_bind(fbl->downsample_fb);
	if (layer >= 0) {
		DRW_draw_pass(psl->maxz_downdepth_layer_ps);
	}
	else {
		DRW_draw_pass(psl->maxz_downdepth_ps);
	}
	DRW_framebuffer_texture_detach(txl->maxzbuffer);

	/* Create lower levels */
	DRW_framebuffer_recursive_downsample(fbl->downsample_fb, txl->maxzbuffer, 8, &max_downsample_cb, vedata);
	DRW_stats_group_end();

	/* Restore */
	DRW_framebuffer_bind(fbl->main);
}

/**
 * Simple downsampling algorithm. Reconstruct mip chain up to mip level.
 **/
void EEVEE_downsample_buffer(EEVEE_Data *vedata, struct GPUFrameBuffer *fb_src, GPUTexture *texture_src, int level)
{
	e_data.color_src = texture_src;

	DRW_stats_group_start("Downsample buffer");
	/* Create lower levels */
	DRW_framebuffer_recursive_downsample(fb_src, texture_src, level, &simple_downsample_cb, vedata);
	DRW_stats_group_end();
}

/**
 * Simple downsampling algorithm for cubemap. Reconstruct mip chain up to mip level.
 **/
void EEVEE_downsample_cube_buffer(EEVEE_Data *vedata, struct GPUFrameBuffer *fb_src, GPUTexture *texture_src, int level)
{
	e_data.color_src = texture_src;

	DRW_stats_group_start("Downsample Cube buffer");
	/* Create lower levels */
	DRW_framebuffer_recursive_downsample(fb_src, texture_src, level, &simple_downsample_cube_cb, vedata);
	DRW_stats_group_end();
}

void EEVEE_draw_effects(EEVEE_Data *vedata)
{
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	/* only once per frame after the first post process */
	effects->swap_double_buffer = ((effects->enabled_effects & EFFECT_DOUBLE_BUFFER) != 0);

	/* Init pointers */
	effects->source_buffer = txl->color; /* latest updated texture */
	effects->target_buffer = fbl->effect_fb; /* next target to render to */

	/* Temporal Anti-Aliasing MUST come first */
	EEVEE_temporal_sampling_draw(vedata);

	/* Detach depth for effects to use it */
	DRW_framebuffer_texture_detach(dtxl->depth);

	/* Post process stack (order matters) */
	EEVEE_motion_blur_draw(vedata);
	EEVEE_depth_of_field_draw(vedata);
	EEVEE_bloom_draw(vedata);

	/* Restore default framebuffer */
	DRW_framebuffer_texture_attach(dfbl->default_fb, dtxl->depth, 0, 0);
	DRW_framebuffer_bind(dfbl->default_fb);

	/* Tonemapping */
	DRW_transform_to_display(effects->source_buffer);

	/* Debug : Ouput buffer to view. */
	if ((G.debug_value > 0) && (G.debug_value <= 6)) {
		switch (G.debug_value) {
			case 1:
				if (txl->maxzbuffer) DRW_transform_to_display(txl->maxzbuffer);
				break;
			case 2:
				if (stl->g_data->ssr_hit_output[0]) DRW_transform_to_display(stl->g_data->ssr_hit_output[0]);
				break;
			case 3:
				if (txl->ssr_normal_input) DRW_transform_to_display(txl->ssr_normal_input);
				break;
			case 4:
				if (txl->ssr_specrough_input) DRW_transform_to_display(txl->ssr_specrough_input);
				break;
			case 5:
				if (txl->color_double_buffer) DRW_transform_to_display(txl->color_double_buffer);
				break;
			case 6:
				if (stl->g_data->gtao_horizons_debug) DRW_transform_to_display(stl->g_data->gtao_horizons_debug);
				break;
			default:
				break;
		}
	}

	/* If no post processes is enabled, buffers are still not swapped, do it now. */
	SWAP_DOUBLE_BUFFERS();

	if (!stl->g_data->valid_double_buffer &&
	    ((effects->enabled_effects & EFFECT_DOUBLE_BUFFER) != 0) &&
	    (DRW_state_is_image_render() == false))
	{
		/* If history buffer is not valid request another frame.
		 * This fix black reflections on area resize. */
		DRW_viewport_request_redraw();
	}

	/* Record pers matrix for the next frame. */
	DRW_viewport_matrix_get(stl->g_data->prev_persmat, DRW_MAT_PERS);

	/* Update double buffer status if render mode. */
	if (DRW_state_is_image_render()) {
		stl->g_data->valid_double_buffer = (txl->color_double_buffer != NULL);
	}
}

void EEVEE_effects_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.downsample_sh);
	DRW_SHADER_FREE_SAFE(e_data.downsample_cube_sh);

	DRW_SHADER_FREE_SAFE(e_data.minz_downlevel_sh);
	DRW_SHADER_FREE_SAFE(e_data.maxz_downlevel_sh);
	DRW_SHADER_FREE_SAFE(e_data.minz_downdepth_sh);
	DRW_SHADER_FREE_SAFE(e_data.maxz_downdepth_sh);
	DRW_SHADER_FREE_SAFE(e_data.minz_downdepth_layer_sh);
	DRW_SHADER_FREE_SAFE(e_data.maxz_downdepth_layer_sh);
	DRW_SHADER_FREE_SAFE(e_data.minz_copydepth_sh);
	DRW_SHADER_FREE_SAFE(e_data.maxz_copydepth_sh);
}
