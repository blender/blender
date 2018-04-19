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


typedef struct WORKBENCH_StorageList {
	struct WORKBENCH_PrivateData *g_data;
} WORKBENCH_StorageList;

typedef struct WORKBENCH_PassList {
	struct DRWPass *depth_pass;
	struct DRWPass *solid_pass;
} WORKBENCH_PassList;

typedef struct WORKBENCH_Data {
	void *engine_type;
	DRWViewportEmptyList *fbl;
	DRWViewportEmptyList *txl;
	WORKBENCH_PassList *psl;
	WORKBENCH_StorageList *stl;
} WORKBENCH_Data;

typedef struct WORKBENCH_PrivateData {
	DRWShadingGroup *depth_shgrp;

	DRWShadingGroup *shadeless_shgrp;

	struct GHash *material_hash;
	short drawtype_lighting;
} WORKBENCH_PrivateData; /* Transient data */

typedef struct WORKBENCH_MaterialData {
	/* Solid color */
	float color[3];
	
	/* Linked shgroup for drawing */
	DRWShadingGroup *shgrp;
} WORKBENCH_MaterialData;


/* workbench_engine.c */
void workbench_solid_materials_init(void);
void workbench_solid_materials_cache_init(WORKBENCH_Data *vedata);
void workbench_solid_materials_cache_populate(WORKBENCH_Data *vedata, Object *ob);
void workbench_solid_materials_cache_finish(WORKBENCH_Data *vedata);
void workbench_solid_materials_draw_scene(WORKBENCH_Data *vedata);
void workbench_solid_materials_free(void);

/* workbench_materials.c */
void workbench_materials_engine_init(void);
void workbench_materials_engine_free(void);
void workbench_materials_draw_scene_finish(WORKBENCH_Data *vedata);
void workbench_materials_cache_init(WORKBENCH_Data *vedata);
void workbench_materials_solid_cache_populate(WORKBENCH_Data *vedata, Object *ob);

#endif
