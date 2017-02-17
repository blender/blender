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

//#define WITH_VIEWPORT_CACHE_TEST

struct CollectionEngineSettings;
struct DRWPass;
struct Material;
struct Scene;

void DRW_engines_init(void);
void DRW_engines_free(void);

/* This is here because GPUViewport needs it */
void DRW_pass_free(struct DRWPass *pass);

/* Settings */
void *DRW_material_settings_get(struct Material *ma, const char *engine_name);
void *DRW_render_settings_get(struct Scene *scene, const char *engine_name);

/* Mode engines initialization */
void OBJECT_collection_settings_create(struct CollectionEngineSettings *ces);
void EDIT_collection_settings_create(struct CollectionEngineSettings *ces);

#endif /* __DRW_ENGINE_H__ */
