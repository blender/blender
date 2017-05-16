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

#include "DRW_render.h"

#include "eevee_engine.h"
#include "eevee_private.h"
#include "GPU_texture.h"
#include "GPU_glew.h"

typedef struct EEVEE_ProbeData {
	short probe_id, shadow_id;
} EEVEE_ProbeData;

/* TODO Option */
#define PROBE_SIZE 512

/* *********** FUNCTIONS *********** */

void EEVEE_probes_init(EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_TextureList *txl = vedata->txl;

	if (!stl->probes) {
		stl->probes       = MEM_callocN(sizeof(EEVEE_ProbesInfo), "EEVEE_ProbesInfo");
	}

	if (!txl->probe_rt) {
		txl->probe_rt = DRW_texture_create_cube(PROBE_SIZE, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
		txl->probe_depth_rt = DRW_texture_create_cube(PROBE_SIZE, DRW_TEX_DEPTH_24, DRW_TEX_FILTER, NULL);
	}

	DRWFboTexture tex_probe[2] = {{&txl->probe_depth_rt, DRW_TEX_DEPTH_24, DRW_TEX_FILTER},
	                              {&txl->probe_rt, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP}};

	DRW_framebuffer_init(&fbl->probe_fb, &draw_engine_eevee_type, PROBE_SIZE, PROBE_SIZE, tex_probe, 2);

	if (!txl->probe_pool) {
		/* TODO array */
		txl->probe_pool = DRW_texture_create_cube(PROBE_SIZE, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP, NULL);
	}

	DRWFboTexture tex_filter = {&txl->probe_pool, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP};

	DRW_framebuffer_init(&fbl->probe_filter_fb, &draw_engine_eevee_type, PROBE_SIZE, PROBE_SIZE, &tex_filter, 1);

	/* Spherical Harmonic Buffer */
	DRWFboTexture tex_sh = {&txl->probe_sh, DRW_TEX_RGBA_16, DRW_TEX_FILTER | DRW_TEX_MIPMAP};

	DRW_framebuffer_init(&fbl->probe_sh_fb, &draw_engine_eevee_type, 9, 1, &tex_sh, 1);
}

void EEVEE_probes_cache_init(EEVEE_Data *UNUSED(vedata))
{
	return;
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
	/* Bind next framebuffer to be able to write to probe_rt. */
	DRW_framebuffer_bind(fbl->probe_filter_fb);
	DRW_texture_generate_mipmaps(txl->probe_rt);

	/* 3 - Render to probe array to the specified layer, do prefiltering. */
	/* Detach to rebind the right mipmap. */
	DRW_framebuffer_texture_detach(txl->probe_pool);
	float mipsize = PROBE_SIZE * 2;
	int miplevels = 1 + (int)floorf(log2f(PROBE_SIZE));
	for (int i = 0; i < miplevels - 2; i++) {
		float bias = (i == 0) ? 0.0f : 1.0f;

		mipsize /= 2;
		CLAMP_MIN(mipsize, 1);

		pinfo->layer = 0;
		pinfo->roughness = (float)i / ((float)miplevels - 3.0f);
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
		pinfo->lodfactor = bias + 0.5f * log((float)(PROBE_SIZE * PROBE_SIZE) * pinfo->invsamples_ct) / log(2);
		pinfo->lodmax = (float)miplevels - 3.0f;

		DRW_framebuffer_texture_attach(fbl->probe_filter_fb, txl->probe_pool, 0, i);
		DRW_framebuffer_viewport_size(fbl->probe_filter_fb, mipsize, mipsize);
		DRW_draw_pass(psl->probe_prefilter);
		DRW_framebuffer_texture_detach(txl->probe_pool);
	}
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
