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

/* Screen space reflections and refractions techniques.
 */

/** \file eevee_screen_raytrace.c
 *  \ingroup draw_engine
 */

#include "DRW_render.h"

#include "BLI_dynstr.h"

#include "eevee_private.h"
#include "GPU_texture.h"

/* SSR shader variations */
enum {
	SSR_SAMPLES      = (1 << 0) | (1 << 1),
	SSR_RESOLVE      = (1 << 2),
	SSR_FULL_TRACE   = (1 << 3),
	SSR_MAX_SHADER   = (1 << 4),
};

static struct {
	/* Screen Space Reflection */
	struct GPUShader *ssr_sh[SSR_MAX_SHADER];

	/* Theses are just references, not actually allocated */
	struct GPUTexture *depth_src;
	struct GPUTexture *color_src;
} e_data = {NULL}; /* Engine data */

extern char datatoc_ambient_occlusion_lib_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];
extern char datatoc_octahedron_lib_glsl[];
extern char datatoc_effect_ssr_frag_glsl[];
extern char datatoc_lightprobe_lib_glsl[];
extern char datatoc_raytrace_lib_glsl[];

static struct GPUShader *eevee_effects_screen_raytrace_shader_get(int options)
{
	if (e_data.ssr_sh[options] == NULL) {
		DynStr *ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, datatoc_bsdf_common_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_bsdf_sampling_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_octahedron_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_lightprobe_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_ambient_occlusion_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_raytrace_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_effect_ssr_frag_glsl);
		char *ssr_shader_str = BLI_dynstr_get_cstring(ds_frag);
		BLI_dynstr_free(ds_frag);

		int samples = (SSR_SAMPLES & options) + 1;

		DynStr *ds_defines = BLI_dynstr_new();
		BLI_dynstr_appendf(ds_defines, SHADER_DEFINES);
		BLI_dynstr_appendf(ds_defines, "#define RAY_COUNT %d\n", samples);
		if (options & SSR_RESOLVE) {
			BLI_dynstr_appendf(ds_defines, "#define STEP_RESOLVE\n");
		}
		else {
			BLI_dynstr_appendf(ds_defines, "#define STEP_RAYTRACE\n");
			BLI_dynstr_appendf(ds_defines, "#define PLANAR_PROBE_RAYTRACE\n");
		}
		if (options & SSR_FULL_TRACE) {
			BLI_dynstr_appendf(ds_defines, "#define FULLRES\n");
		}
		char *ssr_define_str = BLI_dynstr_get_cstring(ds_defines);
		BLI_dynstr_free(ds_defines);

		e_data.ssr_sh[options] = DRW_shader_create_fullscreen(ssr_shader_str, ssr_define_str);

		MEM_freeN(ssr_shader_str);
		MEM_freeN(ssr_define_str);
	}

	return e_data.ssr_sh[options];
}

int EEVEE_screen_raytrace_init(EEVEE_SceneLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects = stl->effects;
	const float *viewport_size = DRW_viewport_size_get();

	const DRWContextState *draw_ctx = DRW_context_state_get();
	SceneLayer *scene_layer = draw_ctx->scene_layer;
	IDProperty *props = BKE_scene_layer_engine_evaluated_get(scene_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_EEVEE);

	/* Compute pixel size, (shared with contact shadows) */
	copy_v2_v2(effects->ssr_pixelsize, viewport_size);
	invert_v2(effects->ssr_pixelsize);

	if (BKE_collection_engine_property_value_get_bool(props, "ssr_enable")) {
		const bool use_refraction = BKE_collection_engine_property_value_get_bool(props, "ssr_refraction");

		if (use_refraction) {
			DRWFboTexture tex = {&txl->refract_color, DRW_TEX_RGB_11_11_10, DRW_TEX_FILTER | DRW_TEX_MIPMAP};

			DRW_framebuffer_init(&fbl->refract_fb, &draw_engine_eevee_type, (int)viewport_size[0], (int)viewport_size[1], &tex, 1);
		}

		effects->ssr_ray_count = BKE_collection_engine_property_value_get_int(props, "ssr_ray_count");
		effects->reflection_trace_full = !BKE_collection_engine_property_value_get_bool(props, "ssr_halfres");
		effects->ssr_use_normalization = BKE_collection_engine_property_value_get_bool(props, "ssr_normalize_weight");
		effects->ssr_quality = 1.0f - BKE_collection_engine_property_value_get_float(props, "ssr_quality");
		effects->ssr_thickness = BKE_collection_engine_property_value_get_float(props, "ssr_thickness");
		effects->ssr_border_fac = BKE_collection_engine_property_value_get_float(props, "ssr_border_fade");
		effects->ssr_firefly_fac = BKE_collection_engine_property_value_get_float(props, "ssr_firefly_fac");
		effects->ssr_max_roughness = BKE_collection_engine_property_value_get_float(props, "ssr_max_roughness");

		if (effects->ssr_firefly_fac < 1e-8f) {
			effects->ssr_firefly_fac = FLT_MAX;
		}

		/* Important, can lead to breakage otherwise. */
		CLAMP(effects->ssr_ray_count, 1, 4);

		const int divisor = (effects->reflection_trace_full) ? 1 : 2;
		int tracing_res[2] = {(int)viewport_size[0] / divisor, (int)viewport_size[1] / divisor};
		const bool high_qual_input = true; /* TODO dither low quality input */

		/* MRT for the shading pass in order to output needed data for the SSR pass. */
		/* TODO create one texture layer per lobe */
		if (txl->ssr_specrough_input == NULL) {
			DRWTextureFormat specrough_format = (high_qual_input) ? DRW_TEX_RGBA_16 : DRW_TEX_RGBA_8;
			txl->ssr_specrough_input = DRW_texture_create_2D((int)viewport_size[0], (int)viewport_size[1], specrough_format, 0, NULL);
		}

		/* Reattach textures to the right buffer (because we are alternating between buffers) */
		/* TODO multiple FBO per texture!!!! */
		DRW_framebuffer_texture_detach(txl->ssr_specrough_input);
		DRW_framebuffer_texture_attach(fbl->main, txl->ssr_specrough_input, 2, 0);

		/* Raytracing output */
		/* TODO try integer format for hit coord to increase precision */
		DRWFboTexture tex_output[4] = {{&stl->g_data->ssr_hit_output[0], DRW_TEX_RGBA_16, DRW_TEX_TEMP},
		                               {&stl->g_data->ssr_hit_output[1], DRW_TEX_RGBA_16, DRW_TEX_TEMP},
		                               {&stl->g_data->ssr_hit_output[2], DRW_TEX_RGBA_16, DRW_TEX_TEMP},
		                               {&stl->g_data->ssr_hit_output[3], DRW_TEX_RGBA_16, DRW_TEX_TEMP}};

		DRW_framebuffer_init(&fbl->screen_tracing_fb, &draw_engine_eevee_type, tracing_res[0], tracing_res[1], tex_output, effects->ssr_ray_count);

		/* Enable double buffering to be able to read previous frame color */
		return EFFECT_SSR | EFFECT_NORMAL_BUFFER | EFFECT_DOUBLE_BUFFER | ((use_refraction) ? EFFECT_REFRACT : 0);
	}

	/* Cleanup to release memory */
	DRW_TEXTURE_FREE_SAFE(txl->ssr_specrough_input);
	DRW_FRAMEBUFFER_FREE_SAFE(fbl->screen_tracing_fb);
	for (int i = 0; i < 4; ++i) {
		stl->g_data->ssr_hit_output[i] = NULL;
	}

	return 0;
}

void EEVEE_screen_raytrace_cache_init(EEVEE_SceneLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects = stl->effects;

	struct Gwn_Batch *quad = DRW_cache_fullscreen_quad_get();

	if ((effects->enabled_effects & EFFECT_SSR) != 0) {
		int options = (effects->reflection_trace_full) ? SSR_FULL_TRACE : 0;
		options |= (effects->ssr_ray_count - 1);

		struct GPUShader *trace_shader = eevee_effects_screen_raytrace_shader_get(options);
		struct GPUShader *resolve_shader = eevee_effects_screen_raytrace_shader_get(SSR_RESOLVE | options);

		/** Screen space raytracing overview
		 *
		 * Following Frostbite stochastic SSR.
		 *
		 * - First pass Trace rays accross the depth buffer. The hit position and pdf are
		 *   recorded in a RGBA16F render target for each ray (sample).
		 *
		 * - We downsample the previous frame color buffer.
		 *
		 * - For each final pixel, we gather neighboors rays and choose a color buffer
		 *   mipmap for each ray using its pdf. (filtered importance sampling)
		 *   We then evaluate the lighting from the probes and mix the results together.
		 */
		psl->ssr_raytrace = DRW_pass_create("SSR Raytrace", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = DRW_shgroup_create(trace_shader, psl->ssr_raytrace);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_uniform_buffer(grp, "normalBuffer", &txl->ssr_normal_input);
		DRW_shgroup_uniform_buffer(grp, "specroughBuffer", &txl->ssr_specrough_input);
		DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
		DRW_shgroup_uniform_buffer(grp, "maxzBuffer", &txl->maxzbuffer);
		DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)stl->g_data->viewvecs, 2);
		DRW_shgroup_uniform_vec2(grp, "mipRatio[0]", (float *)stl->g_data->mip_ratio, 10);
		DRW_shgroup_uniform_vec4(grp, "ssrParameters", &effects->ssr_quality, 1);
		DRW_shgroup_uniform_int(grp, "planar_count", &sldata->probes->num_planar, 1);
		DRW_shgroup_uniform_float(grp, "maxRoughness", &effects->ssr_max_roughness, 1);
		DRW_shgroup_uniform_buffer(grp, "planarDepth", &vedata->txl->planar_depth);
		DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
		DRW_shgroup_call_add(grp, quad, NULL);

		psl->ssr_resolve = DRW_pass_create("SSR Resolve", DRW_STATE_WRITE_COLOR | DRW_STATE_ADDITIVE);
		grp = DRW_shgroup_create(resolve_shader, psl->ssr_resolve);
		DRW_shgroup_uniform_buffer(grp, "depthBuffer", &e_data.depth_src);
		DRW_shgroup_uniform_buffer(grp, "normalBuffer", &txl->ssr_normal_input);
		DRW_shgroup_uniform_buffer(grp, "specroughBuffer", &txl->ssr_specrough_input);
		DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
		DRW_shgroup_uniform_buffer(grp, "prevColorBuffer", &txl->color_double_buffer);
		DRW_shgroup_uniform_mat4(grp, "PastViewProjectionMatrix", (float *)stl->g_data->prev_persmat);
		DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)stl->g_data->viewvecs, 2);
		DRW_shgroup_uniform_int(grp, "planar_count", &sldata->probes->num_planar, 1);
		DRW_shgroup_uniform_int(grp, "probe_count", &sldata->probes->num_render_cube, 1);
		DRW_shgroup_uniform_vec2(grp, "mipRatio[0]", (float *)stl->g_data->mip_ratio, 10);
		DRW_shgroup_uniform_float(grp, "borderFadeFactor", &effects->ssr_border_fac, 1);
		DRW_shgroup_uniform_float(grp, "maxRoughness", &effects->ssr_max_roughness, 1);
		DRW_shgroup_uniform_float(grp, "lodCubeMax", &sldata->probes->lod_cube_max, 1);
		DRW_shgroup_uniform_float(grp, "lodPlanarMax", &sldata->probes->lod_planar_max, 1);
		DRW_shgroup_uniform_float(grp, "fireflyFactor", &effects->ssr_firefly_fac, 1);
		DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
		DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
		DRW_shgroup_uniform_buffer(grp, "probeCubes", &sldata->probe_pool);
		DRW_shgroup_uniform_buffer(grp, "probePlanars", &vedata->txl->planar_pool);
		DRW_shgroup_uniform_buffer(grp, "hitBuffer0", &stl->g_data->ssr_hit_output[0]);
		if (effects->ssr_ray_count > 1) {
			DRW_shgroup_uniform_buffer(grp, "hitBuffer1", &stl->g_data->ssr_hit_output[1]);
		}
		if (effects->ssr_ray_count > 2) {
			DRW_shgroup_uniform_buffer(grp, "hitBuffer2", &stl->g_data->ssr_hit_output[2]);
		}
		if (effects->ssr_ray_count > 3) {
			DRW_shgroup_uniform_buffer(grp, "hitBuffer3", &stl->g_data->ssr_hit_output[3]);
		}

		DRW_shgroup_uniform_vec4(grp, "aoParameters[0]", &effects->ao_dist, 2);
		if (effects->use_ao) {
			DRW_shgroup_uniform_buffer(grp, "horizonBuffer", &vedata->txl->gtao_horizons);
			DRW_shgroup_uniform_ivec2(grp, "aoHorizonTexSize", (int *)vedata->stl->effects->ao_texsize, 1);
		}
		else {
			/* Use shadow_pool as fallback to avoid sampling problem on certain platform, see: T52593 */
			DRW_shgroup_uniform_buffer(grp, "horizonBuffer", &sldata->shadow_pool);
		}

		DRW_shgroup_call_add(grp, quad, NULL);
	}
}

void EEVEE_refraction_compute(EEVEE_SceneLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if ((effects->enabled_effects & EFFECT_REFRACT) != 0) {
		DRW_framebuffer_texture_attach(fbl->refract_fb, txl->refract_color, 0, 0);
		DRW_framebuffer_blit(fbl->main, fbl->refract_fb, false);
		EEVEE_downsample_buffer(vedata, fbl->downsample_fb, txl->refract_color, 9);

		/* Restore */
		DRW_framebuffer_bind(fbl->main);
	}
}

void EEVEE_reflection_compute(EEVEE_SceneLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_EffectsInfo *effects = stl->effects;

	if (((effects->enabled_effects & EFFECT_SSR) != 0) && stl->g_data->valid_double_buffer) {
		DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
		e_data.depth_src = dtxl->depth;

		DRW_stats_group_start("SSR");

		for (int i = 0; i < effects->ssr_ray_count; ++i) {
			DRW_framebuffer_texture_attach(fbl->screen_tracing_fb, stl->g_data->ssr_hit_output[i], i, 0);
		}
		DRW_framebuffer_bind(fbl->screen_tracing_fb);

		/* Raytrace. */
		DRW_draw_pass(psl->ssr_raytrace);

		for (int i = 0; i < effects->ssr_ray_count; ++i) {
			DRW_framebuffer_texture_detach(stl->g_data->ssr_hit_output[i]);
		}

		EEVEE_downsample_buffer(vedata, fbl->downsample_fb, txl->color_double_buffer, 9);

		/* Resolve at fullres */
		DRW_framebuffer_texture_detach(dtxl->depth);
		DRW_framebuffer_texture_detach(txl->ssr_normal_input);
		DRW_framebuffer_texture_detach(txl->ssr_specrough_input);
		DRW_framebuffer_bind(fbl->main);
		DRW_draw_pass(psl->ssr_resolve);

		/* Restore */
		DRW_framebuffer_texture_attach(fbl->main, dtxl->depth, 0, 0);
		DRW_framebuffer_texture_attach(fbl->main, txl->ssr_normal_input, 1, 0);
		DRW_framebuffer_texture_attach(fbl->main, txl->ssr_specrough_input, 2, 0);
		DRW_framebuffer_bind(fbl->main);

		DRW_stats_group_end();
	}
}

void EEVEE_screen_raytrace_free(void)
{
	for (int i = 0; i < SSR_MAX_SHADER; ++i) {
		DRW_SHADER_FREE_SAFE(e_data.ssr_sh[i]);
	}
}
