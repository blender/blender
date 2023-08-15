/* SPDX-FileCopyrightText: Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief General operations for probes.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct LightProbe;
struct Main;
struct BlendWriter;
struct BlendDataReader;
struct LightProbeObjectCache;
struct LightProbeGridCacheFrame;
struct Object;

void BKE_lightprobe_type_set(struct LightProbe *probe, short lightprobe_type);
void *BKE_lightprobe_add(struct Main *bmain, const char *name);

void BKE_lightprobe_cache_blend_write(struct BlendWriter *writer,
                                      struct LightProbeObjectCache *cache);

void BKE_lightprobe_cache_blend_read(struct BlendDataReader *reader,
                                     struct LightProbeObjectCache *cache);

/**
 * Create a single empty irradiance grid cache.
 */
struct LightProbeGridCacheFrame *BKE_lightprobe_grid_cache_frame_create(void);

/**
 * Create a copy of a cache frame.
 * This does not include the in-progress baking data.
 */
LightProbeGridCacheFrame *BKE_lightprobe_grid_cache_frame_copy(LightProbeGridCacheFrame *src);

/**
 * Free a single grid cache.
 */
void BKE_lightprobe_grid_cache_frame_free(struct LightProbeGridCacheFrame *cache);

/**
 * Create the grid cache list depending on the lightprobe baking settings.
 * The list is left empty to be filled by the baking process.
 */
void BKE_lightprobe_cache_create(struct Object *object);

/**
 * Create a copy of a whole cache.
 * This does not include the in-progress baking data.
 */
LightProbeObjectCache *BKE_lightprobe_cache_copy(LightProbeObjectCache *src_cache);

/**
 * Free all irradiance grids allocated for the given object.
 */
void BKE_lightprobe_cache_free(struct Object *object);

/**
 * Return the number of sample stored inside an irradiance cache.
 * This depends on the light cache type.
 */
int64_t BKE_lightprobe_grid_cache_frame_sample_count(const struct LightProbeGridCacheFrame *cache);

#ifdef __cplusplus
}
#endif
