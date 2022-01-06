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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct CacheFile;
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
                                char r_filename[1024]);

float BKE_cachefile_time_offset(const struct CacheFile *cache_file, float time, float fps);

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
                                           struct Scene *scene,
                                           int dag_eval_mode);

#ifdef __cplusplus
}
#endif
