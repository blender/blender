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

#include "eevee_engine.h"
#include "eevee_private.h"
#include "GPU_texture.h"
#include "GPU_glew.h"

typedef struct EEVEE_ProbeData {
	short probe_id, shadow_id;
} EEVEE_ProbeData;

/* TODO Option */
#define PROBE_CUBE_SIZE 512
#define PROBE_SIZE 1024

static struct {
	struct GPUShader *probe_filter_sh;
	struct GPUShader *probe_spherical_harmonic_sh;

	struct GPUTexture *hammersley;

	float camera_pos[3];
} e_data = {NULL}; /* Engine data */

extern char datatoc_probe_filter_frag_glsl[];
extern char datatoc_probe_sh_frag_glsl[];
extern char datatoc_probe_geom_glsl[];
extern char datatoc_probe_vert_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];

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
		texels[i][0] = cos(phi);
		texels[i][1] = sinf(phi);
	}

	tex = DRW_texture_create_1D(samples, DRW_TEX_RG_16, DRW_TEX_WRAP, (float *)texels);
	MEM_freeN(texels);
	return tex;
}

void EEVEE_probes_init(EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;

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

		MEM_freeN(shader_str);
	}

	if (!e_data.hammersley) {
		e_data.hammersley = create_hammersley_sample_texture(1024);
	}

	if (!e_data.probe_spherical_harmonic_sh) {
		e_data.probe_spherical_harmonic_sh = DRW_shader_create_fullscreen(datatoc_probe_sh_frag_glsl, NULL);
	}

	if (!stl->probes) {
		stl->probes       = MEM_callocN(sizeof(EEVEE_ProbesInfo), "EEVEE_ProbesInfo");
	}

	if (!txl->probe_rt) {
		txl->probe_rt = DRW_texture_create_cube(PROBE_CUBE_SIZE, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
		txl->probe_depth_rt = DRW_texture_create_cube(PROBE_CUBE_SIZE, DRW_TEX_DEPTH_24, DRW_TEX_FILTER, NULL);
	}

	DRWFboTexture tex_probe[2] = {{&txl->probe_depth_rt, DRW_TEX_DEPTH_24, DRW_TEX_FILTER},
	                              {&txl->probe_rt, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP}};

	DRW_framebuffer_init(&fbl->probe_fb, &draw_engine_eevee_type, PROBE_CUBE_SIZE, PROBE_CUBE_SIZE, tex_probe, 2);

	if (!txl->probe_pool) {
		/* TODO array */
		txl->probe_pool = DRW_texture_create_2D(PROBE_SIZE, PROBE_SIZE, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
	}

	DRWFboTexture tex_filter = {&txl->probe_pool, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP};

	DRW_framebuffer_init(&fbl->probe_filter_fb, &draw_engine_eevee_type, PROBE_SIZE, PROBE_SIZE, &tex_filter, 1);

	/* Spherical Harmonic Buffer */
	DRWFboTexture tex_sh = {&txl->probe_sh, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP};

	DRW_framebuffer_init(&fbl->probe_sh_fb, &draw_engine_eevee_type, 9, 1, &tex_sh, 1);
}

void EEVEE_probes_cache_init(EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_PassList *psl = vedata->psl;

	{
		psl->probe_prefilter = DRW_pass_create("Probe Filtering", DRW_STATE_WRITE_COLOR);

		struct Batch *geom = DRW_cache_fullscreen_quad_get();

		DRWShadingGroup *grp = DRW_shgroup_instance_create(e_data.probe_filter_sh, psl->probe_prefilter, geom);
		DRW_shgroup_uniform_float(grp, "sampleCount", &stl->probes->samples_ct, 1);
		DRW_shgroup_uniform_float(grp, "invSampleCount", &stl->probes->invsamples_ct, 1);
		DRW_shgroup_uniform_float(grp, "roughnessSquared", &stl->probes->roughness, 1);
		DRW_shgroup_uniform_float(grp, "lodFactor", &stl->probes->lodfactor, 1);
		DRW_shgroup_uniform_float(grp, "lodMax", &stl->probes->lodmax, 1);
		DRW_shgroup_uniform_float(grp, "texelSize", &stl->probes->texel_size, 1);
		DRW_shgroup_uniform_float(grp, "paddingSize", &stl->probes->padding_size, 1);
		DRW_shgroup_uniform_int(grp, "Layer", &stl->probes->layer, 1);
		DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
		// DRW_shgroup_uniform_texture(grp, "texJitter", e_data.jitter);
		DRW_shgroup_uniform_texture(grp, "probeHdr", txl->probe_rt);

		DRW_shgroup_call_dynamic_add_empty(grp);
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
}

void EEVEE_probes_cache_add(EEVEE_Data *UNUSED(vedata), Object *UNUSED(ob))
{
	return;
}

void EEVEE_probes_cache_finish(EEVEE_Data *UNUSED(vedata))
{
	return;
}

void EEVEE_probes_update(EEVEE_Data *UNUSED(vedata))
{
	return;
}

void EEVEE_refresh_probe(EEVEE_Data *vedata)
{
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;
	EEVEE_PassList *psl = vedata->psl;
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_ProbesInfo *pinfo = stl->probes;

	float projmat[4][4];

	/* 1 - Render to cubemap target using geometry shader. */
	/* We don't need to clear since we render the background. */
	pinfo->layer = 0;
	perspective_m4(projmat, -0.1f, 0.1f, -0.1f, 0.1f, 0.1f, 100.0f);
	for (int i = 0; i < 6; ++i) {
		mul_m4_m4m4(pinfo->probemat[i], projmat, cubefacemat[i]);
	}

	DRW_framebuffer_bind(fbl->probe_fb);
	DRW_draw_pass(psl->probe_background);

	/* 2 - Let gpu create Mipmaps for Filtered Importance Sampling. */
	/* Bind next framebuffer to be able to gen. mips for probe_rt. */
	DRW_framebuffer_bind(fbl->probe_filter_fb);
	DRW_texture_generate_mipmaps(txl->probe_rt);

	/* 3 - Render to probe array to the specified layer, do prefiltering. */
	/* Detach to rebind the right mipmap. */
	DRW_framebuffer_texture_detach(txl->probe_pool);
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
		pinfo->layer = 0;
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

		DRW_framebuffer_texture_attach(fbl->probe_filter_fb, txl->probe_pool, 0, i);
		DRW_framebuffer_viewport_size(fbl->probe_filter_fb, mipsize, mipsize);
		DRW_draw_pass(psl->probe_prefilter);
		DRW_framebuffer_texture_detach(txl->probe_pool);

		mipsize /= 2;
		CLAMP_MIN(mipsize, 1);
	}
	/* For shading, save max level of the octahedron map */
	pinfo->lodmax = (float)(maxlevel - min_lod_level) - 1.0f;
	/* reattach to have a valid framebuffer. */
	DRW_framebuffer_texture_attach(fbl->probe_filter_fb, txl->probe_pool, 0, 0);

	/* 4 - Compute spherical harmonics */
	/* Tweaking parameters to balance perf. vs precision */
	pinfo->shres = 16; /* Less texture fetches & reduce branches */
	pinfo->lodfactor = 4.0f; /* Improve cache reuse */
	DRW_framebuffer_bind(fbl->probe_sh_fb);
	DRW_draw_pass(psl->probe_sh_compute);
	DRW_framebuffer_read_data(0, 0, 9, 1, 3, 0, (float *)pinfo->shcoefs);
}

void EEVEE_probes_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.probe_filter_sh);
	DRW_SHADER_FREE_SAFE(e_data.probe_spherical_harmonic_sh);
	DRW_TEXTURE_FREE_SAFE(e_data.hammersley);
}