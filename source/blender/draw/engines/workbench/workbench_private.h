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

/** \file workbench_private.h
 *  \ingroup draw_engine
 */

#ifndef __WORKBENCH_PRIVATE_H__
#define __WORKBENCH_PRIVATE_H__


#include "DRW_render.h"
#include "DNA_view3d_types.h"

#define WORKBENCH_ENGINE "BLENDER_WORKBENCH"
#define M_GOLDEN_RATION_CONJUGATE 0.618033988749895
#define WORKBENCH_ENCODE_NORMALS

typedef struct WORKBENCH_FramebufferList {
	struct GPUFrameBuffer *prepass_fb;
	struct GPUFrameBuffer *composite_fb;
} WORKBENCH_FramebufferList;

typedef struct WORKBENCH_StorageList {
	struct WORKBENCH_PrivateData *g_data;
} WORKBENCH_StorageList;

typedef struct WORKBENCH_PassList {
	struct DRWPass *prepass_pass;
	struct DRWPass *shadow_pass;
	struct DRWPass *composite_pass;
	struct DRWPass *composite_shadow_pass;
	struct DRWPass *composite_light_pass;
} WORKBENCH_PassList;

typedef struct WORKBENCH_Data {
	void *engine_type;
	WORKBENCH_FramebufferList *fbl;
	DRWViewportEmptyList *txl;
	WORKBENCH_PassList *psl;
	WORKBENCH_StorageList *stl;
} WORKBENCH_Data;

typedef struct WORKBENCH_UBO_World {
	float diffuse_light_x_pos[4];
	float diffuse_light_x_neg[4];
	float diffuse_light_y_pos[4];
	float diffuse_light_y_neg[4];
	float diffuse_light_z_pos[4];
	float diffuse_light_z_neg[4];
	float background_color_low[4];
	float background_color_high[4];
} WORKBENCH_UBO_World;
BLI_STATIC_ASSERT_ALIGN(WORKBENCH_UBO_World, 16)

typedef struct WORKBENCH_PrivateData {
	struct GHash *material_hash;
	struct GPUShader *prepass_sh;
	struct GPUShader *composite_sh;
	View3DShading shading;
	struct GPUUniformBuffer *world_ubo;
	struct DRWShadingGroup *shadow_shgrp;
	WORKBENCH_UBO_World world_data;
} WORKBENCH_PrivateData; /* Transient data */

typedef struct WORKBENCH_MaterialData {
	/* Solid color */
	float color[3];
	int object_id;

	/* Linked shgroup for drawing */
	DRWShadingGroup *shgrp;
} WORKBENCH_MaterialData;

typedef struct WORKBENCH_ObjectData {
	struct ObjectEngineData *next, *prev;
	struct DrawEngineType *engine_type;
	/* Only nested data, NOT the engine data itself. */
	ObjectEngineDataFreeCb free;
	/* Accumulated recalc flags, which corresponds to ID->recalc flags. */
	int recalc;

	int object_id;
} WORKBENCH_ObjectData;

/* workbench_engine.c */
void workbench_solid_materials_init(WORKBENCH_Data *vedata);
void workbench_solid_materials_cache_init(WORKBENCH_Data *vedata);
void workbench_solid_materials_cache_populate(WORKBENCH_Data *vedata, Object *ob);
void workbench_solid_materials_cache_finish(WORKBENCH_Data *vedata);
void workbench_solid_materials_draw_scene(WORKBENCH_Data *vedata);
void workbench_solid_materials_free(void);

/* workbench_materials.c */
void workbench_materials_engine_init(WORKBENCH_Data *vedata);
void workbench_materials_engine_free(void);
void workbench_materials_draw_background(WORKBENCH_Data *vedata);
void workbench_materials_draw_scene(WORKBENCH_Data *vedata);
void workbench_materials_cache_init(WORKBENCH_Data *vedata);
void workbench_materials_solid_cache_populate(WORKBENCH_Data *vedata, Object *ob);
void workbench_materials_cache_finish(WORKBENCH_Data *vedata);

/* workbench_studiolight.c */
void studiolight_update_world(int studio_light, WORKBENCH_UBO_World* wd);

#endif
