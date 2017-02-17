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

#include "BKE_icons.h"
#include "BKE_main.h"

#include "BLI_dynstr.h"
#include "BLI_rand.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "UI_resources.h"
#include "UI_interface_icons.h"

#include "clay.h"
#ifdef WITH_CLAY_ENGINE
/* Shaders */

#define CLAY_ENGINE "BLENDER_CLAY"

extern char datatoc_clay_frag_glsl[];
extern char datatoc_clay_vert_glsl[];
extern char datatoc_ssao_alchemy_glsl[];
extern char datatoc_ssao_groundtruth_glsl[];

/* Storage */

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

#define MAX_CLAY_MAT 512 /* 512 = 9 bit material id */

typedef struct CLAY_UBO_Storage {
	CLAY_UBO_Material materials[MAX_CLAY_MAT];
} CLAY_UBO_Storage;

static struct CLAY_data {
	/* Depth Pre Pass */
	struct GPUShader *depth_sh;
	/* Shading Pass */
	struct GPUShader *clay_sh;

	/* Matcap textures */
	struct GPUTexture *matcap_array;
	float matcap_colors[24][3];

	/* Ssao */
	float winmat[4][4];
	float viewvecs[3][4];
	float ssao_params[4];
	struct GPUTexture *jitter_tx;
	struct GPUTexture *sampling_tx;
} data = {NULL};

/* GPUViewport.storage
 * Is freed everytime the viewport engine changes */
typedef struct CLAY_Storage {
	/* Materials Parameter UBO */
	CLAY_UBO_Storage mat_storage;
	int ubo_current_id;
	DRWShadingGroup *shgrps[MAX_CLAY_MAT];
} CLAY_Storage;

/* Just a serie of int from 0 to MAX_CLAY_MAT-1 */
static int ubo_mat_idxs[MAX_CLAY_MAT] = {0};

/* keep it under MAX_STORAGE */
typedef struct CLAY_StorageList {
	struct CLAY_Storage *storage;
	struct GPUUniformBuffer *mat_ubo;
} CLAY_StorageList;

/* keep it under MAX_BUFFERS */
typedef struct CLAY_FramebufferList{
	/* default */
	struct GPUFrameBuffer *default_fb;
	/* engine specific */
	struct GPUFrameBuffer *dupli_depth;
} CLAY_FramebufferList;

/* keep it under MAX_TEXTURES */
typedef struct CLAY_TextureList{
	/* default */
	struct GPUTexture *color;
	struct GPUTexture *depth;
	/* engine specific */
	struct GPUTexture *depth_dup;
} CLAY_TextureList;

/* for clarity follow the same layout as CLAY_TextureList */
enum {
	SCENE_COLOR,
	SCENE_DEPTH,
	SCENE_DEPTH_DUP,
};

/* keep it under MAX_PASSES */
typedef struct CLAY_PassList{
	struct DRWPass *depth_pass;
	struct DRWPass *depth_pass_cull;
	struct DRWPass *clay_pass;
} CLAY_PassList;

//#define GTAO

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
			data.matcap_colors[layer][0] += new_rect[y * 512 * 128 * 4 + x * 128 * 4 + 0];
			data.matcap_colors[layer][1] += new_rect[y * 512 * 128 * 4 + x * 128 * 4 + 1];
			data.matcap_colors[layer][2] += new_rect[y * 512 * 128 * 4 + x * 128 * 4 + 2];
		}
	}

	data.matcap_colors[layer][0] /= 16.0f * 2.0f; /* the * 2 is to darken for shadows */
	data.matcap_colors[layer][1] /= 16.0f * 2.0f;
	data.matcap_colors[layer][2] /= 16.0f * 2.0f;
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
	if (matcap == ICON_MATCAP_02) return 1;
	else if (matcap == ICON_MATCAP_03) return 2;
	else if (matcap == ICON_MATCAP_04) return 3;
	else if (matcap == ICON_MATCAP_05) return 4;
	else if (matcap == ICON_MATCAP_06) return 5;
	else if (matcap == ICON_MATCAP_07) return 6;
	else if (matcap == ICON_MATCAP_08) return 7;
	else if (matcap == ICON_MATCAP_09) return 8;
	else if (matcap == ICON_MATCAP_10) return 9;
	else if (matcap == ICON_MATCAP_11) return 10;
	else if (matcap == ICON_MATCAP_12) return 11;
	else if (matcap == ICON_MATCAP_13) return 12;
	else if (matcap == ICON_MATCAP_14) return 13;
	else if (matcap == ICON_MATCAP_15) return 14;
	else if (matcap == ICON_MATCAP_16) return 15;
	else if (matcap == ICON_MATCAP_17) return 16;
	else if (matcap == ICON_MATCAP_18) return 17;
	else if (matcap == ICON_MATCAP_19) return 18;
	else if (matcap == ICON_MATCAP_20) return 19;
	else if (matcap == ICON_MATCAP_21) return 20;
	else if (matcap == ICON_MATCAP_22) return 21;
	else if (matcap == ICON_MATCAP_23) return 22;
	else if (matcap == ICON_MATCAP_24) return 23;
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
#ifdef GTAO
		jitter[i][0] = BLI_frand();
		jitter[i][1] = BLI_frand();
#else
		jitter[i][0] = 2.0f * BLI_frand() - 1.0f;
		jitter[i][1] = 2.0f * BLI_frand() - 1.0f;
		normalize_v2(jitter[i]);
#endif
	}

	return DRW_texture_create_2D(64, 64, DRW_TEX_RG_16, DRW_TEX_FILTER | DRW_TEX_WRAP, &jitter[0][0]);
}

static void clay_material_settings_init(MaterialEngineSettingsClay *ma)
{
	ma->matcap_icon = ICON_MATCAP_01;
	ma->matcap_rot = 0.0f;
	ma->matcap_hue = 0.5f;
	ma->matcap_sat = 0.5f;
	ma->matcap_val = 0.5f;
	ma->ssao_distance = 0.2;
	ma->ssao_attenuation = 1.0f;
	ma->ssao_factor_cavity = 1.0f;
	ma->ssao_factor_edge = 1.0f;
}

RenderEngineSettings *CLAY_render_settings_create(void)
{
	RenderEngineSettingsClay *settings = MEM_callocN(sizeof(RenderEngineSettingsClay), "RenderEngineSettingsClay");

	clay_material_settings_init((MaterialEngineSettingsClay *)settings);

	settings->ssao_samples = 32;

	return (RenderEngineSettings *)settings;
}

MaterialEngineSettings *CLAY_material_settings_create(void)
{
	MaterialEngineSettingsClay *settings = MEM_callocN(sizeof(MaterialEngineSettingsClay), "MaterialEngineSettingsClay");

	clay_material_settings_init(settings);

	return (MaterialEngineSettings *)settings;
}

static void CLAY_engine_init(void)
{
	CLAY_StorageList *stl = DRW_engine_storage_list_get();
	CLAY_TextureList *txl = DRW_engine_texture_list_get();
	CLAY_FramebufferList *fbl = DRW_engine_framebuffer_list_get();

	/* Create Texture Array */
	if (!data.matcap_array) {
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

		data.matcap_array = load_matcaps(prv, 24);
	}

	/* AO Jitter */
	if (!data.jitter_tx) {
		data.jitter_tx = create_jitter_texture();
	}

	/* AO Samples */
	/* TODO use hammersley sequence */
	if (!data.sampling_tx) {
		data.sampling_tx = create_spiral_sample_texture(500);
	}

	/* Depth prepass */
	if (!data.depth_sh) {
		data.depth_sh = DRW_shader_create_3D_depth_only();
	}

	/* Shading pass */
	if (!data.clay_sh) {
		DynStr *ds = BLI_dynstr_new();
		const char *max_mat =
			"#define MAX_MATERIAL 512\n"
			"#define USE_ROTATION\n"
			"#define USE_AO\n"
			"#define USE_HSV\n";
		char *matcap_with_ao;

		BLI_dynstr_append(ds, datatoc_clay_frag_glsl);
#ifdef GTAO
		BLI_dynstr_append(ds, datatoc_ssao_groundtruth_glsl);
#else
		BLI_dynstr_append(ds, datatoc_ssao_alchemy_glsl);
#endif

		matcap_with_ao = BLI_dynstr_get_cstring(ds);

		data.clay_sh = DRW_shader_create(datatoc_clay_vert_glsl, NULL, matcap_with_ao, max_mat);

		BLI_dynstr_free(ds);
		MEM_freeN(matcap_with_ao);
	}

	if (!stl->storage) {
		stl->storage = MEM_callocN(sizeof(CLAY_Storage), "CLAY_Storage");
	}

	if (!stl->mat_ubo) {
		stl->mat_ubo = DRW_uniformbuffer_create(sizeof(CLAY_UBO_Storage), NULL);
	}

	if (ubo_mat_idxs[1] == 0) {
		/* Just int to have pointers to them */
		for (int i = 0; i < MAX_CLAY_MAT; ++i) {
			ubo_mat_idxs[i] = i;
		}
	}

	{
		float *viewport_size = DRW_viewport_size_get();
		DRWFboTexture tex = {&txl->depth_dup, DRW_BUF_DEPTH_24};
		DRW_framebuffer_init(&fbl->dupli_depth,
		                     (int)viewport_size[0], (int)viewport_size[1],
		                     &tex, 1);
	}

	/* SSAO setup */
	{
		float invproj[4][4];
		float dfdyfacs[2];
		bool is_persp = DRW_viewport_is_persp_get();
		/* view vectors for the corners of the view frustum. Can be used to recreate the world space position easily */
		float viewvecs[3][4] = {
		    {-1.0f, -1.0f, -1.0f, 1.0f},
		    {1.0f, -1.0f, -1.0f, 1.0f},
		    {-1.0f, 1.0f, -1.0f, 1.0f}
		};
		int i;
		float *size = DRW_viewport_size_get();
		RenderEngineSettingsClay *settings = DRW_render_settings_get(NULL, RE_engine_id_BLENDER_CLAY);

		DRW_get_dfdy_factors(dfdyfacs);

		data.ssao_params[0] = settings->ssao_samples;
		data.ssao_params[1] = size[0] / 64.0;
		data.ssao_params[2] = size[1] / 64.0;
		data.ssao_params[3] = dfdyfacs[1]; /* dfdy sign for offscreen */

		/* invert the view matrix */
		DRW_viewport_matrix_get(data.winmat, DRW_MAT_WIN);
		invert_m4_m4(invproj, data.winmat);

		/* convert the view vectors to view space */
		for (i = 0; i < 3; i++) {
			mul_m4_v4(invproj, viewvecs[i]);
			/* normalized trick see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
			mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][3]);
			if (is_persp)
				mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][2]);
			viewvecs[i][3] = 1.0;

			copy_v4_v4(data.viewvecs[i], viewvecs[i]);
		}

		/* we need to store the differences */
		data.viewvecs[1][0] -= data.viewvecs[0][0];
		data.viewvecs[1][1] = data.viewvecs[2][1] - data.viewvecs[0][1];

		/* calculate a depth offset as well */
		if (!is_persp) {
			float vec_far[] = {-1.0f, -1.0f, 1.0f, 1.0f};
			mul_m4_v4(invproj, vec_far);
			mul_v3_fl(vec_far, 1.0f / vec_far[3]);
			data.viewvecs[1][2] = vec_far[2] - data.viewvecs[0][2];
		}
	}
}

static DRWShadingGroup *CLAY_shgroup_create(DRWPass *pass, int *material_id)
{
	const int depthloc = 0, matcaploc = 1, jitterloc = 2, sampleloc = 3;

	DRWShadingGroup *grp = DRW_shgroup_create(data.clay_sh, pass);

	DRW_shgroup_uniform_vec2(grp, "screenres", DRW_viewport_size_get(), 1);
	DRW_shgroup_uniform_buffer(grp, "depthtex", SCENE_DEPTH_DUP, depthloc);
	DRW_shgroup_uniform_texture(grp, "matcaps", data.matcap_array, matcaploc);
	DRW_shgroup_uniform_mat4(grp, "WinMatrix", (float *)data.winmat);
	DRW_shgroup_uniform_vec4(grp, "viewvecs", (float *)data.viewvecs, 3);
	DRW_shgroup_uniform_vec4(grp, "ssao_params", data.ssao_params, 1);
	DRW_shgroup_uniform_vec3(grp, "matcaps_color", (float *)data.matcap_colors, 24);

	DRW_shgroup_uniform_int(grp, "mat_id", material_id, 1);

#ifndef GTAO
	DRW_shgroup_uniform_texture(grp, "ssao_jitter", data.jitter_tx, jitterloc);
	DRW_shgroup_uniform_texture(grp, "ssao_samples", data.sampling_tx, sampleloc);
#endif

	return grp;
}

static int search_mat_to_ubo(CLAY_Storage *storage, float matcap_rot, float matcap_hue, float matcap_sat,
                             float matcap_val, float ssao_distance, float ssao_factor_cavity,
                             float ssao_factor_edge, float ssao_attenuation, int matcap_icon)
{
	/* For now just use a linear search and test all parameters */
	/* TODO make a hash table */
	for (int i = 0; i < storage->ubo_current_id; ++i)
	{
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

	ubo->ssao_params_var[0] = ssao_distance;
	ubo->ssao_params_var[1] = ssao_factor_cavity;
	ubo->ssao_params_var[2] = ssao_factor_edge;
	ubo->ssao_params_var[3] = ssao_attenuation;

	ubo->matcap_id = matcap_to_index(matcap_icon);

	storage->ubo_current_id++;

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

/* Safe way to get override values */
static void override_setting(CollectionEngineSettings *ces, const char *name, void *ret)
{
	CollectionEngineProperty *cep = BKE_collection_engine_property_get(ces, name);

	if (cep == NULL) {
		return;
	}

	if ((cep->flag & COLLECTION_PROP_USE) == 0) {
		return;
	}

	if (cep->type == COLLECTION_PROP_TYPE_INT) {
		CollectionEnginePropertyInt *prop = (CollectionEnginePropertyInt *)cep;
		*((int *)ret) = prop->value;
	}
	else if (cep->type == COLLECTION_PROP_TYPE_FLOAT) {
		CollectionEnginePropertyFloat *prop = (CollectionEnginePropertyFloat *)cep;
		*((float *)ret) = prop->value;
	}
	else if (cep->type == COLLECTION_PROP_TYPE_BOOL) {
		CollectionEnginePropertyBool *prop = (CollectionEnginePropertyBool *)cep;
		*((bool *)ret) = prop->value;
	}
}

static DRWShadingGroup *CLAY_object_shgrp_get(Object *ob, CLAY_StorageList *stl, CLAY_PassList *psl)
{
	DRWShadingGroup **shgrps = stl->storage->shgrps;
	MaterialEngineSettingsClay *settings = DRW_render_settings_get(NULL, RE_engine_id_BLENDER_CLAY);
	CollectionEngineSettings *ces = BKE_object_collection_engine_get(ob, COLLECTION_MODE_NONE, RE_engine_id_BLENDER_CLAY);

	/* Default Settings */
	float matcap_rot = settings->matcap_rot;
	float matcap_hue = settings->matcap_hue;
	float matcap_sat = settings->matcap_sat;
	float matcap_val = settings->matcap_val;
	float ssao_distance = settings->ssao_distance;
	float ssao_factor_cavity = settings->ssao_factor_cavity;
	float ssao_factor_edge = settings->ssao_factor_edge;
	float ssao_attenuation = settings->ssao_attenuation;
	int matcap_icon = settings->matcap_icon;

	/* Override settings */
	if (ces) {
		override_setting(ces, "matcap_rotation", &matcap_rot);
		override_setting(ces, "matcap_hue", &matcap_hue);
		override_setting(ces, "matcap_saturation", &matcap_sat);
		override_setting(ces, "matcap_value", &matcap_val);
		override_setting(ces, "ssao_distance", &ssao_distance);
		override_setting(ces, "ssao_factor_cavity", &ssao_factor_cavity);
		override_setting(ces, "ssao_factor_edge", &ssao_factor_edge);
		override_setting(ces, "ssao_attenuation", &ssao_attenuation);
		override_setting(ces, "matcap_icon", &matcap_icon);
	};

	int id = mat_in_ubo(stl->storage, matcap_rot, matcap_hue, matcap_sat, matcap_val,
	                    ssao_distance, ssao_factor_cavity, ssao_factor_edge,
	                    ssao_attenuation, matcap_icon);

	if (shgrps[id] == NULL) {
		shgrps[id] = CLAY_shgroup_create(psl->clay_pass, &ubo_mat_idxs[id]);
		/* if it's the first shgrp, pass bind the material UBO */
		if (stl->storage->ubo_current_id == 1) {
			DRW_shgroup_uniform_block(shgrps[0], "material_block", stl->mat_ubo, 0);
		}
	}

	return shgrps[id];
}

static DRWShadingGroup *depth_shgrp;
static DRWShadingGroup *depth_shgrp_cull;

static void CLAY_cache_init(void)
{
	CLAY_PassList *psl = DRW_engine_pass_list_get();
	CLAY_StorageList *stl = DRW_engine_storage_list_get();


	/* Depth Pass */
	{
		psl->depth_pass_cull = DRW_pass_create("Depth Pass Cull", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_CULL_BACK);
		psl->depth_pass = DRW_pass_create("Depth Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);

		depth_shgrp_cull = DRW_shgroup_create(data.depth_sh, psl->depth_pass_cull);
		depth_shgrp = DRW_shgroup_create(data.depth_sh, psl->depth_pass);
	}

	/* Clay Pass */
	{
		psl->clay_pass = DRW_pass_create("Clay Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);
		stl->storage->ubo_current_id = 0;
		memset(stl->storage->shgrps, 0, sizeof(DRWShadingGroup *) * MAX_CLAY_MAT);
	}
}

static void CLAY_cache_populate(Object *ob)
{
	const bContext *C = DRW_get_context();
	int mode = CTX_data_mode_enum(C);
	CLAY_StorageList *stl = DRW_engine_storage_list_get();
	CLAY_PassList *psl = DRW_engine_pass_list_get();
	struct Batch *geom;
	DRWShadingGroup *clay_shgrp;
	bool do_occlude_wire = false;
	bool do_cull = false;
	CollectionEngineSettings *ces_mode_ed, *ces_mode_ob;

	if ((ob->base_flag & BASE_VISIBLED) == 0)
		return;

	switch (mode) {
		case CTX_MODE_EDIT_MESH:
		case CTX_MODE_EDIT_CURVE:
		case CTX_MODE_EDIT_SURFACE:
		case CTX_MODE_EDIT_TEXT:
		case CTX_MODE_EDIT_ARMATURE:
		case CTX_MODE_EDIT_METABALL:
		case CTX_MODE_EDIT_LATTICE:
		case CTX_MODE_POSE:
		case CTX_MODE_SCULPT:
		case CTX_MODE_PAINT_WEIGHT:
		case CTX_MODE_PAINT_VERTEX:
		case CTX_MODE_PAINT_TEXTURE:
		case CTX_MODE_PARTICLE:
			ces_mode_ed = BKE_object_collection_engine_get(ob, COLLECTION_MODE_EDIT, "");
			do_occlude_wire = BKE_collection_engine_property_value_get_bool(ces_mode_ed, "show_occlude_wire");
			break;
		case CTX_MODE_OBJECT:
			ces_mode_ob = BKE_object_collection_engine_get(ob, COLLECTION_MODE_OBJECT, "");
			do_cull = BKE_collection_engine_property_value_get_bool(ces_mode_ob, "show_backface_culling");
			break;
	}

	if (do_occlude_wire)
		return;

	switch (ob->type) {
		case OB_MESH:
			geom = DRW_cache_surface_get(ob);

			/* Depth Prepass */
			DRW_shgroup_call_add((do_cull) ? depth_shgrp_cull : depth_shgrp, geom, ob->obmat);

			/* Shading */
			clay_shgrp = CLAY_object_shgrp_get(ob, stl, psl);
			DRW_shgroup_call_add(clay_shgrp, geom, ob->obmat);
			break;
	}
}

static void CLAY_cache_finish(void)
{
	CLAY_StorageList *stl = DRW_engine_storage_list_get();

	DRW_uniformbuffer_update(stl->mat_ubo, &stl->storage->mat_storage);
}

static void CLAY_view_draw(RenderEngine *UNUSED(engine), const bContext *context)
{
	DRW_viewport_init(context);

	/* This function may run for multiple viewports
	 * so get the current viewport buffers */
	CLAY_PassList *psl = DRW_engine_pass_list_get();
	CLAY_FramebufferList *fbl = DRW_engine_framebuffer_list_get();

	CLAY_engine_init();

	/* TODO : tag to refresh by the deps graph */
	/* ideally only refresh when objects are added/removed */
	/* or render properties / materials change */
#ifdef WITH_VIEWPORT_CACHE_TEST
	static bool once = false;
#endif
	if (DRW_viewport_cache_is_dirty()
#ifdef WITH_VIEWPORT_CACHE_TEST
		&& !once
#endif
		) {
#ifdef WITH_VIEWPORT_CACHE_TEST
		once = true;
#endif
		SceneLayer *sl = CTX_data_scene_layer(context);

		CLAY_cache_init();
		DRW_mode_cache_init();

		DEG_OBJECT_ITER(sl, ob);
		{
			CLAY_cache_populate(ob);
			DRW_mode_cache_populate(ob);
		}
		DEG_OBJECT_ITER_END

		CLAY_cache_finish();
		DRW_mode_cache_finish();
	}

	/* Start Drawing */
	DRW_draw_background();

	/* Pass 1 : Depth pre-pass */
	DRW_draw_pass(psl->depth_pass);
	DRW_draw_pass(psl->depth_pass_cull);

	/* Pass 2 : Duplicate depth */
	/* Unless we go for deferred shading we need this to avoid manual depth test and artifacts */
	DRW_framebuffer_blit(fbl->default_fb, fbl->dupli_depth, true);

	/* Pass 3 : Shading */
	DRW_draw_pass(psl->clay_pass);

	/* Pass 4 : Overlays */
	/* At this point all shaded geometry should have been rendered and their depth written */
	DRW_draw_mode_overlays();

	/* Always finish by this */
	DRW_state_reset();
}

static void CLAY_collection_settings_create(RenderEngine *UNUSED(engine), CollectionEngineSettings *ces)
{
	BLI_assert(ces);
	BKE_collection_engine_property_add_int(ces, "matcap_icon", ICON_MATCAP_01);
	BKE_collection_engine_property_add_int(ces, "type", CLAY_MATCAP_NONE);
	BKE_collection_engine_property_add_float(ces, "matcap_rotation", 0.0f);
	BKE_collection_engine_property_add_float(ces, "matcap_hue", 0.5f);
	BKE_collection_engine_property_add_float(ces, "matcap_saturation", 0.5f);
	BKE_collection_engine_property_add_float(ces, "matcap_value", 0.5f);
	BKE_collection_engine_property_add_float(ces, "ssao_distance", 0.2f);
	BKE_collection_engine_property_add_float(ces, "ssao_attenuation", 1.0f);
	BKE_collection_engine_property_add_float(ces, "ssao_factor_cavity", 1.0f);
	BKE_collection_engine_property_add_float(ces, "ssao_factor_edge", 1.0f);
}

void clay_engine_free(void)
{
	/* data.depth_sh Is builtin so it's automaticaly freed */
	if (data.clay_sh) {
		DRW_shader_free(data.clay_sh);
	}

	if (data.matcap_array) {
		DRW_texture_free(data.matcap_array);
	}

	if (data.jitter_tx) {
		DRW_texture_free(data.jitter_tx);
	}

	if (data.sampling_tx) {
		DRW_texture_free(data.sampling_tx);
	}
}

RenderEngineType viewport_clay_type = {
	NULL, NULL,
	CLAY_ENGINE, N_("Clay"), RE_INTERNAL | RE_USE_OGL_PIPELINE,
	NULL, NULL, NULL, NULL, &CLAY_view_draw, NULL, &CLAY_collection_settings_create,
	{NULL, NULL, NULL}
};


#undef CLAY_ENGINE

#endif
