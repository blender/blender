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

/** \file eevee_private.h
 *  \ingroup DNA
 */

#ifndef __EEVEE_PRIVATE_H__
#define __EEVEE_PRIVATE_H__

struct Object;

/* Minimum UBO is 16384 bytes */
#define MAX_LIGHT 128 /* TODO : find size by dividing UBO max size by light data size */
#define MAX_SHADOW_CUBE 42 /* TODO : Make this depends on GL_MAX_ARRAY_TEXTURE_LAYERS */
#define MAX_SHADOW_MAP 64
#define MAX_SHADOW_CASCADE 8
#define MAX_CASCADE_NUM 4
#define MAX_BLOOM_STEP 16

typedef struct EEVEE_PassList {
	/* Shadows */
	struct DRWPass *shadow_pass;
	struct DRWPass *shadow_cube_pass;
	struct DRWPass *shadow_cascade_pass;

	/* Probes */
	struct DRWPass *probe_background;
	struct DRWPass *probe_prefilter;
	struct DRWPass *probe_sh_compute;

	/* Effects */
	struct DRWPass *motion_blur;
	struct DRWPass *bloom_blit;
	struct DRWPass *bloom_downsample_first;
	struct DRWPass *bloom_downsample;
	struct DRWPass *bloom_upsample;
	struct DRWPass *bloom_resolve;
	struct DRWPass *tonemap;

	struct DRWPass *depth_pass;
	struct DRWPass *depth_pass_cull;
	struct DRWPass *default_pass;
	struct DRWPass *material_pass;
	struct DRWPass *background_pass;
} EEVEE_PassList;

typedef struct EEVEE_FramebufferList {
	/* Shadows */
	struct GPUFrameBuffer *shadow_cube_fb;
	struct GPUFrameBuffer *shadow_map_fb;
	struct GPUFrameBuffer *shadow_cascade_fb;
	/* Probes */
	struct GPUFrameBuffer *probe_fb;
	struct GPUFrameBuffer *probe_filter_fb;
	struct GPUFrameBuffer *probe_sh_fb;
	/* Effects */
	struct GPUFrameBuffer *effect_fb; /* HDR */
	struct GPUFrameBuffer *bloom_blit_fb; /* HDR */
	struct GPUFrameBuffer *bloom_down_fb[MAX_BLOOM_STEP]; /* HDR */
	struct GPUFrameBuffer *bloom_accum_fb[MAX_BLOOM_STEP-1]; /* HDR */

	struct GPUFrameBuffer *main; /* HDR */
} EEVEE_FramebufferList;

typedef struct EEVEE_TextureList {
	/* Shadows */
	struct GPUTexture *shadow_depth_cube_pool;
	struct GPUTexture *shadow_depth_map_pool;
	struct GPUTexture *shadow_depth_cascade_pool;
	/* Probes */
	struct GPUTexture *probe_rt; /* R16_G16_B16 */
	struct GPUTexture *probe_depth_rt;
	struct GPUTexture *probe_pool; /* R11_G11_B10 */
	struct GPUTexture *probe_sh; /* R16_G16_B16 */
	/* Effects */
	struct GPUTexture *color_post; /* R16_G16_B16 */
	struct GPUTexture *bloom_blit; /* R16_G16_B16 */
	struct GPUTexture *bloom_downsample[MAX_BLOOM_STEP]; /* R16_G16_B16 */
	struct GPUTexture *bloom_upsample[MAX_BLOOM_STEP-1]; /* R16_G16_B16 */

	struct GPUTexture *color; /* R16_G16_B16 */
} EEVEE_TextureList;

typedef struct EEVEE_StorageList {
	/* Lamps */
	/* XXX this should be per-scenelayer and not per_viewport */
	struct EEVEE_LampsInfo *lamps;
	struct GPUUniformBuffer *light_ubo;
	struct GPUUniformBuffer *shadow_ubo;
	struct GPUUniformBuffer *shadow_render_ubo;

	/* Probes */
	struct EEVEE_ProbesInfo *probes;
	struct GPUUniformBuffer *probe_ubo;

	/* Effects */
	struct EEVEE_EffectsInfo *effects;

	struct EEVEE_PrivateData *g_data;
} EEVEE_StorageList;

/* ************ LIGHT UBO ************* */
typedef struct EEVEE_Light {
	float position[3], dist;
	float color[3], spec;
	float spotsize, spotblend, radius, shadowid;
	float rightvec[3], sizex;
	float upvec[3], sizey;
	float forwardvec[3], lamptype;
} EEVEE_Light;

typedef struct EEVEE_ShadowCube {
	float near, far, bias, pad;
} EEVEE_ShadowCube;

typedef struct EEVEE_ShadowMap {
	float shadowmat[4][4]; /* World->Lamp->NDC->Tex : used for sampling the shadow map. */
	float near, far, bias, pad;
} EEVEE_ShadowMap;

typedef struct EEVEE_ShadowCascade {
	float shadowmat[MAX_CASCADE_NUM][4][4]; /* World->Lamp->NDC->Tex : used for sampling the shadow map. */
	float split[4];
	float bias[4];
} EEVEE_ShadowCascade;

typedef struct EEVEE_ShadowRender {
	float shadowmat[6][4][4]; /* World->Lamp->NDC : used to render the shadow map. 6 frustrum for cubemap shadow */
	int layer;
} EEVEE_ShadowRender;

/* ************ LIGHT DATA ************* */
typedef struct EEVEE_LampsInfo {
	int num_light, cache_num_light;
	int num_cube, cache_num_cube;
	int num_map, cache_num_map;
	int num_cascade, cache_num_cascade;
	/* List of lights in the scene. */
	struct Object *light_ref[MAX_LIGHT];
	struct Object *shadow_cube_ref[MAX_SHADOW_CUBE];
	struct Object *shadow_map_ref[MAX_SHADOW_MAP];
	struct Object *shadow_cascade_ref[MAX_SHADOW_CASCADE];
	/* UBO Storage : data used by UBO */
	struct EEVEE_Light         light_data[MAX_LIGHT];
	struct EEVEE_ShadowRender  shadow_render_data;
	struct EEVEE_ShadowCube    shadow_cube_data[MAX_SHADOW_CUBE];
	struct EEVEE_ShadowMap     shadow_map_data[MAX_SHADOW_MAP];
	struct EEVEE_ShadowCascade shadow_cascade_data[MAX_SHADOW_CASCADE];
} EEVEE_LampsInfo;

/* ************ PROBE DATA ************* */
typedef struct EEVEE_ProbesInfo {
	/* For rendering probes */
	float probemat[6][4][4];
	int layer;
	float samples_ct;
	float invsamples_ct;
	float roughness;
	float lodfactor;
	float lodmax;
	int shres;
	int shnbr;
	float shcoefs[9][3]; /* Temp */
	struct GPUTexture *backgroundtex;
} EEVEE_ProbesInfo;

/* ************ EFFECTS DATA ************* */
typedef struct EEVEE_EffectsInfo {
	int enabled_effects;

	/* Motion Blur */
	float current_ndc_to_world[4][4];
	float past_world_to_ndc[4][4];
	float tmp_mat[4][4];
	float blur_amount;

	/* Bloom */
	int bloom_iteration_ct;
	float source_texel_size[2];
	float blit_texel_size[2];
	float downsamp_texel_size[MAX_BLOOM_STEP][2];
	float bloom_intensity;
	float bloom_sample_scale;
	float bloom_curve_threshold[4];
	float unf_source_texel_size[2];
	struct GPUTexture *unf_source_buffer; /* pointer copy */
	struct GPUTexture *unf_base_buffer; /* pointer copy */

	/* Not alloced, just a copy of a *GPUtexture in EEVEE_TextureList. */
	struct GPUTexture *source_buffer;       /* latest updated texture */
	struct GPUFrameBuffer *target_buffer;   /* next target to render to */
} EEVEE_EffectsInfo;

enum {
	EFFECT_MOTION_BLUR         = (1 << 0),
	EFFECT_BLOOM               = (1 << 1),
};

/* *********************************** */

typedef struct EEVEE_Data {
	void *engine_type;
	EEVEE_FramebufferList *fbl;
	EEVEE_TextureList *txl;
	EEVEE_PassList *psl;
	EEVEE_StorageList *stl;
} EEVEE_Data;

/* Keep it sync with MAX_LAMP_DATA */
typedef struct EEVEE_LampEngineData {
	void *sto;
	void *pad;
} EEVEE_LampEngineData;

typedef struct EEVEE_PrivateData {
	struct DRWShadingGroup *default_lit_grp;
	struct DRWShadingGroup *shadow_shgrp;
	struct DRWShadingGroup *depth_shgrp;
	struct DRWShadingGroup *depth_shgrp_cull;

	struct ListBase lamps; /* Lamps gathered during cache iteration */
} EEVEE_PrivateData; /* Transient data */

/* eevee_lights.c */
void EEVEE_lights_init(EEVEE_StorageList *stl);
void EEVEE_lights_cache_init(EEVEE_StorageList *stl);
void EEVEE_lights_cache_add(EEVEE_StorageList *stl, struct Object *ob);
void EEVEE_lights_cache_finish(EEVEE_StorageList *stl, EEVEE_TextureList *txl, EEVEE_FramebufferList *fbl);
void EEVEE_lights_update(EEVEE_StorageList *stl);
void EEVEE_draw_shadows(EEVEE_Data *vedata);

/* eevee_probes.c */
void EEVEE_probes_init(EEVEE_Data *vedata);
void EEVEE_probes_cache_init(EEVEE_Data *vedata);
void EEVEE_probes_cache_add(EEVEE_Data *vedata, Object *ob);
void EEVEE_probes_cache_finish(EEVEE_Data *vedata);
void EEVEE_probes_update(EEVEE_Data *vedata);
void EEVEE_refresh_probe(EEVEE_Data *vedata);

/* eevee_effects.c */
void EEVEE_effects_init(EEVEE_Data *vedata);
void EEVEE_effects_cache_init(EEVEE_Data *vedata);
void EEVEE_draw_effects(EEVEE_Data *vedata);
void EEVEE_effects_free(void);

/* Shadow Matrix */
static const float texcomat[4][4] = { /* From NDC to TexCo */
	{0.5, 0.0, 0.0, 0.0},
	{0.0, 0.5, 0.0, 0.0},
	{0.0, 0.0, 0.5, 0.0},
	{0.5, 0.5, 0.5, 1.0}
};

/* Cubemap Matrices */
static const float cubefacemat[6][4][4] = {
	/* Pos X */
	{{0.0, 0.0, -1.0, 0.0},
	 {0.0, -1.0, 0.0, 0.0},
	 {-1.0, 0.0, 0.0, 0.0},
	 {0.0, 0.0, 0.0, 1.0}},
	/* Neg X */
	{{0.0, 0.0, 1.0, 0.0},
	 {0.0, -1.0, 0.0, 0.0},
	 {1.0, 0.0, 0.0, 0.0},
	 {0.0, 0.0, 0.0, 1.0}},
	/* Pos Y */
	{{1.0, 0.0, 0.0, 0.0},
	 {0.0, 0.0, 1.0, 0.0},
	 {0.0, -1.0, 0.0, 0.0},
	 {0.0, 0.0, 0.0, 1.0}},
	/* Neg Y */
	{{1.0, 0.0, 0.0, 0.0},
	 {0.0, 0.0, -1.0, 0.0},
	 {0.0, 1.0, 0.0, 0.0},
	 {0.0, 0.0, 0.0, 1.0}},
	/* Pos Z */
	{{1.0, 0.0, 0.0, 0.0},
	 {0.0, -1.0, 0.0, 0.0},
	 {0.0, 0.0, -1.0, 0.0},
	 {0.0, 0.0, 0.0, 1.0}},
	/* Neg Z */
	{{-1.0, 0.0, 0.0, 0.0},
	 {0.0, -1.0, 0.0, 0.0},
	 {0.0, 0.0, 1.0, 0.0},
	 {0.0, 0.0, 0.0, 1.0}},
};

#endif /* __EEVEE_PRIVATE_H__ */
