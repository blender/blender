/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 Blender Foundation */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct CacheFile;
struct CacheFileLayer;
struct CacheReader;
struct Depsgraph;
struct Main;
struct Object;
struct Scene;

void BKE_cachefiles_init(void);
void BKE_cachefiles_exit(void);

void *BKE_cachefile_add(struct Main *bmain, const char *name);

void BKE_cachefile_reload(struct Depsgraph *depsgraph, struct CacheFile *cache_file);

void BKE_cachefile_eval(struct Main *bmain,
                        struct Depsgraph *depsgraph,
                        struct CacheFile *cache_file);

bool BKE_cachefile_filepath_get(const struct Main *bmain,
                                const struct Depsgraph *depsgrah,
                                const struct CacheFile *cache_file,
                                char r_filepath[1024]);

double BKE_cachefile_time_offset(const struct CacheFile *cache_file, double time, double fps);

/* Modifiers and constraints open and free readers through these. */
void BKE_cachefile_reader_open(struct CacheFile *cache_file,
                               struct CacheReader **reader,
                               struct Object *object,
                               const char *object_path);
void BKE_cachefile_reader_free(struct CacheFile *cache_file, struct CacheReader **reader);

/**
 * Determine whether the #CacheFile should use a render engine procedural. If so, data is not read
 * from the file and bounding boxes are used to represent the objects in the Scene.
 * Render engines will receive the bounding box as a placeholder but can instead
 * load the data directly if they support it.
 */
bool BKE_cache_file_uses_render_procedural(const struct CacheFile *cache_file,
                                           struct Scene *scene);

/**
 * Add a layer to the cache_file. Return NULL if the `filepath` is already that of an existing
 * layer or if the number of layers exceeds the maximum allowed layer count.
 */
struct CacheFileLayer *BKE_cachefile_add_layer(struct CacheFile *cache_file,
                                               const char filepath[1024]);

struct CacheFileLayer *BKE_cachefile_get_active_layer(struct CacheFile *cache_file);

void BKE_cachefile_remove_layer(struct CacheFile *cache_file, struct CacheFileLayer *layer);

#ifdef __cplusplus
}
#endif
