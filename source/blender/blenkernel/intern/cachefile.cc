/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstring>
#include <optional>

#include "DNA_cachefile_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_mutex.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_bpath.hh"
#include "BKE_cachefile.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph_query.hh"

#include "RE_engine.h"

#include "BLO_read_write.hh"

#include "MEM_guardedalloc.h"

#ifdef WITH_ALEMBIC
#  include "ABC_alembic.h"
#endif

#ifdef WITH_USD
#  include "usd.hh"
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

static void cache_file_copy_data(Main * /*bmain*/,
                                 std::optional<Library *> /*owner_library*/,
                                 ID *id_dst,
                                 const ID *id_src,
                                 const int /*flag*/)
{
  CacheFile *cache_file_dst = (CacheFile *)id_dst;
  const CacheFile *cache_file_src = (const CacheFile *)id_src;

  cache_file_dst->handle = nullptr;
  cache_file_dst->handle_readers = nullptr;
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
  cache_file->handle = nullptr;
  memset(cache_file->handle_filepath, 0, sizeof(cache_file->handle_filepath));
  cache_file->handle_readers = nullptr;

  BLO_write_id_struct(writer, CacheFile, id_address, &cache_file->id);
  BKE_id_blend_write(writer, &cache_file->id);

  /* write layers */
  LISTBASE_FOREACH (CacheFileLayer *, layer, &cache_file->layers) {
    BLO_write_struct(writer, CacheFileLayer, layer);
  }
}

static void cache_file_blend_read_data(BlendDataReader *reader, ID *id)
{
  CacheFile *cache_file = (CacheFile *)id;
  BLI_listbase_clear(&cache_file->object_paths);
  cache_file->handle = nullptr;
  cache_file->handle_filepath[0] = '\0';
  cache_file->handle_readers = nullptr;

  /* relink layers */
  BLO_read_struct_list(reader, CacheFileLayer, &cache_file->layers);
}

IDTypeInfo IDType_ID_CF = {
    /*id_code*/ CacheFile::id_type,
    /*id_filter*/ FILTER_ID_CF,
    /*dependencies_id_types*/ 0,
    /*main_listbase_index*/ INDEX_ID_CF,
    /*struct_size*/ sizeof(CacheFile),
    /*name*/ "CacheFile",
    /*name_plural*/ N_("cache_files"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_CACHEFILE,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ cache_file_init_data,
    /*copy_data*/ cache_file_copy_data,
    /*free_data*/ cache_file_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ cache_file_foreach_path,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ cache_file_blend_write,
    /*blend_read_data*/ cache_file_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

#if defined(WITH_ALEMBIC) || defined(WITH_USD)
/* TODO: make this per cache file to avoid global locks. */
static blender::Mutex cache_mutex;
#endif

void BKE_cachefile_reader_open(CacheFile *cache_file,
                               CacheReader **reader,
                               Object *object,
                               const char *object_path)
{
#if defined(WITH_ALEMBIC) || defined(WITH_USD)

  BLI_assert(cache_file->id.tag & ID_TAG_COPIED_ON_EVAL);

  if (cache_file->handle == nullptr) {
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
      *reader = blender::io::usd::CacheReader_open_usd_object(
          cache_file->handle, *reader, object, object_path);
#  endif
      break;
    case CACHE_FILE_TYPE_INVALID:
      break;
  }

  /* Multiple modifiers and constraints can call this function concurrently. */
  std::lock_guard lock(cache_mutex);
  if (*reader) {
    /* Register in set so we can free it when the cache file changes. */
    if (cache_file->handle_readers == nullptr) {
      cache_file->handle_readers = BLI_gset_ptr_new("CacheFile.handle_readers");
    }
    BLI_gset_reinsert(cache_file->handle_readers, reader, nullptr);
  }
  else if (cache_file->handle_readers) {
    /* Remove in case CacheReader_open_alembic_object free the existing reader. */
    BLI_gset_remove(cache_file->handle_readers, reader, nullptr);
  }
#else
  UNUSED_VARS(cache_file, reader, object, object_path);
#endif
}

void BKE_cachefile_reader_free(CacheFile *cache_file, CacheReader **reader)
{
#if defined(WITH_ALEMBIC) || defined(WITH_USD)
  /* Multiple modifiers and constraints can call this function concurrently, and
   * cachefile_handle_free() can also be called at the same time. */
  std::lock_guard lock(cache_mutex);
  if (*reader != nullptr) {
    if (cache_file) {
      BLI_assert(cache_file->id.tag & ID_TAG_COPIED_ON_EVAL);

      switch (cache_file->type) {
        case CACHEFILE_TYPE_ALEMBIC:
#  ifdef WITH_ALEMBIC
          ABC_CacheReader_free(*reader);
#  endif
          break;
        case CACHEFILE_TYPE_USD:
#  ifdef WITH_USD
          blender::io::usd::USD_CacheReader_free(*reader);
#  endif
          break;
        case CACHE_FILE_TYPE_INVALID:
          break;
      }
    }

    *reader = nullptr;

    if (cache_file && cache_file->handle_readers) {
      BLI_gset_remove(cache_file->handle_readers, reader, nullptr);
    }
  }
#else
  UNUSED_VARS(cache_file, reader);
#endif
}

static void cachefile_handle_free(CacheFile *cache_file)
{
#if defined(WITH_ALEMBIC) || defined(WITH_USD)

  /* Free readers in all modifiers and constraints that use the handle, before
   * we free the handle itself. */
  {
    std::lock_guard lock(cache_mutex);
    if (cache_file->handle_readers) {
      GSetIterator gs_iter;
      GSET_ITER (gs_iter, cache_file->handle_readers) {
        CacheReader **reader = static_cast<CacheReader **>(BLI_gsetIterator_getKey(&gs_iter));
        if (*reader != nullptr) {
          switch (cache_file->type) {
            case CACHEFILE_TYPE_ALEMBIC:
#  ifdef WITH_ALEMBIC
              ABC_CacheReader_free(*reader);
#  endif
              break;
            case CACHEFILE_TYPE_USD:
#  ifdef WITH_USD
              blender::io::usd::USD_CacheReader_free(*reader);
#  endif
              break;
            case CACHE_FILE_TYPE_INVALID:
              break;
          }

          *reader = nullptr;
        }
      }

      BLI_gset_free(cache_file->handle_readers, nullptr);
      cache_file->handle_readers = nullptr;
    }
  }

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
        blender::io::usd::USD_free_handle(cache_file->handle);
#  endif
        break;
      case CACHE_FILE_TYPE_INVALID:
        break;
    }

    cache_file->handle = nullptr;
  }

  cache_file->handle_filepath[0] = '\0';
#else
  UNUSED_VARS(cache_file);
#endif
}

void *BKE_cachefile_add(Main *bmain, const char *name)
{
  CacheFile *cache_file = BKE_id_new<CacheFile>(bmain, name);

  return cache_file;
}

void BKE_cachefile_reload(Depsgraph *depsgraph, CacheFile *cache_file)
{
  /* To force reload, free the handle and tag depsgraph to load it again. */
  CacheFile *cache_file_eval = DEG_get_evaluated(depsgraph, cache_file);
  if (cache_file_eval) {
    cachefile_handle_free(cache_file_eval);
  }

  DEG_id_tag_update(&cache_file->id, ID_RECALC_SYNC_TO_EVAL);
}

void BKE_cachefile_eval(Main *bmain, Depsgraph *depsgraph, CacheFile *cache_file)
{
  BLI_assert(cache_file->id.tag & ID_TAG_COPIED_ON_EVAL);

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
  if (BLI_path_extension_check_glob(filepath, "*.abc")) {
    cache_file->type = CACHEFILE_TYPE_ALEMBIC;
    cache_file->handle = ABC_create_handle(
        bmain,
        filepath,
        static_cast<const CacheFileLayer *>(cache_file->layers.first),
        &cache_file->object_paths);
    STRNCPY(cache_file->handle_filepath, filepath);
  }
#endif
#ifdef WITH_USD
  if (BLI_path_extension_check_glob(filepath, "*.usd;*.usda;*.usdc;*.usdz")) {
    cache_file->type = CACHEFILE_TYPE_USD;
    cache_file->handle = blender::io::usd::USD_create_handle(
        bmain, filepath, &cache_file->object_paths);
    STRNCPY(cache_file->handle_filepath, filepath);
  }
#endif

  if (DEG_is_active(depsgraph)) {
    /* Flush object paths back to original data-block for UI. */
    CacheFile *cache_file_orig = DEG_get_original(cache_file);
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
    const double fps = double(scene->r.frs_sec) / double(scene->r.frs_sec_base);
    const int frame = int(BKE_cachefile_time_offset(cache_file, double(ctime), fps));

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
  const double time_offset = double(cache_file->frame_offset) / fps;
  const double frame = (cache_file->override_frame ? double(cache_file->frame) : time);
  return cache_file->is_sequence ? frame : frame / fps - time_offset;
}

double BKE_cachefile_frame_offset(const CacheFile *cache_file, const double time)
{
  const double time_offset = double(cache_file->frame_offset);
  const double frame = cache_file->override_frame ? double(cache_file->frame) : time;
  return cache_file->is_sequence ? frame : frame - time_offset;
}

CacheFileLayer *BKE_cachefile_add_layer(CacheFile *cache_file, const char filepath[1024])
{
  LISTBASE_FOREACH (CacheFileLayer *, layer, &cache_file->layers) {
    if (STREQ(layer->filepath, filepath)) {
      return nullptr;
    }
  }

  const int num_layers = BLI_listbase_count(&cache_file->layers);

  CacheFileLayer *layer = MEM_callocN<CacheFileLayer>("CacheFileLayer");
  STRNCPY(layer->filepath, filepath);

  BLI_addtail(&cache_file->layers, layer);

  cache_file->active_layer = char(num_layers + 1);

  return layer;
}

CacheFileLayer *BKE_cachefile_get_active_layer(CacheFile *cache_file)
{
  return static_cast<CacheFileLayer *>(
      BLI_findlink(&cache_file->layers, cache_file->active_layer - 1));
}

void BKE_cachefile_remove_layer(CacheFile *cache_file, CacheFileLayer *layer)
{
  cache_file->active_layer = 0;
  BLI_remlink(&cache_file->layers, layer);
  MEM_freeN(layer);
}
