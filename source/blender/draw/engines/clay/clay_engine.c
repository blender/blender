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

#include "DRW_render.h"

#include "DNA_particle_types.h"

#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_main.h"
#include "BKE_particle.h"

#include "BLI_dynstr.h"
#include "BLI_rand.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "UI_resources.h"
#include "UI_interface_icons.h"

#include "clay_engine.h"
#ifdef WITH_CLAY_ENGINE
/* Shaders */

#define CLAY_ENGINE "BLENDER_CLAY"

extern char datatoc_clay_frag_glsl[];
extern char datatoc_clay_vert_glsl[];
extern char datatoc_ssao_alchemy_glsl[];
extern char datatoc_particle_vert_glsl[];
extern char datatoc_particle_strand_frag_glsl[];

/* *********** LISTS *********** */

/* UBOs data needs to be 16 byte aligned (size of vec4) */
/* Reminder : float, int, bool are 4 bytes */
typedef struct CLAY_UBO_Material {
	float ssao_params_var[4];
	/* - 16 -*/
	float matcap_hsv[3];
	float matcap_id; /* even float encoding have enough precision */
	/* - 16 -*/
	float matcap_rot[2];
	float pad[2]; /* ensure 16 bytes alignement */
} CLAY_UBO_Material; /* 48 bytes */
BLI_STATIC_ASSERT_ALIGN(CLAY_UBO_Material, 16);

typedef struct CLAY_HAIR_UBO_Material {
	float hair_world;
	float hair_diffuse;
	float hair_specular;
	float hair_hardness;
	float hair_randomicity;
	float pad1[3];
	float hair_diffuse_color[3];
	float pad2;
	float hair_specular_color[3];
	float pad3;
} CLAY_HAIR_UBO_Material; /* 48 bytes */

#define MAX_CLAY_MAT 512 /* 512 = 9 bit material id */

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
	DRWShadingGroup *hair_shgrps[MAX_CLAY_MAT];
} CLAY_Storage;

typedef struct CLAY_StorageList {
	struct CLAY_Storage *storage;
	struct GPUUniformBuffer *mat_ubo;
	struct GPUUniformBuffer *hair_mat_ubo;
	struct CLAY_PrivateData *g_data;
} CLAY_StorageList;

typedef struct CLAY_FramebufferList {
	/* default */
	struct GPUFrameBuffer *default_fb;
	/* engine specific */
	struct GPUFrameBuffer *dupli_depth;
} CLAY_FramebufferList;

typedef struct CLAY_TextureList {
	/* default */
	struct GPUTexture *color;
	struct GPUTexture *depth;
	/* engine specific */
	struct GPUTexture *depth_dup;
} CLAY_TextureList;

typedef struct CLAY_PassList {
	struct DRWPass *depth_pass;
	struct DRWPass *depth_pass_cull;
	struct DRWPass *clay_pass;
	struct DRWPass *hair_pass;
} CLAY_PassList;

typedef struct CLAY_Data {
	void *engine_type;
	CLAY_FramebufferList *fbl;
	CLAY_TextureList *txl;
	CLAY_PassList *psl;
	CLAY_StorageList *stl;
} CLAY_Data;

/* *********** STATIC *********** */

static struct {
	/* Depth Pre Pass */
	struct GPUShader *depth_sh;
	/* Shading Pass */
	struct GPUShader *clay_sh;
	struct GPUShader *hair_sh;

	/* Matcap textures */
	struct GPUTexture *matcap_array;
	float matcap_colors[24][3];

	/* Ssao */
	float winmat[4][4];
	float viewvecs[3][4];
	float ssao_params[4];
	int cached_sample_num;
	struct GPUTexture *jitter_tx;
	struct GPUTexture *sampling_tx;

	/* hair */
	float hair_light[3];

	/* Just a serie of int from 0 to MAX_CLAY_MAT-1 */
	int ubo_mat_idxs[MAX_CLAY_MAT];
	int hair_ubo_mat_idxs[MAX_CLAY_MAT];
} e_data = {NULL}; /* Engine data */

typedef struct CLAY_PrivateData {
	DRWShadingGroup *depth_shgrp;
	DRWShadingGroup *depth_shgrp_select;
	DRWShadingGroup *depth_shgrp_active;
	DRWShadingGroup *depth_shgrp_cull;
	DRWShadingGroup *depth_shgrp_cull_select;
	DRWShadingGroup *depth_shgrp_cull_active;
	DRWShadingGroup *hair;
} CLAY_PrivateData; /* Transient data */

/* Functions */

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

	tex = DRW_texture_create_2D_array(w, h, nbr, DRW_TEX_RGBA_8, DRW_TEX_FILTER, final_rect);
	MEM_freeN(final_rect);

	return tex;
}

static int matcap_to_index(int matcap)
{
	switch (matcap) {
		case ICON_MATCAP_01: return 0;
		case ICON_MATCAP_02: return 1;
		case ICON_MATCAP_03: return 2;
		case ICON_MATCAP_04: return 3;
		case ICON_MATCAP_05: return 4;
		case ICON_MATCAP_06: return 5;
		case ICON_MATCAP_07: return 6;
		case ICON_MATCAP_08: return 7;
		case ICON_MATCAP_09: return 8;
		case ICON_MATCAP_10: return 9;
		case ICON_MATCAP_11: return 10;
		case ICON_MATCAP_12: return 11;
		case ICON_MATCAP_13: return 12;
		case ICON_MATCAP_14: return 13;
		case ICON_MATCAP_15: return 14;
		case ICON_MATCAP_16: return 15;
		case ICON_MATCAP_17: return 16;
		case ICON_MATCAP_18: return 17;
		case ICON_MATCAP_19: return 18;
		case ICON_MATCAP_20: return 19;
		case ICON_MATCAP_21: return 20;
		case ICON_MATCAP_22: return 21;
		case ICON_MATCAP_23: return 22;
		case ICON_MATCAP_24: return 23;
	}
	BLI_assert(!"Should not happen");
	return 0;
}

static struct GPUTexture *create_spiral_sample_texture(int numsaples)
{
	struct GPUTexture *tex;
	float (*texels)[2] = MEM_mallocN(sizeof(float[2]) * numsaples, "concentric_tex");
	const float numsaples_inv = 1.0f / numsaples;
	int i;
	/* arbitrary number to ensure we don't get conciding samples every circle */
	const float spirals = 7.357;

	for (i = 0; i < numsaples; i++) {
		float r = (i + 0.5f) * numsaples_inv;
		float phi = r * spirals * (float)(2.0 * M_PI);
		texels[i][0] = r * cosf(phi);
		texels[i][1] = r * sinf(phi);
	}

	tex = DRW_texture_create_1D(numsaples, DRW_TEX_RG_16, 0, (float *)texels);

	MEM_freeN(texels);
	return tex;
}

static struct GPUTexture *create_jitter_texture(void)
{
	float jitter[64 * 64][2];
	int i;

	/* TODO replace by something more evenly distributed like blue noise */
	for (i = 0; i < 64 * 64; i++) {
		jitter[i][0] = 2.0f * BLI_frand() - 1.0f;
		jitter[i][1] = 2.0f * BLI_frand() - 1.0f;
		normalize_v2(jitter[i]);
	}

	return DRW_texture_create_2D(64, 64, DRW_TEX_RG_16, DRW_TEX_FILTER | DRW_TEX_WRAP, &jitter[0][0]);
}

static void CLAY_engine_init(void *vedata)
{
	CLAY_StorageList *stl = ((CLAY_Data *)vedata)->stl;
	CLAY_TextureList *txl = ((CLAY_Data *)vedata)->txl;
	CLAY_FramebufferList *fbl = ((CLAY_Data *)vedata)->fbl;

	/* Create Texture Array */
	if (!e_data.matcap_array) {
		PreviewImage *prv[24]; /* For now use all of the 24 internal matcaps */

		/* TODO only load used matcaps */
		prv[0]  = UI_icon_to_preview(ICON_MATCAP_01);
		prv[1]  = UI_icon_to_preview(ICON_MATCAP_02);
		prv[2]  = UI_icon_to_preview(ICON_MATCAP_03);
		prv[3]  = UI_icon_to_preview(ICON_MATCAP_04);
		prv[4]  = UI_icon_to_preview(ICON_MATCAP_05);
		prv[5]  = UI_icon_to_preview(ICON_MATCAP_06);
		prv[6]  = UI_icon_to_preview(ICON_MATCAP_07);
		prv[7]  = UI_icon_to_preview(ICON_MATCAP_08);
		prv[8]  = UI_icon_to_preview(ICON_MATCAP_09);
		prv[9]  = UI_icon_to_preview(ICON_MATCAP_10);
		prv[10] = UI_icon_to_preview(ICON_MATCAP_11);
		prv[11] = UI_icon_to_preview(ICON_MATCAP_12);
		prv[12] = UI_icon_to_preview(ICON_MATCAP_13);
		prv[13] = UI_icon_to_preview(ICON_MATCAP_14);
		prv[14] = UI_icon_to_preview(ICON_MATCAP_15);
		prv[15] = UI_icon_to_preview(ICON_MATCAP_16);
		prv[16] = UI_icon_to_preview(ICON_MATCAP_17);
		prv[17] = UI_icon_to_preview(ICON_MATCAP_18);
		prv[18] = UI_icon_to_preview(ICON_MATCAP_19);
		prv[19] = UI_icon_to_preview(ICON_MATCAP_20);
		prv[20] = UI_icon_to_preview(ICON_MATCAP_21);
		prv[21] = UI_icon_to_preview(ICON_MATCAP_22);
		prv[22] = UI_icon_to_preview(ICON_MATCAP_23);
		prv[23] = UI_icon_to_preview(ICON_MATCAP_24);

		e_data.matcap_array = load_matcaps(prv, 24);
	}

	/* AO Jitter */
	if (!e_data.jitter_tx) {
		e_data.jitter_tx = create_jitter_texture();
	}


	/* Depth prepass */
	if (!e_data.depth_sh) {
		e_data.depth_sh = DRW_shader_create_3D_depth_only();
	}

	/* Shading pass */
	if (!e_data.clay_sh) {
		DynStr *ds = BLI_dynstr_new();
		const char *max_mat =
			"#define MAX_MATERIAL 512\n"
			"#define USE_ROTATION\n"
			"#define USE_AO\n"
			"#define USE_HSV\n";
		char *matcap_with_ao;

		BLI_dynstr_append(ds, datatoc_clay_frag_glsl);
		BLI_dynstr_append(ds, datatoc_ssao_alchemy_glsl);

		matcap_with_ao = BLI_dynstr_get_cstring(ds);

		e_data.clay_sh = DRW_shader_create(datatoc_clay_vert_glsl, NULL, matcap_with_ao, max_mat);

		BLI_dynstr_free(ds);
		MEM_freeN(matcap_with_ao);
	}

	if (!e_data.hair_sh) {
		e_data.hair_sh = DRW_shader_create(datatoc_particle_vert_glsl, NULL, datatoc_particle_strand_frag_glsl, "#define MAX_MATERIAL 512\n");
	}

	if (!stl->storage) {
		stl->storage = MEM_callocN(sizeof(CLAY_Storage), "CLAY_Storage");
	}

	if (!stl->mat_ubo) {
		stl->mat_ubo = DRW_uniformbuffer_create(sizeof(CLAY_UBO_Storage), NULL);
	}

	if (!stl->hair_mat_ubo) {
		stl->hair_mat_ubo = DRW_uniformbuffer_create(sizeof(CLAY_HAIR_UBO_Storage), NULL);
	}

	if (e_data.ubo_mat_idxs[1] == 0) {
		/* Just int to have pointers to them */
		for (int i = 0; i < MAX_CLAY_MAT; ++i) {
			e_data.ubo_mat_idxs[i] = i;
		}
	}

	if (DRW_state_is_fbo()) {
		const float *viewport_size = DRW_viewport_size_get();
		DRWFboTexture tex = {&txl->depth_dup, DRW_BUF_DEPTH_24, 0};
		DRW_framebuffer_init(&fbl->dupli_depth,
		                     (int)viewport_size[0], (int)viewport_size[1],
		                     &tex, 1);
	}

	/* SSAO setup */
	{
		const DRWContextState *draw_ctx = DRW_context_state_get();
		SceneLayer *scene_layer = draw_ctx->sl;
		IDProperty *props = BKE_scene_layer_engine_evaluated_get(scene_layer, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_CLAY);
		int ssao_samples = BKE_collection_engine_property_value_get_int(props, "ssao_samples");

		float invproj[4][4];
		float dfdyfacs[2];
		const bool is_persp = DRW_viewport_is_persp_get();
		/* view vectors for the corners of the view frustum. Can be used to recreate the world space position easily */
		float viewvecs[3][4] = {
		    {-1.0f, -1.0f, -1.0f, 1.0f},
		    {1.0f, -1.0f, -1.0f, 1.0f},
		    {-1.0f, 1.0f, -1.0f, 1.0f}
		};
		int i;
		const float *size = DRW_viewport_size_get();

		DRW_state_dfdy_factors_get(dfdyfacs);

		e_data.ssao_params[0] = ssao_samples;
		e_data.ssao_params[1] = size[0] / 64.0;
		e_data.ssao_params[2] = size[1] / 64.0;
		e_data.ssao_params[3] = dfdyfacs[1]; /* dfdy sign for offscreen */

		/* invert the view matrix */
		DRW_viewport_matrix_get(e_data.winmat, DRW_MAT_WIN);
		invert_m4_m4(invproj, e_data.winmat);

		/* convert the view vectors to view space */
		for (i = 0; i < 3; i++) {
			mul_m4_v4(invproj, viewvecs[i]);
			/* normalized trick see:
			 * http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
			mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][3]);
			if (is_persp)
				mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][2]);
			viewvecs[i][3] = 1.0;

			copy_v4_v4(e_data.viewvecs[i], viewvecs[i]);
		}

		/* we need to store the differences */
		e_data.viewvecs[1][0] -= e_data.viewvecs[0][0];
		e_data.viewvecs[1][1] = e_data.viewvecs[2][1] - e_data.viewvecs[0][1];

		/* calculate a depth offset as well */
		if (!is_persp) {
			float vec_far[] = {-1.0f, -1.0f, 1.0f, 1.0f};
			mul_m4_v4(invproj, vec_far);
			mul_v3_fl(vec_far, 1.0f / vec_far[3]);
			e_data.viewvecs[1][2] = vec_far[2] - e_data.viewvecs[0][2];
		}

		/* AO Samples Tex */
		if (e_data.sampling_tx && (e_data.cached_sample_num != ssao_samples)) {
			DRW_texture_free(e_data.sampling_tx);
			e_data.sampling_tx = NULL;
		}

		if (!e_data.sampling_tx) {
			e_data.sampling_tx = create_spiral_sample_texture(ssao_samples);
			e_data.cached_sample_num = ssao_samples;
		}
	}

	/* hair setup */
	{
		e_data.hair_light[0] = 1.0f;
		e_data.hair_light[1] = -0.5f;
		e_data.hair_light[2] = -0.7f;
	}
}

static DRWShadingGroup *CLAY_shgroup_create(CLAY_Data *vedata, DRWPass *pass, int *material_id)
{
	CLAY_TextureList *txl = ((CLAY_Data *)vedata)->txl;

	DRWShadingGroup *grp = DRW_shgroup_create(e_data.clay_sh, pass);

	DRW_shgroup_uniform_vec2(grp, "screenres", DRW_viewport_size_get(), 1);
	DRW_shgroup_uniform_buffer(grp, "depthtex", &txl->depth_dup);
	DRW_shgroup_uniform_texture(grp, "matcaps", e_data.matcap_array);
	DRW_shgroup_uniform_mat4(grp, "WinMatrix", (float *)e_data.winmat);
	DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)e_data.viewvecs, 3);
	DRW_shgroup_uniform_vec4(grp, "ssao_params", e_data.ssao_params, 1);
	DRW_shgroup_uniform_vec3(grp, "matcaps_color[0]", (float *)e_data.matcap_colors, 24);

	DRW_shgroup_uniform_int(grp, "mat_id", material_id, 1);

	DRW_shgroup_uniform_texture(grp, "ssao_jitter", e_data.jitter_tx);
	DRW_shgroup_uniform_texture(grp, "ssao_samples", e_data.sampling_tx);

	return grp;
}

static DRWShadingGroup *CLAY_hair_shgroup_create(DRWPass *pass, int *material_id)
{
	DRWShadingGroup *grp = DRW_shgroup_create(e_data.hair_sh, pass);

	DRW_shgroup_uniform_vec3(grp, "light", e_data.hair_light, 1);
	DRW_shgroup_uniform_int(grp, "mat_id", material_id, 1);

	return grp;
}

static int search_mat_to_ubo(
        CLAY_Storage *storage, float matcap_rot, float matcap_hue, float matcap_sat,
        float matcap_val, float ssao_distance, float ssao_factor_cavity,
        float ssao_factor_edge, float ssao_attenuation, int matcap_icon)
{
	/* For now just use a linear search and test all parameters */
	/* TODO make a hash table */
	for (int i = 0; i < storage->ubo_current_id; ++i) {
		CLAY_UBO_Material *ubo = &storage->mat_storage.materials[i];

		if ((ubo->matcap_rot[0] == cosf(matcap_rot * 3.14159f * 2.0f)) &&
		    (ubo->matcap_hsv[0] == matcap_hue + 0.5f) &&
		    (ubo->matcap_hsv[1] == matcap_sat * 2.0f) &&
		    (ubo->matcap_hsv[2] == matcap_val * 2.0f) &&
		    (ubo->ssao_params_var[0] == ssao_distance) &&
		    (ubo->ssao_params_var[1] == ssao_factor_cavity) &&
		    (ubo->ssao_params_var[2] == ssao_factor_edge) &&
		    (ubo->ssao_params_var[3] == ssao_attenuation) &&
		    (ubo->matcap_id == matcap_to_index(matcap_icon)))
		{
			return i;
		}
	}

	return -1;
}

static int search_hair_mat_to_ubo(CLAY_Storage *storage, float hair_world, float hair_diffuse, float hair_specular,
                                  float hair_hardness, float hair_randomicity, const float *hair_diff_color,
                                  const float *hair_spec_color)
{
	/* For now just use a linear search and test all parameters */
	/* TODO make a hash table */
	for (int i = 0; i < storage->hair_ubo_current_id; ++i) {
		CLAY_HAIR_UBO_Material *ubo = &storage->hair_mat_storage.materials[i];

		if ((ubo->hair_world == hair_world) &&
		    (ubo->hair_diffuse == hair_diffuse) &&
		    (ubo->hair_specular == hair_specular) &&
		    (ubo->hair_hardness == hair_hardness) &&
		    (ubo->hair_randomicity == hair_randomicity) &&
		    (ubo->hair_diffuse_color[0] == hair_diff_color[0]) &&
		    (ubo->hair_diffuse_color[1] == hair_diff_color[1]) &&
		    (ubo->hair_diffuse_color[2] == hair_diff_color[2]) &&
		    (ubo->hair_diffuse_color[3] == hair_diff_color[3]) &&
		    (ubo->hair_specular_color[0] == hair_spec_color[0]) &&
		    (ubo->hair_specular_color[1] == hair_spec_color[1]) &&
		    (ubo->hair_specular_color[2] == hair_spec_color[2]) &&
		    (ubo->hair_specular_color[3] == hair_spec_color[3]))
		{
			return i;
		}
	}

	return -1;
}

static int push_mat_to_ubo(CLAY_Storage *storage, float matcap_rot, float matcap_hue, float matcap_sat,
                            float matcap_val, float ssao_distance, float ssao_factor_cavity,
                            float ssao_factor_edge, float ssao_attenuation, int matcap_icon)
{
	int id = storage->ubo_current_id;
	CLAY_UBO_Material *ubo = &storage->mat_storage.materials[id];

	ubo->matcap_rot[0] = cosf(matcap_rot * 3.14159f * 2.0f);
	ubo->matcap_rot[1] = sinf(matcap_rot * 3.14159f * 2.0f);

	ubo->matcap_hsv[0] = matcap_hue + 0.5f;
	ubo->matcap_hsv[1] = matcap_sat * 2.0f;
	ubo->matcap_hsv[2] = matcap_val * 2.0f;

	/* small optimisation : make samples not spread if we don't need ssao */
	ubo->ssao_params_var[0] = (ssao_factor_cavity + ssao_factor_edge > 0.0f) ? ssao_distance : 0.0f;
	ubo->ssao_params_var[1] = ssao_factor_cavity;
	ubo->ssao_params_var[2] = ssao_factor_edge;
	ubo->ssao_params_var[3] = ssao_attenuation;

	ubo->matcap_id = matcap_to_index(matcap_icon);

	storage->ubo_current_id++;

	return id;
}

static int push_hair_mat_to_ubo(CLAY_Storage *storage, float hair_world, float hair_diffuse, float hair_specular,
                                  float hair_hardness, float hair_randomicity, const float *hair_diff_color,
                                const float *hair_spec_color)
{
	int id = storage->hair_ubo_current_id;
	CLAY_HAIR_UBO_Material *ubo = &storage->hair_mat_storage.materials[id];

	ubo->hair_world = hair_world;
	ubo->hair_diffuse = hair_diffuse;
	ubo->hair_specular = hair_specular;
	ubo->hair_hardness = hair_hardness;
	ubo->hair_randomicity = hair_randomicity;
	ubo->hair_diffuse_color[0] = hair_diff_color[0];
	ubo->hair_diffuse_color[1] = hair_diff_color[1];
	ubo->hair_diffuse_color[2] = hair_diff_color[2];
	ubo->hair_diffuse_color[3] = hair_diff_color[3];
	ubo->hair_specular_color[0] = hair_spec_color[0];
	ubo->hair_specular_color[1] = hair_spec_color[1];
	ubo->hair_specular_color[2] = hair_spec_color[2];
	ubo->hair_specular_color[3] = hair_spec_color[3];

	storage->hair_ubo_current_id++;

	return id;
}

static int mat_in_ubo(CLAY_Storage *storage, float matcap_rot, float matcap_hue, float matcap_sat,
                      float matcap_val, float ssao_distance, float ssao_factor_cavity,
                      float ssao_factor_edge, float ssao_attenuation, int matcap_icon)
{
	/* Search material in UBO */
	int id = search_mat_to_ubo(storage, matcap_rot, matcap_hue, matcap_sat, matcap_val,
	                           ssao_distance, ssao_factor_cavity, ssao_factor_edge,
	                           ssao_attenuation, matcap_icon);

	/* if not found create it */
	if (id == -1) {
		id = push_mat_to_ubo(storage, matcap_rot, matcap_hue, matcap_sat, matcap_val,
		                     ssao_distance, ssao_factor_cavity, ssao_factor_edge,
		                     ssao_attenuation, matcap_icon);
	}

	return id;
}

static int hair_mat_in_ubo(CLAY_Storage *storage, float hair_world, float hair_diffuse, float hair_specular,
                           float hair_hardness, float hair_randomicity, const float *hair_diff_color,
                           const float *hair_spec_color)
{
	/* Search material in UBO */
	int id = search_hair_mat_to_ubo(storage, hair_world, hair_diffuse, hair_specular,
	                                hair_hardness, hair_randomicity, hair_diff_color, hair_spec_color);

	/* if not found create it */
	if (id == -1) {
		id = push_hair_mat_to_ubo(storage, hair_world, hair_diffuse, hair_specular,
		                          hair_hardness, hair_randomicity, hair_diff_color, hair_spec_color);
	}

	return id;
}

static DRWShadingGroup *CLAY_object_shgrp_get(CLAY_Data *vedata, Object *ob, CLAY_StorageList *stl, CLAY_PassList *psl)
{
	DRWShadingGroup **shgrps = stl->storage->shgrps;
	IDProperty *props = BKE_layer_collection_engine_evaluated_get(ob, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_CLAY);

	/* Default Settings */
	float matcap_rot = BKE_collection_engine_property_value_get_float(props, "matcap_rotation");
	float matcap_hue = BKE_collection_engine_property_value_get_float(props, "matcap_hue");
	float matcap_sat = BKE_collection_engine_property_value_get_float(props, "matcap_saturation");
	float matcap_val = BKE_collection_engine_property_value_get_float(props, "matcap_value");
	float ssao_distance = BKE_collection_engine_property_value_get_float(props, "ssao_distance");
	float ssao_factor_cavity = BKE_collection_engine_property_value_get_float(props, "ssao_factor_cavity");
	float ssao_factor_edge = BKE_collection_engine_property_value_get_float(props, "ssao_factor_edge");
	float ssao_attenuation = BKE_collection_engine_property_value_get_float(props, "ssao_attenuation");
	int matcap_icon = BKE_collection_engine_property_value_get_int(props, "matcap_icon");

	int id = mat_in_ubo(stl->storage, matcap_rot, matcap_hue, matcap_sat, matcap_val,
	                    ssao_distance, ssao_factor_cavity, ssao_factor_edge,
	                    ssao_attenuation, matcap_icon);

	if (shgrps[id] == NULL) {
		shgrps[id] = CLAY_shgroup_create(vedata, psl->clay_pass, &e_data.ubo_mat_idxs[id]);
		/* if it's the first shgrp, pass bind the material UBO */
		if (stl->storage->ubo_current_id == 1) {
			DRW_shgroup_uniform_block(shgrps[0], "material_block", stl->mat_ubo);
		}
	}

	return shgrps[id];
}

static DRWShadingGroup *CLAY_hair_shgrp_get(Object *ob, CLAY_StorageList *stl, CLAY_PassList *psl)
{
	DRWShadingGroup **hair_shgrps = stl->storage->hair_shgrps;
	IDProperty *props = BKE_layer_collection_engine_evaluated_get(ob, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_CLAY);

	/* Default Settings */
	float hair_world = BKE_collection_engine_property_value_get_float(props, "world_intensity");
	float hair_diffuse = BKE_collection_engine_property_value_get_float(props, "diffuse_intensity");
	float hair_specular = BKE_collection_engine_property_value_get_float(props, "specular_intensity");
	float hair_hardness = BKE_collection_engine_property_value_get_float(props, "specular_hardness");
	float hair_randomicity = BKE_collection_engine_property_value_get_float(props, "color_randomicity");
	const float *hair_diff_color = BKE_collection_engine_property_value_get_float_array(props, "hair_diffuse_color");
	const float *hair_spec_color = BKE_collection_engine_property_value_get_float_array(props, "hair_specular_color");

	int hair_id = hair_mat_in_ubo(stl->storage, hair_world, hair_diffuse, hair_specular,
	                              hair_hardness, hair_randomicity, hair_diff_color, hair_spec_color);

	if (hair_shgrps[hair_id] == NULL) {
		hair_shgrps[hair_id] = CLAY_hair_shgroup_create(psl->hair_pass, &e_data.hair_ubo_mat_idxs[hair_id]);
		/* if it's the first shgrp, pass bind the material UBO */
		if (stl->storage->hair_ubo_current_id == 1) {
			DRW_shgroup_uniform_block(hair_shgrps[0], "material_block", stl->hair_mat_ubo);
		}
	}

	return hair_shgrps[hair_id];
}

static void CLAY_cache_init(void *vedata)
{
	CLAY_PassList *psl = ((CLAY_Data *)vedata)->psl;
	CLAY_StorageList *stl = ((CLAY_Data *)vedata)->stl;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	/* Depth Pass */
	{
		psl->depth_pass = DRW_pass_create("Depth Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
		stl->g_data->depth_shgrp = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass);

		psl->depth_pass_cull = DRW_pass_create(
		        "Depth Pass Cull",
		        DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_CULL_BACK);
		stl->g_data->depth_shgrp_cull = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass_cull);
	}

	/* Clay Pass */
	{
		psl->clay_pass = DRW_pass_create("Clay Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);
		stl->storage->ubo_current_id = 0;
		memset(stl->storage->shgrps, 0, sizeof(DRWShadingGroup *) * MAX_CLAY_MAT);
	}

	/* Hair Pass */
	{
		psl->hair_pass = DRW_pass_create("Hair Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
		stl->storage->hair_ubo_current_id = 0;
		memset(stl->storage->hair_shgrps, 0, sizeof(DRWShadingGroup *) * MAX_CLAY_MAT);
	}
}

static void CLAY_cache_populate(void *vedata, Object *ob)
{
	CLAY_PassList *psl = ((CLAY_Data *)vedata)->psl;
	CLAY_StorageList *stl = ((CLAY_Data *)vedata)->stl;

	DRWShadingGroup *clay_shgrp, *hair_shgrp;

	if (!DRW_object_is_renderable(ob))
		return;

	bool sculpt_mode = ob->mode & OB_MODE_SCULPT;

	struct Batch *geom = DRW_cache_object_surface_get(ob);
	if (geom) {
		IDProperty *ces_mode_ob = BKE_layer_collection_engine_evaluated_get(ob, COLLECTION_MODE_OBJECT, "");
		bool do_cull = BKE_collection_engine_property_value_get_bool(ces_mode_ob, "show_backface_culling");

		/* Depth Prepass */
		if (sculpt_mode) {
			DRW_shgroup_call_sculpt_add((do_cull) ? stl->g_data->depth_shgrp_cull : stl->g_data->depth_shgrp, ob, ob->obmat);
		}
		else {
			DRW_shgroup_call_add((do_cull) ? stl->g_data->depth_shgrp_cull : stl->g_data->depth_shgrp, geom, ob->obmat);
		}

		/* Shading */
		clay_shgrp = CLAY_object_shgrp_get(vedata, ob, stl, psl);

		if (sculpt_mode) {
			DRW_shgroup_call_sculpt_add(clay_shgrp, ob, ob->obmat);
		}
		else {
			DRW_shgroup_call_add(clay_shgrp, geom, ob->obmat);
		}
	}

	if (ob->type == OB_MESH) {
		for (ParticleSystem *psys = ob->particlesystem.first; psys; psys = psys->next) {
			if (psys_check_enabled(ob, psys, false)) {
				ParticleSettings *part = psys->part;
				int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;

				if (draw_as == PART_DRAW_PATH && !psys->pathcache && !psys->childcache) {
					draw_as = PART_DRAW_DOT;
				}

				switch (draw_as) {
					case PART_DRAW_PATH:
						geom = DRW_cache_particles_get_hair(psys);
						break;
					default:
						geom = NULL;
						break;
				}

				if (geom) {
					static float mat[4][4];
					unit_m4(mat);
					hair_shgrp = CLAY_hair_shgrp_get(ob, stl, psl);
					DRW_shgroup_call_add(hair_shgrp, geom, mat);
				}
			}
		}
	}
}

static void CLAY_cache_finish(void *vedata)
{
	CLAY_StorageList *stl = ((CLAY_Data *)vedata)->stl;

	DRW_uniformbuffer_update(stl->mat_ubo, &stl->storage->mat_storage);
	DRW_uniformbuffer_update(stl->hair_mat_ubo, &stl->storage->hair_mat_storage);
}

static void CLAY_draw_scene(void *vedata)
{

	CLAY_PassList *psl = ((CLAY_Data *)vedata)->psl;
	CLAY_FramebufferList *fbl = ((CLAY_Data *)vedata)->fbl;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

	/* Pass 1 : Depth pre-pass */
	DRW_draw_pass(psl->depth_pass);
	DRW_draw_pass(psl->depth_pass_cull);

	/* Pass 2 : Duplicate depth */
	/* Unless we go for deferred shading we need this to avoid manual depth test and artifacts */
	if (DRW_state_is_fbo()) {
		DRW_framebuffer_blit(dfbl->default_fb, fbl->dupli_depth, true);
	}

	/* Pass 3 : Shading */
	DRW_draw_pass(psl->clay_pass);
	DRW_draw_pass(psl->hair_pass);
}

static void CLAY_layer_collection_settings_create(RenderEngine *UNUSED(engine), IDProperty *props)
{
	BLI_assert(props &&
	           props->type == IDP_GROUP &&
	           props->subtype == IDP_GROUP_SUB_ENGINE_RENDER);

	static float default_hair_diffuse_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	static float default_hair_specular_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	BKE_collection_engine_property_add_int(props, "matcap_icon", ICON_MATCAP_01);
	BKE_collection_engine_property_add_int(props, "type", CLAY_MATCAP_NONE);
	BKE_collection_engine_property_add_float(props, "matcap_rotation", 0.0f);
	BKE_collection_engine_property_add_float(props, "matcap_hue", 0.5f);
	BKE_collection_engine_property_add_float(props, "matcap_saturation", 0.5f);
	BKE_collection_engine_property_add_float(props, "matcap_value", 0.5f);
	BKE_collection_engine_property_add_float(props, "ssao_distance", 0.2f);
	BKE_collection_engine_property_add_float(props, "ssao_attenuation", 1.0f);
	BKE_collection_engine_property_add_float(props, "ssao_factor_cavity", 1.0f);
	BKE_collection_engine_property_add_float(props, "world_intensity", 0.1f);
	BKE_collection_engine_property_add_float(props, "diffuse_intensity", 0.2f);
	BKE_collection_engine_property_add_float(props, "specular_intensity", 0.3f);
	BKE_collection_engine_property_add_float(props, "specular_hardness", 4.0f);
	BKE_collection_engine_property_add_float(props, "color_randomicity", 0.0f);
	BKE_collection_engine_property_add_float_array(props, "hair_diffuse_color", default_hair_diffuse_color, 4);
	BKE_collection_engine_property_add_float_array(props, "hair_specular_color", default_hair_specular_color, 4);
}

static void CLAY_scene_layer_settings_create(RenderEngine *UNUSED(engine), IDProperty *props)
{
	BLI_assert(props &&
	           props->type == IDP_GROUP &&
	           props->subtype == IDP_GROUP_SUB_ENGINE_RENDER);

	BKE_collection_engine_property_add_int(props, "ssao_samples", 32);
}

static void CLAY_engine_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.clay_sh);
	DRW_SHADER_FREE_SAFE(e_data.hair_sh);
	DRW_TEXTURE_FREE_SAFE(e_data.matcap_array);
	DRW_TEXTURE_FREE_SAFE(e_data.jitter_tx);
	DRW_TEXTURE_FREE_SAFE(e_data.sampling_tx);
}

static const DrawEngineDataSize CLAY_data_size = DRW_VIEWPORT_DATA_SIZE(CLAY_Data);

DrawEngineType draw_engine_clay_type = {
	NULL, NULL,
	N_("Clay"),
	&CLAY_data_size,
	&CLAY_engine_init,
	&CLAY_engine_free,
	&CLAY_cache_init,
	&CLAY_cache_populate,
	&CLAY_cache_finish,
	NULL,
	&CLAY_draw_scene
};

RenderEngineType DRW_engine_viewport_clay_type = {
	NULL, NULL,
	CLAY_ENGINE, N_("Clay"), RE_INTERNAL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    &CLAY_layer_collection_settings_create,
    &CLAY_scene_layer_settings_create,
	&draw_engine_clay_type,
	{NULL, NULL, NULL}
};


#undef CLAY_ENGINE

#endif
