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

/** \file eevee_engine.c
 *  \ingroup draw_engine
 */

#include "DRW_render.h"

#include "DNA_world_types.h"

#include "BLI_dynstr.h"
#include "BLI_rand.h"

#include "GPU_material.h"
#include "GPU_glew.h"

#include "eevee_engine.h"
#include "eevee_private.h"
#include "eevee_lut.h"

#define EEVEE_ENGINE "BLENDER_EEVEE"

/* *********** STATIC *********** */
static struct {
	char *frag_shader_lib;

	struct GPUShader *default_lit;
	struct GPUShader *default_world;
	struct GPUShader *default_background;
	struct GPUShader *depth_sh;
	struct GPUShader *tonemap;
	struct GPUShader *shadow_sh;

	struct GPUShader *probe_filter_sh;
	struct GPUShader *probe_spherical_harmonic_sh;

	struct GPUTexture *ltc_mat;
	struct GPUTexture *brdf_lut;
	struct GPUTexture *hammersley;
	struct GPUTexture *jitter;

	float camera_pos[3];
} e_data = {NULL}; /* Engine data */

extern char datatoc_default_frag_glsl[];
extern char datatoc_default_world_frag_glsl[];
extern char datatoc_ltc_lib_glsl[];
extern char datatoc_bsdf_lut_frag_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_direct_lib_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];
extern char datatoc_lit_surface_frag_glsl[];
extern char datatoc_lit_surface_vert_glsl[];
extern char datatoc_shadow_frag_glsl[];
extern char datatoc_shadow_geom_glsl[];
extern char datatoc_shadow_vert_glsl[];
extern char datatoc_probe_filter_frag_glsl[];
extern char datatoc_probe_sh_frag_glsl[];
extern char datatoc_probe_geom_glsl[];
extern char datatoc_probe_vert_glsl[];
extern char datatoc_background_vert_glsl[];

extern Material defmaterial;
extern GlobalsUboStorage ts;

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
		texels[i][0] = cos(phi);
		texels[i][1] = sinf(phi);
	}

	tex = DRW_texture_create_1D(samples, DRW_TEX_RG_16, DRW_TEX_WRAP, (float *)texels);
	MEM_freeN(texels);
	return tex;
}

static struct GPUTexture *create_jitter_texture(int w, int h)
{
	struct GPUTexture *tex;
	float (*texels)[2] = MEM_mallocN(sizeof(float[2]) * w * h, "jitter_tex");
	int i;

	/* TODO replace by something more evenly distributed like blue noise */
	for (i = 0; i < w * h; i++) {
		texels[i][0] = 2.0f * BLI_frand() - 1.0f;
		texels[i][1] = 2.0f * BLI_frand() - 1.0f;
		normalize_v2(texels[i]);
	}

	tex = DRW_texture_create_2D(w, h, DRW_TEX_RG_16, DRW_TEX_WRAP, (float *)texels);
	MEM_freeN(texels);
	return tex;
}

static struct GPUTexture *create_ggx_lut_texture(int UNUSED(w), int UNUSED(h))
{
	struct GPUTexture *tex;
#if 0 /* Used only to generate the LUT values */
	struct GPUFrameBuffer *fb = NULL;
	static float samples_ct = 8192.0f;
	static float inv_samples_ct = 1.0f / 8192.0f;

	char *lib_str = NULL;

	DynStr *ds_vert = BLI_dynstr_new();
	BLI_dynstr_append(ds_vert, datatoc_bsdf_common_lib_glsl);
	BLI_dynstr_append(ds_vert, datatoc_bsdf_sampling_lib_glsl);
	lib_str = BLI_dynstr_get_cstring(ds_vert);
	BLI_dynstr_free(ds_vert);

	struct GPUShader *sh = DRW_shader_create_with_lib(
	        datatoc_probe_vert_glsl, datatoc_probe_geom_glsl, datatoc_bsdf_lut_frag_glsl, lib_str,
	        "#define HAMMERSLEY_SIZE 8192\n"
	        "#define BRDF_LUT_SIZE 64\n"
	        "#define NOISE_SIZE 64\n");

	DRWPass *pass = DRW_pass_create("Probe Filtering", DRW_STATE_WRITE_COLOR);
	DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
	DRW_shgroup_uniform_float(grp, "sampleCount", &samples_ct, 1);
	DRW_shgroup_uniform_float(grp, "invSampleCount", &inv_samples_ct, 1);
	DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
	DRW_shgroup_uniform_texture(grp, "texJitter", e_data.jitter);

	struct Batch *geom = DRW_cache_fullscreen_quad_get();
	DRW_shgroup_call_add(grp, geom, NULL);

	float *texels = MEM_mallocN(sizeof(float[2]) * w * h, "lut");

	tex = DRW_texture_create_2D(w, h, DRW_TEX_RG_16, DRW_TEX_FILTER, (float *)texels);

	DRWFboTexture tex_filter = {&tex, DRW_BUF_RG_16, DRW_TEX_FILTER};
	DRW_framebuffer_init(&fb, w, h, &tex_filter, 1);

	DRW_framebuffer_bind(fb);
	DRW_draw_pass(pass);

	float *data = MEM_mallocN(sizeof(float[3]) * w * h, "lut");
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glReadPixels(0, 0, w, h, GL_RGB, GL_FLOAT, data);

	printf("{");
	for (int i = 0; i < w*h * 3; i+=3) {
		printf("%ff, %ff, ", data[i],  data[i+1]); i+=3;
		printf("%ff, %ff, ", data[i],  data[i+1]); i+=3;
		printf("%ff, %ff, ", data[i],  data[i+1]); i+=3;
		printf("%ff, %ff, \n", data[i],  data[i+1]);
	}
	printf("}");

	MEM_freeN(texels);
	MEM_freeN(data);
#else
	float (*texels)[3] = MEM_mallocN(sizeof(float[3]) * 64 * 64, "bsdf lut texels");

	for (int i = 0; i < 64 * 64; i++) {
		texels[i][0] = bsdf_split_sum_ggx[i*2 + 0];
		texels[i][1] = bsdf_split_sum_ggx[i*2 + 1];
		texels[i][2] = ltc_mag_ggx[i];
	}

	tex = DRW_texture_create_2D(64, 64, DRW_TEX_RGB_16, DRW_TEX_FILTER, (float *)texels);
	MEM_freeN(texels);
#endif

	return tex;
}


/* *********** FUNCTIONS *********** */

static void EEVEE_engine_init(void *ved)
{
	EEVEE_Data *vedata = (EEVEE_Data *)ved;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_StorageList *stl = vedata->stl;

	DRWFboTexture tex = {&txl->color, DRW_BUF_RGBA_16, DRW_TEX_FILTER};

	const float *viewport_size = DRW_viewport_size_get();
	DRW_framebuffer_init(&fbl->main,
	                    (int)viewport_size[0], (int)viewport_size[1],
	                    &tex, 1);

	if (!e_data.frag_shader_lib) {
		DynStr *ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, datatoc_bsdf_common_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_ltc_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_bsdf_direct_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_lit_surface_frag_glsl);
		e_data.frag_shader_lib = BLI_dynstr_get_cstring(ds_frag);
		BLI_dynstr_free(ds_frag);
	}

	if (!e_data.depth_sh) {
		e_data.depth_sh = DRW_shader_create_3D_depth_only();
	}

	if (!e_data.default_lit) {
		char *frag_str = NULL;

		DynStr *ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, e_data.frag_shader_lib);
		BLI_dynstr_append(ds_frag, datatoc_default_frag_glsl);
		frag_str = BLI_dynstr_get_cstring(ds_frag);
		BLI_dynstr_free(ds_frag);

		e_data.default_lit = DRW_shader_create(
		        datatoc_lit_surface_vert_glsl, NULL, frag_str,
		        "#define MAX_LIGHT 128\n"
		        "#define MAX_SHADOW_CUBE 42\n"
		        "#define MAX_SHADOW_MAP 64\n"
		        "#define MAX_SHADOW_CASCADE 8\n"
		        "#define MAX_CASCADE_NUM 4\n");

		MEM_freeN(frag_str);
	}

	if (!e_data.shadow_sh) {
		e_data.shadow_sh = DRW_shader_create(
		        datatoc_shadow_vert_glsl, datatoc_shadow_geom_glsl, datatoc_shadow_frag_glsl, NULL);
	}

	if (!e_data.default_world) {
		e_data.default_world = DRW_shader_create(
		        datatoc_probe_vert_glsl, datatoc_probe_geom_glsl, datatoc_default_world_frag_glsl, NULL);
	}

	if (!e_data.default_background) {
		e_data.default_background = DRW_shader_create_fullscreen(datatoc_default_world_frag_glsl, NULL);
	}

	if (!e_data.probe_filter_sh) {
		char *shader_str = NULL;

		DynStr *ds_frag = BLI_dynstr_new();
		BLI_dynstr_append(ds_frag, datatoc_bsdf_common_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_bsdf_sampling_lib_glsl);
		BLI_dynstr_append(ds_frag, datatoc_probe_filter_frag_glsl);
		shader_str = BLI_dynstr_get_cstring(ds_frag);
		BLI_dynstr_free(ds_frag);

		e_data.probe_filter_sh = DRW_shader_create(
		        datatoc_probe_vert_glsl, datatoc_probe_geom_glsl, shader_str,
		        "#define HAMMERSLEY_SIZE 8192\n"
		        "#define NOISE_SIZE 64\n");

		MEM_freeN(shader_str);
	}

	if (!e_data.probe_spherical_harmonic_sh) {
		e_data.probe_spherical_harmonic_sh = DRW_shader_create_fullscreen(datatoc_probe_sh_frag_glsl, NULL);
	}

	if (!e_data.ltc_mat) {
		e_data.ltc_mat = DRW_texture_create_2D(64, 64, DRW_TEX_RGBA_16, DRW_TEX_FILTER, ltc_mat_ggx);
	}

	if (!e_data.hammersley) {
		e_data.hammersley = create_hammersley_sample_texture(8192);
	}

	if (!e_data.jitter) {
		e_data.jitter = create_jitter_texture(64, 64);
	}

	if (!e_data.brdf_lut) {
		e_data.brdf_lut = create_ggx_lut_texture(64, 64);
	}

	{
		float viewinvmat[4][4];
		DRW_viewport_matrix_get(viewinvmat, DRW_MAT_VIEWINV);

		copy_v3_v3(e_data.camera_pos, viewinvmat[3]);
	}

	EEVEE_lights_init(stl);

	EEVEE_probes_init(vedata);

	EEVEE_effects_init(vedata);

	// EEVEE_lights_update(stl);
}

static DRWShadingGroup *eevee_cube_shgroup(struct GPUShader *sh, DRWPass *pass, struct Batch *geom)
{
	DRWShadingGroup *grp = DRW_shgroup_instance_create(sh, pass, geom);

	for (int i = 0; i < 6; ++i)
		DRW_shgroup_call_dynamic_add_empty(grp);

	return grp;
}

static DRWShadingGroup *eevee_cube_shadow_shgroup(
        EEVEE_PassList *psl, EEVEE_StorageList *stl, struct Batch *geom, float (*obmat)[4])
{
	DRWShadingGroup *grp = DRW_shgroup_instance_create(e_data.shadow_sh, psl->shadow_cube_pass, geom);
	DRW_shgroup_uniform_block(grp, "shadow_render_block", stl->shadow_render_ubo);
	DRW_shgroup_uniform_mat4(grp, "ShadowModelMatrix", (float *)obmat);

	for (int i = 0; i < 6; ++i)
		DRW_shgroup_call_dynamic_add_empty(grp);

	return grp;
}

static DRWShadingGroup *eevee_cascade_shadow_shgroup(
        EEVEE_PassList *psl, EEVEE_StorageList *stl, struct Batch *geom, float (*obmat)[4])
{
	DRWShadingGroup *grp = DRW_shgroup_instance_create(e_data.shadow_sh, psl->shadow_cascade_pass, geom);
	DRW_shgroup_uniform_block(grp, "shadow_render_block", stl->shadow_render_ubo);
	DRW_shgroup_uniform_mat4(grp, "ShadowModelMatrix", (float *)obmat);

	for (int i = 0; i < MAX_CASCADE_NUM; ++i)
		DRW_shgroup_call_dynamic_add_empty(grp);

	return grp;
}

static void EEVEE_cache_init(void *vedata)
{
	static int zero = 0;

	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_TextureList *txl = ((EEVEE_Data *)vedata)->txl;
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	{
		psl->shadow_cube_pass = DRW_pass_create("Shadow Cube Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
	}

	{
		psl->shadow_cascade_pass = DRW_pass_create("Shadow Cascade Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
	}

	{
		// psl->shadow_pass = DRW_pass_create("Shadow Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
		// stl->g_data->shadow_shgrp = DRW_shgroup_create(e_data.shadow_sh, psl->shadow_pass);
		// DRW_shgroup_uniform_mat4(stl->g_data->shadow_shgrp, "ShadowMatrix", (float *)stl->lamps->shadowmat);
		// DRW_shgroup_uniform_int(stl->g_data->shadow_shgrp, "Layer", &stl->lamps->layer, 1);
	}

	{
		psl->probe_background = DRW_pass_create("Probe Background Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR);

		struct Batch *geom = DRW_cache_fullscreen_quad_get();
		DRWShadingGroup *grp = NULL;

		const DRWContextState *draw_ctx = DRW_context_state_get();
		Scene *scene = draw_ctx->scene;
		World *wo = scene->world;

		float *col = ts.colorBackground;
		if (wo) {
			col = &wo->horr;
		}

		if (wo && wo->use_nodes && wo->nodetree) {
			struct GPUMaterial *gpumat = GPU_material_from_nodetree(
				scene, wo->nodetree, &wo->gpumaterial, &DRW_engine_viewport_eevee_type, 0,
			    datatoc_probe_vert_glsl, datatoc_probe_geom_glsl, e_data.frag_shader_lib,
			    "#define PROBE_CAPTURE\n"
			    "#define MAX_LIGHT 128\n"
			    "#define MAX_SHADOW_CUBE 42\n"
			    "#define MAX_SHADOW_MAP 64\n"
			    "#define MAX_SHADOW_CASCADE 8\n"
			    "#define MAX_CASCADE_NUM 4\n");

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

		/* Fallback if shader fails or if not using nodetree. */
		if (grp == NULL) {
			grp = eevee_cube_shgroup(e_data.default_world, psl->probe_background, geom);
			DRW_shgroup_uniform_vec3(grp, "color", col, 1);
			DRW_shgroup_uniform_int(grp, "Layer", &zero, 1);
		}
	}

	{
		psl->background_pass = DRW_pass_create("Background Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR);

		struct Batch *geom = DRW_cache_fullscreen_quad_get();
		DRWShadingGroup *grp = NULL;

		const DRWContextState *draw_ctx = DRW_context_state_get();
		Scene *scene = draw_ctx->scene;
		World *wo = scene->world;

		float *col = ts.colorBackground;
		if (wo) {
			col = &wo->horr;
		}

		if (wo && wo->use_nodes && wo->nodetree) {
			struct GPUMaterial *gpumat = GPU_material_from_nodetree(
				scene, wo->nodetree, &wo->gpumaterial, &DRW_engine_viewport_eevee_type, 1,
			    datatoc_background_vert_glsl, NULL, e_data.frag_shader_lib,
			    "#define WORLD_BACKGROUND\n"
			    "#define MAX_LIGHT 128\n"
			    "#define MAX_SHADOW_CUBE 42\n"
			    "#define MAX_SHADOW_MAP 64\n"
			    "#define MAX_SHADOW_CASCADE 8\n"
			    "#define MAX_CASCADE_NUM 4\n");

			grp = DRW_shgroup_material_create(gpumat, psl->background_pass);

			if (grp) {
				DRW_shgroup_call_add(grp, geom, NULL);
			}
			else {
				/* Shader failed : pink background */
				static float pink[3] = {1.0f, 0.0f, 1.0f};
				col = pink;
			}
		}

		/* Fallback if shader fails or if not using nodetree. */
		if (grp == NULL) {
			grp = DRW_shgroup_create(e_data.default_background, psl->background_pass);
			DRW_shgroup_uniform_vec3(grp, "color", col, 1);
			DRW_shgroup_call_add(grp, geom, NULL);
		}
	}

	{
		psl->probe_prefilter = DRW_pass_create("Probe Filtering", DRW_STATE_WRITE_COLOR);

		struct Batch *geom = DRW_cache_fullscreen_quad_get();
		DRWShadingGroup *grp = eevee_cube_shgroup(e_data.probe_filter_sh, psl->probe_prefilter, geom);
		DRW_shgroup_uniform_float(grp, "sampleCount", &stl->probes->samples_ct, 1);
		DRW_shgroup_uniform_float(grp, "invSampleCount", &stl->probes->invsamples_ct, 1);
		DRW_shgroup_uniform_float(grp, "roughnessSquared", &stl->probes->roughness, 1);
		DRW_shgroup_uniform_float(grp, "lodFactor", &stl->probes->lodfactor, 1);
		DRW_shgroup_uniform_float(grp, "lodMax", &stl->probes->lodmax, 1);
		DRW_shgroup_uniform_int(grp, "Layer", &stl->probes->layer, 1);
		DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
		// DRW_shgroup_uniform_texture(grp, "texJitter", e_data.jitter);
		DRW_shgroup_uniform_texture(grp, "probeHdr", txl->probe_rt);
	}

	{
		psl->probe_sh_compute = DRW_pass_create("Probe SH Compute", DRW_STATE_WRITE_COLOR);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.probe_spherical_harmonic_sh, psl->probe_sh_compute);
		DRW_shgroup_uniform_int(grp, "probeSize", &stl->probes->shres, 1);
		DRW_shgroup_uniform_float(grp, "lodBias", &stl->probes->lodfactor, 1);
		DRW_shgroup_uniform_texture(grp, "probeHdr", txl->probe_rt);

		struct Batch *geom = DRW_cache_fullscreen_quad_get();
		DRW_shgroup_call_add(grp, geom, NULL);
	}

	{
		psl->depth_pass = DRW_pass_create("Depth Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
		stl->g_data->depth_shgrp = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass);

		psl->depth_pass_cull = DRW_pass_create(
		        "Depth Pass Cull",
		        DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_CULL_BACK);
		stl->g_data->depth_shgrp_cull = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass_cull);
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;
		psl->default_pass = DRW_pass_create("Default Shader Pass", state);

		stl->g_data->default_lit_grp = DRW_shgroup_create(e_data.default_lit, psl->default_pass);
		DRW_shgroup_uniform_block(stl->g_data->default_lit_grp, "light_block", stl->light_ubo);
		DRW_shgroup_uniform_block(stl->g_data->default_lit_grp, "shadow_block", stl->shadow_ubo);
		DRW_shgroup_uniform_int(stl->g_data->default_lit_grp, "light_count", &stl->lamps->num_light, 1);
		DRW_shgroup_uniform_float(stl->g_data->default_lit_grp, "lodMax", &stl->probes->lodmax, 1);
		DRW_shgroup_uniform_vec3(stl->g_data->default_lit_grp, "shCoefs[0]", (float *)stl->probes->shcoefs, 9);
		DRW_shgroup_uniform_vec3(stl->g_data->default_lit_grp, "cameraPos", e_data.camera_pos, 1);
		DRW_shgroup_uniform_texture(stl->g_data->default_lit_grp, "ltcMat", e_data.ltc_mat);
		DRW_shgroup_uniform_texture(stl->g_data->default_lit_grp, "brdfLut", e_data.brdf_lut);
		DRW_shgroup_uniform_texture(stl->g_data->default_lit_grp, "probeFiltered", txl->probe_pool);
		/* NOTE : Adding Shadow Map textures uniform in EEVEE_cache_finish */
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;
		psl->material_pass = DRW_pass_create("Material Shader Pass", state);
	}


	EEVEE_lights_cache_init(stl);

	EEVEE_effects_cache_init(vedata);
}

static void EEVEE_cache_populate(void *vedata, Object *ob)
{
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
	EEVEE_TextureList *txl = ((EEVEE_Data *)vedata)->txl;
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;

	struct Batch *geom = DRW_cache_object_surface_get(ob);
	if (geom) {
		IDProperty *ces_mode_ob = BKE_layer_collection_engine_evaluated_get(ob, COLLECTION_MODE_OBJECT, "");
		const bool do_cull = BKE_collection_engine_property_value_get_bool(ces_mode_ob, "show_backface_culling");

		/* Depth Prepass */
		DRW_shgroup_call_add((do_cull) ? stl->g_data->depth_shgrp_cull : stl->g_data->depth_shgrp, geom, ob->obmat);

		/* Get per-material split surface */
		struct Batch **mat_geom = DRW_cache_object_surface_material_get(ob);
		if (mat_geom) {
			for (int i = 0; i < MAX2(1, ob->totcol); ++i) {
				Material *ma = give_current_material(ob, i + 1);

				if (ma == NULL)
					ma = &defmaterial;

				if (ma->use_nodes && ma->nodetree) {
					const DRWContextState *draw_ctx = DRW_context_state_get();
					Scene *scene = draw_ctx->scene;
					struct GPUMaterial *gpumat = GPU_material_from_nodetree(
					    scene, ma->nodetree, &ma->gpumaterial, &DRW_engine_viewport_eevee_type, 0,
					    datatoc_lit_surface_vert_glsl, NULL, e_data.frag_shader_lib,
					    "#define PROBE_CAPTURE\n"
					    "#define MAX_LIGHT 128\n"
					    "#define MAX_SHADOW_CUBE 42\n"
					    "#define MAX_SHADOW_MAP 64\n"
					    "#define MAX_SHADOW_CASCADE 8\n"
					    "#define MAX_CASCADE_NUM 4\n");

					DRWShadingGroup *shgrp = DRW_shgroup_material_create(gpumat, psl->material_pass);

					if (shgrp) {
						DRW_shgroup_call_add(shgrp, mat_geom[i], ob->obmat);

						DRW_shgroup_uniform_block(shgrp, "light_block", stl->light_ubo);
						DRW_shgroup_uniform_block(shgrp, "shadow_block", stl->shadow_ubo);
						DRW_shgroup_uniform_int(shgrp, "light_count", &stl->lamps->num_light, 1);
						DRW_shgroup_uniform_float(shgrp, "lodMax", &stl->probes->lodmax, 1);
						DRW_shgroup_uniform_vec3(shgrp, "shCoefs[0]", (float *)stl->probes->shcoefs, 9);
						DRW_shgroup_uniform_vec3(shgrp, "cameraPos", e_data.camera_pos, 1);
						DRW_shgroup_uniform_texture(shgrp, "ltcMat", e_data.ltc_mat);
						DRW_shgroup_uniform_texture(shgrp, "brdfLut", e_data.brdf_lut);
						DRW_shgroup_uniform_texture(shgrp, "probeFiltered", txl->probe_pool);
					}
					else {
						/* Shader failed : pink color */
						static float col[3] = {1.0f, 0.0f, 1.0f};
						static float spec[3] = {1.0f, 0.0f, 1.0f};
						static short hardness = 1;
						shgrp = DRW_shgroup_create(e_data.default_lit, psl->default_pass);
						DRW_shgroup_uniform_vec3(shgrp, "diffuse_col", col, 1);
						DRW_shgroup_uniform_vec3(shgrp, "specular_col", spec, 1);
						DRW_shgroup_uniform_short(shgrp, "hardness", &hardness, 1);
						DRW_shgroup_uniform_texture(stl->g_data->default_lit_grp, "ltcMat", e_data.ltc_mat);
						DRW_shgroup_uniform_texture(stl->g_data->default_lit_grp, "brdfLut", e_data.brdf_lut);
						DRW_shgroup_uniform_texture(stl->g_data->default_lit_grp, "probeFiltered", txl->probe_pool);
						DRW_shgroup_call_add(shgrp, mat_geom[i], ob->obmat);
					}
				}
				else {
					DRWShadingGroup *shgrp = DRW_shgroup_create(e_data.default_lit, psl->default_pass);
					DRW_shgroup_uniform_vec3(shgrp, "diffuse_col", &ma->r, 1);
					DRW_shgroup_uniform_vec3(shgrp, "specular_col", &ma->specr, 1);
					DRW_shgroup_uniform_short(shgrp, "hardness", &ma->har, 1);
					DRW_shgroup_uniform_texture(stl->g_data->default_lit_grp, "ltcMat", e_data.ltc_mat);
					DRW_shgroup_uniform_texture(stl->g_data->default_lit_grp, "brdfLut", e_data.brdf_lut);
					DRW_shgroup_uniform_texture(stl->g_data->default_lit_grp, "probeFiltered", txl->probe_pool);
					DRW_shgroup_call_add(shgrp, mat_geom[i], ob->obmat);
				}

			}
		}
		// GPUMaterial *gpumat = GPU_material_from_nodetree(struct bNodeTree *ntree, ListBase *gpumaterials, void *engine_type, int options)

		// DRW_shgroup_call_add(stl->g_data->shadow_shgrp, geom, ob->obmat);
		eevee_cascade_shadow_shgroup(psl, stl, geom, ob->obmat);
		eevee_cube_shadow_shgroup(psl, stl, geom, ob->obmat);
	}
	else if (ob->type == OB_LAMP) {
		EEVEE_lights_cache_add(stl, ob);
	}
}

typedef struct eevee_bind_shadow_data {
	struct GPUTexture *shadow_depth_map_pool;
	struct GPUTexture *shadow_depth_cube_pool;
	struct GPUTexture *shadow_depth_cascade_pool;
} eevee_bind_shadow_data;

static void eevee_bind_shadow(void *data, DRWShadingGroup *shgrp)
{
	eevee_bind_shadow_data *shdw_data = data;
	DRW_shgroup_uniform_texture(shgrp, "shadowMaps", shdw_data->shadow_depth_map_pool);
	DRW_shgroup_uniform_texture(shgrp, "shadowCubes", shdw_data->shadow_depth_cube_pool);
	DRW_shgroup_uniform_texture(shgrp, "shadowCascades", shdw_data->shadow_depth_cascade_pool);
}

static void EEVEE_cache_finish(void *vedata)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
	EEVEE_TextureList *txl = ((EEVEE_Data *)vedata)->txl;
	EEVEE_FramebufferList *fbl = ((EEVEE_Data *)vedata)->fbl;

	EEVEE_lights_cache_finish(stl, txl, fbl);

	/* Shadows binding */
	eevee_bind_shadow_data data;

	data.shadow_depth_map_pool = txl->shadow_depth_map_pool;
	data.shadow_depth_cube_pool = txl->shadow_depth_cube_pool;
	data.shadow_depth_cascade_pool = txl->shadow_depth_cascade_pool;

	DRW_pass_foreach_shgroup(psl->default_pass, eevee_bind_shadow, &data);
	DRW_pass_foreach_shgroup(psl->material_pass, eevee_bind_shadow, &data);
}

static void EEVEE_draw_scene(void *vedata)
{
	EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
	EEVEE_FramebufferList *fbl = ((EEVEE_Data *)vedata)->fbl;

	/* Default framebuffer and texture */
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	/* Refresh Probes */
	EEVEE_refresh_probe((EEVEE_Data *)vedata);

	/* Refresh shadows */
	EEVEE_draw_shadows((EEVEE_Data *)vedata);

	/* Attach depth to the hdr buffer and bind it */	
	DRW_framebuffer_texture_detach(dtxl->depth);
	DRW_framebuffer_texture_attach(fbl->main, dtxl->depth, 0, 0);
	DRW_framebuffer_bind(fbl->main);
	DRW_framebuffer_clear(false, true, false, NULL, 1.0f);

	DRW_draw_pass(psl->background_pass);
	DRW_draw_pass(psl->depth_pass);
	DRW_draw_pass(psl->depth_pass_cull);
	DRW_draw_pass(psl->default_pass);
	DRW_draw_pass(psl->material_pass);

	EEVEE_draw_effects(vedata);
}

static void EEVEE_engine_free(void)
{
	EEVEE_effects_free();

	MEM_SAFE_FREE(e_data.frag_shader_lib);
	DRW_SHADER_FREE_SAFE(e_data.default_lit);
	DRW_SHADER_FREE_SAFE(e_data.shadow_sh);
	DRW_SHADER_FREE_SAFE(e_data.default_world);
	DRW_SHADER_FREE_SAFE(e_data.default_background);
	DRW_SHADER_FREE_SAFE(e_data.probe_filter_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_spherical_harmonic_sh);
	DRW_TEXTURE_FREE_SAFE(e_data.ltc_mat);
	DRW_TEXTURE_FREE_SAFE(e_data.brdf_lut);
	DRW_TEXTURE_FREE_SAFE(e_data.hammersley);
	DRW_TEXTURE_FREE_SAFE(e_data.jitter);
}

static void EEVEE_layer_collection_settings_create(RenderEngine *UNUSED(engine), IDProperty *props)
{
	BLI_assert(props &&
	           props->type == IDP_GROUP &&
	           props->subtype == IDP_GROUP_SUB_ENGINE_RENDER);
	// BKE_collection_engine_property_add_int(props, "high_quality_sphere_lamps", false);
	UNUSED_VARS_NDEBUG(props);
}


static void EEVEE_scene_layer_settings_create(RenderEngine *UNUSED(engine), IDProperty *props)
{
	BLI_assert(props &&
	           props->type == IDP_GROUP &&
	           props->subtype == IDP_GROUP_SUB_ENGINE_RENDER);

	BKE_collection_engine_property_add_bool(props, "dof_enable", false);
	BKE_collection_engine_property_add_float(props, "bokeh_max_size", 100.0f);
	BKE_collection_engine_property_add_float(props, "bokeh_threshold", 1.0f);

	BKE_collection_engine_property_add_bool(props, "bloom_enable", false);
	BKE_collection_engine_property_add_float(props, "bloom_threshold", 0.8f);
	BKE_collection_engine_property_add_float(props, "bloom_knee", 0.5f);
	BKE_collection_engine_property_add_float(props, "bloom_intensity", 0.8f);
	BKE_collection_engine_property_add_float(props, "bloom_radius", 6.5f);

	BKE_collection_engine_property_add_bool(props, "motion_blur_enable", false);
	BKE_collection_engine_property_add_int(props, "motion_blur_samples", 8);
	BKE_collection_engine_property_add_float(props, "motion_blur_shutter", 1.0f);
}

static const DrawEngineDataSize EEVEE_data_size = DRW_VIEWPORT_DATA_SIZE(EEVEE_Data);

DrawEngineType draw_engine_eevee_type = {
	NULL, NULL,
	N_("Eevee"),
	&EEVEE_data_size,
	&EEVEE_engine_init,
	&EEVEE_engine_free,
	&EEVEE_cache_init,
	&EEVEE_cache_populate,
	&EEVEE_cache_finish,
	&EEVEE_draw_scene,
	NULL//&EEVEE_draw_scene
};

RenderEngineType DRW_engine_viewport_eevee_type = {
	NULL, NULL,
	EEVEE_ENGINE, N_("Eevee"), RE_INTERNAL | RE_USE_SHADING_NODES,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	&EEVEE_layer_collection_settings_create, &EEVEE_scene_layer_settings_create,
	&draw_engine_eevee_type,
	{NULL, NULL, NULL}
};


#undef EEVEE_ENGINE
