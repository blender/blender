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

#include "BLI_dynstr.h"

#include "DRW_render.h"

#include "GPU_material.h"
#include "GPU_texture.h"
#include "GPU_glew.h"

#include "eevee_engine.h"
#include "eevee_private.h"

typedef struct EEVEE_ProbeCubeData {
	short probe_id;
} EEVEE_ProbeCubeData;

/* TODO Option */
#define PROBE_CUBE_SIZE 512
#define PROBE_SIZE 1024

static struct {
	struct GPUShader *probe_default_sh;
	struct GPUShader *probe_filter_sh;
	struct GPUShader *probe_spherical_harmonic_sh;

	struct GPUTexture *hammersley;

	bool update_world;
} e_data = {NULL}; /* Engine data */

extern char datatoc_default_world_frag_glsl[];
extern char datatoc_probe_filter_frag_glsl[];
extern char datatoc_probe_sh_frag_glsl[];
extern char datatoc_probe_geom_glsl[];
extern char datatoc_probe_vert_glsl[];
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

void EEVEE_probes_init(EEVEE_SceneLayerData *sldata)
{
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
		        "#define HAMMERSLEY_SIZE 1024\n"
		        "#define NOISE_SIZE 64\n");

		e_data.probe_default_sh = DRW_shader_create(
		        datatoc_probe_vert_glsl, datatoc_probe_geom_glsl, datatoc_default_world_frag_glsl, NULL);

		MEM_freeN(shader_str);
	}

	/* Shaders */
	if (!e_data.hammersley) {
		e_data.hammersley = create_hammersley_sample_texture(1024);
		e_data.probe_spherical_harmonic_sh = DRW_shader_create_fullscreen(datatoc_probe_sh_frag_glsl, NULL);
	}

	if (!sldata->probes) {
		sldata->probes = MEM_callocN(sizeof(EEVEE_ProbesInfo), "EEVEE_ProbesInfo");
		sldata->probe_ubo = DRW_uniformbuffer_create(sizeof(EEVEE_Probe) * MAX_PROBE, NULL);
	}

	/* Setup Render Target Cubemap */
	if (!sldata->probe_rt) {
		sldata->probe_rt = DRW_texture_create_cube(PROBE_CUBE_SIZE, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
		sldata->probe_depth_rt = DRW_texture_create_cube(PROBE_CUBE_SIZE, DRW_TEX_DEPTH_24, DRW_TEX_FILTER, NULL);
	}

	DRWFboTexture tex_probe[2] = {{&sldata->probe_depth_rt, DRW_TEX_DEPTH_24, DRW_TEX_FILTER},
	                              {&sldata->probe_rt, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP}};

	DRW_framebuffer_init(&sldata->probe_fb, &draw_engine_eevee_type, PROBE_CUBE_SIZE, PROBE_CUBE_SIZE, tex_probe, 2);

	/* Spherical Harmonic Buffer */
	DRWFboTexture tex_sh = {&sldata->probe_sh, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP};

	DRW_framebuffer_init(&sldata->probe_sh_fb, &draw_engine_eevee_type, 9, 1, &tex_sh, 1);
}

void EEVEE_probes_cache_init(EEVEE_SceneLayerData *sldata, EEVEE_PassList *psl)
{
	EEVEE_ProbesInfo *pinfo = sldata->probes;

	pinfo->num_cube = 1; /* at least one for the world */
	memset(pinfo->probes_ref, 0, sizeof(pinfo->probes_ref));

	{
		psl->probe_background = DRW_pass_create("World Probe Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR);

		struct Batch *geom = DRW_cache_fullscreen_quad_get();
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
				struct GPUMaterial *gpumat = EEVEE_material_world_probe_get(scene, wo);

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
		psl->probe_prefilter = DRW_pass_create("Probe Filtering", DRW_STATE_WRITE_COLOR);

		struct Batch *geom = DRW_cache_fullscreen_quad_get();

		DRWShadingGroup *grp = DRW_shgroup_instance_create(e_data.probe_filter_sh, psl->probe_prefilter, geom);
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
		psl->probe_sh_compute = DRW_pass_create("Probe SH Compute", DRW_STATE_WRITE_COLOR);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.probe_spherical_harmonic_sh, psl->probe_sh_compute);
		DRW_shgroup_uniform_int(grp, "probeSize", &sldata->probes->shres, 1);
		DRW_shgroup_uniform_float(grp, "lodBias", &sldata->probes->lodfactor, 1);
		DRW_shgroup_uniform_texture(grp, "probeHdr", sldata->probe_rt);

		struct Batch *geom = DRW_cache_fullscreen_quad_get();
		DRW_shgroup_call_add(grp, geom, NULL);
	}
}

void EEVEE_probes_cache_add(EEVEE_SceneLayerData *UNUSED(sldata), Object *UNUSED(ob))
{
	return;
}

void EEVEE_probes_cache_finish(EEVEE_SceneLayerData *sldata)
{
	EEVEE_ProbesInfo *pinfo = sldata->probes;

	/* Setup enough layers. */
	/* Free textures if number mismatch. */
	if (pinfo->num_cube != pinfo->cache_num_cube) {
		DRW_TEXTURE_FREE_SAFE(sldata->probe_pool);
		pinfo->cache_num_cube = pinfo->num_cube;
		pinfo->update_flag |= PROBE_UPDATE_CUBE;
	}

	if (!sldata->probe_pool) {
		sldata->probe_pool = DRW_texture_create_2D_array(PROBE_SIZE, PROBE_SIZE, max_ff(1, pinfo->num_cube),
		                                                 DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
	}

	DRWFboTexture tex_filter = {&sldata->probe_pool, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP};

	DRW_framebuffer_init(&sldata->probe_filter_fb, &draw_engine_eevee_type, PROBE_SIZE, PROBE_SIZE, &tex_filter, 1);

	DRW_uniformbuffer_update(sldata->probe_ubo, &sldata->probes->probe_data);
}

/* Renders the probe with index probe_idx.
 * Renders the world probe if probe_idx = -1. */
static void render_one_probe(EEVEE_SceneLayerData *sldata, EEVEE_PassList *psl, int probe_idx)
{
	EEVEE_ProbesInfo *pinfo = sldata->probes;
	EEVEE_Probe *eprobe = &pinfo->probe_data[probe_idx];
	Object *ob = NULL;
	struct DRWPass *probe_pass;
	bool is_object_probe = (probe_idx > 0);

	float projmat[4][4], posmat[4][4];
	float near, far;
	float clear_color[4] = {0.5, 0.5, 0.5, 0.0f};

	unit_m4(posmat);

	if (is_object_probe) {
		ob = pinfo->probes_ref[probe_idx];
		near = 0.1f;
		far = 100.0f;

		/* Move to capture position */
		negate_v3_v3(posmat[3], ob->obmat[3]);
		probe_pass = psl->probe_background; /* TODO use objects in the scene */
	}
	else {
		/* World cubemap */
		near = 0.1f;
		far = 100.0f;
		probe_pass = psl->probe_background;
	}

	/* 1 - Render to cubemap target using geometry shader. */
	/* For world probe, we don't need to clear since we render the background directly. */
	pinfo->layer = 0;
	perspective_m4(projmat, -near, near, -near, near, near, far);

	for (int i = 0; i < 6; ++i) {
		float tmp[4][4];
		mul_m4_m4m4(tmp, cubefacemat[i], posmat);
		mul_m4_m4m4(pinfo->probemat[i], projmat, tmp);
	}

	DRW_framebuffer_bind(sldata->probe_fb);
	DRW_draw_pass(probe_pass);

	if (is_object_probe) {
		DRW_framebuffer_clear(true, true, false, clear_color, 1.0);
	}

	/* 2 - Let gpu create Mipmaps for Filtered Importance Sampling. */
	/* Bind next framebuffer to be able to gen. mips for probe_rt. */
	DRW_framebuffer_bind(sldata->probe_filter_fb);
	DRW_texture_generate_mipmaps(sldata->probe_rt);

	/* 3 - Render to probe array to the specified layer, do prefiltering. */
	/* Detach to rebind the right mipmap. */
	DRW_framebuffer_texture_detach(sldata->probe_pool);
	float mipsize = PROBE_SIZE;
	const int maxlevel = (int)floorf(log2f(PROBE_SIZE));
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
		pinfo->lodfactor = bias + 0.5f * log((float)(PROBE_CUBE_SIZE * PROBE_CUBE_SIZE) * pinfo->invsamples_ct) / log(2);
		pinfo->lodmax = floorf(log2f(PROBE_CUBE_SIZE)) - 2.0f;

		DRW_framebuffer_texture_attach(sldata->probe_filter_fb, sldata->probe_pool, 0, i);
		DRW_framebuffer_viewport_size(sldata->probe_filter_fb, mipsize, mipsize);
		DRW_draw_pass(psl->probe_prefilter);
		DRW_framebuffer_texture_detach(sldata->probe_pool);

		mipsize /= 2;
		CLAMP_MIN(mipsize, 1);
	}
	/* For shading, save max level of the octahedron map */
	pinfo->lodmax = (float)(maxlevel - min_lod_level) - 1.0f;

	/* 4 - Compute spherical harmonics */
	/* Tweaking parameters to balance perf. vs precision */
	pinfo->shres = 16; /* Less texture fetches & reduce branches */
	pinfo->lodfactor = 4.0f; /* Improve cache reuse */
	DRW_framebuffer_bind(sldata->probe_sh_fb);
	DRW_draw_pass(psl->probe_sh_compute);
	DRW_framebuffer_read_data(0, 0, 9, 1, 3, 0, (float *)eprobe->shcoefs);

	/* reattach to have a valid framebuffer. */
	DRW_framebuffer_texture_attach(sldata->probe_filter_fb, sldata->probe_pool, 0, 0);
}

void EEVEE_probes_refresh(EEVEE_SceneLayerData *sldata, EEVEE_PassList *psl)
{
	if (e_data.update_world) {
		render_one_probe(sldata, psl, 0);
		e_data.update_world = false;

		DRW_uniformbuffer_update(sldata->probe_ubo, &sldata->probes->probe_data);
	}
}

void EEVEE_probes_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.probe_default_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_filter_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_spherical_harmonic_sh);
	DRW_TEXTURE_FREE_SAFE(e_data.hammersley);
}
