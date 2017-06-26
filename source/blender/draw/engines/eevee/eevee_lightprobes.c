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

/** \file eevee_lights.c
 *  \ingroup DNA
 */

#include "DNA_world_types.h"
#include "DNA_texture_types.h"
#include "DNA_image_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_view3d_types.h"

#include "BKE_object.h"

#include "BLI_dynstr.h"

#include "ED_screen.h"

#include "DRW_render.h"

#include "GPU_material.h"
#include "GPU_texture.h"
#include "GPU_glew.h"

#include "DRW_render.h"

#include "eevee_engine.h"
#include "eevee_private.h"

/* TODO Option */
#define PROBE_RT_SIZE 512 /* Cube render target */
#define PROBE_OCTAHEDRON_SIZE 1024
#define IRRADIANCE_POOL_SIZE 1024

static struct {
	struct GPUShader *probe_default_sh;
	struct GPUShader *probe_filter_glossy_sh;
	struct GPUShader *probe_filter_diffuse_sh;
	struct GPUShader *probe_grid_display_sh;
	struct GPUShader *probe_planar_display_sh;
	struct GPUShader *probe_planar_downsample_sh;
	struct GPUShader *probe_cube_display_sh;

	struct GPUTexture *hammersley;
	struct GPUTexture *planar_depth;
	struct GPUTexture *planar_minmaxz;
	struct GPUTexture *planar_pool_placeholder;
	struct GPUTexture *cube_face_depth;
	struct GPUTexture *cube_face_minmaxz;

	bool update_world;
	bool world_ready_to_shade;
} e_data = {NULL}; /* Engine data */

extern char datatoc_default_world_frag_glsl[];
extern char datatoc_fullscreen_vert_glsl[];
extern char datatoc_lightprobe_filter_glossy_frag_glsl[];
extern char datatoc_lightprobe_filter_diffuse_frag_glsl[];
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
extern char datatoc_irradiance_lib_glsl[];
extern char datatoc_octahedron_lib_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];

extern GlobalsUboStorage ts;

/* *********** FUNCTIONS *********** */

/* Van der Corput sequence */
 /* From http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html */
static float radical_inverse(int i) {
	unsigned int bits = (unsigned int)i;
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return (float)bits * 2.3283064365386963e-10f;
}

static struct GPUTexture *create_hammersley_sample_texture(int samples)
{
	struct GPUTexture *tex;
	float (*texels)[2] = MEM_mallocN(sizeof(float[2]) * samples, "hammersley_tex");
	int i;

	for (i = 0; i < samples; i++) {
		float phi = radical_inverse(i) * 2.0f * M_PI;
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
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;

	const float *viewport_size = DRW_viewport_size_get();

	/* TODO get screen percentage from layer setting */
	// const DRWContextState *draw_ctx = DRW_context_state_get();
	// SceneLayer *sl = draw_ctx->sl;
	float screen_percentage = 1.0f;

	int width = (int)(viewport_size[0] * screen_percentage);
	int height = (int)(viewport_size[1] * screen_percentage);

	/* We need an Array texture so allocate it ourself */
	if (!txl->planar_pool) {
		if (num_planar_ref > 0) {
			txl->planar_pool = DRW_texture_create_2D_array(width, height, max_ff(1, num_planar_ref),
			                                                 DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
		}
		else if (num_planar_ref == 0) {
			/* Makes Opengl Happy : Create a placeholder texture that will never be sampled but still bound to shader. */
			txl->planar_pool = DRW_texture_create_2D_array(1, 1, 1, DRW_TEX_RGBA_8, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
		}
	}

	if (num_planar_ref > 0) {
		/* NOTE : Depth buffer is 2D but the planar_pool tex is 2D array.
		 * DRW_framebuffer_init binds the whole texture making the framebuffer invalid.
		 * To overcome this, we bind the planar pool ourselves later */

		/* XXX Do this one first so it gets it's mipmap done. */
		DRWFboTexture tex_minmaxz = {&e_data.planar_minmaxz, DRW_TEX_RG_32, DRW_TEX_MIPMAP | DRW_TEX_TEMP};
		DRW_framebuffer_init(&fbl->planarref_fb, &draw_engine_eevee_type,
		                     width / 2, height / 2, &tex_minmaxz, 1);

		/* Note: this is not the configuration used when rendering. */
		DRWFboTexture tex = {&e_data.planar_depth, DRW_TEX_DEPTH_24, DRW_TEX_TEMP};
		DRW_framebuffer_init(&fbl->planarref_fb, &draw_engine_eevee_type,
		                     width, height, &tex, 1);
	}
}

void EEVEE_lightprobes_init(EEVEE_SceneLayerData *sldata, EEVEE_Data *UNUSED(vedata))
{
	/* Shaders */
	if (!e_data.probe_filter_glossy_sh) {
		char *shader_str = NULL;

		DynStr *ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, datatoc_bsdf_common_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_bsdf_sampling_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_lightprobe_filter_glossy_frag_glsl);
		shader_str = BLI_dynstr_get_cstring(ds_frag);
		BLI_dynstr_free(ds_frag);

		e_data.probe_filter_glossy_sh = DRW_shader_create(
		        datatoc_lightprobe_vert_glsl, datatoc_lightprobe_geom_glsl, shader_str,
		        "#define HAMMERSLEY_SIZE 1024\n"
		        "#define NOISE_SIZE 64\n");

		e_data.probe_default_sh = DRW_shader_create(
		        datatoc_lightprobe_vert_glsl, datatoc_lightprobe_geom_glsl, datatoc_default_world_frag_glsl, NULL);

		MEM_freeN(shader_str);

		ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, datatoc_bsdf_common_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_bsdf_sampling_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_lightprobe_filter_diffuse_frag_glsl);
		shader_str = BLI_dynstr_get_cstring(ds_frag);
		BLI_dynstr_free(ds_frag);

		e_data.probe_filter_diffuse_sh = DRW_shader_create_fullscreen(
		        shader_str,
#if defined(IRRADIANCE_SH_L2)
		        "#define IRRADIANCE_SH_L2\n"
#elif defined(IRRADIANCE_CUBEMAP)
		        "#define IRRADIANCE_CUBEMAP\n"
#elif defined(IRRADIANCE_HL2)
		        "#define IRRADIANCE_HL2\n"
#endif
		        "#define HAMMERSLEY_SIZE 1024\n"
		        "#define NOISE_SIZE 64\n");

		MEM_freeN(shader_str);

		ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, datatoc_octahedron_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_irradiance_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_lightprobe_grid_display_frag_glsl);
		shader_str = BLI_dynstr_get_cstring(ds_frag);
		BLI_dynstr_free(ds_frag);

		e_data.probe_grid_display_sh = DRW_shader_create(
		        datatoc_lightprobe_grid_display_vert_glsl, NULL, shader_str,
#if defined(IRRADIANCE_SH_L2)
		        "#define IRRADIANCE_SH_L2\n"
#elif defined(IRRADIANCE_CUBEMAP)
		        "#define IRRADIANCE_CUBEMAP\n"
#elif defined(IRRADIANCE_HL2)
		        "#define IRRADIANCE_HL2\n"
#endif
		        );

		MEM_freeN(shader_str);

		ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, datatoc_octahedron_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_lightprobe_cube_display_frag_glsl);
		shader_str = BLI_dynstr_get_cstring(ds_frag);
		BLI_dynstr_free(ds_frag);

		e_data.probe_cube_display_sh = DRW_shader_create(
		        datatoc_lightprobe_cube_display_vert_glsl, NULL, shader_str, NULL);

		MEM_freeN(shader_str);

		ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, datatoc_bsdf_common_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_lightprobe_planar_display_frag_glsl);
		shader_str = BLI_dynstr_get_cstring(ds_frag);
		BLI_dynstr_free(ds_frag);

		e_data.probe_planar_display_sh = DRW_shader_create(
		        datatoc_lightprobe_planar_display_vert_glsl, NULL, shader_str,
		        "#define MAX_PLANAR " STRINGIFY(MAX_PLANAR) "\n");

		MEM_freeN(shader_str);

		e_data.probe_planar_downsample_sh = DRW_shader_create(
		        datatoc_lightprobe_planar_downsample_vert_glsl,
		        datatoc_lightprobe_planar_downsample_geom_glsl,
		        datatoc_lightprobe_planar_downsample_frag_glsl,
		        NULL);

		e_data.hammersley = create_hammersley_sample_texture(1024);
	}

	if (!sldata->probes) {
		sldata->probes = MEM_callocN(sizeof(EEVEE_LightProbesInfo), "EEVEE_LightProbesInfo");
		sldata->probes->specular_toggle = true;
		sldata->probe_ubo = DRW_uniformbuffer_create(sizeof(EEVEE_LightProbe) * MAX_PROBE, NULL);
		sldata->grid_ubo = DRW_uniformbuffer_create(sizeof(EEVEE_LightGrid) * MAX_GRID, NULL);
		sldata->planar_ubo = DRW_uniformbuffer_create(sizeof(EEVEE_PlanarReflection) * MAX_PLANAR, NULL);
	}

	/* Setup Render Target Cubemap */

	/* We do this detach / attach dance to not generate an invalid framebuffer (mixed cubemap / 2D map) */
	if (sldata->probe_rt) {
		/* XXX Silly,TODO Cleanup this mess */
		DRW_framebuffer_texture_detach(sldata->probe_rt);
	}

	DRWFboTexture tex_probe = {&e_data.cube_face_depth, DRW_TEX_DEPTH_24, DRW_TEX_TEMP};
	DRW_framebuffer_init(&sldata->probe_fb, &draw_engine_eevee_type, PROBE_RT_SIZE, PROBE_RT_SIZE, &tex_probe, 1);

	if (!sldata->probe_rt) {
		sldata->probe_rt = DRW_texture_create_cube(PROBE_RT_SIZE, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
	}

	if (sldata->probe_rt) {
		/* XXX Silly,TODO Cleanup this mess */
		DRW_framebuffer_texture_attach(sldata->probe_fb, sldata->probe_rt, 0, 0);
	}

	/* Minmaxz Pyramid */
	/* Highjacking the minmaxz_fb but that's ok because it's reconfigured before usage. */
	// DRWFboTexture tex_minmaxz = {&e_data.cube_face_minmaxz, DRW_TEX_RG_32, DRW_TEX_MIPMAP | DRW_TEX_TEMP};
	// DRW_framebuffer_init(&vedata->fbl->minmaxz_fb, &draw_engine_eevee_type, PROBE_RT_SIZE / 2, PROBE_RT_SIZE / 2, &tex_minmaxz, 1);

	/* Placeholder planar pool: used when rendering planar reflections (avoid dependency loop). */
	if (!e_data.planar_pool_placeholder) {
		e_data.planar_pool_placeholder = DRW_texture_create_2D_array(1, 1, 1, DRW_TEX_RGBA_8, DRW_TEX_FILTER, NULL);
	}
}

void EEVEE_lightprobes_cache_init(EEVEE_SceneLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_LightProbesInfo *pinfo = sldata->probes;

	pinfo->num_cube = 1; /* at least one for the world */
	pinfo->num_grid = 1;
	pinfo->num_planar = 0;
	memset(pinfo->probes_cube_ref, 0, sizeof(pinfo->probes_cube_ref));
	memset(pinfo->probes_grid_ref, 0, sizeof(pinfo->probes_grid_ref));
	memset(pinfo->probes_planar_ref, 0, sizeof(pinfo->probes_planar_ref));

	{
		psl->probe_background = DRW_pass_create("World Probe Pass", DRW_STATE_WRITE_COLOR);

		struct Gwn_Batch *geom = DRW_cache_fullscreen_quad_get();
		DRWShadingGroup *grp = NULL;

		const DRWContextState *draw_ctx = DRW_context_state_get();
		Scene *scene = draw_ctx->scene;
		World *wo = scene->world;

		static int zero = 0;
		float *col = ts.colorBackground;
		if (wo) {
			col = &wo->horr;
			e_data.update_world = (wo->update_flag != 0);
			wo->update_flag = 0;

			if (wo->use_nodes && wo->nodetree) {
				struct GPUMaterial *gpumat = EEVEE_material_world_lightprobe_get(scene, wo);

				grp = DRW_shgroup_material_instance_create(gpumat, psl->probe_background, geom);

				if (grp) {
					DRW_shgroup_uniform_int(grp, "Layer", &zero, 1);

					for (int i = 0; i < 6; ++i)
						DRW_shgroup_call_dynamic_add_empty(grp);
				}
				else {
					/* Shader failed : pink background */
					static float pink[3] = {1.0f, 0.0f, 1.0f};
					col = pink;
				}
			}
		}

		/* Fallback if shader fails or if not using nodetree. */
		if (grp == NULL) {
			grp = DRW_shgroup_instance_create(e_data.probe_default_sh, psl->probe_background, geom);
			DRW_shgroup_uniform_vec3(grp, "color", col, 1);
			DRW_shgroup_uniform_int(grp, "Layer", &zero, 1);

			for (int i = 0; i < 6; ++i)
				DRW_shgroup_call_dynamic_add_empty(grp);
		}
	}

	{
		psl->probe_glossy_compute = DRW_pass_create("LightProbe Glossy Compute", DRW_STATE_WRITE_COLOR);

		struct Gwn_Batch *geom = DRW_cache_fullscreen_quad_get();

		DRWShadingGroup *grp = DRW_shgroup_instance_create(e_data.probe_filter_glossy_sh, psl->probe_glossy_compute, geom);
		DRW_shgroup_uniform_float(grp, "sampleCount", &sldata->probes->samples_ct, 1);
		DRW_shgroup_uniform_float(grp, "invSampleCount", &sldata->probes->invsamples_ct, 1);
		DRW_shgroup_uniform_float(grp, "roughnessSquared", &sldata->probes->roughness, 1);
		DRW_shgroup_uniform_float(grp, "lodFactor", &sldata->probes->lodfactor, 1);
		DRW_shgroup_uniform_float(grp, "lodMax", &sldata->probes->lodmax, 1);
		DRW_shgroup_uniform_float(grp, "texelSize", &sldata->probes->texel_size, 1);
		DRW_shgroup_uniform_float(grp, "paddingSize", &sldata->probes->padding_size, 1);
		DRW_shgroup_uniform_int(grp, "Layer", &sldata->probes->layer, 1);
		DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
		// DRW_shgroup_uniform_texture(grp, "texJitter", e_data.jitter);
		DRW_shgroup_uniform_texture(grp, "probeHdr", sldata->probe_rt);

		DRW_shgroup_call_dynamic_add_empty(grp);
	}

	{
		psl->probe_diffuse_compute = DRW_pass_create("LightProbe Diffuse Compute", DRW_STATE_WRITE_COLOR);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.probe_filter_diffuse_sh, psl->probe_diffuse_compute);
#ifdef IRRADIANCE_SH_L2
		DRW_shgroup_uniform_int(grp, "probeSize", &sldata->probes->shres, 1);
#else
		DRW_shgroup_uniform_float(grp, "sampleCount", &sldata->probes->samples_ct, 1);
		DRW_shgroup_uniform_float(grp, "invSampleCount", &sldata->probes->invsamples_ct, 1);
		DRW_shgroup_uniform_float(grp, "lodFactor", &sldata->probes->lodfactor, 1);
		DRW_shgroup_uniform_float(grp, "lodMax", &sldata->probes->lodmax, 1);
		DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
#endif
		DRW_shgroup_uniform_texture(grp, "probeHdr", sldata->probe_rt);

		struct Gwn_Batch *geom = DRW_cache_fullscreen_quad_get();
		DRW_shgroup_call_add(grp, geom, NULL);
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_CULL_BACK;
		psl->probe_display = DRW_pass_create("LightProbe Display", state);

		struct Gwn_Batch *geom = DRW_cache_sphere_get();
		DRWShadingGroup *grp = stl->g_data->cube_display_shgrp = DRW_shgroup_instance_create(e_data.probe_cube_display_sh, psl->probe_display, geom);
		DRW_shgroup_attrib_float(grp, "probe_id", 1); /* XXX this works because we are still uploading 4bytes and using the right stride */
		DRW_shgroup_attrib_float(grp, "probe_location", 3);
		DRW_shgroup_attrib_float(grp, "sphere_size", 1);
		DRW_shgroup_uniform_float(grp, "lodMax", &sldata->probes->lodmax, 1);
		DRW_shgroup_uniform_buffer(grp, "probeCubes", &sldata->probe_pool);
	}

	{
		psl->probe_planar_downsample_ps = DRW_pass_create("LightProbe Planar Downsample", DRW_STATE_WRITE_COLOR);

		struct Gwn_Batch *geom = DRW_cache_fullscreen_quad_get();
		DRWShadingGroup *grp = stl->g_data->planar_downsample = DRW_shgroup_instance_create(e_data.probe_planar_downsample_sh, psl->probe_planar_downsample_ps, geom);
		DRW_shgroup_uniform_buffer(grp, "source", &txl->planar_pool);
		DRW_shgroup_uniform_vec2(grp, "texelSize", stl->g_data->texel_size, 1);
	}
}

void EEVEE_lightprobes_cache_add(EEVEE_SceneLayerData *sldata, Object *ob)
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	LightProbe *probe = (LightProbe *)ob->data;

	/* Step 1 find all lamps in the scene and setup them */
	if ((probe->type == LIGHTPROBE_TYPE_CUBE && pinfo->num_cube >= MAX_PROBE) ||
		(probe->type == LIGHTPROBE_TYPE_GRID && pinfo->num_grid >= MAX_PROBE))
	{
		printf("Too much probes in the scene !!!\n");
		return;
	}

	EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_get(ob);

	ped->num_cell = probe->grid_resolution_x * probe->grid_resolution_y * probe->grid_resolution_z;

	if ((ob->deg_update_flag & DEG_RUNTIME_DATA_UPDATE) != 0) {
		ped->need_update = true;
		ped->probe_id = 0;

		if (probe->type == LIGHTPROBE_TYPE_GRID) {
			ped->updated_cells = 0;
			pinfo->updated_bounce = 0;
		}
	}

	if (e_data.update_world) {
		ped->need_update = true;
		ped->updated_cells = 0;
		ped->probe_id = 0;
		pinfo->updated_bounce = 0;
	}

	if (probe->type == LIGHTPROBE_TYPE_CUBE) {
		pinfo->probes_cube_ref[pinfo->num_cube] = ob;
		pinfo->num_cube++;
	}
	else if (probe->type == LIGHTPROBE_TYPE_PLANAR) {
		pinfo->probes_planar_ref[pinfo->num_planar] = ob;
		pinfo->num_planar++;
	}
	else { /* GRID */
		pinfo->probes_grid_ref[pinfo->num_grid] = ob;
		pinfo->num_grid++;
	}
}

/* TODO find a nice name to push it to math_matrix.c */
static void scale_m4_v3(float R[4][4], float v[3])
{
	for (int i = 0; i < 4; ++i)
		mul_v3_v3(R[i], v);
}

static void EEVEE_planar_reflections_updates(EEVEE_SceneLayerData *sldata, EEVEE_PassList *psl, EEVEE_TextureList *txl)
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	Object *ob;
	float mtx[4][4], normat[4][4], imat[4][4], rangemat[4][4];

	float viewmat[4][4], winmat[4][4];
	DRW_viewport_matrix_get(viewmat, DRW_MAT_VIEW);
	DRW_viewport_matrix_get(winmat, DRW_MAT_WIN);

	zero_m4(rangemat);
	rangemat[0][0] = rangemat[1][1] = rangemat[2][2] = 0.5f;
	rangemat[3][0] = rangemat[3][1] = rangemat[3][2] = 0.5f;
	rangemat[3][3] = 1.0f;

	/* PLANAR REFLECTION */
	for (int i = 0; (ob = pinfo->probes_planar_ref[i]) && (i < MAX_PLANAR); i++) {
		LightProbe *probe = (LightProbe *)ob->data;
		EEVEE_PlanarReflection *eplanar = &pinfo->planar_data[i];
		EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_get(ob);

		/* Computing mtx : matrix that mirror position around object's XY plane. */
		normalize_m4_m4(normat, ob->obmat);  /* object > world */
		invert_m4_m4(imat, normat); /* world > object */

		float reflect[3] = {1.0f, 1.0f, -1.0f}; /* XY reflection plane */
		scale_m4_v3(imat, reflect); /* world > object > mirrored obj */
		mul_m4_m4m4(mtx, normat, imat); /* world > object > mirrored obj > world */

		/* Reflect Camera Matrix. */
		mul_m4_m4m4(ped->viewmat, viewmat, mtx);

		/* TODO FOV margin */
		float winmat_fov[4][4];
		copy_m4_m4(winmat_fov, winmat);

		/* Apply Perspective Matrix. */
		mul_m4_m4m4(ped->persmat, winmat_fov, ped->viewmat);

		/* This is the matrix used to reconstruct texture coordinates.
		 * We use the original view matrix because it does not create
		 * visual artifacts if receiver is not perfectly aligned with
		 * the planar reflection probe. */
		mul_m4_m4m4(eplanar->reflectionmat, winmat_fov, viewmat); /* TODO FOV margin */
		/* Convert from [-1, 1] to [0, 1] (NDC to Texture coord). */
		mul_m4_m4m4(eplanar->reflectionmat, rangemat, eplanar->reflectionmat);

		/* TODO frustum check. */
		ped->need_update = true;

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

		/* Debug Display */
		if ((probe->flag & LIGHTPROBE_FLAG_SHOW_DATA) != 0) {
			DRWShadingGroup *grp = DRW_shgroup_create(e_data.probe_planar_display_sh, psl->probe_display);

			DRW_shgroup_uniform_int(grp, "probeIdx", &ped->probe_id, 1);
			DRW_shgroup_uniform_buffer(grp, "probePlanars", &txl->planar_pool);
			DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);

			struct Gwn_Batch *geom = DRW_cache_fullscreen_quad_get();
			DRW_shgroup_call_add(grp, geom, ob->obmat);
		}
	}
}

static void EEVEE_lightprobes_updates(EEVEE_SceneLayerData *sldata, EEVEE_PassList *psl, EEVEE_StorageList *stl)
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	Object *ob;

	/* CUBE REFLECTION */
	for (int i = 1; (ob = pinfo->probes_cube_ref[i]) && (i < MAX_PROBE); i++) {
		LightProbe *probe = (LightProbe *)ob->data;
		EEVEE_LightProbe *eprobe = &pinfo->probe_data[i];
		EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_get(ob);

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
		if ((probe->flag & LIGHTPROBE_FLAG_SHOW_DATA) != 0) {
			DRW_shgroup_call_dynamic_add(stl->g_data->cube_display_shgrp, &ped->probe_id, ob->obmat[3], &probe->data_draw_size);
		}
	}

	/* IRRADIANCE GRID */
	int offset = 1; /* to account for the world probe */
	for (int i = 1; (ob = pinfo->probes_grid_ref[i]) && (i < MAX_GRID); i++) {
		LightProbe *probe = (LightProbe *)ob->data;
		EEVEE_LightGrid *egrid = &pinfo->grid_data[i];
		EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_get(ob);

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

		/* Debug Display */
		if ((probe->flag & LIGHTPROBE_FLAG_SHOW_DATA) != 0) {
			struct Gwn_Batch *geom = DRW_cache_sphere_get();
			DRWShadingGroup *grp = DRW_shgroup_instance_create(e_data.probe_grid_display_sh, psl->probe_display, geom);
			DRW_shgroup_set_instance_count(grp, ped->num_cell);
			DRW_shgroup_uniform_int(grp, "offset", &egrid->offset, 1);
			DRW_shgroup_uniform_ivec3(grp, "grid_resolution", egrid->resolution, 1);
			DRW_shgroup_uniform_vec3(grp, "corner", egrid->corner, 1);
			DRW_shgroup_uniform_vec3(grp, "increment_x", egrid->increment_x, 1);
			DRW_shgroup_uniform_vec3(grp, "increment_y", egrid->increment_y, 1);
			DRW_shgroup_uniform_vec3(grp, "increment_z", egrid->increment_z, 1);
			DRW_shgroup_uniform_buffer(grp, "irradianceGrid", &sldata->irradiance_pool);
			DRW_shgroup_uniform_float(grp, "sphere_size", &probe->data_draw_size, 1);
		}
	}
}

void EEVEE_lightprobes_cache_finish(EEVEE_SceneLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	Object *ob;

	/* Setup enough layers. */
	/* Free textures if number mismatch. */
	if (pinfo->num_cube != pinfo->cache_num_cube) {
		DRW_TEXTURE_FREE_SAFE(sldata->probe_pool);
	}

	if (pinfo->num_planar != pinfo->cache_num_planar) {
		DRW_TEXTURE_FREE_SAFE(vedata->txl->planar_pool);
		pinfo->cache_num_planar = pinfo->num_planar;
	}

	/* XXX this should be run each frame as it ensure planar_depth is set */
	planar_pool_ensure_alloc(vedata, pinfo->num_planar);

	/* Setup planar filtering pass */
	DRW_shgroup_set_instance_count(stl->g_data->planar_downsample, pinfo->num_planar);

	if (!sldata->probe_pool) {
		sldata->probe_pool = DRW_texture_create_2D_array(PROBE_OCTAHEDRON_SIZE, PROBE_OCTAHEDRON_SIZE, max_ff(1, pinfo->num_cube),
		                                                 DRW_TEX_RGB_11_11_10, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
		if (sldata->probe_filter_fb) {
			DRW_framebuffer_texture_attach(sldata->probe_filter_fb, sldata->probe_pool, 0, 0);
		}

		/* Tag probes to refresh */
		e_data.update_world = true;
		e_data.world_ready_to_shade = false;
		pinfo->num_render_cube = 0;
		pinfo->update_flag |= PROBE_UPDATE_CUBE;
		pinfo->cache_num_cube = pinfo->num_cube;

		for (int i = 1; (ob = pinfo->probes_cube_ref[i]) && (i < MAX_PROBE); i++) {
			EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_get(ob);
			ped->need_update = true;
			ped->ready_to_shade = false;
			ped->probe_id = 0;
		}
	}

	DRWFboTexture tex_filter = {&sldata->probe_pool, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP};

	DRW_framebuffer_init(&sldata->probe_filter_fb, &draw_engine_eevee_type, PROBE_OCTAHEDRON_SIZE, PROBE_OCTAHEDRON_SIZE, &tex_filter, 1);


#ifdef IRRADIANCE_SH_L2
	/* we need a signed format for Spherical Harmonics */
	int irradiance_format = DRW_TEX_RGBA_16;
#else
	int irradiance_format = DRW_TEX_RGB_11_11_10;
#endif

	/* TODO Allocate bigger storage if needed. */
	if (!sldata->irradiance_pool) {
		sldata->irradiance_pool = DRW_texture_create_2D(IRRADIANCE_POOL_SIZE, IRRADIANCE_POOL_SIZE, irradiance_format, DRW_TEX_FILTER, NULL);
		pinfo->num_render_grid = 0;
		pinfo->updated_bounce = 0;

		for (int i = 1; (ob = pinfo->probes_grid_ref[i]) && (i < MAX_PROBE); i++) {
			EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_get(ob);
			ped->need_update = true;
			ped->updated_cells = 0;
		}
	}

	if (!sldata->irradiance_rt) {
		sldata->irradiance_rt = DRW_texture_create_2D(IRRADIANCE_POOL_SIZE, IRRADIANCE_POOL_SIZE, irradiance_format, DRW_TEX_FILTER, NULL);
		pinfo->num_render_grid = 0;
		pinfo->updated_bounce = 0;

		for (int i = 1; (ob = pinfo->probes_grid_ref[i]) && (i < MAX_PROBE); i++) {
			EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_get(ob);
			ped->need_update = true;
			ped->updated_cells = 0;
		}
	}

	EEVEE_lightprobes_updates(sldata, vedata->psl, vedata->stl);
	EEVEE_planar_reflections_updates(sldata, vedata->psl, vedata->txl);

	DRW_uniformbuffer_update(sldata->probe_ubo, &sldata->probes->probe_data);
	DRW_uniformbuffer_update(sldata->grid_ubo, &sldata->probes->grid_data);
	DRW_uniformbuffer_update(sldata->planar_ubo, &sldata->probes->planar_data);
}

static void downsample_planar(void *vedata, int level)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;

	const float *size = DRW_viewport_size_get();
	copy_v2_v2(stl->g_data->texel_size, size);
	for (int i = 0; i < level - 1; ++i) {
		stl->g_data->texel_size[0] /= 2.0f;
		stl->g_data->texel_size[1] /= 2.0f;
		min_ff(floorf(stl->g_data->texel_size[0]), 1.0f);
		min_ff(floorf(stl->g_data->texel_size[1]), 1.0f);
	}
	invert_v2(stl->g_data->texel_size);

	DRW_draw_pass(psl->probe_planar_downsample_ps);
}

/* Glossy filter probe_rt to probe_pool at index probe_idx */
static void glossy_filter_probe(EEVEE_SceneLayerData *sldata, EEVEE_PassList *psl, int probe_idx)
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;

	/* 2 - Let gpu create Mipmaps for Filtered Importance Sampling. */
	/* Bind next framebuffer to be able to gen. mips for probe_rt. */
	DRW_framebuffer_bind(sldata->probe_filter_fb);
	DRW_texture_generate_mipmaps(sldata->probe_rt);

	/* 3 - Render to probe array to the specified layer, do prefiltering. */
	/* Detach to rebind the right mipmap. */
	DRW_framebuffer_texture_detach(sldata->probe_pool);
	float mipsize = PROBE_OCTAHEDRON_SIZE;
	const int maxlevel = (int)floorf(log2f(PROBE_OCTAHEDRON_SIZE));
	const int min_lod_level = 3;
	for (int i = 0; i < maxlevel - min_lod_level; i++) {
		float bias = (i == 0) ? 0.0f : 1.0f;
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
		pinfo->lodfactor = bias + 0.5f * log((float)(PROBE_RT_SIZE * PROBE_RT_SIZE) * pinfo->invsamples_ct) / log(2);
		pinfo->lodmax = floorf(log2f(PROBE_RT_SIZE)) - 2.0f;

		DRW_framebuffer_texture_attach(sldata->probe_filter_fb, sldata->probe_pool, 0, i);
		DRW_framebuffer_viewport_size(sldata->probe_filter_fb, 0, 0, mipsize, mipsize);
		DRW_draw_pass(psl->probe_glossy_compute);
		DRW_framebuffer_texture_detach(sldata->probe_pool);

		mipsize /= 2;
		CLAMP_MIN(mipsize, 1);
	}
	/* For shading, save max level of the octahedron map */
	pinfo->lodmax = (float)(maxlevel - min_lod_level) - 1.0f;

	/* reattach to have a valid framebuffer. */
	DRW_framebuffer_texture_attach(sldata->probe_filter_fb, sldata->probe_pool, 0, 0);
}

/* Diffuse filter probe_rt to irradiance_pool at index probe_idx */
static void diffuse_filter_probe(EEVEE_SceneLayerData *sldata, EEVEE_PassList *psl, int offset)
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;

	/* TODO do things properly */
	float lodmax = pinfo->lodmax;

	/* 4 - Compute spherical harmonics */
	/* Tweaking parameters to balance perf. vs precision */
	DRW_framebuffer_bind(sldata->probe_filter_fb);
	DRW_texture_generate_mipmaps(sldata->probe_rt);

	/* Bind the right texture layer (one layer per irradiance grid) */
	DRW_framebuffer_texture_detach(sldata->probe_pool);
	DRW_framebuffer_texture_attach(sldata->probe_filter_fb, sldata->irradiance_rt, 0, 0);

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

	int cell_per_row = IRRADIANCE_POOL_SIZE / size[0];
	int x = size[0] * (offset % cell_per_row);
	int y = size[1] * (offset / cell_per_row);

#ifndef IRRADIANCE_SH_L2
	const float bias = 0.0f;
	pinfo->invsamples_ct = 1.0f / pinfo->samples_ct;
	pinfo->lodfactor = bias + 0.5f * log((float)(PROBE_RT_SIZE * PROBE_RT_SIZE) * pinfo->invsamples_ct) / log(2);
	pinfo->lodmax = floorf(log2f(PROBE_RT_SIZE)) - 2.0f;
#else
	pinfo->shres = 32; /* Less texture fetches & reduce branches */
	pinfo->lodmax = 2.0f; /* Improve cache reuse */
#endif

	DRW_framebuffer_viewport_size(sldata->probe_filter_fb, x, y, size[0], size[1]);
	DRW_draw_pass(psl->probe_diffuse_compute);

	/* reattach to have a valid framebuffer. */
	DRW_framebuffer_texture_detach(sldata->irradiance_rt);
	DRW_framebuffer_texture_attach(sldata->probe_filter_fb, sldata->probe_pool, 0, 0);

	/* restore */
	pinfo->lodmax = lodmax;
}

/* Render the scene to the probe_rt texture. */
static void render_scene_to_probe(
        EEVEE_SceneLayerData *sldata, EEVEE_Data *vedata,
        const float pos[3], float clipsta, float clipend)
{
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_LightProbesInfo *pinfo = sldata->probes;

	float winmat[4][4], posmat[4][4], tmp_ao_dist, tmp_ao_samples;

	unit_m4(posmat);

	/* Move to capture position */
	negate_v3_v3(posmat[3], pos);

	/* Disable specular lighting when rendering probes to avoid feedback loops (looks bad). */
	sldata->probes->specular_toggle = false;

	/* Disable AO until we find a way to hide really bad discontinuities between cubefaces. */
	tmp_ao_dist = stl->effects->ao_dist;
	tmp_ao_samples = stl->effects->ao_samples;
	stl->effects->ao_dist = 0.0f;
	stl->effects->ao_samples = 0.0f;

	/* 1 - Render to each cubeface individually.
	 * We do this instead of using geometry shader because a) it's faster,
	 * b) it's easier than fixing the nodetree shaders (for view dependant effects). */
	pinfo->layer = 0;
	perspective_m4(winmat, -clipsta, clipsta, -clipsta, clipsta, clipsta, clipend);

	/* Avoid using the texture attached to framebuffer when rendering. */
	/* XXX */
	GPUTexture *tmp_planar_pool = txl->planar_pool;
	GPUTexture *tmp_minmaxz = stl->g_data->minmaxz;
	txl->planar_pool = e_data.planar_pool_placeholder;
	// stl->g_data->minmaxz = e_data.cube_face_minmaxz;

	/* Detach to rebind the right cubeface. */
	DRW_framebuffer_bind(sldata->probe_fb);
	DRW_framebuffer_texture_attach(sldata->probe_fb, e_data.cube_face_depth, 0, 0);
	DRW_framebuffer_texture_detach(sldata->probe_rt);
	for (int i = 0; i < 6; ++i) {
		float viewmat[4][4], persmat[4][4];
		float viewinv[4][4], persinv[4][4];

		DRW_framebuffer_cubeface_attach(sldata->probe_fb, sldata->probe_rt, 0, i, 0);
		DRW_framebuffer_viewport_size(sldata->probe_fb, 0, 0, PROBE_RT_SIZE, PROBE_RT_SIZE);

		DRW_framebuffer_clear(false, true, false, NULL, 1.0);

		/* Setup custom matrices */
		mul_m4_m4m4(viewmat, cubefacemat[i], posmat);
		mul_m4_m4m4(persmat, winmat, viewmat);
		invert_m4_m4(persinv, persmat);
		invert_m4_m4(viewinv, viewmat);

		DRW_viewport_matrix_override_set(persmat, DRW_MAT_PERS);
		DRW_viewport_matrix_override_set(persinv, DRW_MAT_PERSINV);
		DRW_viewport_matrix_override_set(viewmat, DRW_MAT_VIEW);
		DRW_viewport_matrix_override_set(viewinv, DRW_MAT_VIEWINV);
		DRW_viewport_matrix_override_set(winmat, DRW_MAT_WIN);

		DRW_draw_pass(psl->background_pass);

		/* Depth prepass */
		DRW_draw_pass(psl->depth_pass);
		DRW_draw_pass(psl->depth_pass_cull);

		// EEVEE_create_minmax_buffer(vedata, e_data.cube_face_depth);

		/* Rebind Planar FB */
		DRW_framebuffer_bind(sldata->probe_fb);

		/* Shading pass */
		EEVEE_draw_default_passes(psl);
		DRW_draw_pass(psl->material_pass);

		DRW_framebuffer_texture_detach(sldata->probe_rt);
	}
	DRW_framebuffer_texture_attach(sldata->probe_fb, sldata->probe_rt, 0, 0);
	DRW_framebuffer_texture_detach(e_data.cube_face_depth);

	DRW_viewport_matrix_override_unset(DRW_MAT_PERS);
	DRW_viewport_matrix_override_unset(DRW_MAT_PERSINV);
	DRW_viewport_matrix_override_unset(DRW_MAT_VIEW);
	DRW_viewport_matrix_override_unset(DRW_MAT_VIEWINV);
	DRW_viewport_matrix_override_unset(DRW_MAT_WIN);

	/* Restore */
	sldata->probes->specular_toggle = true;
	txl->planar_pool = tmp_planar_pool;
	stl->g_data->minmaxz = tmp_minmaxz;
	stl->effects->ao_dist = tmp_ao_dist;
	stl->effects->ao_samples = tmp_ao_samples;
}

static void render_scene_to_planar(
        EEVEE_Data *vedata, int layer,
        float (*viewmat)[4], float (*persmat)[4],
        float clip_plane[4])
{
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;

	float viewinv[4][4];
	float persinv[4][4];

	invert_m4_m4(viewinv, viewmat);
	invert_m4_m4(persinv, persmat);

	/* Attach depth here since it's a DRW_TEX_TEMP */
	DRW_framebuffer_texture_attach(fbl->planarref_fb, e_data.planar_depth, 0, 0);
	DRW_framebuffer_texture_layer_attach(fbl->planarref_fb, txl->planar_pool, 0, layer, 0);
	DRW_framebuffer_bind(fbl->planarref_fb);

	DRW_framebuffer_clear(false, true, false, NULL, 1.0);

	/* Avoid using the texture attached to framebuffer when rendering. */
	/* XXX */
	GPUTexture *tmp_planar_pool = txl->planar_pool;
	GPUTexture *tmp_minmaxz = stl->g_data->minmaxz;
	txl->planar_pool = e_data.planar_pool_placeholder;
	stl->g_data->minmaxz = e_data.planar_minmaxz;
	stl->g_data->background_alpha = FLT_MAX; /* Alpha is distance for planar reflections. */

	DRW_viewport_matrix_override_set(persmat, DRW_MAT_PERS);
	DRW_viewport_matrix_override_set(persinv, DRW_MAT_PERSINV);
	DRW_viewport_matrix_override_set(viewmat, DRW_MAT_VIEW);
	DRW_viewport_matrix_override_set(viewinv, DRW_MAT_VIEWINV);

	/* Background */
	DRW_draw_pass(psl->background_pass);

	/* Since we are rendering with an inverted view matrix, we need
	 * to invert the facing for backface culling to be the same. */
	DRW_state_invert_facing();
	DRW_state_clip_planes_add(clip_plane);

	/* Depth prepass */
	DRW_draw_pass(psl->depth_pass_clip);
	DRW_draw_pass(psl->depth_pass_clip_cull);

	EEVEE_create_minmax_buffer(vedata, e_data.planar_depth);

	/* Rebind Planar FB */
	DRW_framebuffer_bind(fbl->planarref_fb);

	/* Shading pass */
	EEVEE_draw_default_passes(psl);
	DRW_draw_pass(psl->material_pass);

	DRW_state_invert_facing();
	DRW_state_clip_planes_reset();

	/* Restore */
	txl->planar_pool = tmp_planar_pool;
	stl->g_data->minmaxz = tmp_minmaxz;
	stl->g_data->background_alpha = 1.0;
	DRW_viewport_matrix_override_unset(DRW_MAT_PERS);
	DRW_viewport_matrix_override_unset(DRW_MAT_PERSINV);
	DRW_viewport_matrix_override_unset(DRW_MAT_VIEW);
	DRW_viewport_matrix_override_unset(DRW_MAT_VIEWINV);

	DRW_framebuffer_texture_detach(txl->planar_pool);
	DRW_framebuffer_texture_detach(e_data.planar_depth);
}

static void render_world_to_probe(EEVEE_SceneLayerData *sldata, EEVEE_PassList *psl)
{
	EEVEE_LightProbesInfo *pinfo = sldata->probes;

	/* 1 - Render to cubemap target using geometry shader. */
	/* For world probe, we don't need to clear since we render the background directly. */
	pinfo->layer = 0;

	DRW_framebuffer_bind(sldata->probe_fb);
	DRW_draw_pass(psl->probe_background);
}

static void lightprobe_cell_location_get(EEVEE_LightGrid *egrid, int cell_idx, float r_pos[3])
{
	float tmp[3], local_cell[3];
	/* Keep in sync with lightprobe_grid_display_vert */
	local_cell[2] = (float)(cell_idx % egrid->resolution[2]);
	local_cell[1] = (float)((cell_idx / egrid->resolution[2]) % egrid->resolution[1]);
	local_cell[0] = (float)(cell_idx / (egrid->resolution[2] * egrid->resolution[1]));

	copy_v3_v3(r_pos, egrid->corner);
	mul_v3_v3fl(tmp, egrid->increment_x, local_cell[0]);
	add_v3_v3(r_pos, tmp);
	mul_v3_v3fl(tmp, egrid->increment_y, local_cell[1]);
	add_v3_v3(r_pos, tmp);
	mul_v3_v3fl(tmp, egrid->increment_z, local_cell[2]);
	add_v3_v3(r_pos, tmp);
}

void EEVEE_lightprobes_refresh(EEVEE_SceneLayerData *sldata, EEVEE_Data *vedata)
{
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_LightProbesInfo *pinfo = sldata->probes;
	Object *ob;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	RegionView3D *rv3d = draw_ctx->rv3d;

	/* Render world in priority */
	if (e_data.update_world) {
		render_world_to_probe(sldata, psl);
		glossy_filter_probe(sldata, psl, 0);
		diffuse_filter_probe(sldata, psl, 0);

		/* Swap and redo prefiltering for other rendertarget.
		 * This way we have world lighting waiting for irradiance grids to catch up. */
		SWAP(GPUTexture *, sldata->irradiance_pool, sldata->irradiance_rt);
		diffuse_filter_probe(sldata, psl, 0);

		e_data.update_world = false;

		if (!e_data.world_ready_to_shade) {
			e_data.world_ready_to_shade = true;
			pinfo->num_render_cube = 1;
			pinfo->num_render_grid = 1;
		}

		DRW_viewport_request_redraw();
	}
	else if (true) { /* TODO if at least one probe needs refresh */

		if (draw_ctx->evil_C != NULL) {
			/* Only compute probes if not navigating or in playback */
			struct wmWindowManager *wm = CTX_wm_manager(draw_ctx->evil_C);
			if (((rv3d->rflag & RV3D_NAVIGATING) != 0) || ED_screen_animation_no_scrub(wm) != NULL) {
				goto update_planar;
			}
		}

		/* Reflection probes depend on diffuse lighting thus on irradiance grid */
		const int max_bounce = 3;
		while (pinfo->updated_bounce < max_bounce) {
			pinfo->num_render_grid = pinfo->num_grid;

			for (int i = 1; (ob = pinfo->probes_grid_ref[i]) && (i < MAX_GRID); i++) {
				EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_get(ob);

				if (ped->need_update) {
					EEVEE_LightGrid *egrid = &pinfo->grid_data[i];
					LightProbe *prb = (LightProbe *)ob->data;
					int cell_id = ped->updated_cells;

					SWAP(GPUTexture *, sldata->irradiance_pool, sldata->irradiance_rt);

					/* Temporary Remove all probes. */
					int tmp_num_render_grid = pinfo->num_render_grid;
					int tmp_num_render_cube = pinfo->num_render_cube;
					int tmp_num_planar = pinfo->num_planar;
					pinfo->num_render_cube = 0;
					pinfo->num_planar = 0;

					/* Use light from previous bounce when capturing radiance. */
					if (pinfo->updated_bounce == 0) {
						pinfo->num_render_grid = 0;
					}

					float pos[3];
					lightprobe_cell_location_get(egrid, cell_id, pos);

					render_scene_to_probe(sldata, vedata, pos, prb->clipsta, prb->clipend);
					diffuse_filter_probe(sldata, psl, egrid->offset + cell_id);

					/* Restore */
					pinfo->num_render_grid = tmp_num_render_grid;
					pinfo->num_render_cube = tmp_num_render_cube;
					pinfo->num_planar = tmp_num_planar;

					/* To see what is going on. */
					SWAP(GPUTexture *, sldata->irradiance_pool, sldata->irradiance_rt);

					ped->updated_cells++;
					if (ped->updated_cells >= ped->num_cell) {
						ped->need_update = false;
					}

					/* Only do one probe per frame */
					DRW_viewport_request_redraw();
					goto update_planar;
				}
			}

			pinfo->updated_bounce++;
			pinfo->num_render_grid = pinfo->num_grid;

			if (pinfo->updated_bounce < max_bounce) {
				/* Retag all grids to update for next bounce */
				for (int i = 1; (ob = pinfo->probes_grid_ref[i]) && (i < MAX_GRID); i++) {
					EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_get(ob);
					ped->need_update = true;
					ped->updated_cells = 0;
				}
				SWAP(GPUTexture *, sldata->irradiance_pool, sldata->irradiance_rt);
			}
		}

		for (int i = 1; (ob = pinfo->probes_cube_ref[i]) && (i < MAX_PROBE); i++) {
			EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_get(ob);

			if (ped->need_update) {
				LightProbe *prb = (LightProbe *)ob->data;

				render_scene_to_probe(sldata, vedata, ob->obmat[3], prb->clipsta, prb->clipend);
				glossy_filter_probe(sldata, psl, i);

				ped->need_update = false;
				ped->probe_id = i;

				if (!ped->ready_to_shade) {
					pinfo->num_render_cube++;
					ped->ready_to_shade = true;
				}

				DRW_viewport_request_redraw();

				/* Only do one probe per frame */
				goto update_planar;
			}
		}
	}

update_planar:

	for (int i = 0; (ob = pinfo->probes_planar_ref[i]) && (i < MAX_PLANAR); i++) {
		EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_get(ob);

		if (ped->need_update) {
			/* Temporary Remove all planar reflections (avoid lag effect). */
			int tmp_num_planar = pinfo->num_planar;
			pinfo->num_planar = 0;

			render_scene_to_planar(vedata, i, ped->viewmat, ped->persmat, ped->planer_eq_offset);

			/* Restore */
			pinfo->num_planar = tmp_num_planar;

			ped->need_update = false;
			ped->probe_id = i;
		}
	}

	/* If there is at least one planar probe */
	if (pinfo->num_planar > 0) {
		DRW_framebuffer_recursive_downsample(vedata->fbl->minmaxz_fb, txl->planar_pool, 5, &downsample_planar, vedata);
	}
}

void EEVEE_lightprobes_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.probe_default_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_filter_glossy_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_filter_diffuse_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_grid_display_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_planar_display_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_planar_downsample_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_cube_display_sh);
	DRW_TEXTURE_FREE_SAFE(e_data.hammersley);
	DRW_TEXTURE_FREE_SAFE(e_data.planar_pool_placeholder);
}
