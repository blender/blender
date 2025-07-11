/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#pragma once

#include <cstdint>
#include <memory>

#include "BLI_fileops.h"

#include "DNA_listBase.h"
#include "DNA_space_enums.h"
#include "DNA_space_types.h"

#define FILEDIR_NBR_ENTRIES_UNSET -1

using FileUID = uint32_t;

struct AssetLibraryReference;
struct BlendHandle;
struct FileDirEntry;
struct FileIndexerType;
struct GHash;
struct ID;
struct PreviewImage;
struct ThreadQueue;
struct TaskPool;

namespace blender {
namespace asset_system {
class AssetLibrary;
class AssetRepresentation;
}  // namespace asset_system
namespace ed::asset_browser {
class AssetCatalogFilterSettings;
}
}  // namespace blender

/* ------------------FILELIST------------------------ */

struct FileListInternEntry {
  FileListInternEntry *next = nullptr, *prev = nullptr;

  FileUID uid = 0;

  eFileSel_File_Types typeflag = eFileSel_File_Types(0);
  /** ID type, in case typeflag has FILE_TYPE_BLENDERLIB set. */
  int blentype = 0;

  char *relpath = nullptr;
  /** Optional argument for shortcuts, aliases etc. */
  char *redirection_path = nullptr;
  /** not strictly needed, but used during sorting, avoids to have to recompute it there... */
  const char *name = nullptr;
  bool free_name = false;

  /**
   * This is data from the current main, represented by this file. It's crucial that this is
   * updated correctly on undo, redo and file reading (without UI). The space is responsible to
   * take care of that.
   */
  struct {
    /** When showing local IDs (FILE_MAIN, FILE_MAIN_ASSET), the ID this file entry represents. */
    ID *id = nullptr;

    /* For the few file types that have the preview already in memory. For others, there's delayed
     * preview reading from disk. Non-owning pointer. */
    PreviewImage *preview_image = nullptr;
  } local_data;

  /**
   * References an asset in the asset library storage.
   * The file list inserts this asset representation into the library, and removes it again when
   * the file list is destructed. In that sense it manages the asset but doesn't own it.
   *
   * Weak pointer so access is protected in case the asset library gets destructed externally.
   */
  std::weak_ptr<blender::asset_system::AssetRepresentation> asset;

  /* See #FILE_ENTRY_BLENDERLIB_NO_PREVIEW. */
  bool blenderlib_has_no_preview = false;

  /** Defined in BLI_fileops.h */
  eFileAttributes attributes = eFileAttributes(0);
  BLI_stat_t st = {0};

  /**
   * Be careful not to use the returned asset pointer in a context where it might be dangling, e.g.
   * because the file list or the asset library were destroyed.
   */
  blender::asset_system::AssetRepresentation *get_asset() const
  {
    if (std::shared_ptr<blender::asset_system::AssetRepresentation> asset_ptr = asset.lock()) {
      /* Returning a raw pointer from a shared pointer and destructing the shared pointer
       * immediately afterwards isn't entirely clean. But it's just a way to get the raw pointer
       * from the weak pointer. Nothing should free the asset in the asset library meanwhile, so
       * this should be fine really. */
      BLI_assert(asset_ptr.use_count() > 1);
      return asset_ptr.get();
    }
    return nullptr;
  }
};

struct FileListIntern {
  /** FileListInternEntry items. */
  ListBase entries;
  FileListInternEntry **filtered;

  FileUID curr_uid; /* Used to generate UID during internal listing. */
};

#define FILELIST_ENTRYCACHESIZE_DEFAULT 1024 /* Keep it a power of two! */
struct FileListEntryCache {
  size_t size = 0; /* The size of the cache... */

  int flags = 0;

  /* This one gathers all entries from both block and misc caches. Used for easy bulk-freeing. */
  ListBase cached_entries = {};

  /* Block cache: all entries between start and end index.
   * used for part of the list on display. */
  FileDirEntry **block_entries = nullptr;
  int block_start_index = 0;
  int block_end_index = 0;
  int block_center_index = 0;
  int block_cursor = 0;

  /* Misc cache: random indices, FIFO behavior.
   * NOTE: Not 100% sure we actually need that, time will say. */
  int misc_cursor = 0;
  int *misc_entries_indices = nullptr;
  GHash *misc_entries = nullptr;

  /* Allows to quickly get a cached entry from its UID. */
  GHash *uids = nullptr;

  /* Previews handling. */
  TaskPool *previews_pool = nullptr;
  ThreadQueue *previews_done = nullptr;
  /** Counter for previews that are not fully loaded and ready to display yet. So includes all
   * previews either in `previews_pool` or `previews_done`. #filelist_cache_previews_update() makes
   * previews in `preview_done` ready for display, so the counter is decremented there. */
  int previews_todo_count = 0;

  FileListEntryCache();
  ~FileListEntryCache();
};

/** #FileListCache.flags */
enum {
  FLC_PREVIEWS_ACTIVE = 1 << 0,
};

struct FileListEntryPreview {
  /** Use #FILE_MAX_LIBEXTRA as this is the size written into by #filelist_file_get_full_path. */
  char filepath[FILE_MAX_LIBEXTRA];
  uint flags;
  int index;
  int icon_id;
};

/* Dummy wrapper around FileListEntryPreview to ensure we do not access freed memory when freeing
 * tasks' data (see #74609). */
struct FileListEntryPreviewTaskData {
  FileListEntryPreview *preview;
};

struct FileListFilter {
  uint64_t filter;
  uint64_t filter_id;
  char filter_glob[FILE_MAXFILE];
  char filter_search[66]; /* + 2 for heading/trailing implicit '*' wildcards. */
  short flags;

  blender::ed::asset_browser::AssetCatalogFilterSettings *asset_catalog_filter;
};

/** #FileListFilter.flags */
enum {
  FLF_DO_FILTER = 1 << 0,
  FLF_HIDE_DOT = 1 << 1,
  FLF_HIDE_PARENT = 1 << 2,
  FLF_HIDE_LIB_DIR = 1 << 3,
  FLF_ASSETS_ONLY = 1 << 4,
};

struct FileListReadJob;
struct FileList {
  FileDirEntryArr filelist;

  eFileSelectType type;
  /* The library this list was created for. Stored here so we know when to re-read. */
  AssetLibraryReference *asset_library_ref;
  blender::asset_system::AssetLibrary *asset_library; /* Non-owning. */

  short flags;

  short sort;

  FileListFilter filter_data;

  /**
   * File indexer to use. Attribute is always set.
   */
  const FileIndexerType *indexer;

  FileListIntern filelist_intern;

  FileListEntryCache *filelist_cache;

  /**
   * We need to keep those info outside of actual file-list items,
   * because those are no more persistent
   * (only generated on demand, and freed as soon as possible).
   * Persistent part (mere list of paths + stat info)
   * is kept as small as possible, and file-browser agnostic.
   *
   * - The key is a #FileDirEntry::uid
   * - The value is an #eDirEntry_SelectFlag.
   */
  GHash *selection_state;

  short max_recursion;
  short recursion_level;

  BlendHandle *libfiledata;

  /**
   * Set given path as root directory.
   *
   * \param do_change: When true, the callback may change given string in place to a valid value.
   * \return True when `dirpath` is valid.
   */
  bool (*check_dir_fn)(const FileList *filelist,
                       char dirpath[FILE_MAX_LIBEXTRA],
                       const bool do_change);

  /** Fill `filelist` (to be called by read job). */
  void (*read_job_fn)(FileListReadJob *job_params, bool *stop, bool *do_update, float *progress);

  /** Filter an entry of current `filelist`. */
  bool (*filter_fn)(FileListInternEntry *file, const char *root, FileListFilter *filter);
  /** Executed before filtering individual items, to set up additional filter data. */
  void (*prepare_filter_fn)(const FileList *filelist, FileListFilter *filter);

  /** #FileListTags. */
  short tags;
};

/** #FileList.flags */
enum {
  FL_FORCE_RESET = 1 << 0,
  /* Don't do a full reset (unless #FL_FORCE_RESET is also set), only reset files representing main
   * data (assets from the current file/#Main). */
  FL_FORCE_RESET_MAIN_FILES = 1 << 1,
  FL_IS_READY = 1 << 2,
  FL_IS_PENDING = 1 << 3,
  FL_NEED_SORTING = 1 << 4,
  FL_NEED_FILTERING = 1 << 5,
  FL_SORT_INVERT = 1 << 6,
  /** Trigger a call to #AS_asset_library_load() to update asset catalogs (won't reload the actual
   * assets) */
  FL_RELOAD_ASSET_LIBRARY = 1 << 7,
};

/** #FileList.tags */
enum FileListTags {
  /** The file list has references to main data (IDs) and needs special care. */
  FILELIST_TAGS_USES_MAIN_DATA = (1 << 0),
  /** The file list type is not thread-safe. */
  FILELIST_TAGS_NO_THREADS = (1 << 2),
};

enum class SpecialFileImages {
  Document,
  Folder,
  Parent,
  /* Keep this last. */
  _Max,
};

void filelist_cache_clear(FileListEntryCache *cache, size_t new_size);

bool filelist_intern_entry_is_main_file(const FileListInternEntry *intern_entry);

void prepare_filter_asset_library(const FileList *filelist, FileListFilter *filter);

/** \return true when the file should be in the result set, false if it should be filtered out. */
bool is_filtered_file(FileListInternEntry *file, const char * /*root*/, FileListFilter *filter);
bool is_filtered_asset(FileListInternEntry *file, FileListFilter *filter);
bool is_filtered_lib(FileListInternEntry *file, const char *root, FileListFilter *filter);
bool is_filtered_main(FileListInternEntry *file, const char * /*dir*/, FileListFilter *filter);
bool is_filtered_main_assets(FileListInternEntry *file,
                             const char * /*dir*/,
                             FileListFilter *filter);
bool is_filtered_asset_library(FileListInternEntry *file,
                               const char *root,
                               FileListFilter *filter);
