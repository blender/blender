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

#ifndef __BKE_CACHEFILE_H__
#define __BKE_CACHEFILE_H__

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

void BKE_cachefile_init(struct CacheFile *cache_file);

void BKE_cachefile_free(struct CacheFile *cache_file);

void BKE_cachefile_copy_data(struct Main *bmain,
                             struct CacheFile *cache_file_dst,
                             const struct CacheFile *cache_file_src,
                             const int flag);
struct CacheFile *BKE_cachefile_copy(struct Main *bmain, const struct CacheFile *cache_file);

void BKE_cachefile_make_local(struct Main *bmain,
                              struct CacheFile *cache_file,
                              const bool lib_local);

void BKE_cachefile_reload(struct Depsgraph *depsgraph, struct CacheFile *cache_file);

void BKE_cachefile_eval(struct Main *bmain,
                        struct Depsgraph *depsgraph,
                        struct CacheFile *cache_file);

bool BKE_cachefile_filepath_get(const struct Main *bmain,
                                const struct Depsgraph *depsgrah,
                                const struct CacheFile *cache_file,
                                char r_filename[1024]);

float BKE_cachefile_time_offset(const struct CacheFile *cache_file,
                                const float time,
                                const float fps);

/* Modifiers and constraints open and free readers through these. */
void BKE_cachefile_reader_open(struct CacheFile *cache_file,
                               struct CacheReader **reader,
                               struct Object *object,
                               const char *object_path);
void BKE_cachefile_reader_free(struct CacheFile *cache_file, struct CacheReader **reader);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_CACHEFILE_H__ */
