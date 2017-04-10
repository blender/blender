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

struct Object;

/* Minimum UBO is 16384 bytes */
#define MAX_LIGHT 128 /* TODO : find size by dividing UBO max size by light data size */
#define MAX_SHADOW_CUBE 42 /* TODO : Make this depends on GL_MAX_ARRAY_TEXTURE_LAYERS */
#define MAX_SHADOW_MAP 64
#define MAX_SHADOW_CASCADE 8
#define MAX_CASCADE_NUM 4

/* keep it under MAX_PASSES */
typedef struct EEVEE_PassList {
	struct DRWPass *shadow_pass;
	struct DRWPass *depth_pass;
	struct DRWPass *depth_pass_cull;
	struct DRWPass *pass;
	struct DRWPass *tonemap;
} EEVEE_PassList;

/* keep it under MAX_BUFFERS */
typedef struct EEVEE_FramebufferList {
	struct GPUFrameBuffer *main; /* HDR */
	struct GPUFrameBuffer *shadow_cube_fb;
	struct GPUFrameBuffer *shadow_map_fb;
	struct GPUFrameBuffer *shadow_cascade_fb;
} EEVEE_FramebufferList;

/* keep it under MAX_TEXTURES */
typedef struct EEVEE_TextureList {
	struct GPUTexture *color; /* R11_G11_B10 */
	struct GPUTexture *shadow_depth_cube_pool;
	struct GPUTexture *shadow_depth_map_pool;
	struct GPUTexture *shadow_depth_cascade_pool;
} EEVEE_TextureList;

/* keep it under MAX_STORAGE */
typedef struct EEVEE_StorageList {
	/* Lamps */
	/* XXX this should be per-scenelayer and not per_viewport */
	struct EEVEE_LampsInfo *lamps;
	struct GPUUniformBuffer *light_ubo;
	struct GPUUniformBuffer *shadow_ubo;

	struct g_data *g_data;
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
	float bias, count, pad[2];
	float near[MAX_CASCADE_NUM];
	float far[MAX_CASCADE_NUM];
} EEVEE_ShadowCascade;

/* ************ LIGHT DATA ************* */
typedef struct EEVEE_LampsInfo {
	/* For rendering shadows */
	float shadowmat[4][4];
	int layer;

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
	struct EEVEE_ShadowCube    shadow_cube_data[MAX_SHADOW_CUBE];
	struct EEVEE_ShadowMap     shadow_map_data[MAX_SHADOW_MAP];
	struct EEVEE_ShadowCascade shadow_cascade_data[MAX_SHADOW_CASCADE];
} EEVEE_LampsInfo;
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

typedef struct g_data{
	struct DRWShadingGroup *default_lit_grp;
	struct DRWShadingGroup *shadow_shgrp;
	struct DRWShadingGroup *depth_shgrp;
	struct DRWShadingGroup *depth_shgrp_select;
	struct DRWShadingGroup *depth_shgrp_active;
	struct DRWShadingGroup *depth_shgrp_cull;
	struct DRWShadingGroup *depth_shgrp_cull_select;
	struct DRWShadingGroup *depth_shgrp_cull_active;

	struct ListBase lamps; /* Lamps gathered during cache iteration */
} g_data; /* Transient data */

/* eevee_lights.c */
void EEVEE_lights_init(EEVEE_StorageList *stl);
void EEVEE_lights_cache_init(EEVEE_StorageList *stl);
void EEVEE_lights_cache_add(EEVEE_StorageList *stl, struct Object *ob);
void EEVEE_lights_cache_finish(EEVEE_StorageList *stl, EEVEE_TextureList *txl, EEVEE_FramebufferList *fbl);
void EEVEE_lights_update(EEVEE_StorageList *stl);
void EEVEE_draw_shadows(EEVEE_Data *vedata);