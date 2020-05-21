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

/** \file
 * \ingroup bke
 */

#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_cachefile.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#ifdef WITH_ALEMBIC
#  include "ABC_alembic.h"
#endif

static void cachefile_handle_free(CacheFile *cache_file);

static void cache_file_init_data(ID *id)
{
  CacheFile *cache_file = (CacheFile *)id;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(cache_file, id));

  cache_file->scale = 1.0f;
}

static void cache_file_copy_data(Main *UNUSED(bmain),
                                 ID *id_dst,
                                 const ID *id_src,
                                 const int UNUSED(flag))
{
  CacheFile *cache_file_dst = (CacheFile *)id_dst;
  const CacheFile *cache_file_src = (const CacheFile *)id_src;

  cache_file_dst->handle = NULL;
  cache_file_dst->handle_readers = NULL;
  BLI_duplicatelist(&cache_file_dst->object_paths, &cache_file_src->object_paths);
}

static void cache_file_free_data(ID *id)
{
  CacheFile *cache_file = (CacheFile *)id;
  cachefile_handle_free(cache_file);
  BLI_freelistN(&cache_file->object_paths);
}

IDTypeInfo IDType_ID_CF = {
    .id_code = ID_CF,
    .id_filter = FILTER_ID_CF,
    .main_listbase_index = INDEX_ID_CF,
    .struct_size = sizeof(CacheFile),
    .name = "CacheFile",
    .name_plural = "cache_files",
    .translation_context = BLT_I18NCONTEXT_ID_CACHEFILE,
    .flags = 0,

    .init_data = cache_file_init_data,
    .copy_data = cache_file_copy_data,
    .free_data = cache_file_free_data,
    .make_local = NULL,
    .foreach_id = NULL,
};

/* TODO: make this per cache file to avoid global locks. */
static SpinLock spin;

void BKE_cachefiles_init(void)
{
  BLI_spin_init(&spin);
}

void BKE_cachefiles_exit(void)
{
  BLI_spin_end(&spin);
}

void BKE_cachefile_reader_open(CacheFile *cache_file,
                               struct CacheReader **reader,
                               Object *object,
                               const char *object_path)
{
#ifdef WITH_ALEMBIC
  BLI_assert(cache_file->id.tag & LIB_TAG_COPIED_ON_WRITE);

  if (cache_file->handle == NULL) {
    return;
  }

  /* Open Alembic cache reader. */
  *reader = CacheReader_open_alembic_object(cache_file->handle, *reader, object, object_path);

  /* Multiple modifiers and constraints can call this function concurrently. */
  BLI_spin_lock(&spin);
  if (*reader) {
    /* Register in set so we can free it when the cache file changes. */
    if (cache_file->handle_readers == NULL) {
      cache_file->handle_readers = BLI_gset_ptr_new("CacheFile.handle_readers");
    }
    BLI_gset_reinsert(cache_file->handle_readers, reader, NULL);
  }
  else if (cache_file->handle_readers) {
    /* Remove in case CacheReader_open_alembic_object free the existing reader. */
    BLI_gset_remove(cache_file->handle_readers, reader, NULL);
  }
  BLI_spin_unlock(&spin);
#else
  UNUSED_VARS(cache_file, reader, object, object_path);
#endif
}

void BKE_cachefile_reader_free(CacheFile *cache_file, struct CacheReader **reader)
{
#ifdef WITH_ALEMBIC
  if (*reader != NULL) {
    if (cache_file) {
      BLI_assert(cache_file->id.tag & LIB_TAG_COPIED_ON_WRITE);
    }

    CacheReader_free(*reader);
    *reader = NULL;

    /* Multiple modifiers and constraints can call this function concurrently. */
    BLI_spin_lock(&spin);
    if (cache_file && cache_file->handle_readers) {
      BLI_gset_remove(cache_file->handle_readers, reader, NULL);
    }
    BLI_spin_unlock(&spin);
  }
#else
  UNUSED_VARS(cache_file, reader);
#endif
}

static void cachefile_handle_free(CacheFile *cache_file)
{
#ifdef WITH_ALEMBIC
  /* Free readers in all modifiers and constraints that use the handle, before
   * we free the handle itself. */
  BLI_spin_lock(&spin);
  if (cache_file->handle_readers) {
    GSetIterator gs_iter;
    GSET_ITER (gs_iter, cache_file->handle_readers) {
      struct CacheReader **reader = BLI_gsetIterator_getKey(&gs_iter);
      if (*reader != NULL) {
        CacheReader_free(*reader);
        *reader = NULL;
      }
    }

    BLI_gset_free(cache_file->handle_readers, NULL);
    cache_file->handle_readers = NULL;
  }
  BLI_spin_unlock(&spin);

  /* Free handle. */
  if (cache_file->handle) {
    ABC_free_handle(cache_file->handle);
    cache_file->handle = NULL;
  }

  cache_file->handle_filepath[0] = '\0';
#else
  UNUSED_VARS(cache_file);
#endif
}

void *BKE_cachefile_add(Main *bmain, const char *name)
{
  CacheFile *cache_file = BKE_libblock_alloc(bmain, ID_CF, name, 0);

  cache_file_init_data(&cache_file->id);

  return cache_file;
}

CacheFile *BKE_cachefile_copy(Main *bmain, const CacheFile *cache_file)
{
  CacheFile *cache_file_copy;
  BKE_id_copy(bmain, &cache_file->id, (ID **)&cache_file_copy);
  return cache_file_copy;
}

void BKE_cachefile_reload(Depsgraph *depsgraph, CacheFile *cache_file)
{
  /* To force reload, free the handle and tag depsgraph to load it again. */
  CacheFile *cache_file_eval = (CacheFile *)DEG_get_evaluated_id(depsgraph, &cache_file->id);
  if (cache_file_eval) {
    cachefile_handle_free(cache_file_eval);
  }

  DEG_id_tag_update(&cache_file->id, ID_RECALC_COPY_ON_WRITE);
}

void BKE_cachefile_eval(Main *bmain, Depsgraph *depsgraph, CacheFile *cache_file)
{
  BLI_assert(cache_file->id.tag & LIB_TAG_COPIED_ON_WRITE);

  /* Compute filepath. */
  char filepath[FILE_MAX];
  if (!BKE_cachefile_filepath_get(bmain, depsgraph, cache_file, filepath)) {
    return;
  }

  /* Test if filepath change or if we can keep the existing handle. */
  if (STREQ(cache_file->handle_filepath, filepath)) {
    return;
  }

  cachefile_handle_free(cache_file);
  BLI_freelistN(&cache_file->object_paths);

#ifdef WITH_ALEMBIC
  cache_file->handle = ABC_create_handle(bmain, filepath, &cache_file->object_paths);
  BLI_strncpy(cache_file->handle_filepath, filepath, FILE_MAX);
#endif

  if (DEG_is_active(depsgraph)) {
    /* Flush object paths back to original datablock for UI. */
    CacheFile *cache_file_orig = (CacheFile *)DEG_get_original_id(&cache_file->id);
    BLI_freelistN(&cache_file_orig->object_paths);
    BLI_duplicatelist(&cache_file_orig->object_paths, &cache_file->object_paths);
  }
}

bool BKE_cachefile_filepath_get(const Main *bmain,
                                const Depsgraph *depsgraph,
                                const CacheFile *cache_file,
                                char r_filepath[FILE_MAX])
{
  BLI_strncpy(r_filepath, cache_file->filepath, FILE_MAX);
  BLI_path_abs(r_filepath, ID_BLEND_PATH(bmain, &cache_file->id));

  int fframe;
  int frame_len;

  if (cache_file->is_sequence && BLI_path_frame_get(r_filepath, &fframe, &frame_len)) {
    Scene *scene = DEG_get_evaluated_scene(depsgraph);
    const float ctime = BKE_scene_frame_get(scene);
    const float fps = (((double)scene->r.frs_sec) / (double)scene->r.frs_sec_base);
    const float frame = BKE_cachefile_time_offset(cache_file, ctime, fps);

    char ext[32];
    BLI_path_frame_strip(r_filepath, ext);
    BLI_path_frame(r_filepath, frame, frame_len);
    BLI_path_extension_ensure(r_filepath, FILE_MAX, ext);

    /* TODO(kevin): store sequence range? */
    return BLI_exists(r_filepath);
  }

  return true;
}

float BKE_cachefile_time_offset(const CacheFile *cache_file, const float time, const float fps)
{
  const float time_offset = cache_file->frame_offset / fps;
  const float frame = (cache_file->override_frame ? cache_file->frame : time);
  return cache_file->is_sequence ? frame : frame / fps - time_offset;
}
