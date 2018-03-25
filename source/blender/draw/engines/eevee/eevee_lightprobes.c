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

/** \file eevee_lightprobes.c
 *  \ingroup DNA
 */

#include "DRW_render.h"

#include "BLI_utildefines.h"
#include "BLI_string_utils.h"
#include "BLI_rand.h"

#include "DNA_world_types.h"
#include "DNA_texture_types.h"
#include "DNA_image_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_view3d_types.h"

#include "BKE_object.h"
#include "MEM_guardedalloc.h"

#include "GPU_material.h"
#include "GPU_texture.h"
#include "GPU_glew.h"

#include "eevee_engine.h"
#include "eevee_private.h"

#include "ED_screen.h"

/* Rounded to nearest PowerOfTwo */
#if defined(IRRADIANCE_SH_L2)
#define IRRADIANCE_SAMPLE_SIZE_X 4 /* 3 in reality */
#define IRRADIANCE_SAMPLE_SIZE_Y 4 /* 3 in reality */
#elif defined(IRRADIANCE_CUBEMAP)
#define IRRADIANCE_SAMPLE_SIZE_X 8
#define IRRADIANCE_SAMPLE_SIZE_Y 8
#elif defined(IRRADIANCE_HL2)
#define IRRADIANCE_SAMPLE_SIZE_X 4 /* 3 in reality */
#define IRRADIANCE_SAMPLE_SIZE_Y 2
#endif

#define IRRADIANCE_MAX_POOL_LAYER 256 /* OpenGL 3.3 core requirement, can be extended but it's already very big */
#define IRRADIANCE_MAX_POOL_SIZE 1024
#define MAX_IRRADIANCE_SAMPLES \
        (IRRADIANCE_MAX_POOL_SIZE / IRRADIANCE_SAMPLE_SIZE_X) * \
        (IRRADIANCE_MAX_POOL_SIZE / IRRADIANCE_SAMPLE_SIZE_Y)
#define HAMMERSLEY_SIZE 1024

static struct {
	struct GPUShader *probe_default_sh;
	struct GPUShader *probe_filter_glossy_sh;
	struct GPUShader *probe_filter_diffuse_sh;
	struct GPUShader *probe_filter_visibility_sh;
	struct GPUShader *probe_grid_fill_sh;
	struct GPUShader *probe_grid_display_sh;
	struct GPUShader *probe_planar_display_sh;
	struct GPUShader *probe_planar_downsample_sh;
	struct GPUShader *probe_cube_display_sh;

	struct GPUTexture *hammersley;
	struct GPUTexture *planar_pool_placeholder;
	struct GPUTexture *depth_placeholder;
	struct GPUTexture *depth_array_placeholder;
	struct GPUTexture *cube_face_minmaxz;

	struct Gwn_VertFormat *format_probe_display_cube;
	struct Gwn_VertFormat *format_probe_display_planar;
} e_data = {NULL}; /* Engine data */

extern char datatoc_background_vert_glsl[];
extern char datatoc_default_world_frag_glsl[];
extern char datatoc_lightprobe_filter_glossy_frag_glsl[];
extern char datatoc_lightprobe_filter_diffuse_frag_glsl[];
extern char datatoc_lightprobe_filter_visibility_frag_glsl[];
extern char datatoc_lightprobe_geom_glsl[];
extern char datatoc_lightprobe_vert_glsl[];
extern char datatoc_lightprobe_planar_display_frag_glsl[];
extern char datatoc_lightprobe_planar_display_vert_glsl[];
extern char datatoc_lightprobe_planar_downsample_frag_glsl[];
extern char datatoc_lightprobe_planar_downsample_geom_glsl[];
extern char datatoc_lightprobe_planar_downsample_vert_glsl[];
extern char datatoc_lightprobe_cube_display_frag_glsl[];
extern char datatoc_lightprobe_cube_display_vert_glsl[];
extern char datatoc_lightprobe_grid_display_frag_glsl[];
extern char datatoc_lightprobe_grid_display_vert_glsl[];
extern char datatoc_lightprobe_grid_fill_frag_glsl[];
extern char datatoc_irradiance_lib_glsl[];
extern char datatoc_lightprobe_lib_glsl[];
extern char datatoc_octahedron_lib_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];

extern GlobalsUboStorage ts;

/* *********** FUNCTIONS *********** */

static void irradiance_pool_size_get(int visibility_size, int total_samples, int r_size[3])
{
	/* Compute how many irradiance samples we can store per visibility sample. */
	int irr_per_vis = (visibility_size / IRRADIANCE_SAMPLE_SIZE_X) *
	                  (visibility_size / IRRADIANCE_SAMPLE_SIZE_Y);

	/* The irradiance itself take one layer, hence the +1 */
	int layer_ct = MIN2(irr_per_vis + 1, IRRADIANCE_MAX_POOL_LAYER);

	int texel_ct = (int)ceilf((float)total_samples / (float)(layer_ct - 1));
	r_size[0] = visibility_size * max_ii(1, min_ii(texel_ct, (IRRADIANCE_MAX_POOL_SIZE / visibility_size)));
	r_size[1] = visibility_size * max_ii(1, (texel_ct / (IRRADIANCE_MAX_POOL_SIZE / visibility_size)));
	r_size[2] = layer_ct;
}

static struct GPUTexture *create_hammersley_sample_texture(int samples)
{
	struct GPUTexture *tex;
	float (*texels)[2] = MEM_mallocN(sizeof(float[2]) * samples, "hammersley_tex");
	int i;

	for (i = 0; i < samples; i++) {
		double dphi;
		BLI_hammersley_1D(i, &dphi);
		float phi = (float)dphi * 2.0f * M_PI;
		texels[i][0] = cosf(phi);
		texels[i][1] = sinf(phi);
	}

	tex = DRW_texture_create_1D(samples, DRW_TEX_RG_16, DRW_TEX_WRAP, (float *)texels);
	MEM_freeN(texels);
	return tex;
}

static void planar_pool_ensure_alloc(EEVEE_Data *vedata, int num_planar_ref)
{
	/* XXX TODO OPTIMISATION : This is a complete waist of texture memory.
	 * Instead of allocating each planar probe for each viewport,
	 * only alloc them once using the biggest viewport resolution. */
	EEVEE_TextureList *txl = vedata->txl;

	const float *viewport_size = DRW_viewport_size_get();

	/* TODO get screen percentage from layer setting */
	// const DRWContextState *draw_ctx = DRW_context_state_get();
	// ViewLayer *view_layer = draw_ctx->view_layer;
	float screen_percentage = 1.0f;

	int width = (int)(viewport_size[0] * screen_percentage);
	int height = (int)(viewport_size[1] * screen_percentage);

	/* We need an Array texture so allocate it ourself */
	if (!txl->planar_pool) {
		if (num_planar_ref > 0) {
			txl->planar_pool = DRW_texture_create_2D_array(width, height, max_ff(1, num_planar_ref),
			                                                 DRW_TEX_RGB_11_11_10, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
			txl->planar_depth = DRW_texture_create_2D_array(width, height, max_ff(1, num_planar_ref),
			                                                DRW_TEX_DEPTH_24, 0, NULL);
		}
		else if (num_planar_ref == 0) {
			/* Makes Opengl Happy : Create a placeholder texture that will never be sampled but still bound to shader. */
			txl->planar_pool = DRW_texture_create_2D_array(1, 1, 1, DRW_TEX_RGBA_8, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
			txl->planar_depth = DRW_texture_create_2D_array(1, 1, 1, DRW_TEX_DEPTH_24, 0, NULL);
		}
	}
}

static void lightprobe_shaders_init(void)
{
	const char *filter_defines = "#define HAMMERSLEY_SIZE " STRINGIFY(HAMMERSLEY_SIZE) "\n"
#if defined(IRRADIANCE_SH_L2)
	                             "#define IRRADIANCE_SH_L2\n"
#elif defined(IRRADIANCE_CUBEMAP)
	                             "#define IRRADIANCE_CUBEMAP\n"
#elif defined(IRRADIANCE_HL2)
	                             "#define IRRADIANCE_HL2\n"
#endif
	                             "#define NOISE_SIZE 64\n";

	char *shader_str = NULL;
	char *vert_str = NULL;

	shader_str = BLI_string_joinN(
	        datatoc_common_view_lib_glsl,
	        datatoc_common_uniforms_lib_glsl,
	        datatoc_bsdf_common_lib_glsl,
	        datatoc_bsdf_sampling_lib_glsl,
	        datatoc_lightprobe_filter_glossy_frag_glsl);

	e_data.probe_filter_glossy_sh = DRW_shader_create(
	        datatoc_lightprobe_vert_glsl, datatoc_lightprobe_geom_glsl, shader_str, filter_defines);

	e_data.probe_default_sh = DRW_shader_create(
	        datatoc_background_vert_glsl, NULL, datatoc_default_world_frag_glsl, NULL);

	MEM_freeN(shader_str);

	shader_str = BLI_string_joinN(
	        datatoc_common_view_lib_glsl,
	        datatoc_common_uniforms_lib_glsl,
	        datatoc_bsdf_common_lib_glsl,
	        datatoc_bsdf_sampling_lib_glsl,
	        datatoc_lightprobe_filter_diffuse_frag_glsl);

	e_data.probe_filter_diffuse_sh = DRW_shader_create_fullscreen(shader_str, filter_defines);

	MEM_freeN(shader_str);

	shader_str = BLI_string_joinN(
	        datatoc_common_view_lib_glsl,
	        datatoc_common_uniforms_lib_glsl,
	        datatoc_bsdf_common_lib_glsl,
	        datatoc_bsdf_sampling_lib_glsl,
	        datatoc_lightprobe_filter_visibility_frag_glsl);

	e_data.probe_filter_visibility_sh = DRW_shader_create_fullscreen(shader_str, filter_defines);

	MEM_freeN(shader_str);

	shader_str = BLI_string_joinN(
	        datatoc_octahedron_lib_glsl,
	        datatoc_common_view_lib_glsl,
	        datatoc_common_uniforms_lib_glsl,
	        datatoc_bsdf_common_lib_glsl,
	        datatoc_irradiance_lib_glsl,
	        datatoc_lightprobe_lib_glsl,
	        datatoc_lightprobe_grid_display_frag_glsl);

	vert_str = BLI_string_joinN(
	        datatoc_common_view_lib_glsl,
	        datatoc_lightprobe_grid_display_vert_glsl);

	e_data.probe_grid_display_sh = DRW_shader_create(vert_str, NULL, shader_str, filter_defines);

	MEM_freeN(vert_str);
	MEM_freeN(shader_str);

	e_data.probe_grid_fill_sh = DRW_shader_create_fullscreen(
	        datatoc_lightprobe_grid_fill_frag_glsl, filter_defines);

	shader_str = BLI_string_joinN(
	        datatoc_octahedron_lib_glsl,
	        datatoc_common_view_lib_glsl,
	        datatoc_common_uniforms_lib_glsl,
	        datatoc_bsdf_common_lib_glsl,
	        datatoc_lightprobe_lib_glsl,
	        datatoc_lightprobe_cube_display_frag_glsl);

	vert_str = BLI_string_joinN(
	        datatoc_common_view_lib_glsl,
	        datatoc_lightprobe_cube_display_vert_glsl);

	e_data.probe_cube_display_sh = DRW_shader_create(vert_str, NULL, shader_str, NULL);

	MEM_freeN(vert_str);
	MEM_freeN(shader_str);

	vert_str = BLI_string_joinN(
	        datatoc_common_view_lib_glsl,
	        datatoc_lightprobe_planar_display_vert_glsl);

	shader_str = BLI_string_joinN(
	        datatoc_common_view_lib_glsl,
	        datatoc_lightprobe_planar_display_frag_glsl);

	e_data.probe_planar_display_sh = DRW_shader_create(vert_str, NULL, shader_str, NULL);

	MEM_freeN(vert_str);
	MEM_freeN(shader_str);

	e_data.probe_planar_downsample_sh = DRW_shader_create(
	        datatoc_lightprobe_planar_downsample_vert_glsl,
	        datatoc_lightprobe_planar_downsample_geom_glsl,
	        datatoc_lightprobe_planar_downsample_frag_glsl,
	        NULL);

	e_data.hammersley = create_hammersley_sample_texture(HAMMERSLEY_SIZE);
}

void EEVEE_lightprobes_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *UNUSED(vedata))
{
	EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
	bool update_all = false;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	IDProperty *props = BKE_view_layer_engine_evaluated_get(view_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_EEVEE);

	/* Shaders */
	if (!e_data.probe_filter_glossy_sh) {
		lightprobe_shaders_init();
	}

	if (!sldata->probes) {
		sldata->probes = MEM_callocN(sizeof(EEVEE_LightProbesInfo), "EEVEE_LightProbesInfo");
		sldata->probes->grid_initialized = false;
		sldata->probe_ubo = DRW_uniformbuffer_create(sizeof(EEVEE_LightProbe) * MAX_PROBE, NULL);
		sldata->grid_ubo = DRW_uniformbuffer_create(sizeof(EEVEE_LightGrid) * MAX_GRID, NULL);
		sldata->planar_ubo = DRW_uniformbuffer_create(sizeof(EEVEE_PlanarReflection) * MAX_PLANAR, NULL);
	}

	/* Only start doing probes if all materials have finished compiling. */
	sldata->probes->all_materials_updated = true;

	common_data->spec_toggle = true;
	common_data->ssr_toggle = true;
	common_data->sss_toggle = true;

	int prop_bounce_num = BKE_collection_engine_property_value_get_int(props, "gi_diffuse_bounces");
	if (sldata->probes->num_bounce != prop_bounce_num) {
		sldata->probes->num_bounce = prop_bounce_num;
		update_all = true;
	}

	int prop_cubemap_res = BKE_collection_engine_property_value_get_int(props, "gi_cubemap_resolution");
	if (sldata->probes->cubemap_res != prop_cubemap_res) {
		sldata->probes->cubemap_res = prop_cubemap_res;
		update_all = true;

		sldata->probes->target_size = prop_cubemap_res >> 1;

		DRW_TEXTURE_FREE_SAFE(sldata->probe_depth_rt);
		DRW_TEXTURE_FREE_SAFE(sldata->probe_rt);
		DRW_TEXTURE_FREE_SAFE(sldata->probe_pool);
	}

	int visibility_res = BKE_collection_engine_property_value_get_int(props, "gi_visibility_resolution");
	if (common_data->prb_irradiance_vis_size != visibility_res) {
		common_data->prb_irradiance_vis_size = visibility_res;
		update_all = true;
	}

	if (update_all) {
		sldata->probes->update_world |= PROBE_UPDATE_ALL;
		sldata->probes->updated_bounce = 0;
		sldata->probes->grid_initialized = false;
	}

	/* Setup Render Target Cubemap */
	if (!sldata->probe_rt) {
		sldata->probe_depth_rt = DRW_texture_create_cube(sldata->probes->target_size, DRW_TEX_DEPTH_24, 0, NULL);
		sldata->probe_rt = DRW_texture_create_cube(sldata->probes->target_size, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
	}

	for (int i = 0; i < 6; ++i) {
		GPU_framebuffer_ensure_config(&sldata->probe_face_fb[i], {
			GPU_ATTACHMENT_TEXTURE_CUBEFACE(sldata->probe_depth_rt, i),
			GPU_ATTACHMENT_TEXTURE_CUBEFACE(sldata->probe_rt, i)
		});
	}

	/* Placeholder planar pool: used when rendering planar reflections (avoid dependency loop). */
	if (!e_data.planar_pool_placeholder) {
		e_data.planar_pool_placeholder = DRW_texture_create_2D_array(1, 1, 1, DRW_TEX_RGBA_8, DRW_TEX_FILTER, NULL);
	}

	if (!e_data.depth_placeholder) {
		e_data.depth_placeholder = DRW_texture_create_2D(1, 1, DRW_TEX_DEPTH_24, 0, NULL);
	}
	if (!e_data.depth_array_placeholder) {
		e_data.depth_array_placeholder = DRW_texture_create_2D_array(1, 1, 1, DRW_TEX_DEPTH_24, 0, NULL);
	}
}

void EEVEE_lightprobes_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_LightProbesInfo *pinfo = sldata->probes;

	pinfo->do_cube_update = false;
	pinfo->num_cube = 1; /* at least one for the world */
	pinfo->num_grid = 1;
	pinfo->num_planar = 0;
	pinfo->total_irradiance_samples = 1;
	memset(pinfo->probes_cube_ref, 0, sizeof(pinfo->probes_cube_ref));
	memset(pinfo->probes_grid_ref, 0, sizeof(pinfo->probes_grid_ref));
	memset(pinfo->probes_planar_ref, 0, sizeof(pinfo->probes_planar_ref));

	{
		psl->probe_background = DRW_pass_create("World Probe Background Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);

		struct Gwn_Batch *geom = DRW_cache_fullscreen_quad_get();
		DRWShadingGroup *grp = NULL;

		const DRWContextState *draw_ctx = DRW_context_state_get();
		Scene *scene = draw_ctx->scene;
		World *wo = scene->world;

		float *col = ts.colorBackground;
		if (wo) {
			col = &wo->horr;
			bool wo_sh_compiled = true;

			if (wo->use_nodes && wo->nodetree) {
				static float error_col[3] = {1.0f, 0.0f, 1.0f};
				static float compile_col[3] = {0.5f, 0.5f, 0.5f};
				struct GPUMaterial *gpumat = EEVEE_material_world_lightprobe_get(scene, wo);

				GPUMaterialStatus status = GPU_material_status(gpumat);

				switch (status) {
					case GPU_MAT_SUCCESS:
						grp = DRW_shgroup_material_create(gpumat, psl->probe_background);
						DRW_shgroup_uniform_float(grp, "backgroundAlpha", &stl->g_data->background_alpha, 1);
						DRW_shgroup_call_add(grp, geom, NULL);
						wo_sh_compiled = true;
						break;
					case GPU_MAT_QUEUED:
						pinfo->all_materials_updated = false;
						wo_sh_compiled = false;
						/* TODO Bypass probe compilation. */
						col = compile_col;
						break;
					case GPU_MAT_FAILED:
					default:
						wo_sh_compiled = true;
						col = error_col;
						break;
				}
			}

			if (wo->update_flag != 0 || pinfo->prev_world != wo || pinfo->prev_wo_sh_compiled != wo_sh_compiled) {
				pinfo->update_world |= PROBE_UPDATE_ALL;
				pinfo->prev_wo_sh_compiled = wo_sh_compiled;
				pinfo->prev_world = wo;
			}
			wo->update_flag = 0;
		}
		else if (pinfo->prev_world) {
			pinfo->update_world |= PROBE_UPDATE_ALL;
			pinfo->prev_wo_sh_compiled = false;
			pinfo->prev_world = NULL;
		}

		/* Fallback if shader fails or if not using nodetree. */
		if (grp == NULL) {
			grp = DRW_shgroup_create(e_data.probe_default_sh, psl->probe_background);
			DRW_shgroup_uniform_vec3(grp, "color", col, 1);
			DRW_shgroup_uniform_float(grp, "backgroundAlpha", &stl->g_data->background_alpha, 1);
			DRW_shgroup_call_add(grp, geom, NULL);
		}
	}

	{
		psl->probe_glossy_compute = DRW_pass_create("LightProbe Glossy Compute", DRW_STATE_WRITE_COLOR);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.probe_filter_glossy_sh, psl->probe_glossy_compute);
		DRW_shgroup_uniform_float(grp, "intensityFac", &pinfo->intensity_fac, 1);
		DRW_shgroup_uniform_float(grp, "sampleCount", &pinfo->samples_ct, 1);
		DRW_shgroup_uniform_float(grp, "invSampleCount", &pinfo->invsamples_ct, 1);
		DRW_shgroup_uniform_float(grp, "roughnessSquared", &pinfo->roughness, 1);
		DRW_shgroup_uniform_float(grp, "lodFactor", &pinfo->lodfactor, 1);
		DRW_shgroup_uniform_float(grp, "lodMax", &pinfo->lod_rt_max, 1);
		DRW_shgroup_uniform_float(grp, "texelSize", &pinfo->texel_size, 1);
		DRW_shgroup_uniform_float(grp, "paddingSize", &pinfo->padding_size, 1);
		DRW_shgroup_uniform_int(grp, "Layer", &pinfo->layer, 1);
		DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
		// DRW_shgroup_uniform_texture(grp, "texJitter", e_data.jitter);
		DRW_shgroup_uniform_texture(grp, "probeHdr", sldata->probe_rt);
		DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);
	}

	{
		psl->probe_diffuse_compute = DRW_pass_create("LightProbe Diffuse Compute", DRW_STATE_WRITE_COLOR);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.probe_filter_diffuse_sh, psl->probe_diffuse_compute);
#ifdef IRRADIANCE_SH_L2
		DRW_shgroup_uniform_int(grp, "probeSize", &pinfo->shres, 1);
#else
		DRW_shgroup_uniform_float(grp, "sampleCount", &pinfo->samples_ct, 1);
		DRW_shgroup_uniform_float(grp, "invSampleCount", &pinfo->invsamples_ct, 1);
		DRW_shgroup_uniform_float(grp, "lodFactor", &pinfo->lodfactor, 1);
		DRW_shgroup_uniform_float(grp, "lodMax", &pinfo->lod_rt_max, 1);
		DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
#endif
		DRW_shgroup_uniform_float(grp, "intensityFac", &pinfo->intensity_fac, 1);
		DRW_shgroup_uniform_texture(grp, "probeHdr", sldata->probe_rt);

		struct Gwn_Batch *geom = DRW_cache_fullscreen_quad_get();
		DRW_shgroup_call_add(grp, geom, NULL);
	}

	{
		psl->probe_visibility_compute = DRW_pass_create("LightProbe Visibility Compute", DRW_STATE_WRITE_COLOR);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.probe_filter_visibility_sh, psl->probe_visibility_compute);
		DRW_shgroup_uniform_int(grp, "outputSize", &pinfo->shres, 1);
		DRW_shgroup_uniform_float(grp, "visibilityRange", &pinfo->visibility_range, 1);
		DRW_shgroup_uniform_float(grp, "visibilityBlur", &pinfo->visibility_blur, 1);
		DRW_shgroup_uniform_float(grp, "sampleCount", &pinfo->samples_ct, 1);
		DRW_shgroup_uniform_float(grp, "invSampleCount", &pinfo->invsamples_ct, 1);
		DRW_shgroup_uniform_float(grp, "storedTexelSize", &pinfo->texel_size, 1);
		DRW_shgroup_uniform_float(grp, "nearClip", &pinfo->near_clip, 1);
		DRW_shgroup_uniform_float(grp, "farClip", &pinfo->far_clip, 1);
		DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
		DRW_shgroup_uniform_texture(grp, "probeDepth", sldata->probe_depth_rt);

		struct Gwn_Batch *geom = DRW_cache_fullscreen_quad_get();
		DRW_shgroup_call_add(grp, geom, NULL);
	}

	{
		psl->probe_grid_fill = DRW_pass_create("LightProbe Grid Floodfill", DRW_STATE_WRITE_COLOR);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.probe_grid_fill_sh, psl->probe_grid_fill);
		DRW_shgroup_uniform_texture_ref(grp, "irradianceGrid", &sldata->irradiance_pool);

		struct Gwn_Batch *geom = DRW_cache_fullscreen_quad_get();
		DRW_shgroup_call_add(grp, geom, NULL);
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_CULL_BACK;
		psl->probe_display = DRW_pass_create("LightProbe Display", state);

		DRW_shgroup_instance_format(e_data.format_probe_display_cube, {
		    {"probe_id",       DRW_ATTRIB_INT, 1},
		    {"probe_location", DRW_ATTRIB_FLOAT, 3},
		    {"sphere_size",    DRW_ATTRIB_FLOAT, 1},
		});

		DRWShadingGroup *grp = DRW_shgroup_instance_create(
		        e_data.probe_cube_display_sh,
		        psl->probe_display,
		        DRW_cache_sphere_get(),
		        e_data.format_probe_display_cube);
		stl->g_data->cube_display_shgrp = grp;
		DRW_shgroup_uniform_texture_ref(grp, "probeCubes", &sldata->probe_pool);
		DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);

		DRW_shgroup_instance_format(e_data.format_probe_display_planar, {
		    {"probe_id", DRW_ATTRIB_INT, 1},
		    {"probe_mat", DRW_ATTRIB_FLOAT, 16},
		});

		grp = DRW_shgroup_instance_create(
		        e_data.probe_planar_display_sh,
		        psl->probe_display,
		        DRW_cache_quad_get(),
		        e_data.format_probe_display_planar);
		stl->g_data->planar_display_shgrp = grp;
		DRW_shgroup_uniform_texture_ref(grp, "probePlanars", &txl->planar_pool);
	}

	{
		psl->probe_planar_downsample_ps = DRW_pass_create("LightProbe Planar Downsample", DRW_STATE_WRITE_COLOR);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.probe_planar_downsample_sh, psl->probe_planar_downsample_ps);
		DRW_shgroup_uniform_texture_ref(grp, "source", &txl->planar_pool);
		DRW_shgroup_uniform_float(grp, "fireflyFactor", &sldata->common_data.ssr_firefly_fac, 1);
		DRW_shgroup_call_instances_add(grp, DRW_cache_fullscreen_quad_get(), NULL, (unsigned int *)&pinfo->num_planar);
	}
}

void EEVEE_lightprobes_cache_add(EEVEE_ViewLayerData *sldata, Object *ob)
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	LightProbe *probe = (LightProbe *)ob->data;

	if ((probe->type == LIGHTPROBE_TYPE_CUBE && pinfo->num_cube >= MAX_PROBE) ||
	    (probe->type == LIGHTPROBE_TYPE_GRID && pinfo->num_grid >= MAX_PROBE) ||
	    (probe->type == LIGHTPROBE_TYPE_PLANAR && pinfo->num_grid >= MAX_PLANAR))
	{
		printf("Too much probes in the scene !!!\n");
		return;
	}

	if (probe->type == LIGHTPROBE_TYPE_PLANAR) {
		/* See if this planar probe is inside the view frustum. If not, no need to update it. */
		/* NOTE: this could be bypassed if we want feedback loop mirrors for rendering. */
		BoundBox bbox; float tmp[4][4];
		const float min[3] = {-1.0f, -1.0f, -1.0f};
		const float max[3] = { 1.0f,  1.0f,  1.0f};
		BKE_boundbox_init_from_minmax(&bbox, min, max);

		copy_m4_m4(tmp, ob->obmat);
		normalize_v3(tmp[2]);
		mul_v3_fl(tmp[2], probe->distinf);

		for (int v = 0; v < 8; ++v) {
			mul_m4_v3(tmp, bbox.vec[v]);
		}
		if (!DRW_culling_box_test(&bbox)) {
			return; /* Culled */
		}
	}

	EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_ensure(ob);

	ped->num_cell = probe->grid_resolution_x * probe->grid_resolution_y * probe->grid_resolution_z;

	if ((probe->type == LIGHTPROBE_TYPE_GRID) &&
	    ((pinfo->total_irradiance_samples + ped->num_cell) >= MAX_IRRADIANCE_SAMPLES))
	{
		printf("Too much grid samples !!!\n");
		return;
	}

	if (ped->need_full_update) {
		ped->need_full_update = false;

		ped->need_update = true;
		ped->probe_id = 0;
		if (probe->type == LIGHTPROBE_TYPE_GRID) {
			ped->updated_cells = 0;
			ped->updated_lvl = 0;
			pinfo->updated_bounce = 0;
			pinfo->grid_initialized = false;
		}
	}

	if (pinfo->update_world) {
		ped->need_update = true;
		ped->updated_cells = 0;
		ped->updated_lvl = 0;
		ped->probe_id = 0;
	}

	pinfo->do_cube_update |= ped->need_update;

	switch (probe->type) {
		case LIGHTPROBE_TYPE_CUBE:
			pinfo->probes_cube_ref[pinfo->num_cube] = ob;
			pinfo->num_cube++;
			break;
		case LIGHTPROBE_TYPE_PLANAR:
			pinfo->probes_planar_ref[pinfo->num_planar] = ob;
			pinfo->num_planar++;
			break;
		case LIGHTPROBE_TYPE_GRID:
			pinfo->probes_grid_ref[pinfo->num_grid] = ob;
			pinfo->num_grid++;
			pinfo->total_irradiance_samples += ped->num_cell;
			break;
	}
}

/* TODO find a nice name to push it to math_matrix.c */
static void scale_m4_v3(float R[4][4], float v[3])
{
	for (int i = 0; i < 4; ++i)
		mul_v3_v3(R[i], v);
}

static void EEVEE_planar_reflections_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_StorageList *stl)
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	Object *ob;

	for (int i = 0; (ob = pinfo->probes_planar_ref[i]) && (i < MAX_PLANAR); i++) {
		LightProbe *probe = (LightProbe *)ob->data;
		EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_ensure(ob);

		ped->probe_id = i;

		/* Debug Display */
		if (DRW_state_draw_support() &&
		    (probe->flag & LIGHTPROBE_FLAG_SHOW_DATA))
		{
			DRW_shgroup_call_dynamic_add(stl->g_data->planar_display_shgrp, &ped->probe_id, ob->obmat);
		}
	}
}

static void EEVEE_planar_reflections_updates(EEVEE_ViewLayerData *sldata)
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	Object *ob;
	float mtx[4][4], normat[4][4], imat[4][4], rangemat[4][4];

	float viewmat[4][4];
	DRW_viewport_matrix_get(viewmat, DRW_MAT_VIEW);

	zero_m4(rangemat);
	rangemat[0][0] = rangemat[1][1] = rangemat[2][2] = 0.5f;
	rangemat[3][0] = rangemat[3][1] = rangemat[3][2] = 0.5f;
	rangemat[3][3] = 1.0f;

	/* PLANAR REFLECTION */
	for (int i = 0; (ob = pinfo->probes_planar_ref[i]) && (i < MAX_PLANAR); i++) {
		LightProbe *probe = (LightProbe *)ob->data;
		EEVEE_PlanarReflection *eplanar = &pinfo->planar_data[i];
		EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_ensure(ob);
		/* Computing mtx : matrix that mirror position around object's XY plane. */
		normalize_m4_m4(normat, ob->obmat);  /* object > world */
		invert_m4_m4(imat, normat); /* world > object */
		float reflect[3] = {1.0f, 1.0f, -1.0f}; /* XY reflection plane */
		scale_m4_v3(imat, reflect); /* world > object > mirrored obj */
		mul_m4_m4m4(mtx, normat, imat); /* world > object > mirrored obj > world */
		/* Reflect Camera Matrix. */
		mul_m4_m4m4(ped->mats.mat[DRW_MAT_VIEW], viewmat, mtx);
		/* TODO FOV margin */
		/* Temporal sampling jitter should be already applied to the DRW_MAT_WIN. */
		DRW_viewport_matrix_get(ped->mats.mat[DRW_MAT_WIN], DRW_MAT_WIN);
		/* Apply Projection Matrix. */
		mul_m4_m4m4(ped->mats.mat[DRW_MAT_PERS], ped->mats.mat[DRW_MAT_WIN], ped->mats.mat[DRW_MAT_VIEW]);
		/* This is the matrix used to reconstruct texture coordinates.
		 * We use the original view matrix because it does not create
		 * visual artifacts if receiver is not perfectly aligned with
		 * the planar reflection probe. */
		mul_m4_m4m4(eplanar->reflectionmat, ped->mats.mat[DRW_MAT_WIN], viewmat); /* TODO FOV margin */
		/* Convert from [-1, 1] to [0, 1] (NDC to Texture coord). */
		mul_m4_m4m4(eplanar->reflectionmat, rangemat, eplanar->reflectionmat);

		/* Compute clip plane equation / normal. */
		float refpoint[3];
		copy_v3_v3(eplanar->plane_equation, ob->obmat[2]);
		normalize_v3(eplanar->plane_equation); /* plane normal */
		eplanar->plane_equation[3] = -dot_v3v3(eplanar->plane_equation, ob->obmat[3]);

		/* Compute offset plane equation (fix missing texels near reflection plane). */
		copy_v3_v3(ped->planer_eq_offset, eplanar->plane_equation);
		mul_v3_v3fl(refpoint, eplanar->plane_equation, -probe->clipsta);
		add_v3_v3(refpoint, ob->obmat[3]);
		ped->planer_eq_offset[3] = -dot_v3v3(eplanar->plane_equation, refpoint);

		/* Compute XY clip planes. */
		normalize_v3_v3(eplanar->clip_vec_x, ob->obmat[0]);
		normalize_v3_v3(eplanar->clip_vec_y, ob->obmat[1]);

		float vec[3] = {0.0f, 0.0f, 0.0f};
		vec[0] = 1.0f; vec[1] = 0.0f; vec[2] = 0.0f;
		mul_m4_v3(ob->obmat, vec); /* Point on the edge */
		eplanar->clip_edge_x_pos = dot_v3v3(eplanar->clip_vec_x, vec);

		vec[0] = 0.0f; vec[1] = 1.0f; vec[2] = 0.0f;
		mul_m4_v3(ob->obmat, vec); /* Point on the edge */
		eplanar->clip_edge_y_pos = dot_v3v3(eplanar->clip_vec_y, vec);

		vec[0] = -1.0f; vec[1] = 0.0f; vec[2] = 0.0f;
		mul_m4_v3(ob->obmat, vec); /* Point on the edge */
		eplanar->clip_edge_x_neg = dot_v3v3(eplanar->clip_vec_x, vec);

		vec[0] = 0.0f; vec[1] = -1.0f; vec[2] = 0.0f;
		mul_m4_v3(ob->obmat, vec); /* Point on the edge */
		eplanar->clip_edge_y_neg = dot_v3v3(eplanar->clip_vec_y, vec);

		/* Facing factors */
		float max_angle = max_ff(1e-2f, probe->falloff) * M_PI * 0.5f;
		float min_angle = 0.0f;
		eplanar->facing_scale = 1.0f / max_ff(1e-8f, cosf(min_angle) - cosf(max_angle));
		eplanar->facing_bias = -min_ff(1.0f - 1e-8f, cosf(max_angle)) * eplanar->facing_scale;

		/* Distance factors */
		float max_dist = probe->distinf;
		float min_dist = min_ff(1.0f - 1e-8f, 1.0f - probe->falloff) * probe->distinf;
		eplanar->attenuation_scale = -1.0f / max_ff(1e-8f, max_dist - min_dist);
		eplanar->attenuation_bias = max_dist * -eplanar->attenuation_scale;
	}
}

static void EEVEE_lightprobes_updates(EEVEE_ViewLayerData *sldata, EEVEE_PassList *psl, EEVEE_StorageList *stl)
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	Object *ob;

	/* CUBE REFLECTION */
	for (int i = 1; (ob = pinfo->probes_cube_ref[i]) && (i < MAX_PROBE); i++) {
		LightProbe *probe = (LightProbe *)ob->data;
		EEVEE_LightProbe *eprobe = &pinfo->probe_data[i];
		EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_ensure(ob);

		/* Update transforms */
		copy_v3_v3(eprobe->position, ob->obmat[3]);

		/* Attenuation */
		eprobe->attenuation_type = probe->attenuation_type;
		eprobe->attenuation_fac = 1.0f / max_ff(1e-8f, probe->falloff);

		unit_m4(eprobe->attenuationmat);
		scale_m4_fl(eprobe->attenuationmat, probe->distinf);
		mul_m4_m4m4(eprobe->attenuationmat, ob->obmat, eprobe->attenuationmat);
		invert_m4(eprobe->attenuationmat);

		/* Parallax */
		float dist;
		if ((probe->flag & LIGHTPROBE_FLAG_CUSTOM_PARALLAX) != 0) {
			eprobe->parallax_type = probe->parallax_type;
			dist = probe->distpar;
		}
		else {
			eprobe->parallax_type = probe->attenuation_type;
			dist = probe->distinf;
		}

		unit_m4(eprobe->parallaxmat);
		scale_m4_fl(eprobe->parallaxmat, dist);
		mul_m4_m4m4(eprobe->parallaxmat, ob->obmat, eprobe->parallaxmat);
		invert_m4(eprobe->parallaxmat);

		/* Debug Display */
		if (DRW_state_draw_support() &&
		    (probe->flag & LIGHTPROBE_FLAG_SHOW_DATA))
		{
			ped->probe_size = probe->data_draw_size * 0.1f;
			DRW_shgroup_call_dynamic_add(
			            stl->g_data->cube_display_shgrp, &ped->probe_id, ob->obmat[3], &ped->probe_size);
		}
	}

	/* IRRADIANCE GRID */
	int offset = 1; /* to account for the world probe */
	for (int i = 1; (ob = pinfo->probes_grid_ref[i]) && (i < MAX_GRID); i++) {
		LightProbe *probe = (LightProbe *)ob->data;
		EEVEE_LightGrid *egrid = &pinfo->grid_data[i];
		EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_ensure(ob);

		/* If one grid has move we need to recompute all the lighting. */
		if (!pinfo->grid_initialized) {
			ped->updated_cells = 0;
			ped->updated_lvl = 0;
			ped->need_update = true;
		}

		/* Add one for level 0 */
		ped->max_lvl = 1.0f + floorf(log2f((float)MAX3(probe->grid_resolution_x,
		                                               probe->grid_resolution_y,
		                                               probe->grid_resolution_z)));

		egrid->offset = offset;
		float fac = 1.0f / max_ff(1e-8f, probe->falloff);
		egrid->attenuation_scale = fac / max_ff(1e-8f, probe->distinf);
		egrid->attenuation_bias = fac;

		/* Set offset for the next grid */
		offset += ped->num_cell;

		/* Update transforms */
		float cell_dim[3], half_cell_dim[3];
		cell_dim[0] = 2.0f / (float)(probe->grid_resolution_x);
		cell_dim[1] = 2.0f / (float)(probe->grid_resolution_y);
		cell_dim[2] = 2.0f / (float)(probe->grid_resolution_z);

		mul_v3_v3fl(half_cell_dim, cell_dim, 0.5f);

		/* Matrix converting world space to cell ranges. */
		invert_m4_m4(egrid->mat, ob->obmat);

		/* First cell. */
		copy_v3_fl(egrid->corner, -1.0f);
		add_v3_v3(egrid->corner, half_cell_dim);
		mul_m4_v3(ob->obmat, egrid->corner);

		/* Opposite neighbor cell. */
		copy_v3_fl3(egrid->increment_x, cell_dim[0], 0.0f, 0.0f);
		add_v3_v3(egrid->increment_x, half_cell_dim);
		add_v3_fl(egrid->increment_x, -1.0f);
		mul_m4_v3(ob->obmat, egrid->increment_x);
		sub_v3_v3(egrid->increment_x, egrid->corner);

		copy_v3_fl3(egrid->increment_y, 0.0f, cell_dim[1], 0.0f);
		add_v3_v3(egrid->increment_y, half_cell_dim);
		add_v3_fl(egrid->increment_y, -1.0f);
		mul_m4_v3(ob->obmat, egrid->increment_y);
		sub_v3_v3(egrid->increment_y, egrid->corner);

		copy_v3_fl3(egrid->increment_z, 0.0f, 0.0f, cell_dim[2]);
		add_v3_v3(egrid->increment_z, half_cell_dim);
		add_v3_fl(egrid->increment_z, -1.0f);
		mul_m4_v3(ob->obmat, egrid->increment_z);
		sub_v3_v3(egrid->increment_z, egrid->corner);

		copy_v3_v3_int(egrid->resolution, &probe->grid_resolution_x);

		/* Visibility bias */
		egrid->visibility_bias = 0.05f * probe->vis_bias;
		egrid->visibility_bleed = probe->vis_bleedbias;
		egrid->visibility_range = (
		        sqrtf(max_fff(len_squared_v3(egrid->increment_x),
		                      len_squared_v3(egrid->increment_y),
		                      len_squared_v3(egrid->increment_z))) + 1.0f);

		/* Debug Display */
		if (DRW_state_draw_support() &&
		    (probe->flag & LIGHTPROBE_FLAG_SHOW_DATA))
		{
			DRWShadingGroup *grp = DRW_shgroup_create(e_data.probe_grid_display_sh, psl->probe_display);
			DRW_shgroup_uniform_int(grp, "offset", &egrid->offset, 1);
			DRW_shgroup_uniform_ivec3(grp, "grid_resolution", egrid->resolution, 1);
			DRW_shgroup_uniform_vec3(grp, "corner", egrid->corner, 1);
			DRW_shgroup_uniform_vec3(grp, "increment_x", egrid->increment_x, 1);
			DRW_shgroup_uniform_vec3(grp, "increment_y", egrid->increment_y, 1);
			DRW_shgroup_uniform_vec3(grp, "increment_z", egrid->increment_z, 1);
			DRW_shgroup_uniform_texture_ref(grp, "irradianceGrid", &sldata->irradiance_pool);
			DRW_shgroup_uniform_float(grp, "sphere_size", &probe->data_draw_size, 1);
			DRW_shgroup_call_instances_add(grp, DRW_cache_sphere_get(), NULL, (unsigned int *)&ped->num_cell);
		}
	}
}

void EEVEE_lightprobes_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	Object *ob;

	/* Setup enough layers. */
	/* Free textures if number mismatch. */
	if (pinfo->num_cube != pinfo->cache_num_cube) {
		DRW_TEXTURE_FREE_SAFE(sldata->probe_pool);
		pinfo->cache_num_cube = pinfo->num_cube;
	}

	if (pinfo->num_planar != pinfo->cache_num_planar) {
		DRW_TEXTURE_FREE_SAFE(vedata->txl->planar_pool);
		DRW_TEXTURE_FREE_SAFE(vedata->txl->planar_depth);
		pinfo->cache_num_planar = pinfo->num_planar;
	}

	int irr_size[3];
	irradiance_pool_size_get(common_data->prb_irradiance_vis_size, pinfo->total_irradiance_samples, irr_size);

	if ((irr_size[0] != pinfo->cache_irradiance_size[0]) ||
	    (irr_size[1] != pinfo->cache_irradiance_size[1]) ||
	    (irr_size[2] != pinfo->cache_irradiance_size[2]))
	{
		DRW_TEXTURE_FREE_SAFE(sldata->irradiance_pool);
		DRW_TEXTURE_FREE_SAFE(sldata->irradiance_rt);
		copy_v3_v3_int(pinfo->cache_irradiance_size, irr_size);
	}

	/* XXX this should be run each frame as it ensure planar_depth is set */
	planar_pool_ensure_alloc(vedata, pinfo->num_planar);

	if (!sldata->probe_pool) {
		sldata->probe_pool = DRW_texture_create_2D_array(pinfo->cubemap_res, pinfo->cubemap_res, max_ff(1, pinfo->num_cube),
		                                                 DRW_TEX_RGB_11_11_10, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
		if (sldata->probe_filter_fb) {
			GPU_framebuffer_texture_attach(sldata->probe_filter_fb, sldata->probe_pool, 0, 0);
		}
		/* Tag probes to refresh */
		pinfo->update_world |= PROBE_UPDATE_CUBE;
	}

	if ((pinfo->update_world & PROBE_UPDATE_CUBE) != 0) {
		common_data->prb_num_render_cube = 0;
		for (int i = 1; (ob = pinfo->probes_cube_ref[i]) && (i < MAX_PROBE); i++) {
			EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_ensure(ob);
			ped->need_update = true;
			ped->ready_to_shade = false;
			ped->probe_id = 0;
		}
	}

#ifdef IRRADIANCE_SH_L2
	/* we need a signed format for Spherical Harmonics */
	int irradiance_format = DRW_TEX_RGBA_16;
#else
	int irradiance_format = DRW_TEX_RGBA_8;
#endif

	if (!sldata->irradiance_pool || !sldata->irradiance_rt) {
		if (!sldata->irradiance_pool) {
			sldata->irradiance_pool = DRW_texture_create_2D_array(irr_size[0], irr_size[1], irr_size[2],
			                                                      irradiance_format, DRW_TEX_FILTER, NULL);
		}
		if (!sldata->irradiance_rt) {
			sldata->irradiance_rt = DRW_texture_create_2D_array(irr_size[0], irr_size[1], irr_size[2],
			                                                    irradiance_format, DRW_TEX_FILTER, NULL);
		}
		/* Tag probes to refresh */
		pinfo->update_world |= PROBE_UPDATE_GRID;
		pinfo->grid_initialized = false;
	}

	if ((pinfo->update_world & PROBE_UPDATE_GRID) != 0) {
		common_data->prb_num_render_grid = 0;
		pinfo->updated_bounce = 0;
		for (int i = 1; (ob = pinfo->probes_grid_ref[i]) && (i < MAX_PROBE); i++) {
			EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_ensure(ob);
			ped->need_update = true;
			ped->updated_cells = 0;
		}
	}

	if (common_data->prb_num_render_grid > pinfo->num_grid) {
		/* This can happen when deleting a probe. */
		common_data->prb_num_render_grid = pinfo->num_grid;
	}

	EEVEE_planar_reflections_cache_finish(sldata, vedata->stl);

	EEVEE_lightprobes_updates(sldata, vedata->psl, vedata->stl);

	DRW_uniformbuffer_update(sldata->probe_ubo, &sldata->probes->probe_data);
	DRW_uniformbuffer_update(sldata->grid_ubo, &sldata->probes->grid_data);
}

static void downsample_planar(void *vedata, int level)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;

	const float *size = DRW_viewport_size_get();
	copy_v2_v2(stl->g_data->planar_texel_size, size);
	for (int i = 0; i < level - 1; ++i) {
		stl->g_data->planar_texel_size[0] /= 2.0f;
		stl->g_data->planar_texel_size[1] /= 2.0f;
		min_ff(floorf(stl->g_data->planar_texel_size[0]), 1.0f);
		min_ff(floorf(stl->g_data->planar_texel_size[1]), 1.0f);
	}
	invert_v2(stl->g_data->planar_texel_size);

	DRW_draw_pass(psl->probe_planar_downsample_ps);
}

/* Glossy filter probe_rt to probe_pool at index probe_idx */
static void glossy_filter_probe(
        EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, EEVEE_PassList *psl, int probe_idx, float intensity)
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;

	pinfo->intensity_fac = intensity;

	/* Max lod used from the render target probe */
	pinfo->lod_rt_max = floorf(log2f(pinfo->target_size)) - 2.0f;

	/* Start fresh */
	GPU_framebuffer_ensure_config(&sldata->probe_filter_fb, {
		GPU_ATTACHMENT_NONE,
		GPU_ATTACHMENT_NONE
	});

	/* 2 - Let gpu create Mipmaps for Filtered Importance Sampling. */
	/* Bind next framebuffer to be able to gen. mips for probe_rt. */
	EEVEE_downsample_cube_buffer(vedata, sldata->probe_rt, (int)(pinfo->lod_rt_max));

	/* 3 - Render to probe array to the specified layer, do prefiltering. */
	float mipsize = pinfo->cubemap_res;
	const int maxlevel = (int)floorf(log2f(pinfo->cubemap_res));
	const int min_lod_level = 3;
	for (int i = 0; i < maxlevel - min_lod_level; i++) {
		float bias = (i == 0) ? -1.0f : 1.0f;
		pinfo->texel_size = 1.0f / mipsize;
		pinfo->padding_size = powf(2.0f, (float)(maxlevel - min_lod_level - 1 - i));
		/* XXX : WHY THE HECK DO WE NEED THIS ??? */
		/* padding is incorrect without this! float precision issue? */
		if (pinfo->padding_size > 32) {
			pinfo->padding_size += 5;
		}
		if (pinfo->padding_size > 16) {
			pinfo->padding_size += 4;
		}
		else if (pinfo->padding_size > 8) {
			pinfo->padding_size += 2;
		}
		else if (pinfo->padding_size > 4) {
			pinfo->padding_size += 1;
		}
		pinfo->layer = probe_idx;
		pinfo->roughness = (float)i / ((float)maxlevel - 4.0f);
		pinfo->roughness *= pinfo->roughness; /* Disney Roughness */
		pinfo->roughness *= pinfo->roughness; /* Distribute Roughness accros lod more evenly */
		CLAMP(pinfo->roughness, 1e-8f, 0.99999f); /* Avoid artifacts */

#if 1 /* Variable Sample count (fast) */
		switch (i) {
			case 0: pinfo->samples_ct = 1.0f; break;
			case 1: pinfo->samples_ct = 16.0f; break;
			case 2: pinfo->samples_ct = 32.0f; break;
			case 3: pinfo->samples_ct = 64.0f; break;
			default: pinfo->samples_ct = 128.0f; break;
		}
#else /* Constant Sample count (slow) */
		pinfo->samples_ct = 1024.0f;
#endif

		pinfo->invsamples_ct = 1.0f / pinfo->samples_ct;
		pinfo->lodfactor = bias + 0.5f * log((float)(pinfo->target_size * pinfo->target_size) * pinfo->invsamples_ct) / log(2);

		GPU_framebuffer_ensure_config(&sldata->probe_filter_fb, {
			GPU_ATTACHMENT_NONE,
			GPU_ATTACHMENT_TEXTURE_MIP(sldata->probe_pool, i)
		});
		GPU_framebuffer_bind(sldata->probe_filter_fb);
		GPU_framebuffer_viewport_set(sldata->probe_filter_fb, 0, 0, mipsize, mipsize);
		DRW_draw_pass(psl->probe_glossy_compute);

		mipsize /= 2;
		CLAMP_MIN(mipsize, 1);
	}
	/* For shading, save max level of the octahedron map */
	sldata->common_data.prb_lod_cube_max = (float)(maxlevel - min_lod_level) - 1.0f;
}

/* Diffuse filter probe_rt to irradiance_pool at index probe_idx */
static void diffuse_filter_probe(
	EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, EEVEE_PassList *psl, int offset,
	float clipsta, float clipend, float vis_range, float vis_blur, float intensity)
{
	EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
	EEVEE_LightProbesInfo *pinfo = sldata->probes;

	pinfo->intensity_fac = intensity;

	int pool_size[3];
	irradiance_pool_size_get(common_data->prb_irradiance_vis_size, pinfo->total_irradiance_samples, pool_size);

	/* find cell position on the virtual 3D texture */
	/* NOTE : Keep in sync with load_irradiance_cell() */
#if defined(IRRADIANCE_SH_L2)
	int size[2] = {3, 3};
#elif defined(IRRADIANCE_CUBEMAP)
	int size[2] = {8, 8};
	pinfo->samples_ct = 1024.0f;
#elif defined(IRRADIANCE_HL2)
	int size[2] = {3, 2};
	pinfo->samples_ct = 1024.0f;
#endif

	int cell_per_row = pool_size[0] / size[0];
	int x = size[0] * (offset % cell_per_row);
	int y = size[1] * (offset / cell_per_row);

#ifndef IRRADIANCE_SH_L2
	/* Tweaking parameters to balance perf. vs precision */
	const float bias = 0.0f;
	pinfo->invsamples_ct = 1.0f / pinfo->samples_ct;
	pinfo->lodfactor = bias + 0.5f * log((float)(pinfo->target_size * pinfo->target_size) * pinfo->invsamples_ct) / log(2);
	pinfo->lod_rt_max = floorf(log2f(pinfo->target_size)) - 2.0f;
#else
	pinfo->shres = 32; /* Less texture fetches & reduce branches */
	pinfo->lod_rt_max = 2.0f; /* Improve cache reuse */
#endif

	/* Start fresh */
	GPU_framebuffer_ensure_config(&sldata->probe_filter_fb, {
		GPU_ATTACHMENT_NONE,
		GPU_ATTACHMENT_NONE
	});

	/* 4 - Compute spherical harmonics */
	EEVEE_downsample_cube_buffer(vedata, sldata->probe_rt, (int)(pinfo->lod_rt_max));

	GPU_framebuffer_ensure_config(&sldata->probe_filter_fb, {
		GPU_ATTACHMENT_NONE,
		GPU_ATTACHMENT_TEXTURE_LAYER(sldata->irradiance_rt, 0)
	});
	GPU_framebuffer_bind(sldata->probe_filter_fb);
	GPU_framebuffer_viewport_set(sldata->probe_filter_fb, x, y, size[0], size[1]);
	DRW_draw_pass(psl->probe_diffuse_compute);

	/* World irradiance have no visibility */
	if (offset > 0) {
		/* Compute visibility */
		pinfo->samples_ct = 512.0f; /* TODO refine */
		pinfo->invsamples_ct = 1.0f / pinfo->samples_ct;
		pinfo->shres = common_data->prb_irradiance_vis_size;
		pinfo->visibility_range = vis_range;
		pinfo->visibility_blur = vis_blur;
		pinfo->near_clip = -clipsta;
		pinfo->far_clip = -clipend;
		pinfo->texel_size = 1.0f / (float)common_data->prb_irradiance_vis_size;

		int cell_per_col = pool_size[1] / common_data->prb_irradiance_vis_size;
		cell_per_row = pool_size[0] / common_data->prb_irradiance_vis_size;
		x = common_data->prb_irradiance_vis_size * (offset % cell_per_row);
		y = common_data->prb_irradiance_vis_size * ((offset / cell_per_row) % cell_per_col);
		int layer = 1 + ((offset / cell_per_row) / cell_per_col);
		const int vis_size = common_data->prb_irradiance_vis_size;

		GPU_framebuffer_ensure_config(&sldata->probe_filter_fb, {
			GPU_ATTACHMENT_NONE,
			GPU_ATTACHMENT_TEXTURE_LAYER(sldata->irradiance_rt, layer)
		});
		GPU_framebuffer_bind(sldata->probe_filter_fb);
		GPU_framebuffer_viewport_set(sldata->probe_filter_fb, x, y, vis_size, vis_size);
		DRW_draw_pass(psl->probe_visibility_compute);
	}
}

/* Render the scene to the probe_rt texture. */
static void render_scene_to_probe(
        EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata,
        const float pos[3], float clipsta, float clipend)
{
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_LightProbesInfo *pinfo = sldata->probes;

	DRWMatrixState matstate;
	float (*viewmat)[4] = matstate.mat[DRW_MAT_VIEW];
	float (*viewinv)[4] = matstate.mat[DRW_MAT_VIEWINV];
	float (*persmat)[4] = matstate.mat[DRW_MAT_PERS];
	float (*persinv)[4] = matstate.mat[DRW_MAT_PERSINV];
	float (*winmat)[4] = matstate.mat[DRW_MAT_WIN];
	float (*wininv)[4] = matstate.mat[DRW_MAT_WININV];

	float posmat[4][4];
	unit_m4(posmat);

	/* Move to capture position */
	negate_v3_v3(posmat[3], pos);

	/* 1 - Render to each cubeface individually.
	 * We do this instead of using geometry shader because a) it's faster,
	 * b) it's easier than fixing the nodetree shaders (for view dependant effects). */
	pinfo->layer = 0;
	perspective_m4(winmat, -clipsta, clipsta, -clipsta, clipsta, clipsta, clipend);

	/* Avoid using the texture attached to framebuffer when rendering. */
	/* XXX */
	GPUTexture *tmp_planar_pool = txl->planar_pool;
	GPUTexture *tmp_maxz = txl->maxzbuffer;
	txl->planar_pool = e_data.planar_pool_placeholder;
	txl->maxzbuffer = e_data.depth_placeholder;

	/* Update common uniforms */
	DRW_uniformbuffer_update(sldata->common_ubo, &sldata->common_data);

	for (int i = 0; i < 6; ++i) {
		/* Setup custom matrices */
		mul_m4_m4m4(viewmat, cubefacemat[i], posmat);
		mul_m4_m4m4(persmat, winmat, viewmat);
		invert_m4_m4(persinv, persmat);
		invert_m4_m4(viewinv, viewmat);
		invert_m4_m4(wininv, winmat);

		DRW_viewport_matrix_override_set_all(&matstate);

		/* Be sure that cascaded shadow maps are updated. */
		EEVEE_draw_shadows(sldata, psl);

		GPU_framebuffer_bind(sldata->probe_face_fb[i]);
		GPU_framebuffer_clear_depth(sldata->probe_face_fb[i], 1.0);

		/* Depth prepass */
		DRW_draw_pass(psl->depth_pass);
		DRW_draw_pass(psl->depth_pass_cull);

		DRW_draw_pass(psl->probe_background);

		// EEVEE_create_minmax_buffer(vedata, sldata->probe_depth_rt);

		/* Rebind Target FB */
		GPU_framebuffer_bind(sldata->probe_face_fb[i]);

		/* Shading pass */
		EEVEE_draw_default_passes(psl);
		DRW_draw_pass(psl->material_pass);
		DRW_draw_pass(psl->sss_pass); /* Only output standard pass */
	}

	/* Restore */
	txl->planar_pool = tmp_planar_pool;
	txl->maxzbuffer = tmp_maxz;
}

static void render_scene_to_planar(
        EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, int layer,
        EEVEE_LightProbeEngineData *ped)
{
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_PassList *psl = vedata->psl;

	float (*viewmat)[4] = ped->mats.mat[DRW_MAT_VIEW];
	float (*viewinv)[4] = ped->mats.mat[DRW_MAT_VIEWINV];
	float (*persmat)[4] = ped->mats.mat[DRW_MAT_PERS];
	float (*persinv)[4] = ped->mats.mat[DRW_MAT_PERSINV];
	float (*winmat)[4] = ped->mats.mat[DRW_MAT_WIN];
	float (*wininv)[4] = ped->mats.mat[DRW_MAT_WININV];

	invert_m4_m4(viewinv, viewmat);
	invert_m4_m4(persinv, persmat);
	invert_m4_m4(wininv, winmat);

	DRW_viewport_matrix_override_set_all(&ped->mats);

	/* Be sure that cascaded shadow maps are updated. */
	EEVEE_draw_shadows(sldata, psl);

	/* Since we are rendering with an inverted view matrix, we need
	 * to invert the facing for backface culling to be the same. */
	DRW_state_invert_facing();
	/* Set clipping plan */
	copy_v4_v4(sldata->clip_data.clip_planes[0], ped->planer_eq_offset);
	DRW_uniformbuffer_update(sldata->clip_ubo, &sldata->clip_data);
	DRW_state_clip_planes_count_set(1);

	GPU_framebuffer_ensure_config(&fbl->planarref_fb, {
		GPU_ATTACHMENT_TEXTURE_LAYER(txl->planar_depth, layer),
		GPU_ATTACHMENT_TEXTURE_LAYER(txl->planar_pool, layer)
	});

	GPU_framebuffer_bind(fbl->planarref_fb);
	GPU_framebuffer_clear_depth(fbl->planarref_fb, 1.0);

	/* Avoid using the texture attached to framebuffer when rendering. */
	/* XXX */
	GPUTexture *tmp_planar_pool = txl->planar_pool;
	GPUTexture *tmp_planar_depth = txl->planar_depth;
	txl->planar_pool = e_data.planar_pool_placeholder;
	txl->planar_depth = e_data.depth_array_placeholder;

	/* Slight modification: we handle refraction as normal
	 * shading and don't do SSRefraction. */

	/* Depth prepass */
	DRW_draw_pass(psl->depth_pass_clip);
	DRW_draw_pass(psl->depth_pass_clip_cull);
	DRW_draw_pass(psl->refract_depth_pass);
	DRW_draw_pass(psl->refract_depth_pass_cull);

	/* Background */
	DRW_draw_pass(psl->probe_background);

	EEVEE_create_minmax_buffer(vedata, tmp_planar_depth, layer);

	/* Compute GTAO Horizons */
	EEVEE_occlusion_compute(sldata, vedata, tmp_planar_depth, layer);

	/* Rebind Planar FB */
	GPU_framebuffer_bind(fbl->planarref_fb);

	/* Shading pass */
	EEVEE_draw_default_passes(psl);
	DRW_draw_pass(psl->material_pass);
	DRW_draw_pass(psl->sss_pass); /* Only output standard pass */
	DRW_draw_pass(psl->refract_pass);

	/* Transparent */
	if (DRW_state_is_image_render()) {
		/* Do the reordering only for offline because it can be costly. */
		DRW_pass_sort_shgroup_z(psl->transparent_pass);
	}
	DRW_draw_pass(psl->transparent_pass);

	DRW_state_invert_facing();
	DRW_state_clip_planes_reset();

	/* Restore */
	txl->planar_pool = tmp_planar_pool;
	txl->planar_depth = tmp_planar_depth;
}

static void render_world_to_probe(EEVEE_ViewLayerData *sldata, EEVEE_PassList *psl)
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	DRWMatrixState matstate;
	float (*viewmat)[4] = matstate.mat[DRW_MAT_VIEW];
	float (*viewinv)[4] = matstate.mat[DRW_MAT_VIEWINV];
	float (*persmat)[4] = matstate.mat[DRW_MAT_PERS];
	float (*persinv)[4] = matstate.mat[DRW_MAT_PERSINV];
	float (*winmat)[4] = matstate.mat[DRW_MAT_WIN];
	float (*wininv)[4] = matstate.mat[DRW_MAT_WININV];

	/* For world probe, we don't need to clear since we render the background directly. */
	pinfo->layer = 0;

	perspective_m4(winmat, -0.1f, 0.1f, -0.1f, 0.1f, 0.1f, 1.0f);
	invert_m4_m4(wininv, winmat);

	for (int i = 0; i < 6; ++i) {
		/* Setup custom matrices */
		copy_m4_m4(viewmat, cubefacemat[i]);
		mul_m4_m4m4(persmat, winmat, viewmat);
		invert_m4_m4(persinv, persmat);
		invert_m4_m4(viewinv, viewmat);
		DRW_viewport_matrix_override_set_all(&matstate);

		GPU_framebuffer_bind(sldata->probe_face_fb[i]);
		GPU_framebuffer_clear_depth(sldata->probe_face_fb[i], 1.0f);
		DRW_draw_pass(psl->probe_background);
	}
}

static void lightprobe_cell_grid_location_get(EEVEE_LightGrid *egrid, int cell_idx, float r_local_cell[3])
{
	/* Keep in sync with lightprobe_grid_display_vert */
	r_local_cell[2] = (float)(cell_idx % egrid->resolution[2]);
	r_local_cell[1] = (float)((cell_idx / egrid->resolution[2]) % egrid->resolution[1]);
	r_local_cell[0] = (float)(cell_idx / (egrid->resolution[2] * egrid->resolution[1]));
}

static void lightprobe_cell_world_location_get(EEVEE_LightGrid *egrid, float local_cell[3], float r_pos[3])
{
	float tmp[3];

	copy_v3_v3(r_pos, egrid->corner);
	mul_v3_v3fl(tmp, egrid->increment_x, local_cell[0]);
	add_v3_v3(r_pos, tmp);
	mul_v3_v3fl(tmp, egrid->increment_y, local_cell[1]);
	add_v3_v3(r_pos, tmp);
	mul_v3_v3fl(tmp, egrid->increment_z, local_cell[2]);
	add_v3_v3(r_pos, tmp);
}

static void lightprobes_refresh_world(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	DRWMatrixState saved_mats;

	/* We need to save the Matrices before overidding them */
	DRW_viewport_matrix_get_all(&saved_mats);

	render_world_to_probe(sldata, psl);
	if (pinfo->update_world & PROBE_UPDATE_CUBE) {
		glossy_filter_probe(sldata, vedata, psl, 0, 1.0);
		common_data->prb_num_render_cube = 1;
	}
	if (pinfo->update_world & PROBE_UPDATE_GRID) {
		diffuse_filter_probe(sldata, vedata, psl, 0, 0.0, 0.0, 0.0, 0.0, 1.0);

		SWAP(GPUTexture *, sldata->irradiance_pool, sldata->irradiance_rt);

		GPU_framebuffer_texture_attach(sldata->probe_filter_fb, sldata->irradiance_rt, 0, 0);
		GPU_framebuffer_bind(sldata->probe_filter_fb);
		DRW_draw_pass(psl->probe_grid_fill);

		common_data->prb_num_render_grid = 1;
		/* Reset volume history. */
		stl->effects->volume_current_sample = -1;
		common_data->vol_history_alpha = 0.0f;
	}
	pinfo->update_world = 0;
	DRW_viewport_request_redraw();
	/* Do not let this frame accumulate. */
	stl->effects->taa_current_sample = 1;

	DRW_viewport_matrix_override_set_all(&saved_mats);
}

static void lightprobes_refresh_initialize_grid(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	EEVEE_PassList *psl = vedata->psl;
	if (pinfo->grid_initialized) {
		/* Grid is already initialized, nothing to do. */
		return;
	}
	/* Flood fill with world irradiance. */
	GPU_framebuffer_texture_attach(sldata->probe_filter_fb, sldata->irradiance_rt, 0, 0);
	GPU_framebuffer_bind(sldata->probe_filter_fb);
	DRW_draw_pass(psl->probe_grid_fill);

	SWAP(GPUTexture *, sldata->irradiance_pool, sldata->irradiance_rt);

	GPU_framebuffer_texture_attach(sldata->probe_filter_fb, sldata->irradiance_rt, 0, 0);
	GPU_framebuffer_bind(sldata->probe_filter_fb);
	DRW_draw_pass(psl->probe_grid_fill);

	SWAP(GPUTexture *, sldata->irradiance_pool, sldata->irradiance_rt);

	pinfo->grid_initialized = true;
}

void EEVEE_lightprobes_refresh_planar(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	Object *ob;
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	DRWMatrixState saved_mats;

	if (pinfo->num_planar == 0) {
		/* Disable SSR if we cannot read previous frame */
		common_data->ssr_toggle = vedata->stl->g_data->valid_double_buffer;
		common_data->prb_num_planar = 0;
		return;
	}

	EEVEE_planar_reflections_updates(sldata);
	DRW_uniformbuffer_update(sldata->planar_ubo, &sldata->probes->planar_data);

	/* We need to save the Matrices before overidding them */
	DRW_viewport_matrix_get_all(&saved_mats);

	/* Temporary Remove all planar reflections (avoid lag effect). */
	common_data->prb_num_planar = 0;
	/* Turn off ssr to avoid black specular */
	common_data->ssr_toggle = false;
	common_data->sss_toggle = false;

	DRW_uniformbuffer_update(sldata->common_ubo, &sldata->common_data);

	for (int i = 0; (ob = pinfo->probes_planar_ref[i]) && (i < MAX_PLANAR); i++) {
		EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_ensure(ob);
		render_scene_to_planar(sldata, vedata, i, ped);
	}

	/* Restore */
	common_data->prb_num_planar = pinfo->num_planar;
	common_data->ssr_toggle = true;
	common_data->sss_toggle = true;

	/* Prefilter for SSR */
	if ((vedata->stl->effects->enabled_effects & EFFECT_SSR) != 0) {
		const int max_lod = 9;
		DRW_stats_group_start("Planar Probe Downsample");

		GPU_framebuffer_ensure_config(&fbl->planar_downsample_fb, {
			GPU_ATTACHMENT_NONE,
			GPU_ATTACHMENT_TEXTURE(txl->planar_pool)
		});
		GPU_framebuffer_recursive_downsample(fbl->planar_downsample_fb, max_lod, &downsample_planar, vedata);
		/* For shading, save max level of the planar map */
		common_data->prb_lod_planar_max = (float)(max_lod);
		DRW_stats_group_end();
	}

	DRW_viewport_matrix_override_set_all(&saved_mats);

	if (DRW_state_is_image_render()) {
		/* Sort transparents because planar reflections could have re-sorted them. */
		DRW_pass_sort_shgroup_z(vedata->psl->transparent_pass);
	}

	/* Disable SSR if we cannot read previous frame */
	common_data->ssr_toggle = vedata->stl->g_data->valid_double_buffer;
}

static void lightprobes_refresh_cube(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	Object *ob;
	for (int i = 1; (ob = pinfo->probes_cube_ref[i]) && (i < MAX_PROBE); i++) {
		EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_ensure(ob);
		if (!ped->need_update) {
			continue;
		}
		LightProbe *prb = (LightProbe *)ob->data;
		render_scene_to_probe(sldata, vedata, ob->obmat[3], prb->clipsta, prb->clipend);
		glossy_filter_probe(sldata, vedata, psl, i, prb->intensity);
		ped->need_update = false;
		ped->probe_id = i;
		if (!ped->ready_to_shade) {
			common_data->prb_num_render_cube++;
			ped->ready_to_shade = true;
		}
#if 0
		printf("Update Cubemap %d\n", i);
#endif
		DRW_viewport_request_redraw();
		/* Do not let this frame accumulate. */
		stl->effects->taa_current_sample = 1;
		/* Only do one probe per frame */
		return;
	}

	pinfo->do_cube_update = false;
}

static void lightprobes_refresh_all_no_world(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	Object *ob;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	RegionView3D *rv3d = draw_ctx->rv3d;

	if (draw_ctx->evil_C != NULL) {
		/* Only compute probes if not navigating or in playback */
		struct wmWindowManager *wm = CTX_wm_manager(draw_ctx->evil_C);
		if (((rv3d->rflag & RV3D_NAVIGATING) != 0) || ED_screen_animation_no_scrub(wm) != NULL) {
			return;
		}
	}
	/* We need to save the Matrices before overidding them */
	DRWMatrixState saved_mats;
	DRW_viewport_matrix_get_all(&saved_mats);
	/* Make sure grid is initialized. */
	lightprobes_refresh_initialize_grid(sldata, vedata);
	/* Reflection probes depend on diffuse lighting thus on irradiance grid,
	 * so update them first. */
	while (pinfo->updated_bounce < pinfo->num_bounce) {
		common_data->prb_num_render_grid = pinfo->num_grid;
		/* TODO(sergey): This logic can be split into smaller functions. */
		for (int i = 1; (ob = pinfo->probes_grid_ref[i]) && (i < MAX_GRID); i++) {
			EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_ensure(ob);
			if (!ped->need_update) {
				continue;
			}
			EEVEE_LightGrid *egrid = &pinfo->grid_data[i];
			LightProbe *prb = (LightProbe *)ob->data;
			/* Find the next cell corresponding to the current level. */
			bool valid_cell = false;
			int cell_id = ped->updated_cells;
			float pos[3], grid_loc[3];
			/* Other levels */
			int current_stride = 1 << max_ii(0, ped->max_lvl - ped->updated_lvl);
			int prev_stride = current_stride << 1;
			bool do_rendering = true;
			while (!valid_cell) {
				cell_id = ped->updated_cells;
				lightprobe_cell_grid_location_get(egrid, cell_id, grid_loc);
				if (ped->updated_lvl == 0 && cell_id == 0) {
					valid_cell = true;
					ped->updated_cells = ped->num_cell;
					continue;
				}
				else if (((((int)grid_loc[0] % current_stride) == 0) &&
				          (((int)grid_loc[1] % current_stride) == 0) &&
				          (((int)grid_loc[2] % current_stride) == 0)) &&
				         !((((int)grid_loc[0] % prev_stride) == 0) &&
				           (((int)grid_loc[1] % prev_stride) == 0) &&
				           (((int)grid_loc[2] % prev_stride) == 0)))
				{
					valid_cell = true;
				}
				ped->updated_cells++;
				if (ped->updated_cells > ped->num_cell) {
					do_rendering = false;
					break;
				}
			}
			if (do_rendering) {
				lightprobe_cell_world_location_get(egrid, grid_loc, pos);
				SWAP(GPUTexture *, sldata->irradiance_pool, sldata->irradiance_rt);
				/* Temporary Remove all probes. */
				int tmp_num_render_grid = common_data->prb_num_render_grid;
				int tmp_num_render_cube = common_data->prb_num_render_cube;
				int tmp_num_planar = common_data->prb_num_planar;
				float tmp_level_bias = egrid->level_bias;
				common_data->prb_num_render_cube = 0;
				common_data->prb_num_planar = 0;
				/* Use light from previous bounce when capturing radiance. */
				if (pinfo->updated_bounce == 0) {
					/* But not on first bounce. */
					common_data->prb_num_render_grid = 0;
				}
				else {
					/* Remove bias */
					egrid->level_bias = (float)(1 << 0);
					DRW_uniformbuffer_update(sldata->grid_ubo, &sldata->probes->grid_data);
				}
				render_scene_to_probe(sldata, vedata, pos, prb->clipsta, prb->clipend);
				diffuse_filter_probe(sldata, vedata, psl, egrid->offset + cell_id,
				                     prb->clipsta, prb->clipend, egrid->visibility_range, prb->vis_blur,
				                     prb->intensity);
				/* To see what is going on. */
				SWAP(GPUTexture *, sldata->irradiance_pool, sldata->irradiance_rt);
				/* Restore */
				common_data->prb_num_render_cube = tmp_num_render_cube;
				pinfo->num_planar = tmp_num_planar;
				if (pinfo->updated_bounce == 0) {
					common_data->prb_num_render_grid = tmp_num_render_grid;
				}
				else {
					egrid->level_bias = tmp_level_bias;
					DRW_uniformbuffer_update(sldata->grid_ubo, &sldata->probes->grid_data);
				}
#if 0
				printf("Updated Grid %d : cell %d / %d, bounce %d / %d\n",
				       i, cell_id + 1, ped->num_cell, pinfo->updated_bounce + 1, pinfo->num_bounce);
#endif
			}
			if (ped->updated_cells >= ped->num_cell) {
				ped->updated_lvl++;
				ped->updated_cells = 0;
				if (ped->updated_lvl > ped->max_lvl) {
					ped->need_update = false;
				}
				egrid->level_bias = (float)(1 << max_ii(0, ped->max_lvl - ped->updated_lvl + 1));
				DRW_uniformbuffer_update(sldata->grid_ubo, &sldata->probes->grid_data);
			}
			/* Only do one probe per frame */
			DRW_viewport_request_redraw();
			/* Do not let this frame accumulate. */
			stl->effects->taa_current_sample = 1;
			/* Reset volume history. */
			stl->effects->volume_current_sample = -1;
			common_data->vol_history_alpha = 0.0f;
			/* Restore matrices */
			DRW_viewport_matrix_override_set_all(&saved_mats);
			return;
		}

		pinfo->updated_bounce++;
		common_data->prb_num_render_grid = pinfo->num_grid;

		if (pinfo->updated_bounce < pinfo->num_bounce) {
			/* Retag all grids to update for next bounce */
			for (int i = 1; (ob = pinfo->probes_grid_ref[i]) && (i < MAX_GRID); i++) {
				EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_ensure(ob);
				ped->need_update = true;
				ped->updated_cells = 0;
				ped->updated_lvl = 0;
			}
			/* Reset the next buffer so we can see the progress. */
			/* irradiance_rt is already the next rt because of the previous SWAP */
			GPU_framebuffer_texture_attach(sldata->probe_filter_fb, sldata->irradiance_rt, 0, 0);
			GPU_framebuffer_bind(sldata->probe_filter_fb);
			DRW_draw_pass(psl->probe_grid_fill);

			GPU_framebuffer_texture_attach(sldata->probe_filter_fb, sldata->probe_pool, 0, 0);
			/* Swap AFTER */
			SWAP(GPUTexture *, sldata->irradiance_pool, sldata->irradiance_rt);
		}
	}
	/* Refresh cube probe when needed. */
	lightprobes_refresh_cube(sldata, vedata);
	/* Restore matrices */
	DRW_viewport_matrix_override_set_all(&saved_mats);
}

bool EEVEE_lightprobes_all_probes_ready(EEVEE_ViewLayerData *sldata, EEVEE_Data *UNUSED(vedata))
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;

	return ((pinfo->do_cube_update == false) &&
	        (pinfo->updated_bounce == pinfo->num_bounce) &&
	        (common_data->prb_num_render_cube == pinfo->num_cube));
}

void EEVEE_lightprobes_refresh(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
	EEVEE_LightProbesInfo *pinfo = sldata->probes;

	/* Disable specular lighting when rendering probes to avoid feedback loops (looks bad). */
	common_data->spec_toggle = false;
	common_data->ssr_toggle = false;
	common_data->sss_toggle = false;

	/* Disable AO until we find a way to hide really bad discontinuities between cubefaces. */
	float tmp_ao_dist = common_data->ao_dist;
	float tmp_ao_settings = common_data->ao_settings;
	common_data->ao_settings = 0.0f;
	common_data->ao_dist = 0.0f;

	/* Render world in priority */
	if (pinfo->update_world) {
		lightprobes_refresh_world(sldata, vedata);
	}
	else if (EEVEE_lightprobes_all_probes_ready(sldata, vedata) == false && pinfo->all_materials_updated) {
		lightprobes_refresh_all_no_world(sldata, vedata);
	}

	/* Restore */
	common_data->spec_toggle = true;
	common_data->ssr_toggle = true;
	common_data->sss_toggle = true;
	common_data->ao_dist = tmp_ao_dist;
	common_data->ao_settings = tmp_ao_settings;
}

void EEVEE_lightprobes_free(void)
{
	MEM_SAFE_FREE(e_data.format_probe_display_cube);
	MEM_SAFE_FREE(e_data.format_probe_display_planar);
	DRW_SHADER_FREE_SAFE(e_data.probe_default_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_filter_glossy_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_filter_diffuse_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_filter_visibility_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_grid_fill_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_grid_display_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_planar_display_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_planar_downsample_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_cube_display_sh);
	DRW_TEXTURE_FREE_SAFE(e_data.hammersley);
	DRW_TEXTURE_FREE_SAFE(e_data.planar_pool_placeholder);
	DRW_TEXTURE_FREE_SAFE(e_data.depth_placeholder);
	DRW_TEXTURE_FREE_SAFE(e_data.depth_array_placeholder);
}
