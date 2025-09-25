/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

struct CacheFile;
struct CacheFileLayer;
struct CacheReader;
struct Depsgraph;
struct Main;
struct Object;
struct Scene;

void *BKE_cachefile_add(Main *bmain, const char *name);

void BKE_cachefile_reload(Depsgraph *depsgraph, CacheFile *cache_file);

void BKE_cachefile_eval(Main *bmain, Depsgraph *depsgraph, CacheFile *cache_file);

bool BKE_cachefile_filepath_get(const Main *bmain,
                                const Depsgraph *depsgraph,
                                const CacheFile *cache_file,
                                char r_filepath[1024]);

double BKE_cachefile_time_offset(const CacheFile *cache_file, double time, double fps);
double BKE_cachefile_frame_offset(const CacheFile *cache_file, double time);

/* Modifiers and constraints open and free readers through these. */
void BKE_cachefile_reader_open(CacheFile *cache_file,
                               CacheReader **reader,
                               Object *object,
                               const char *object_path);
void BKE_cachefile_reader_free(CacheFile *cache_file, CacheReader **reader);

/**
 * Add a layer to the cache_file. Return NULL if the `filepath` is already that of an existing
 * layer or if the number of layers exceeds the maximum allowed layer count.
 */
CacheFileLayer *BKE_cachefile_add_layer(CacheFile *cache_file, const char filepath[1024]);

CacheFileLayer *BKE_cachefile_get_active_layer(CacheFile *cache_file);

void BKE_cachefile_remove_layer(CacheFile *cache_file, CacheFileLayer *layer);
