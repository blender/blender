/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
void EEVEE_lightbake_job(void *custom_data, bool *stop, bool *do_update, float *progress);

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
