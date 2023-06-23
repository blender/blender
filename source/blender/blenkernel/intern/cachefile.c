/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "BKE_anim_data.h"
#include "BKE_bpath.h"
#include "BKE_cachefile.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "RE_engine.h"

#include "BLO_read_write.h"

#include "MEM_guardedalloc.h"

#ifdef WITH_ALEMBIC
#  include "ABC_alembic.h"
#endif

#ifdef WITH_USD
#  include "usd.h"
#endif

static void cachefile_handle_free(CacheFile *cache_file);

static void cache_file_init_data(ID *id)
{
  CacheFile *cache_file = (CacheFile *)id;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(cache_file, id));

  cache_file->scale = 1.0f;
  cache_file->velocity_unit = CACHEFILE_VELOCITY_UNIT_SECOND;
  STRNCPY(cache_file->velocity_name, ".velocities");
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
  BLI_duplicatelist(&cache_file_dst->layers, &cache_file_src->layers);
}

static void cache_file_free_data(ID *id)
{
  CacheFile *cache_file = (CacheFile *)id;
  cachefile_handle_free(cache_file);
  BLI_freelistN(&cache_file->object_paths);
  BLI_freelistN(&cache_file->layers);
}

static void cache_file_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  CacheFile *cache_file = (CacheFile *)id;
  BKE_bpath_foreach_path_fixed_process(
      bpath_data, cache_file->filepath, sizeof(cache_file->filepath));
}

static void cache_file_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  CacheFile *cache_file = (CacheFile *)id;

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  BLI_listbase_clear(&cache_file->object_paths);
  cache_file->handle = NULL;
  memset(cache_file->handle_filepath, 0, sizeof(cache_file->handle_filepath));
  cache_file->handle_readers = NULL;

  BLO_write_id_struct(writer, CacheFile, id_address, &cache_file->id);
  BKE_id_blend_write(writer, &cache_file->id);

  if (cache_file->adt) {
    BKE_animdata_blend_write(writer, cache_file->adt);
  }

  /* write layers */
  LISTBASE_FOREACH (CacheFileLayer *, layer, &cache_file->layers) {
    BLO_write_struct(writer, CacheFileLayer, layer);
  }
}

static void cache_file_blend_read_data(BlendDataReader *reader, ID *id)
{
  CacheFile *cache_file = (CacheFile *)id;
  BLI_listbase_clear(&cache_file->object_paths);
  cache_file->handle = NULL;
  cache_file->handle_filepath[0] = '\0';
  cache_file->handle_readers = NULL;

  /* relink animdata */
  BLO_read_data_address(reader, &cache_file->adt);
  BKE_animdata_blend_read_data(reader, cache_file->adt);

  /* relink layers */
  BLO_read_list(reader, &cache_file->layers);
}

IDTypeInfo IDType_ID_CF = {
    .id_code = ID_CF,
    .id_filter = FILTER_ID_CF,
    .main_listbase_index = INDEX_ID_CF,
    .struct_size = sizeof(CacheFile),
    .name = "CacheFile",
    .name_plural = "cache_files",
    .translation_context = BLT_I18NCONTEXT_ID_CACHEFILE,
    .flags = IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    .asset_type_info = NULL,

    .init_data = cache_file_init_data,
    .copy_data = cache_file_copy_data,
    .free_data = cache_file_free_data,
    .make_local = NULL,
    .foreach_id = NULL,
    .foreach_cache = NULL,
    .foreach_path = cache_file_foreach_path,
    .owner_pointer_get = NULL,

    .blend_write = cache_file_blend_write,
    .blend_read_data = cache_file_blend_read_data,
    .blend_read_lib = NULL,
    .blend_read_expand = NULL,

    .blend_read_undo_preserve = NULL,

    .lib_override_apply_post = NULL,
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
#if defined(WITH_ALEMBIC) || defined(WITH_USD)

  BLI_assert(cache_file->id.tag & LIB_TAG_COPIED_ON_WRITE);

  if (cache_file->handle == NULL) {
    return;
  }

  switch (cache_file->type) {
    case CACHEFILE_TYPE_ALEMBIC:
#  ifdef WITH_ALEMBIC
      /* Open Alembic cache reader. */
      *reader = CacheReader_open_alembic_object(
          cache_file->handle, *reader, object, object_path, cache_file->is_sequence);
#  endif
      break;
    case CACHEFILE_TYPE_USD:
#  ifdef WITH_USD
      /* Open USD cache reader. */
      *reader = CacheReader_open_usd_object(cache_file->handle, *reader, object, object_path);
#  endif
      break;
    case CACHE_FILE_TYPE_INVALID:
      break;
  }

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
#if defined(WITH_ALEMBIC) || defined(WITH_USD)
  /* Multiple modifiers and constraints can call this function concurrently, and
   * cachefile_handle_free() can also be called at the same time. */
  BLI_spin_lock(&spin);
  if (*reader != NULL) {
    if (cache_file) {
      BLI_assert(cache_file->id.tag & LIB_TAG_COPIED_ON_WRITE);

      switch (cache_file->type) {
        case CACHEFILE_TYPE_ALEMBIC:
#  ifdef WITH_ALEMBIC
          ABC_CacheReader_free(*reader);
#  endif
          break;
        case CACHEFILE_TYPE_USD:
#  ifdef WITH_USD
          USD_CacheReader_free(*reader);
#  endif
          break;
        case CACHE_FILE_TYPE_INVALID:
          break;
      }
    }

    *reader = NULL;

    if (cache_file && cache_file->handle_readers) {
      BLI_gset_remove(cache_file->handle_readers, reader, NULL);
    }
  }
  BLI_spin_unlock(&spin);
#else
  UNUSED_VARS(cache_file, reader);
#endif
}

static void cachefile_handle_free(CacheFile *cache_file)
{
#if defined(WITH_ALEMBIC) || defined(WITH_USD)

  /* Free readers in all modifiers and constraints that use the handle, before
   * we free the handle itself. */
  BLI_spin_lock(&spin);
  if (cache_file->handle_readers) {
    GSetIterator gs_iter;
    GSET_ITER (gs_iter, cache_file->handle_readers) {
      struct CacheReader **reader = BLI_gsetIterator_getKey(&gs_iter);
      if (*reader != NULL) {
        switch (cache_file->type) {
          case CACHEFILE_TYPE_ALEMBIC:
#  ifdef WITH_ALEMBIC
            ABC_CacheReader_free(*reader);
#  endif
            break;
          case CACHEFILE_TYPE_USD:
#  ifdef WITH_USD
            USD_CacheReader_free(*reader);
#  endif
            break;
          case CACHE_FILE_TYPE_INVALID:
            break;
        }

        *reader = NULL;
      }
    }

    BLI_gset_free(cache_file->handle_readers, NULL);
    cache_file->handle_readers = NULL;
  }
  BLI_spin_unlock(&spin);

  /* Free handle. */
  if (cache_file->handle) {

    switch (cache_file->type) {
      case CACHEFILE_TYPE_ALEMBIC:
#  ifdef WITH_ALEMBIC
        ABC_free_handle(cache_file->handle);
#  endif
        break;
      case CACHEFILE_TYPE_USD:
#  ifdef WITH_USD
        USD_free_handle(cache_file->handle);
#  endif
        break;
      case CACHE_FILE_TYPE_INVALID:
        break;
    }

    cache_file->handle = NULL;
  }

  cache_file->handle_filepath[0] = '\0';
#else
  UNUSED_VARS(cache_file);
#endif
}

void *BKE_cachefile_add(Main *bmain, const char *name)
{
  CacheFile *cache_file = BKE_id_new(bmain, ID_CF, name);

  return cache_file;
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
  if (BLI_path_extension_check_glob(filepath, "*abc")) {
    cache_file->type = CACHEFILE_TYPE_ALEMBIC;
    cache_file->handle = ABC_create_handle(
        bmain, filepath, cache_file->layers.first, &cache_file->object_paths);
    STRNCPY(cache_file->handle_filepath, filepath);
  }
#endif
#ifdef WITH_USD
  if (BLI_path_extension_check_glob(filepath, "*.usd;*.usda;*.usdc;*.usdz")) {
    cache_file->type = CACHEFILE_TYPE_USD;
    cache_file->handle = USD_create_handle(bmain, filepath, &cache_file->object_paths);
    BLI_strncpy(cache_file->handle_filepath, filepath, FILE_MAX);
  }
#endif

  if (DEG_is_active(depsgraph)) {
    /* Flush object paths back to original data-block for UI. */
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
    const float ctime = BKE_scene_ctime_get(scene);
    const double fps = (((double)scene->r.frs_sec) / (double)scene->r.frs_sec_base);
    const int frame = (int)BKE_cachefile_time_offset(cache_file, (double)ctime, fps);

    char ext[32];
    BLI_path_frame_strip(r_filepath, ext, sizeof(ext));
    BLI_path_frame(r_filepath, FILE_MAX, frame, frame_len);
    BLI_path_extension_ensure(r_filepath, FILE_MAX, ext);

    /* TODO(kevin): store sequence range? */
    return BLI_exists(r_filepath);
  }

  return true;
}

double BKE_cachefile_time_offset(const CacheFile *cache_file, const double time, const double fps)
{
  const double time_offset = (double)cache_file->frame_offset / fps;
  const double frame = (cache_file->override_frame ? (double)cache_file->frame : time);
  return cache_file->is_sequence ? frame : frame / fps - time_offset;
}

bool BKE_cache_file_uses_render_procedural(const CacheFile *cache_file, Scene *scene)
{
  RenderEngineType *render_engine_type = RE_engines_find(scene->r.engine);

  if (cache_file->type != CACHEFILE_TYPE_ALEMBIC ||
      !RE_engine_supports_alembic_procedural(render_engine_type, scene))
  {
    return false;
  }

  return cache_file->use_render_procedural;
}

CacheFileLayer *BKE_cachefile_add_layer(CacheFile *cache_file, const char filepath[1024])
{
  for (CacheFileLayer *layer = cache_file->layers.first; layer; layer = layer->next) {
    if (STREQ(layer->filepath, filepath)) {
      return NULL;
    }
  }

  const int num_layers = BLI_listbase_count(&cache_file->layers);

  CacheFileLayer *layer = MEM_callocN(sizeof(CacheFileLayer), "CacheFileLayer");
  STRNCPY(layer->filepath, filepath);

  BLI_addtail(&cache_file->layers, layer);

  cache_file->active_layer = (char)(num_layers + 1);

  return layer;
}

CacheFileLayer *BKE_cachefile_get_active_layer(CacheFile *cache_file)
{
  return BLI_findlink(&cache_file->layers, cache_file->active_layer - 1);
}

void BKE_cachefile_remove_layer(CacheFile *cache_file, CacheFileLayer *layer)
{
  cache_file->active_layer = 0;
  BLI_remlink(&cache_file->layers, layer);
  MEM_freeN(layer);
}
