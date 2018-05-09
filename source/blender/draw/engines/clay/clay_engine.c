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

#include "BLI_utildefines.h"
#include "BLI_string_utils.h"
#include "BLI_rand.h"

#include "DNA_particle_types.h"
#include "DNA_view3d_types.h"

#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_main.h"
#include "BKE_particle.h"

#include "GPU_shader.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "UI_resources.h"
#include "UI_interface_icons.h"

#include "DRW_render.h"

#include "clay_engine.h"

#ifdef WITH_CLAY_ENGINE
#include "../eevee/eevee_lut.h" /* TODO find somewhere to share blue noise Table */

/* Shaders */

#define CLAY_ENGINE "BLENDER_CLAY"

#define MAX_CLAY_MAT 512 /* 512 = 9 bit material id */

#define SHADER_DEFINES_NO_AO \
	"#define MAX_MATERIAL " STRINGIFY(MAX_CLAY_MAT) "\n" \
	"#define USE_ROTATION\n" \
	"#define USE_HSV\n"

#define SHADER_DEFINES \
	SHADER_DEFINES_NO_AO \
	"#define USE_AO\n"

extern char datatoc_clay_frag_glsl[];
extern char datatoc_clay_prepass_frag_glsl[];
extern char datatoc_clay_copy_glsl[];
extern char datatoc_clay_vert_glsl[];
extern char datatoc_clay_fxaa_glsl[];
extern char datatoc_clay_particle_vert_glsl[];
extern char datatoc_clay_particle_strand_frag_glsl[];
extern char datatoc_ssao_alchemy_glsl[];
extern char datatoc_common_fxaa_lib_glsl[];

/* *********** LISTS *********** */

/**
 * UBOs data needs to be 16 byte aligned (size of vec4)
 *
 * Reminder: float, int, bool are 4 bytes
 *
 * \note struct is expected to be initialized with all pad-bits zero'd
 * so we can use 'memcmp' to check for duplicates. Possibly hash data later.
 */
typedef struct CLAY_UBO_Material {
	float ssao_params_var[4];
	/* - 16 -*/
	float matcap_hsv[3];
	float matcap_id; /* even float encoding have enough precision */
	/* - 16 -*/
	float matcap_rot[2];
	float pad[2]; /* ensure 16 bytes alignement */
} CLAY_UBO_Material; /* 48 bytes */
BLI_STATIC_ASSERT_ALIGN(CLAY_UBO_Material, 16)

typedef struct CLAY_HAIR_UBO_Material {
	float hair_randomness;
	float matcap_id;
	float matcap_rot[2];
	float matcap_hsv[3];
	float pad;
} CLAY_HAIR_UBO_Material; /* 32 bytes */
BLI_STATIC_ASSERT_ALIGN(CLAY_HAIR_UBO_Material, 16)

typedef struct CLAY_UBO_Storage {
	CLAY_UBO_Material materials[MAX_CLAY_MAT];
} CLAY_UBO_Storage;

typedef struct CLAY_HAIR_UBO_Storage {
	CLAY_HAIR_UBO_Material materials[MAX_CLAY_MAT];
} CLAY_HAIR_UBO_Storage;

/* GPUViewport.storage
 * Is freed everytime the viewport engine changes */
typedef struct CLAY_Storage {
	/* Materials Parameter UBO */
	CLAY_UBO_Storage mat_storage;
	CLAY_HAIR_UBO_Storage hair_mat_storage;
	int ubo_current_id;
	int hair_ubo_current_id;
	DRWShadingGroup *shgrps[MAX_CLAY_MAT];
	DRWShadingGroup *shgrps_flat[MAX_CLAY_MAT];
	DRWShadingGroup *shgrps_pre[MAX_CLAY_MAT];
	DRWShadingGroup *shgrps_pre_flat[MAX_CLAY_MAT];
	DRWShadingGroup *hair_shgrps[MAX_CLAY_MAT];
} CLAY_Storage;

typedef struct CLAY_StorageList {
	struct CLAY_Storage *storage;
	struct CLAY_PrivateData *g_data;
} CLAY_StorageList;

typedef struct CLAY_FramebufferList {
	struct GPUFrameBuffer *antialias_fb;
	struct GPUFrameBuffer *prepass_fb;
} CLAY_FramebufferList;

typedef struct CLAY_PassList {
	struct DRWPass *clay_ps;
	struct DRWPass *clay_cull_ps;
	struct DRWPass *clay_flat_ps;
	struct DRWPass *clay_flat_cull_ps;
	struct DRWPass *clay_pre_ps;
	struct DRWPass *clay_pre_cull_ps;
	struct DRWPass *clay_flat_pre_ps;
	struct DRWPass *clay_flat_pre_cull_ps;
	struct DRWPass *clay_deferred_ps;
	struct DRWPass *fxaa_ps;
	struct DRWPass *copy_ps;
	struct DRWPass *hair_pass;
} CLAY_PassList;


typedef struct CLAY_Data {
	void *engine_type;
	CLAY_FramebufferList *fbl;
	DRWViewportEmptyList *txl;
	CLAY_PassList *psl;
	CLAY_StorageList *stl;
} CLAY_Data;

typedef struct CLAY_ViewLayerData {
	struct GPUTexture *jitter_tx;
	struct GPUUniformBuffer *mat_ubo;
	struct GPUUniformBuffer *matcaps_ubo;
	struct GPUUniformBuffer *hair_mat_ubo;
	struct GPUUniformBuffer *sampling_ubo;
	int cached_sample_num;
} CLAY_ViewLayerData;

/* *********** STATIC *********** */

static struct {
	/* Shading Pass */
	struct GPUShader *clay_sh;
	struct GPUShader *clay_flat_sh;
	struct GPUShader *clay_prepass_flat_sh;
	struct GPUShader *clay_prepass_sh;
	struct GPUShader *clay_deferred_shading_sh;
	struct GPUShader *fxaa_sh;
	struct GPUShader *copy_sh;
	struct GPUShader *hair_sh;
	/* Matcap textures */
	struct GPUTexture *matcap_array;
	float matcap_colors[24][4];
	/* Just a serie of int from 0 to MAX_CLAY_MAT-1 */
	int ubo_mat_idxs[MAX_CLAY_MAT];
	/* To avoid useless texture and ubo binds. */
	bool first_shgrp;
} e_data = {NULL}; /* Engine data */

typedef struct CLAY_PrivateData {
	DRWShadingGroup *depth_shgrp;
	DRWShadingGroup *depth_shgrp_select;
	DRWShadingGroup *depth_shgrp_active;
	DRWShadingGroup *depth_shgrp_cull;
	DRWShadingGroup *depth_shgrp_cull_select;
	DRWShadingGroup *depth_shgrp_cull_active;
	/* Deferred shading */
	struct GPUTexture *depth_tx; /* ref only, not alloced */
	struct GPUTexture *normal_tx; /* ref only, not alloced */
	struct GPUTexture *id_tx; /* ref only, not alloced */
	struct GPUTexture *color_copy; /* ref only, not alloced */
	bool enable_deferred_path;
	/* Ssao */
	float winmat[4][4];
	float viewvecs[3][4];
	float ssao_params[4];
} CLAY_PrivateData; /* Transient data */

/* Functions */

static void clay_view_layer_data_free(void *storage)
{
	CLAY_ViewLayerData *sldata = (CLAY_ViewLayerData *)storage;

	DRW_UBO_FREE_SAFE(sldata->mat_ubo);
	DRW_UBO_FREE_SAFE(sldata->matcaps_ubo);
	DRW_UBO_FREE_SAFE(sldata->hair_mat_ubo);
	DRW_UBO_FREE_SAFE(sldata->sampling_ubo);
	DRW_TEXTURE_FREE_SAFE(sldata->jitter_tx);
}

static CLAY_ViewLayerData *CLAY_view_layer_data_get(void)
{
	CLAY_ViewLayerData **sldata = (CLAY_ViewLayerData **)DRW_view_layer_engine_data_ensure(&draw_engine_clay_type, &clay_view_layer_data_free);

	if (*sldata == NULL) {
		*sldata = MEM_callocN(sizeof(**sldata), "CLAY_ViewLayerData");
	}

	return *sldata;
}

static void add_icon_to_rect(PreviewImage *prv, float *final_rect, int layer)
{
	int image_size = prv->w[0] * prv->h[0];
	float *new_rect = &final_rect[image_size * 4 * layer];

	IMB_buffer_float_from_byte(new_rect, (unsigned char *)prv->rect[0], IB_PROFILE_SRGB, IB_PROFILE_SRGB,
	                           false, prv->w[0], prv->h[0], prv->w[0], prv->w[0]);

	/* Find overall color */
	for (int y = 0; y < 4; ++y)	{
		for (int x = 0; x < 4; ++x) {
			e_data.matcap_colors[layer][0] += new_rect[y * 512 * 128 * 4 + x * 128 * 4 + 0];
			e_data.matcap_colors[layer][1] += new_rect[y * 512 * 128 * 4 + x * 128 * 4 + 1];
			e_data.matcap_colors[layer][2] += new_rect[y * 512 * 128 * 4 + x * 128 * 4 + 2];
		}
	}

	e_data.matcap_colors[layer][0] /= 16.0f * 2.0f; /* the * 2 is to darken for shadows */
	e_data.matcap_colors[layer][1] /= 16.0f * 2.0f;
	e_data.matcap_colors[layer][2] /= 16.0f * 2.0f;
}

static struct GPUTexture *load_matcaps(PreviewImage *prv[24], int nbr)
{
	struct GPUTexture *tex;
	int w = prv[0]->w[0];
	int h = prv[0]->h[0];
	float *final_rect = MEM_callocN(sizeof(float) * 4 * w * h * nbr, "Clay Matcap array rect");

	for (int i = 0; i < nbr; ++i) {
		add_icon_to_rect(prv[i], final_rect, i);
		BKE_previewimg_free(&prv[i]);
	}

	tex = DRW_texture_create_2D_array(w, h, nbr, GPU_RGBA8, DRW_TEX_FILTER, final_rect);
	MEM_freeN(final_rect);

	return tex;
}

static int matcap_to_index(int matcap)
{
	return (int)matcap - (int)ICON_MATCAP_01;
}

/* Using Hammersley distribution */
static float *create_disk_samples(int num_samples)
{
	/* vec4 to ensure memory alignment. */
	float (*texels)[4] = MEM_mallocN(sizeof(float[4]) * num_samples, "concentric_tex");
	const float num_samples_inv = 1.0f / num_samples;

	for (int i = 0; i < num_samples; i++) {
		float r = (i + 0.5f) * num_samples_inv;
		double dphi;
		BLI_hammersley_1D(i, &dphi);

		float phi = (float)dphi * 2.0f * M_PI;
		texels[i][0] = cosf(phi);
		texels[i][1] = sinf(phi);
		/* This deliberatly distribute more samples
		 * at the center of the disk (and thus the shadow). */
		texels[i][2] = r;
	}

	return (float *)texels;
}

static struct GPUTexture *create_jitter_texture(int num_samples)
{
	float jitter[64 * 64][3];
	const float num_samples_inv = 1.0f / num_samples;

	for (int i = 0; i < 64 * 64; i++) {
		float phi = blue_noise[i][0] * 2.0f * M_PI;
		/* This rotate the sample per pixels */
		jitter[i][0] = cosf(phi);
		jitter[i][1] = sinf(phi);
		/* This offset the sample along it's direction axis (reduce banding) */
		float bn = blue_noise[i][1] - 0.5f;
		CLAMP(bn, -0.499f, 0.499f); /* fix fireflies */
		jitter[i][2] = bn * num_samples_inv;
	}

	UNUSED_VARS(bsdf_split_sum_ggx, btdf_split_sum_ggx, ltc_mag_ggx, ltc_mat_ggx, ltc_disk_integral);

	return DRW_texture_create_2D(64, 64, GPU_RGB16F, DRW_TEX_FILTER | DRW_TEX_WRAP, &jitter[0][0]);
}

static void clay_engine_init(void *vedata)
{
	CLAY_StorageList *stl = ((CLAY_Data *)vedata)->stl;
	CLAY_FramebufferList *fbl = ((CLAY_Data *)vedata)->fbl;
	CLAY_ViewLayerData *sldata = CLAY_view_layer_data_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	/* Create Texture Array */
	if (!e_data.matcap_array) {
		PreviewImage *prv[24]; /* For now use all of the 24 internal matcaps */
		const int num_matcap = ARRAY_SIZE(prv);

		/* TODO only load used matcaps */
		for (int i = 0; i < num_matcap; ++i) {
			prv[i] = UI_icon_to_preview((int)ICON_MATCAP_01 + i);
		}

		e_data.matcap_array = load_matcaps(prv, num_matcap);
	}

	/* Shading pass */
	if (!e_data.clay_sh) {
		char *matcap_with_ao = BLI_string_joinN(
		        datatoc_clay_frag_glsl,
		        datatoc_ssao_alchemy_glsl);

		e_data.clay_sh = DRW_shader_create(
		        datatoc_clay_vert_glsl, NULL, datatoc_clay_frag_glsl,
		        SHADER_DEFINES_NO_AO);
		e_data.clay_flat_sh = DRW_shader_create(
		        datatoc_clay_vert_glsl, NULL, datatoc_clay_frag_glsl,
		        SHADER_DEFINES_NO_AO
		        "#define USE_FLAT_NORMAL\n");

		e_data.clay_prepass_sh = DRW_shader_create(
		        datatoc_clay_vert_glsl, NULL, datatoc_clay_prepass_frag_glsl,
		        SHADER_DEFINES);
		e_data.clay_prepass_flat_sh = DRW_shader_create(
		        datatoc_clay_vert_glsl, NULL, datatoc_clay_prepass_frag_glsl,
		        SHADER_DEFINES
		        "#define USE_FLAT_NORMAL\n");

		e_data.clay_deferred_shading_sh = DRW_shader_create_fullscreen(
		        matcap_with_ao,
		        SHADER_DEFINES
		        "#define DEFERRED_SHADING\n");

		MEM_freeN(matcap_with_ao);

		char *fxaa_str = BLI_string_joinN(
		        datatoc_common_fxaa_lib_glsl,
		        datatoc_clay_fxaa_glsl);

		e_data.fxaa_sh = DRW_shader_create_fullscreen(fxaa_str, NULL);

		MEM_freeN(fxaa_str);

		e_data.copy_sh = DRW_shader_create_fullscreen(datatoc_clay_copy_glsl, NULL);
	}

	if (!stl->storage) {
		stl->storage = MEM_callocN(sizeof(CLAY_Storage), "CLAY_Storage");
	}

	if (!stl->g_data) {
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), "CLAY_PrivateStorage");
	}

	CLAY_PrivateData *g_data = stl->g_data;

	if (!sldata->mat_ubo) {
		sldata->mat_ubo = DRW_uniformbuffer_create(sizeof(CLAY_UBO_Storage), NULL);
	}

	if (!sldata->hair_mat_ubo) {
		sldata->hair_mat_ubo = DRW_uniformbuffer_create(sizeof(CLAY_HAIR_UBO_Storage), NULL);
	}

	if (!sldata->matcaps_ubo) {
		sldata->matcaps_ubo = DRW_uniformbuffer_create(sizeof(e_data.matcap_colors), e_data.matcap_colors);
	}

	if (e_data.ubo_mat_idxs[1] == 0) {
		/* Just int to have pointers to them */
		for (int i = 0; i < MAX_CLAY_MAT; ++i) {
			e_data.ubo_mat_idxs[i] = i;
		}
	}

	/* FBO setup */
	{
		const float *viewport_size = DRW_viewport_size_get();
		const int size[2] = {(int)viewport_size[0], (int)viewport_size[1]};

		g_data->normal_tx = DRW_texture_pool_query_2D(size[0], size[1], GPU_RG8, &draw_engine_clay_type);
		g_data->id_tx =     DRW_texture_pool_query_2D(size[0], size[1], GPU_R16UI, &draw_engine_clay_type);

		GPU_framebuffer_ensure_config(&fbl->prepass_fb, {
			GPU_ATTACHMENT_TEXTURE(dtxl->depth),
			GPU_ATTACHMENT_TEXTURE(g_data->normal_tx),
			GPU_ATTACHMENT_TEXTURE(g_data->id_tx)
		});

		/* For FXAA */
		/* TODO(fclem): OPTI: we could merge normal_tx and id_tx into a GPU_RGBA8
		 * and reuse it for the fxaa target. */
		g_data->color_copy = DRW_texture_pool_query_2D(size[0], size[1], GPU_RGBA8, &draw_engine_clay_type);

		GPU_framebuffer_ensure_config(&fbl->antialias_fb, {
			GPU_ATTACHMENT_NONE,
			GPU_ATTACHMENT_TEXTURE(g_data->color_copy)
		});
	}

	/* SSAO setup */
	{
		const DRWContextState *draw_ctx = DRW_context_state_get();
		ViewLayer *view_layer = draw_ctx->view_layer;
		IDProperty *props = BKE_view_layer_engine_evaluated_get(
		        view_layer, RE_engine_id_BLENDER_CLAY);
		int ssao_samples = BKE_collection_engine_property_value_get_int(props, "ssao_samples");

		float invproj[4][4];
		float dfdyfacs[2];
		const bool is_persp = DRW_viewport_is_persp_get();
		/* view vectors for the corners of the view frustum.
		 * Can be used to recreate the world space position easily */
		float viewvecs[3][4] = {
		    {-1.0f, -1.0f, -1.0f, 1.0f},
		    {1.0f, -1.0f, -1.0f, 1.0f},
		    {-1.0f, 1.0f, -1.0f, 1.0f}
		};
		int i;
		const float *size = DRW_viewport_size_get();

		DRW_state_dfdy_factors_get(dfdyfacs);

		g_data->ssao_params[0] = ssao_samples;
		g_data->ssao_params[1] = size[0] / 64.0;
		g_data->ssao_params[2] = size[1] / 64.0;
		g_data->ssao_params[3] = dfdyfacs[1]; /* dfdy sign for offscreen */

		/* invert the view matrix */
		DRW_viewport_matrix_get(g_data->winmat, DRW_MAT_WIN);
		invert_m4_m4(invproj, g_data->winmat);

		/* convert the view vectors to view space */
		for (i = 0; i < 3; i++) {
			mul_m4_v4(invproj, viewvecs[i]);
			/* normalized trick see:
			 * http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
			mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][3]);
			if (is_persp)
				mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][2]);
			viewvecs[i][3] = 1.0;

			copy_v4_v4(g_data->viewvecs[i], viewvecs[i]);
		}

		/* we need to store the differences */
		g_data->viewvecs[1][0] -= g_data->viewvecs[0][0];
		g_data->viewvecs[1][1] = g_data->viewvecs[2][1] - g_data->viewvecs[0][1];

		/* calculate a depth offset as well */
		if (!is_persp) {
			float vec_far[] = {-1.0f, -1.0f, 1.0f, 1.0f};
			mul_m4_v4(invproj, vec_far);
			mul_v3_fl(vec_far, 1.0f / vec_far[3]);
			g_data->viewvecs[1][2] = vec_far[2] - g_data->viewvecs[0][2];
		}

		/* AO Samples Tex */
		if (sldata->sampling_ubo && (sldata->cached_sample_num != ssao_samples)) {
			DRW_UBO_FREE_SAFE(sldata->sampling_ubo);
			DRW_TEXTURE_FREE_SAFE(sldata->jitter_tx);
		}

		if (sldata->sampling_ubo == NULL) {
			float *samples = create_disk_samples(ssao_samples);
			sldata->jitter_tx = create_jitter_texture(ssao_samples);
			sldata->sampling_ubo = DRW_uniformbuffer_create(sizeof(float[4]) * ssao_samples, samples);
			sldata->cached_sample_num = ssao_samples;
			MEM_freeN(samples);
		}
	}
}

static DRWShadingGroup *CLAY_shgroup_create(DRWPass *pass, GPUShader *sh, int id)
{
	CLAY_ViewLayerData *sldata = CLAY_view_layer_data_get();
	DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
	DRW_shgroup_uniform_int(grp, "mat_id", &e_data.ubo_mat_idxs[id], 1);
	if (e_data.first_shgrp) {
		DRW_shgroup_uniform_texture_persistent(grp, "matcaps", e_data.matcap_array);
		DRW_shgroup_uniform_block_persistent(grp, "material_block", sldata->mat_ubo);
		DRW_shgroup_uniform_block_persistent(grp, "matcaps_block", sldata->matcaps_ubo);
	}
	return grp;
}

static DRWShadingGroup *CLAY_shgroup_deferred_prepass_create(DRWPass *pass, GPUShader *sh, int id)
{
	DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
	DRW_shgroup_uniform_int(grp, "mat_id", &e_data.ubo_mat_idxs[id], 1);

	return grp;
}

static DRWShadingGroup *CLAY_shgroup_deferred_shading_create(DRWPass *pass, CLAY_PrivateData *g_data)
{
	CLAY_ViewLayerData *sldata = CLAY_view_layer_data_get();
	DRWShadingGroup *grp = DRW_shgroup_create(e_data.clay_deferred_shading_sh, pass);
	DRW_shgroup_uniform_texture_ref(grp, "depthtex", &g_data->depth_tx);
	DRW_shgroup_uniform_texture_ref(grp, "normaltex", &g_data->normal_tx);
	DRW_shgroup_uniform_texture_ref(grp, "idtex", &g_data->id_tx);
	DRW_shgroup_uniform_texture(grp, "matcaps", e_data.matcap_array);
	DRW_shgroup_uniform_texture(grp, "ssao_jitter", sldata->jitter_tx);
	DRW_shgroup_uniform_block(grp, "samples_block", sldata->sampling_ubo);
	DRW_shgroup_uniform_block(grp, "material_block", sldata->mat_ubo);
	DRW_shgroup_uniform_block(grp, "matcaps_block", sldata->matcaps_ubo);
	/* TODO put in ubo */
	DRW_shgroup_uniform_mat4(grp, "WinMatrix", g_data->winmat);
	DRW_shgroup_uniform_vec2(grp, "invscreenres", DRW_viewport_invert_size_get(), 1);
	DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)g_data->viewvecs, 3);
	DRW_shgroup_uniform_vec4(grp, "ssao_params", g_data->ssao_params, 1);
	return grp;
}

static DRWShadingGroup *CLAY_hair_shgroup_create(DRWPass *pass, int id)
{
	CLAY_ViewLayerData *sldata = CLAY_view_layer_data_get();

	if (!e_data.hair_sh) {
		e_data.hair_sh = DRW_shader_create(
		        datatoc_clay_particle_vert_glsl, NULL, datatoc_clay_particle_strand_frag_glsl,
		        "#define MAX_MATERIAL " STRINGIFY(MAX_CLAY_MAT) "\n" );
	}

	DRWShadingGroup *grp = DRW_shgroup_create(e_data.hair_sh, pass);
	DRW_shgroup_uniform_texture(grp, "matcaps", e_data.matcap_array);
	DRW_shgroup_uniform_block(grp, "material_block", sldata->mat_ubo);
	DRW_shgroup_uniform_int(grp, "mat_id", &e_data.ubo_mat_idxs[id], 1);

	return grp;
}

static int search_mat_to_ubo(CLAY_Storage *storage, const CLAY_UBO_Material *mat_ubo_test)
{
	/* For now just use a linear search and test all parameters */
	/* TODO make a hash table */
	for (int i = 0; i < storage->ubo_current_id; ++i) {
		CLAY_UBO_Material *ubo = &storage->mat_storage.materials[i];
		if (memcmp(ubo, mat_ubo_test, sizeof(*mat_ubo_test)) == 0) {
			return i;
		}
	}

	return -1;
}

static int search_hair_mat_to_ubo(CLAY_Storage *storage, const CLAY_HAIR_UBO_Material *hair_mat_ubo_test)
{
	/* For now just use a linear search and test all parameters */
	/* TODO make a hash table */
	for (int i = 0; i < storage->hair_ubo_current_id; ++i) {
		CLAY_HAIR_UBO_Material *ubo = &storage->hair_mat_storage.materials[i];
		if (memcmp(ubo, hair_mat_ubo_test, sizeof(*hair_mat_ubo_test)) == 0) {
			return i;
		}
	}

	return -1;
}

static int push_mat_to_ubo(CLAY_Storage *storage, const CLAY_UBO_Material *mat_ubo_test)
{
	int id = storage->ubo_current_id++;
	id = min_ii(MAX_CLAY_MAT, id);
	storage->mat_storage.materials[id] = *mat_ubo_test;
	return id;
}

static int push_hair_mat_to_ubo(CLAY_Storage *storage, const CLAY_HAIR_UBO_Material *hair_mat_ubo_test)
{
	int id = storage->hair_ubo_current_id++;
	id = min_ii(MAX_CLAY_MAT, id);
	storage->hair_mat_storage.materials[id] = *hair_mat_ubo_test;
	return id;
}

static int mat_in_ubo(CLAY_Storage *storage, const CLAY_UBO_Material *mat_ubo_test)
{
	/* Search material in UBO */
	int id = search_mat_to_ubo(storage, mat_ubo_test);

	/* if not found create it */
	if (id == -1) {
		id = push_mat_to_ubo(storage, mat_ubo_test);
	}

	return id;
}

static int hair_mat_in_ubo(CLAY_Storage *storage, const CLAY_HAIR_UBO_Material *hair_mat_ubo_test)
{
	/* Search material in UBO */
	int id = search_hair_mat_to_ubo(storage, hair_mat_ubo_test);

	/* if not found create it */
	if (id == -1) {
		id = push_hair_mat_to_ubo(storage, hair_mat_ubo_test);
	}

	return id;
}

static void ubo_mat_from_object(CLAY_Storage *storage, Object *UNUSED(ob), bool *r_needs_ao, int *r_id)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	IDProperty *props = BKE_view_layer_engine_evaluated_get(view_layer, RE_engine_id_BLENDER_CLAY);

	int matcap_icon = BKE_collection_engine_property_value_get_int(props, "matcap_icon");
	float matcap_rot = BKE_collection_engine_property_value_get_float(props, "matcap_rotation");
	float matcap_hue = BKE_collection_engine_property_value_get_float(props, "matcap_hue");
	float matcap_sat = BKE_collection_engine_property_value_get_float(props, "matcap_saturation");
	float matcap_val = BKE_collection_engine_property_value_get_float(props, "matcap_value");
	float ssao_distance = BKE_collection_engine_property_value_get_float(props, "ssao_distance");
	float ssao_factor_cavity = BKE_collection_engine_property_value_get_float(props, "ssao_factor_cavity");
	float ssao_factor_edge = BKE_collection_engine_property_value_get_float(props, "ssao_factor_edge");
	float ssao_attenuation = BKE_collection_engine_property_value_get_float(props, "ssao_attenuation");

	CLAY_UBO_Material r_ubo = {{0.0f}};

	if (((ssao_factor_cavity > 0.0) || (ssao_factor_edge > 0.0)) &&
	    (ssao_distance > 0.0))
	{
		*r_needs_ao = true;

		r_ubo.ssao_params_var[0] = ssao_distance;
		r_ubo.ssao_params_var[1] = ssao_factor_cavity;
		r_ubo.ssao_params_var[2] = ssao_factor_edge;
		r_ubo.ssao_params_var[3] = ssao_attenuation;
	}
	else {
		*r_needs_ao = false;
	}

	r_ubo.matcap_rot[0] = cosf(matcap_rot * 3.14159f * 2.0f);
	r_ubo.matcap_rot[1] = sinf(matcap_rot * 3.14159f * 2.0f);

	r_ubo.matcap_hsv[0] = matcap_hue + 0.5f;
	r_ubo.matcap_hsv[1] = matcap_sat * 2.0f;
	r_ubo.matcap_hsv[2] = matcap_val * 2.0f;

	r_ubo.matcap_id = matcap_to_index(matcap_icon);

	*r_id = mat_in_ubo(storage, &r_ubo);
}

static void hair_ubo_mat_from_object(Object *UNUSED(ob), CLAY_HAIR_UBO_Material *r_ubo)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	ViewLayer *view_layer = draw_ctx->view_layer;
	IDProperty *props = BKE_view_layer_engine_evaluated_get(view_layer, RE_engine_id_BLENDER_CLAY);

	int matcap_icon = BKE_collection_engine_property_value_get_int(props, "matcap_icon");
	float matcap_rot = BKE_collection_engine_property_value_get_float(props, "matcap_rotation");
	float matcap_hue = BKE_collection_engine_property_value_get_float(props, "matcap_hue");
	float matcap_sat = BKE_collection_engine_property_value_get_float(props, "matcap_saturation");
	float matcap_val = BKE_collection_engine_property_value_get_float(props, "matcap_value");
	float hair_randomness = BKE_collection_engine_property_value_get_float(props, "hair_brightness_randomness");

	memset(r_ubo, 0x0, sizeof(*r_ubo));

	r_ubo->matcap_rot[0] = cosf(matcap_rot * 3.14159f * 2.0f);
	r_ubo->matcap_rot[1] = sinf(matcap_rot * 3.14159f * 2.0f);
	r_ubo->matcap_hsv[0] = matcap_hue + 0.5f;
	r_ubo->matcap_hsv[1] = matcap_sat * 2.0f;
	r_ubo->matcap_hsv[2] = matcap_val * 2.0f;
	r_ubo->hair_randomness = hair_randomness;
	r_ubo->matcap_id = matcap_to_index(matcap_icon);
}

static DRWShadingGroup *CLAY_object_shgrp_get(CLAY_Data *vedata, Object *ob, bool use_flat, bool cull)
{
	bool prepass; int id;
	CLAY_PassList *psl = vedata->psl;
	CLAY_Storage *storage = vedata->stl->storage;
	DRWShadingGroup **shgrps;
	DRWPass *pass; GPUShader *sh;

	ubo_mat_from_object(storage, ob, &prepass, &id);

	if (prepass) {
		if (use_flat) {
			shgrps = storage->shgrps_pre_flat;
			pass = (cull) ? psl->clay_flat_pre_cull_ps : psl->clay_flat_pre_ps;
			sh = e_data.clay_prepass_flat_sh;
		}
		else {
			shgrps = storage->shgrps_pre;
			pass = (cull) ? psl->clay_pre_cull_ps : psl->clay_pre_ps;
			sh = e_data.clay_prepass_sh;
		}

		if (shgrps[id] == NULL) {
			shgrps[id] = CLAY_shgroup_deferred_prepass_create(pass, sh, id);
		}

		vedata->stl->g_data->enable_deferred_path = true;
	}
	else {
		if (use_flat) {
			shgrps = storage->shgrps_flat;
			pass = (cull) ? psl->clay_flat_cull_ps : psl->clay_flat_ps;
			sh = e_data.clay_flat_sh;
		}
		else {
			shgrps = storage->shgrps;
			pass = (cull) ? psl->clay_cull_ps : psl->clay_ps;
			sh = e_data.clay_sh;
		}

		if (shgrps[id] == NULL) {
			shgrps[id] = CLAY_shgroup_create(pass, sh, id);
			e_data.first_shgrp = false;
		}
	}

	return shgrps[id];
}

static DRWShadingGroup *CLAY_hair_shgrp_get(CLAY_Data *UNUSED(vedata), Object *ob, CLAY_StorageList *stl, CLAY_PassList *psl)
{
	DRWShadingGroup **hair_shgrps = stl->storage->hair_shgrps;

	CLAY_HAIR_UBO_Material hair_mat_ubo_test;
	hair_ubo_mat_from_object(ob, &hair_mat_ubo_test);

	int hair_id = hair_mat_in_ubo(stl->storage, &hair_mat_ubo_test);

	if (hair_shgrps[hair_id] == NULL) {
		hair_shgrps[hair_id] = CLAY_hair_shgroup_create(psl->hair_pass, hair_id);
	}

	return hair_shgrps[hair_id];
}

static void clay_cache_init(void *vedata)
{
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
	CLAY_PassList *psl = ((CLAY_Data *)vedata)->psl;
	CLAY_StorageList *stl = ((CLAY_Data *)vedata)->stl;

	/* Disable AO unless a material needs it. */
	stl->g_data->enable_deferred_path = false;

	/* Reset UBO datas, shgrp pointers and material id counters. */
	memset(stl->storage, 0, sizeof(*stl->storage));
	e_data.first_shgrp = true;

	/* Solid Passes */
	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
		psl->clay_ps =           DRW_pass_create("Clay", state);
		psl->clay_cull_ps =      DRW_pass_create("Clay Culled", state | DRW_STATE_CULL_BACK);
		psl->clay_flat_ps =      DRW_pass_create("Clay Flat", state);
		psl->clay_flat_cull_ps = DRW_pass_create("Clay Flat Culled", state | DRW_STATE_CULL_BACK);

		DRWState prepass_state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
		DRWState prepass_cull_state = prepass_state | DRW_STATE_CULL_BACK;
		psl->clay_pre_ps =           DRW_pass_create("Clay Deferred Pre", prepass_state);
		psl->clay_pre_cull_ps =      DRW_pass_create("Clay Deferred Pre Culled", prepass_cull_state);
		psl->clay_flat_pre_ps =      DRW_pass_create("Clay Deferred Flat Pre", prepass_state);
		psl->clay_flat_pre_cull_ps = DRW_pass_create("Clay Deferred Flat Pre Culled", prepass_cull_state);

		psl->clay_deferred_ps = DRW_pass_create("Clay Deferred Shading", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = CLAY_shgroup_deferred_shading_create(psl->clay_deferred_ps, stl->g_data);
		DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);
	}

	/* Hair Pass */
	{
		psl->hair_pass = DRW_pass_create(
		                     "Hair Pass",
		                     DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_WIRE);
	}

	{
		psl->fxaa_ps = DRW_pass_create("Fxaa", DRW_STATE_WRITE_COLOR);
		DRWShadingGroup *grp = DRW_shgroup_create(e_data.fxaa_sh, psl->fxaa_ps);
		DRW_shgroup_uniform_texture_ref(grp, "colortex", &dtxl->color);
		DRW_shgroup_uniform_vec2(grp, "invscreenres", DRW_viewport_invert_size_get(), 1);
		DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);

		psl->copy_ps = DRW_pass_create("Copy", DRW_STATE_WRITE_COLOR);
		grp = DRW_shgroup_create(e_data.copy_sh, psl->copy_ps);
		DRW_shgroup_uniform_texture_ref(grp, "colortex", &stl->g_data->color_copy);
		DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);
	}
}

static void clay_cache_populate_particles(void *vedata, Object *ob)
{
	CLAY_PassList *psl = ((CLAY_Data *)vedata)->psl;
	CLAY_StorageList *stl = ((CLAY_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();

	if (ob == draw_ctx->object_edit) {
		return;
	}

	if (!DRW_check_particles_visible_within_active_context(ob)) {
		return;
	}

	for (ParticleSystem *psys = ob->particlesystem.first; psys; psys = psys->next) {
		if (!psys_check_enabled(ob, psys, false)) {
			continue;
		}
		ParticleSettings *part = psys->part;
		int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;

		if (draw_as == PART_DRAW_PATH && !psys->pathcache && !psys->childcache) {
			draw_as = PART_DRAW_DOT;
		}

		if (draw_as == PART_DRAW_PATH) {
			struct Gwn_Batch *geom = DRW_cache_particles_get_hair(psys, NULL);
			DRWShadingGroup *hair_shgrp = CLAY_hair_shgrp_get(vedata, ob, stl, psl);
			DRW_shgroup_call_add(hair_shgrp, geom, NULL);
		}
	}
}

static void clay_cache_populate(void *vedata, Object *ob)
{
	DRWShadingGroup *clay_shgrp;

	if (!DRW_object_is_renderable(ob))
		return;

	const DRWContextState *draw_ctx = DRW_context_state_get();
	const bool is_active = (ob == draw_ctx->obact);
	if (is_active) {
		if (DRW_object_is_mode_shade(ob) == true) {
			return;
		}
	}

	/* Handle particles first in case the emitter itself shouldn't be rendered. */
	if (ob->type == OB_MESH) {
		clay_cache_populate_particles(vedata, ob);
	}

	if (DRW_check_object_visible_within_active_context(ob) == false) {
		return;
	}

	struct Gwn_Batch *geom = DRW_cache_object_surface_get(ob);
	if (geom) {
		const bool do_cull = (draw_ctx->v3d && (draw_ctx->v3d->flag2 & V3D_BACKFACE_CULLING));
		const bool is_sculpt_mode = is_active && (draw_ctx->object_mode & OB_MODE_SCULPT) != 0;
		const bool use_flat = is_sculpt_mode && DRW_object_is_flat_normal(ob);

		clay_shgrp = CLAY_object_shgrp_get(vedata, ob, use_flat, do_cull);

		if (is_sculpt_mode) {
			DRW_shgroup_call_sculpt_add(clay_shgrp, ob, ob->obmat);
		}
		else {
			DRW_shgroup_call_object_add(clay_shgrp, geom, ob);
		}
	}
}

static void clay_cache_finish(void *vedata)
{
	CLAY_ViewLayerData *sldata = CLAY_view_layer_data_get();
	CLAY_StorageList *stl = ((CLAY_Data *)vedata)->stl;

	DRW_uniformbuffer_update(sldata->mat_ubo, &stl->storage->mat_storage);
	DRW_uniformbuffer_update(sldata->hair_mat_ubo, &stl->storage->hair_mat_storage);
}

static void clay_draw_scene(void *vedata)
{
	CLAY_StorageList *stl = ((CLAY_Data *)vedata)->stl;
	CLAY_PassList *psl = ((CLAY_Data *)vedata)->psl;
	CLAY_FramebufferList *fbl = ((CLAY_Data *)vedata)->fbl;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
	stl->g_data->depth_tx = dtxl->depth;

	/* Passes are ordered to have less _potential_ overdraw */
	DRW_draw_pass(psl->clay_cull_ps);
	DRW_draw_pass(psl->clay_flat_cull_ps);
	DRW_draw_pass(psl->clay_ps);
	DRW_draw_pass(psl->clay_flat_ps);
	DRW_draw_pass(psl->hair_pass);

	if (stl->g_data->enable_deferred_path) {
		GPU_framebuffer_bind(fbl->prepass_fb);
		/* We need to clear the id texture unfortunately. */
		const float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		GPU_framebuffer_clear_color(fbl->prepass_fb, clear_col);

		DRW_draw_pass(psl->clay_pre_cull_ps);
		DRW_draw_pass(psl->clay_flat_pre_cull_ps);
		DRW_draw_pass(psl->clay_pre_ps);
		DRW_draw_pass(psl->clay_flat_pre_ps);

		GPU_framebuffer_bind(dfbl->color_only_fb);
		DRW_draw_pass(psl->clay_deferred_ps);
	}

	if (true) { /* Always on for now. We might want a parameter for this. */
		GPU_framebuffer_bind(fbl->antialias_fb);
		DRW_draw_pass(psl->fxaa_ps);

		GPU_framebuffer_bind(dfbl->color_only_fb);
		DRW_draw_pass(psl->copy_ps);
	}
}

static void clay_layer_collection_settings_create(RenderEngine *UNUSED(engine), IDProperty *props)
{
	BLI_assert(props &&
	           props->type == IDP_GROUP &&
	           props->subtype == IDP_GROUP_SUB_ENGINE_RENDER);

	BKE_collection_engine_property_add_int(props, "matcap_icon", ICON_MATCAP_01);
	BKE_collection_engine_property_add_int(props, "type", CLAY_MATCAP_NONE);
	BKE_collection_engine_property_add_float(props, "matcap_rotation", 0.0f);
	BKE_collection_engine_property_add_float(props, "matcap_hue", 0.5f);
	BKE_collection_engine_property_add_float(props, "matcap_saturation", 0.5f);
	BKE_collection_engine_property_add_float(props, "matcap_value", 0.5f);
	BKE_collection_engine_property_add_float(props, "ssao_distance", 0.2f);
	BKE_collection_engine_property_add_float(props, "ssao_attenuation", 1.0f);
	BKE_collection_engine_property_add_float(props, "ssao_factor_cavity", 1.0f);
	BKE_collection_engine_property_add_float(props, "ssao_factor_edge", 1.0f);
	BKE_collection_engine_property_add_float(props, "hair_brightness_randomness", 0.0f);
}

static void clay_view_layer_settings_create(RenderEngine *UNUSED(engine), IDProperty *props)
{
	BLI_assert(props &&
	           props->type == IDP_GROUP &&
	           props->subtype == IDP_GROUP_SUB_ENGINE_RENDER);

	BKE_collection_engine_property_add_int(props, "ssao_samples", 16);
}

static void clay_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.clay_sh);
	DRW_SHADER_FREE_SAFE(e_data.clay_flat_sh);
	DRW_SHADER_FREE_SAFE(e_data.clay_prepass_flat_sh);
	DRW_SHADER_FREE_SAFE(e_data.clay_prepass_sh);
	DRW_SHADER_FREE_SAFE(e_data.clay_deferred_shading_sh);
	DRW_SHADER_FREE_SAFE(e_data.fxaa_sh);
	DRW_SHADER_FREE_SAFE(e_data.copy_sh);
	DRW_SHADER_FREE_SAFE(e_data.hair_sh);
	DRW_TEXTURE_FREE_SAFE(e_data.matcap_array);
}

static const DrawEngineDataSize clay_data_size = DRW_VIEWPORT_DATA_SIZE(CLAY_Data);

DrawEngineType draw_engine_clay_type = {
	NULL, NULL,
	N_("Clay"),
	&clay_data_size,
	&clay_engine_init,
	&clay_engine_free,
	&clay_cache_init,
	&clay_cache_populate,
	&clay_cache_finish,
	NULL,
	&clay_draw_scene,
	NULL,
	NULL,
	NULL,
};

RenderEngineType DRW_engine_viewport_clay_type = {
	NULL, NULL,
	CLAY_ENGINE, N_("Clay"), RE_INTERNAL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	&clay_layer_collection_settings_create,
	&clay_view_layer_settings_create,
	&draw_engine_clay_type,
	{NULL, NULL, NULL}
};


#undef CLAY_ENGINE

#endif  /* WITH_CLAY_ENGINE */
