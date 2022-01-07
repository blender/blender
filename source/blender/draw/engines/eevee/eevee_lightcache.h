/*
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
 * Copyright 2018, Blender Foundation.
 */

/** \file
 * \ingroup eevee
 */

#pragma once

#include "BLI_sys_types.h" /* for bool */

struct BlendDataReader;
struct BlendWriter;
struct EEVEE_Data;
struct EEVEE_ViewLayerData;
struct LightCache;
struct Scene;
struct SceneEEVEE;
struct ViewLayer;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Light Bake.
 */
struct wmJob *EEVEE_lightbake_job_create(struct wmWindowManager *wm,
                                         struct wmWindow *win,
                                         struct Main *bmain,
                                         struct ViewLayer *view_layer,
                                         struct Scene *scene,
                                         int delay,
                                         int frame);
/**
 * MUST run on the main thread.
 */
void *EEVEE_lightbake_job_data_alloc(struct Main *bmain,
                                     struct ViewLayer *view_layer,
                                     struct Scene *scene,
                                     bool run_as_job,
                                     int frame);
void EEVEE_lightbake_job_data_free(void *custom_data);
void EEVEE_lightbake_update(void *custom_data);
void EEVEE_lightbake_job(void *custom_data, short *stop, short *do_update, float *progress);

/**
 * This is to update the world irradiance and reflection contribution from
 * within the viewport drawing (does not have the overhead of a full light cache rebuild.)
 */
void EEVEE_lightbake_update_world_quick(struct EEVEE_ViewLayerData *sldata,
                                        struct EEVEE_Data *vedata,
                                        const Scene *scene);

/**
 * Light Cache.
 */
struct LightCache *EEVEE_lightcache_create(
    int grid_len, int cube_len, int cube_size, int vis_size, const int irr_size[3]);
void EEVEE_lightcache_free(struct LightCache *lcache);
bool EEVEE_lightcache_load(struct LightCache *lcache);
void EEVEE_lightcache_info_update(struct SceneEEVEE *eevee);

void EEVEE_lightcache_blend_write(struct BlendWriter *writer, struct LightCache *cache);
void EEVEE_lightcache_blend_read_data(struct BlendDataReader *reader, struct LightCache *cache);

#ifdef __cplusplus
}
#endif
