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

/* keep it under MAX_PASSES */
typedef struct EEVEE_PassList {
	struct DRWPass *depth_pass;
	struct DRWPass *depth_pass_cull;
	struct DRWPass *pass;
	struct DRWPass *tonemap;
} EEVEE_PassList;

/* keep it under MAX_BUFFERS */
typedef struct EEVEE_FramebufferList {
	struct GPUFrameBuffer *main; /* HDR */
} EEVEE_FramebufferList;

/* keep it under MAX_TEXTURES */
typedef struct EEVEE_TextureList {
	struct GPUTexture *color; /* R11_G11_B10 */
} EEVEE_TextureList;

/* keep it under MAX_STORAGE */
typedef struct EEVEE_StorageList {
	/* Lights */
	struct EEVEE_LightsInfo *lights_info;       /* Number of lights, ... */
	struct EEVEE_Light *lights_data;            /* Array, Packed lights data info, duplication of what is in the Uniform Buffer in Vram */
	struct Object **lights_ref;                 /* List of all lights in the buffer. */
	struct GPUUniformBuffer *lights_ubo;
} EEVEE_StorageList;

typedef struct EEVEE_LightsInfo {
	int light_count;
} EEVEE_LightsInfo;

typedef struct EEVEE_Data {
	char engine_name[32];
	EEVEE_FramebufferList *fbl;
	EEVEE_TextureList *txl;
	EEVEE_PassList *psl;
	EEVEE_StorageList *stl;
} EEVEE_Data;

/* eevee_lights.c */
void EEVEE_lights_init(EEVEE_StorageList *stl);
void EEVEE_lights_cache_init(EEVEE_StorageList *stl);
void EEVEE_lights_cache_add(EEVEE_StorageList *stl, struct Object *ob);
void EEVEE_lights_cache_finish(EEVEE_StorageList *stl);
void EEVEE_lights_update(EEVEE_StorageList *stl);
