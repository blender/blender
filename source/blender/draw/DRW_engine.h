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

/** \file DRW_engine.h
 *  \ingroup draw
 */

#ifndef __DRW_ENGINE_H__
#define __DRW_ENGINE_H__

struct CollectionEngineSettings;
struct DRWPass;
struct Material;
struct Scene;
struct DrawEngineType;
struct IDProperty;
struct bContext;
struct Object;

/* Buffer and textures used by the viewport by default */
typedef struct DefaultFramebufferList {
	struct GPUFrameBuffer *default_fb;
} DefaultFramebufferList;

typedef struct DefaultTextureList {
	struct GPUTexture *color;
	struct GPUTexture *depth;
} DefaultTextureList;

void DRW_engines_register(void);
void DRW_engines_free(void);

void DRW_engine_register(struct DrawEngineType *draw_engine_type);

void DRW_draw_view(const struct bContext *C);

void DRW_object_engine_data_free(struct Object *ob);

/* This is here because GPUViewport needs it */
void DRW_pass_free(struct DRWPass *pass);

/* Mode engines initialization */
void OBJECT_collection_settings_create(struct IDProperty *properties);
void EDIT_MESH_collection_settings_create(struct IDProperty *properties);
void EDIT_ARMATURE_collection_settings_create(struct IDProperty *properties);

#endif /* __DRW_ENGINE_H__ */
