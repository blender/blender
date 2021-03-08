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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spfile
 */

/* global includes */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifndef WIN32
#  include <unistd.h>
#else
#  include <direct.h>
#  include <io.h>
#endif
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_fnmatch.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_stack.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BKE_asset.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_main_idmap.h"
#include "BKE_preferences.h"
#include "BLO_readfile.h"

#include "DNA_asset_types.h"
#include "DNA_space_types.h"

#include "ED_datafiles.h"
#include "ED_fileselect.h"
#include "ED_screen.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "PIL_time.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "atomic_ops.h"

#include "filelist.h"

#define FILEDIR_NBR_ENTRIES_UNSET -1

/* ----------------- FOLDERLIST (previous/next) -------------- */

typedef struct FolderList {
  struct FolderList *next, *prev;
  char *foldername;
} FolderList;

void folderlist_popdir(struct ListBase *folderlist, char *dir)
{
  const char *prev_dir;
  struct FolderList *folder;
  folder = folderlist->last;

  if (folder) {
    /* remove the current directory */
    MEM_freeN(folder->foldername);
    BLI_freelinkN(folderlist, folder);

    folder = folderlist->last;
    if (folder) {
      prev_dir = folder->foldername;
      BLI_strncpy(dir, prev_dir, FILE_MAXDIR);
    }
  }
  /* delete the folder next or use setdir directly before PREVIOUS OP */
}

void folderlist_pushdir(ListBase *folderlist, const char *dir)
{
  if (!dir[0]) {
    return;
  }

  struct FolderList *folder, *previous_folder;
  previous_folder = folderlist->last;

  /* check if already exists */
  if (previous_folder && previous_folder->foldername) {
    if (BLI_path_cmp(previous_folder->foldername, dir) == 0) {
      return;
    }
  }

  /* create next folder element */
  folder = MEM_mallocN(sizeof(*folder), __func__);
  folder->foldername = BLI_strdup(dir);

  /* add it to the end of the list */
  BLI_addtail(folderlist, folder);
}

const char *folderlist_peeklastdir(ListBase *folderlist)
{
  struct FolderList *folder;

  if (!folderlist->last) {
    return NULL;
  }

  folder = folderlist->last;
  return folder->foldername;
}

int folderlist_clear_next(struct SpaceFile *sfile)
{
  const FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  struct FolderList *folder;

  /* if there is no folder_next there is nothing we can clear */
  if (BLI_listbase_is_empty(sfile->folders_next)) {
    return 0;
  }

  /* if previous_folder, next_folder or refresh_folder operators are executed
   * it doesn't clear folder_next */
  folder = sfile->folders_prev->last;
  if ((!folder) || (BLI_path_cmp(folder->foldername, params->dir) == 0)) {
    return 0;
  }

  /* eventually clear flist->folders_next */
  return 1;
}

/* not listbase itself */
void folderlist_free(ListBase *folderlist)
{
  if (folderlist) {
    FolderList *folder;
    for (folder = folderlist->first; folder; folder = folder->next) {
      MEM_freeN(folder->foldername);
    }
    BLI_freelistN(folderlist);
  }
}

static ListBase folderlist_duplicate(ListBase *folderlist)
{
  ListBase folderlistn = {NULL};

  BLI_duplicatelist(&folderlistn, folderlist);

  for (FolderList *folder = folderlistn.first; folder; folder = folder->next) {
    folder->foldername = MEM_dupallocN(folder->foldername);
  }
  return folderlistn;
}

/* ----------------- Folder-History (wraps/owns file list above) -------------- */

static FileFolderHistory *folder_history_find(const SpaceFile *sfile, eFileBrowse_Mode browse_mode)
{
  LISTBASE_FOREACH (FileFolderHistory *, history, &sfile->folder_histories) {
    if (history->browse_mode == browse_mode) {
      return history;
    }
  }

  return NULL;
}

void folder_history_list_ensure_for_active_browse_mode(SpaceFile *sfile)
{
  FileFolderHistory *history = folder_history_find(sfile, sfile->browse_mode);

  if (!history) {
    history = MEM_callocN(sizeof(*history), __func__);
    history->browse_mode = sfile->browse_mode;
    BLI_addtail(&sfile->folder_histories, history);
  }

  sfile->folders_next = &history->folders_next;
  sfile->folders_prev = &history->folders_prev;
}

static void folder_history_entry_free(SpaceFile *sfile, FileFolderHistory *history)
{
  if (sfile->folders_prev == &history->folders_prev) {
    sfile->folders_prev = NULL;
  }
  if (sfile->folders_next == &history->folders_next) {
    sfile->folders_next = NULL;
  }
  folderlist_free(&history->folders_prev);
  folderlist_free(&history->folders_next);
  BLI_freelinkN(&sfile->folder_histories, history);
}

void folder_history_list_free(SpaceFile *sfile)
{
  LISTBASE_FOREACH_MUTABLE (FileFolderHistory *, history, &sfile->folder_histories) {
    folder_history_entry_free(sfile, history);
  }
}

ListBase folder_history_list_duplicate(ListBase *listbase)
{
  ListBase histories = {NULL};

  LISTBASE_FOREACH (FileFolderHistory *, history, listbase) {
    FileFolderHistory *history_new = MEM_dupallocN(history);
    history_new->folders_prev = folderlist_duplicate(&history->folders_prev);
    history_new->folders_next = folderlist_duplicate(&history->folders_next);
    BLI_addtail(&histories, history_new);
  }

  return histories;
}

/* ------------------FILELIST------------------------ */

typedef struct FileListInternEntry {
  struct FileListInternEntry *next, *prev;

  /** ASSET_UUID_LENGTH */
  char uuid[16];

  /** eFileSel_File_Types */
  int typeflag;
  /** ID type, in case typeflag has FILE_TYPE_BLENDERLIB set. */
  int blentype;

  char *relpath;
  /** Optional argument for shortcuts, aliases etc. */
  char *redirection_path;
  /** not strictly needed, but used during sorting, avoids to have to recompute it there... */
  char *name;
  bool free_name;

  /**
   * This is data from the current main, represented by this file. It's crucial that this is
   * updated correctly on undo, redo and file reading (without UI). The space is responsible to
   * take care of that.
   */
  struct {
    /** When showing local IDs (FILE_MAIN, FILE_MAIN_ASSET), the ID this file entry represents. */
    ID *id;

    /* For the few file types that have the preview already in memory. For others, there's delayed
     * preview reading from disk. Non-owning pointer. */
    PreviewImage *preview_image;
  } local_data;

  /** When the file represents an asset read from another file, it is stored here.
   * Owning pointer. */
  AssetMetaData *imported_asset_data;

  /** Defined in BLI_fileops.h */
  eFileAttributes attributes;
  BLI_stat_t st;
} FileListInternEntry;

typedef struct FileListIntern {
  /** FileListInternEntry items. */
  ListBase entries;
  FileListInternEntry **filtered;

  char curr_uuid[16]; /* Used to generate uuid during internal listing. */
} FileListIntern;

#define FILELIST_ENTRYCACHESIZE_DEFAULT 1024 /* Keep it a power of two! */
typedef struct FileListEntryCache {
  size_t size; /* The size of the cache... */

  int flags;

  /* This one gathers all entries from both block and misc caches. Used for easy bulk-freing. */
  ListBase cached_entries;

  /* Block cache: all entries between start and end index.
   * used for part of the list on display. */
  FileDirEntry **block_entries;
  int block_start_index, block_end_index, block_center_index, block_cursor;

  /* Misc cache: random indices, FIFO behavior.
   * Note: Not 100% sure we actually need that, time will say. */
  int misc_cursor;
  int *misc_entries_indices;
  GHash *misc_entries;

  /* Allows to quickly get a cached entry from its UUID. */
  GHash *uuids;

  /* Previews handling. */
  TaskPool *previews_pool;
  ThreadQueue *previews_done;
} FileListEntryCache;

/* FileListCache.flags */
enum {
  FLC_IS_INIT = 1 << 0,
  FLC_PREVIEWS_ACTIVE = 1 << 1,
};

typedef struct FileListEntryPreview {
  char path[FILE_MAX];
  uint flags;
  int index;
  /* Some file types load the memory from runtime data, not from disk. We just wait until it's done
   * generating (BKE_previewimg_is_finished()). */
  PreviewImage *in_memory_preview;

  int icon_id;
} FileListEntryPreview;

/* Dummy wrapper around FileListEntryPreview to ensure we do not access freed memory when freeing
 * tasks' data (see T74609). */
typedef struct FileListEntryPreviewTaskData {
  FileListEntryPreview *preview;
} FileListEntryPreviewTaskData;

typedef struct FileListFilter {
  uint64_t filter;
  uint64_t filter_id;
  char filter_glob[FILE_MAXFILE];
  char filter_search[66]; /* + 2 for heading/trailing implicit '*' wildcards. */
  short flags;
} FileListFilter;

/* FileListFilter.flags */
enum {
  FLF_DO_FILTER = 1 << 0,
  FLF_HIDE_DOT = 1 << 1,
  FLF_HIDE_PARENT = 1 << 2,
  FLF_HIDE_LIB_DIR = 1 << 3,
  FLF_ASSETS_ONLY = 1 << 4,
};

typedef struct FileList {
  FileDirEntryArr filelist;

  eFileSelectType type;
  /* The library this list was created for. Stored here so we know when to re-read. */
  FileSelectAssetLibraryUID *asset_library;

  short flags;

  short sort;

  FileListFilter filter_data;

  struct FileListIntern filelist_intern;

  struct FileListEntryCache filelist_cache;

  /* We need to keep those info outside of actual filelist items,
   * because those are no more persistent
   * (only generated on demand, and freed as soon as possible).
   * Persistent part (mere list of paths + stat info)
   * is kept as small as possible, and filebrowser-agnostic.
   */
  GHash *selection_state;

  short max_recursion;
  short recursion_level;

  struct BlendHandle *libfiledata;

  /* Set given path as root directory,
   * if last bool is true may change given string in place to a valid value.
   * Returns True if valid dir. */
  bool (*check_dir_fn)(struct FileList *, char *, const bool);

  /* Fill filelist (to be called by read job). */
  void (*read_job_fn)(
      Main *, struct FileList *, const char *, short *, short *, float *, ThreadMutex *);

  /* Filter an entry of current filelist. */
  bool (*filter_fn)(struct FileListInternEntry *, const char *, FileListFilter *);

  short tags; /* FileListTags */
} FileList;

/* FileList.flags */
enum {
  FL_FORCE_RESET = 1 << 0,
  FL_IS_READY = 1 << 1,
  FL_IS_PENDING = 1 << 2,
  FL_NEED_SORTING = 1 << 3,
  FL_NEED_FILTERING = 1 << 4,
  FL_SORT_INVERT = 1 << 5,
};

/* FileList.tags */
enum FileListTags {
  /** The file list has references to main data (IDs) and needs special care. */
  FILELIST_TAGS_USES_MAIN_DATA = (1 << 0),
  /** The file list type is not thread-safe. */
  FILELIST_TAGS_NO_THREADS = (1 << 2),
};

#define SPECIAL_IMG_SIZE 256
#define SPECIAL_IMG_ROWS 1
#define SPECIAL_IMG_COLS 7

enum {
  SPECIAL_IMG_DOCUMENT = 0,
  SPECIAL_IMG_DRIVE_DISC = 1,
  SPECIAL_IMG_FOLDER = 2,
  SPECIAL_IMG_PARENT = 3,
  SPECIAL_IMG_DRIVE_FIXED = 4,
  SPECIAL_IMG_DRIVE_ATTACHED = 5,
  SPECIAL_IMG_DRIVE_REMOTE = 6,
  SPECIAL_IMG_MAX,
};

static ImBuf *gSpecialFileImages[SPECIAL_IMG_MAX];

static void filelist_readjob_main(Main *current_main,
                                  FileList *filelist,
                                  const char *main_name,
                                  short *stop,
                                  short *do_update,
                                  float *progress,
                                  ThreadMutex *lock);
static void filelist_readjob_lib(Main *current_main,
                                 FileList *filelist,
                                 const char *main_name,
                                 short *stop,
                                 short *do_update,
                                 float *progress,
                                 ThreadMutex *lock);
static void filelist_readjob_dir(Main *current_main,
                                 FileList *filelist,
                                 const char *main_name,
                                 short *stop,
                                 short *do_update,
                                 float *progress,
                                 ThreadMutex *lock);
static void filelist_readjob_main_assets(Main *current_main,
                                         FileList *filelist,
                                         const char *main_name,
                                         short *stop,
                                         short *do_update,
                                         float *progress,
                                         ThreadMutex *lock);

/* helper, could probably go in BKE actually? */
static int groupname_to_code(const char *group);
static uint64_t groupname_to_filter_id(const char *group);

static void filelist_filter_clear(FileList *filelist);
static void filelist_cache_clear(FileListEntryCache *cache, size_t new_size);

/* ********** Sort helpers ********** */

struct FileSortData {
  bool inverted;
};

static int compare_apply_inverted(int val, const struct FileSortData *sort_data)
{
  return sort_data->inverted ? -val : val;
}

/**
 * Handles inverted sorting itself (currently there's nothing to invert), so if this returns non-0,
 * it should be used as-is and not inverted.
 */
static int compare_direntry_generic(const FileListInternEntry *entry1,
                                    const FileListInternEntry *entry2)
{
  /* type is equal to stat.st_mode */

  if (entry1->typeflag & FILE_TYPE_DIR) {
    if (entry2->typeflag & FILE_TYPE_DIR) {
      /* If both entries are tagged as dirs, we make a 'sub filter' that shows first the real dirs,
       * then libs (.blend files), then categories in libs. */
      if (entry1->typeflag & FILE_TYPE_BLENDERLIB) {
        if (!(entry2->typeflag & FILE_TYPE_BLENDERLIB)) {
          return 1;
        }
      }
      else if (entry2->typeflag & FILE_TYPE_BLENDERLIB) {
        return -1;
      }
      else if (entry1->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) {
        if (!(entry2->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP))) {
          return 1;
        }
      }
      else if (entry2->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) {
        return -1;
      }
    }
    else {
      return -1;
    }
  }
  else if (entry2->typeflag & FILE_TYPE_DIR) {
    return 1;
  }

  /* make sure "." and ".." are always first */
  if (FILENAME_IS_CURRENT(entry1->relpath)) {
    return -1;
  }
  if (FILENAME_IS_CURRENT(entry2->relpath)) {
    return 1;
  }
  if (FILENAME_IS_PARENT(entry1->relpath)) {
    return -1;
  }
  if (FILENAME_IS_PARENT(entry2->relpath)) {
    return 1;
  }

  return 0;
}

static int compare_name(void *user_data, const void *a1, const void *a2)
{
  const FileListInternEntry *entry1 = a1;
  const FileListInternEntry *entry2 = a2;
  const struct FileSortData *sort_data = user_data;
  char *name1, *name2;
  int ret;

  if ((ret = compare_direntry_generic(entry1, entry2))) {
    return ret;
  }

  name1 = entry1->name;
  name2 = entry2->name;

  return compare_apply_inverted(BLI_strcasecmp_natural(name1, name2), sort_data);
}

static int compare_date(void *user_data, const void *a1, const void *a2)
{
  const FileListInternEntry *entry1 = a1;
  const FileListInternEntry *entry2 = a2;
  const struct FileSortData *sort_data = user_data;
  char *name1, *name2;
  int64_t time1, time2;
  int ret;

  if ((ret = compare_direntry_generic(entry1, entry2))) {
    return ret;
  }

  time1 = (int64_t)entry1->st.st_mtime;
  time2 = (int64_t)entry2->st.st_mtime;
  if (time1 < time2) {
    return compare_apply_inverted(1, sort_data);
  }
  if (time1 > time2) {
    return compare_apply_inverted(-1, sort_data);
  }

  name1 = entry1->name;
  name2 = entry2->name;

  return compare_apply_inverted(BLI_strcasecmp_natural(name1, name2), sort_data);
}

static int compare_size(void *user_data, const void *a1, const void *a2)
{
  const FileListInternEntry *entry1 = a1;
  const FileListInternEntry *entry2 = a2;
  const struct FileSortData *sort_data = user_data;
  char *name1, *name2;
  uint64_t size1, size2;
  int ret;

  if ((ret = compare_direntry_generic(entry1, entry2))) {
    return ret;
  }

  size1 = entry1->st.st_size;
  size2 = entry2->st.st_size;
  if (size1 < size2) {
    return compare_apply_inverted(1, sort_data);
  }
  if (size1 > size2) {
    return compare_apply_inverted(-1, sort_data);
  }

  name1 = entry1->name;
  name2 = entry2->name;

  return compare_apply_inverted(BLI_strcasecmp_natural(name1, name2), sort_data);
}

static int compare_extension(void *user_data, const void *a1, const void *a2)
{
  const FileListInternEntry *entry1 = a1;
  const FileListInternEntry *entry2 = a2;
  const struct FileSortData *sort_data = user_data;
  char *name1, *name2;
  int ret;

  if ((ret = compare_direntry_generic(entry1, entry2))) {
    return ret;
  }

  if ((entry1->typeflag & FILE_TYPE_BLENDERLIB) && !(entry2->typeflag & FILE_TYPE_BLENDERLIB)) {
    return -1;
  }
  if (!(entry1->typeflag & FILE_TYPE_BLENDERLIB) && (entry2->typeflag & FILE_TYPE_BLENDERLIB)) {
    return 1;
  }
  if ((entry1->typeflag & FILE_TYPE_BLENDERLIB) && (entry2->typeflag & FILE_TYPE_BLENDERLIB)) {
    if ((entry1->typeflag & FILE_TYPE_DIR) && !(entry2->typeflag & FILE_TYPE_DIR)) {
      return 1;
    }
    if (!(entry1->typeflag & FILE_TYPE_DIR) && (entry2->typeflag & FILE_TYPE_DIR)) {
      return -1;
    }
    if (entry1->blentype < entry2->blentype) {
      return compare_apply_inverted(-1, sort_data);
    }
    if (entry1->blentype > entry2->blentype) {
      return compare_apply_inverted(1, sort_data);
    }
  }
  else {
    const char *sufix1, *sufix2;

    if (!(sufix1 = strstr(entry1->relpath, ".blend.gz"))) {
      sufix1 = strrchr(entry1->relpath, '.');
    }
    if (!(sufix2 = strstr(entry2->relpath, ".blend.gz"))) {
      sufix2 = strrchr(entry2->relpath, '.');
    }
    if (!sufix1) {
      sufix1 = "";
    }
    if (!sufix2) {
      sufix2 = "";
    }

    if ((ret = BLI_strcasecmp(sufix1, sufix2))) {
      return compare_apply_inverted(ret, sort_data);
    }
  }

  name1 = entry1->name;
  name2 = entry2->name;

  return compare_apply_inverted(BLI_strcasecmp_natural(name1, name2), sort_data);
}

void filelist_sort(struct FileList *filelist)
{
  if (filelist->flags & FL_NEED_SORTING) {
    void *sort_cb = NULL;

    switch (filelist->sort) {
      case FILE_SORT_ALPHA:
        sort_cb = compare_name;
        break;
      case FILE_SORT_TIME:
        sort_cb = compare_date;
        break;
      case FILE_SORT_SIZE:
        sort_cb = compare_size;
        break;
      case FILE_SORT_EXTENSION:
        sort_cb = compare_extension;
        break;
      case FILE_SORT_DEFAULT:
      default:
        BLI_assert(0);
        break;
    }
    BLI_listbase_sort_r(
        &filelist->filelist_intern.entries,
        sort_cb,
        &(struct FileSortData){.inverted = (filelist->flags & FL_SORT_INVERT) != 0});

    filelist_filter_clear(filelist);
    filelist->flags &= ~FL_NEED_SORTING;
  }
}

void filelist_setsorting(struct FileList *filelist, const short sort, bool invert_sort)
{
  const bool was_invert_sort = filelist->flags & FL_SORT_INVERT;

  if ((filelist->sort != sort) || (was_invert_sort != invert_sort)) {
    filelist->sort = sort;
    filelist->flags |= FL_NEED_SORTING;
    filelist->flags = invert_sort ? (filelist->flags | FL_SORT_INVERT) :
                                    (filelist->flags & ~FL_SORT_INVERT);
  }
}

/* ********** Filter helpers ********** */

/* True if filename is meant to be hidden, eg. starting with period. */
static bool is_hidden_dot_filename(const char *filename, const FileListInternEntry *file)
{
  if (filename[0] == '.' && !ELEM(filename[1], '.', '\0')) {
    return true; /* ignore .file */
  }

  int len = strlen(filename);
  if ((len > 0) && (filename[len - 1] == '~')) {
    return true; /* ignore file~ */
  }

  /* filename might actually be a piece of path, in which case we have to check all its parts. */

  bool hidden = false;
  char *sep = (char *)BLI_path_slash_rfind(filename);

  if (!hidden && sep) {
    char tmp_filename[FILE_MAX_LIBEXTRA];

    BLI_strncpy(tmp_filename, filename, sizeof(tmp_filename));
    sep = tmp_filename + (sep - filename);
    while (sep) {
      /* This happens when a path contains 'ALTSEP', '\' on Unix for e.g.
       * Supporting alternate slashes in paths is a bigger task involving changes
       * in many parts of the code, for now just prevent an assert, see T74579. */
#if 0
      BLI_assert(sep[1] != '\0');
#endif
      if (is_hidden_dot_filename(sep + 1, file)) {
        hidden = true;
        break;
      }
      *sep = '\0';
      sep = (char *)BLI_path_slash_rfind(tmp_filename);
    }
  }
  return hidden;
}

/* True if should be hidden, based on current filtering. */
static bool is_filtered_hidden(const char *filename,
                               const FileListFilter *filter,
                               const FileListInternEntry *file)
{
  if ((filename[0] == '.') && (filename[1] == '\0')) {
    return true; /* Ignore . */
  }

  if (filter->flags & FLF_HIDE_PARENT) {
    if (filename[0] == '.' && filename[1] == '.' && filename[2] == '\0') {
      return true; /* Ignore .. */
    }
  }

  if ((filter->flags & FLF_HIDE_DOT) && (file->attributes & FILE_ATTR_HIDDEN)) {
    return true; /* Ignore files with Hidden attribute. */
  }

#ifndef WIN32
  /* Check for unix-style names starting with period. */
  if ((filter->flags & FLF_HIDE_DOT) && is_hidden_dot_filename(filename, file)) {
    return true;
  }
#endif
  /* For data-blocks (but not the group directories), check the asset-only filter. */
  if (!(file->typeflag & FILE_TYPE_DIR) && (file->typeflag & FILE_TYPE_BLENDERLIB) &&
      (filter->flags & FLF_ASSETS_ONLY) && !(file->typeflag & FILE_TYPE_ASSET)) {
    return true;
  }

  return false;
}

static bool is_filtered_file(FileListInternEntry *file,
                             const char *UNUSED(root),
                             FileListFilter *filter)
{
  bool is_filtered = !is_filtered_hidden(file->relpath, filter, file);

  if (is_filtered && !FILENAME_IS_CURRPAR(file->relpath)) {
    /* We only check for types if some type are enabled in filtering. */
    if (filter->filter && (filter->flags & FLF_DO_FILTER)) {
      if (file->typeflag & FILE_TYPE_DIR) {
        if (file->typeflag &
            (FILE_TYPE_BLENDERLIB | FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) {
          if (!(filter->filter & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP))) {
            is_filtered = false;
          }
        }
        else {
          if (!(filter->filter & FILE_TYPE_FOLDER)) {
            is_filtered = false;
          }
        }
      }
      else {
        if (!(file->typeflag & filter->filter)) {
          is_filtered = false;
        }
      }
    }
    /* If there's a filter string, apply it as filter even if FLF_DO_FILTER is not set. */
    if (is_filtered && (filter->filter_search[0] != '\0')) {
      if (fnmatch(filter->filter_search, file->relpath, FNM_CASEFOLD) != 0) {
        is_filtered = false;
      }
    }
  }

  return is_filtered;
}

static bool is_filtered_id_file(const FileListInternEntry *file,
                                const char *id_group,
                                const char *name,
                                const FileListFilter *filter)
{
  bool is_filtered = !is_filtered_hidden(file->relpath, filter, file);
  if (is_filtered && !FILENAME_IS_CURRPAR(file->relpath)) {
    /* We only check for types if some type are enabled in filtering. */
    if ((filter->filter || filter->filter_id) && (filter->flags & FLF_DO_FILTER)) {
      if (file->typeflag & FILE_TYPE_DIR) {
        if (file->typeflag &
            (FILE_TYPE_BLENDERLIB | FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) {
          if (!(filter->filter & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP))) {
            is_filtered = false;
          }
        }
        else {
          if (!(filter->filter & FILE_TYPE_FOLDER)) {
            is_filtered = false;
          }
        }
      }
      if (is_filtered && id_group) {
        if (!name && (filter->flags & FLF_HIDE_LIB_DIR)) {
          is_filtered = false;
        }
        else {
          uint64_t filter_id = groupname_to_filter_id(id_group);
          if (!(filter_id & filter->filter_id)) {
            is_filtered = false;
          }
        }
      }
    }
    /* If there's a filter string, apply it as filter even if FLF_DO_FILTER is not set. */
    if (is_filtered && (filter->filter_search[0] != '\0')) {
      if (fnmatch(filter->filter_search, file->relpath, FNM_CASEFOLD) != 0) {
        is_filtered = false;
      }
    }
  }

  return is_filtered;
}

static bool is_filtered_lib(FileListInternEntry *file, const char *root, FileListFilter *filter)
{
  bool is_filtered;
  char path[FILE_MAX_LIBEXTRA], dir[FILE_MAX_LIBEXTRA], *group, *name;

  BLI_join_dirfile(path, sizeof(path), root, file->relpath);

  if (BLO_library_path_explode(path, dir, &group, &name)) {
    is_filtered = is_filtered_id_file(file, group, name, filter);
  }
  else {
    is_filtered = is_filtered_file(file, root, filter);
  }

  return is_filtered;
}

static bool is_filtered_main(FileListInternEntry *file,
                             const char *UNUSED(dir),
                             FileListFilter *filter)
{
  return !is_filtered_hidden(file->relpath, filter, file);
}

static bool is_filtered_main_assets(FileListInternEntry *file,
                                    const char *UNUSED(dir),
                                    FileListFilter *filter)
{
  /* "Filtered" means *not* being filtered out... So return true if the file should be visible. */
  return is_filtered_id_file(file, file->relpath, file->name, filter);
}

static void filelist_filter_clear(FileList *filelist)
{
  filelist->flags |= FL_NEED_FILTERING;
}

void filelist_filter(FileList *filelist)
{
  int num_filtered = 0;
  const int num_files = filelist->filelist.nbr_entries;
  FileListInternEntry **filtered_tmp, *file;

  if (ELEM(filelist->filelist.nbr_entries, FILEDIR_NBR_ENTRIES_UNSET, 0)) {
    return;
  }

  if (!(filelist->flags & FL_NEED_FILTERING)) {
    /* Assume it has already been filtered, nothing else to do! */
    return;
  }

  filelist->filter_data.flags &= ~FLF_HIDE_LIB_DIR;
  if (filelist->max_recursion) {
    /* Never show lib ID 'categories' directories when we are in 'flat' mode, unless
     * root path is a blend file. */
    char dir[FILE_MAX_LIBEXTRA];
    if (!filelist_islibrary(filelist, dir, NULL)) {
      filelist->filter_data.flags |= FLF_HIDE_LIB_DIR;
    }
  }

  filtered_tmp = MEM_mallocN(sizeof(*filtered_tmp) * (size_t)num_files, __func__);

  /* Filter remap & count how many files are left after filter in a single loop. */
  for (file = filelist->filelist_intern.entries.first; file; file = file->next) {
    if (filelist->filter_fn(file, filelist->filelist.root, &filelist->filter_data)) {
      filtered_tmp[num_filtered++] = file;
    }
  }

  if (filelist->filelist_intern.filtered) {
    MEM_freeN(filelist->filelist_intern.filtered);
  }
  filelist->filelist_intern.filtered = MEM_mallocN(
      sizeof(*filelist->filelist_intern.filtered) * (size_t)num_filtered, __func__);
  memcpy(filelist->filelist_intern.filtered,
         filtered_tmp,
         sizeof(*filelist->filelist_intern.filtered) * (size_t)num_filtered);
  filelist->filelist.nbr_entries_filtered = num_filtered;
  //  printf("Filetered: %d over %d entries\n", num_filtered, filelist->filelist.nbr_entries);

  filelist_cache_clear(&filelist->filelist_cache, filelist->filelist_cache.size);
  filelist->flags &= ~FL_NEED_FILTERING;

  MEM_freeN(filtered_tmp);
}

void filelist_setfilter_options(FileList *filelist,
                                const bool do_filter,
                                const bool hide_dot,
                                const bool hide_parent,
                                const uint64_t filter,
                                const uint64_t filter_id,
                                const bool filter_assets_only,
                                const char *filter_glob,
                                const char *filter_search)
{
  bool update = false;

  if (((filelist->filter_data.flags & FLF_DO_FILTER) != 0) != (do_filter != 0)) {
    filelist->filter_data.flags ^= FLF_DO_FILTER;
    update = true;
  }
  if (((filelist->filter_data.flags & FLF_HIDE_DOT) != 0) != (hide_dot != 0)) {
    filelist->filter_data.flags ^= FLF_HIDE_DOT;
    update = true;
  }
  if (((filelist->filter_data.flags & FLF_HIDE_PARENT) != 0) != (hide_parent != 0)) {
    filelist->filter_data.flags ^= FLF_HIDE_PARENT;
    update = true;
  }
  if (((filelist->filter_data.flags & FLF_ASSETS_ONLY) != 0) != (filter_assets_only != 0)) {
    filelist->filter_data.flags ^= FLF_ASSETS_ONLY;
    update = true;
  }
  if (filelist->filter_data.filter != filter) {
    filelist->filter_data.filter = filter;
    update = true;
  }
  const uint64_t new_filter_id = (filter & FILE_TYPE_BLENDERLIB) ? filter_id : FILTER_ID_ALL;
  if (filelist->filter_data.filter_id != new_filter_id) {
    filelist->filter_data.filter_id = new_filter_id;
    update = true;
  }
  if (!STREQ(filelist->filter_data.filter_glob, filter_glob)) {
    BLI_strncpy(
        filelist->filter_data.filter_glob, filter_glob, sizeof(filelist->filter_data.filter_glob));
    update = true;
  }
  if ((BLI_strcmp_ignore_pad(filelist->filter_data.filter_search, filter_search, '*') != 0)) {
    BLI_strncpy_ensure_pad(filelist->filter_data.filter_search,
                           filter_search,
                           '*',
                           sizeof(filelist->filter_data.filter_search));
    update = true;
  }

  if (update) {
    /* And now, free filtered data so that we know we have to filter again. */
    filelist_filter_clear(filelist);
  }
}

/**
 * Checks two libraries for equality.
 * \return True if the libraries match.
 */
static bool filelist_compare_asset_libraries(const FileSelectAssetLibraryUID *library_a,
                                             const FileSelectAssetLibraryUID *library_b)
{
  if (library_a->type != library_b->type) {
    return false;
  }
  if (library_a->type == FILE_ASSET_LIBRARY_CUSTOM) {
    /* Don't only check the index, also check that it's valid. */
    bUserAssetLibrary *library_ptr_a = BKE_preferences_asset_library_find_from_index(
        &U, library_a->custom_library_index);
    return (library_ptr_a != NULL) &&
           (library_a->custom_library_index == library_b->custom_library_index);
  }

  return true;
}

/**
 * \param asset_library: May be NULL to unset the library.
 */
void filelist_setlibrary(FileList *filelist, const FileSelectAssetLibraryUID *asset_library)
{
  /* Unset if needed. */
  if (!asset_library) {
    if (filelist->asset_library) {
      MEM_SAFE_FREE(filelist->asset_library);
      filelist->flags |= FL_FORCE_RESET;
    }
    return;
  }

  if (!filelist->asset_library) {
    filelist->asset_library = MEM_mallocN(sizeof(*filelist->asset_library),
                                          "filelist asset library");
    *filelist->asset_library = *asset_library;

    filelist->flags |= FL_FORCE_RESET;
  }
  else if (!filelist_compare_asset_libraries(filelist->asset_library, asset_library)) {
    *filelist->asset_library = *asset_library;
    filelist->flags |= FL_FORCE_RESET;
  }
}

/* ********** Icon/image helpers ********** */

void filelist_init_icons(void)
{
  short x, y, k;
  ImBuf *bbuf;
  ImBuf *ibuf;

  BLI_assert(G.background == false);

#ifdef WITH_HEADLESS
  bbuf = NULL;
#else
  bbuf = IMB_ibImageFromMemory(
      (const uchar *)datatoc_prvicons_png, datatoc_prvicons_png_size, IB_rect, NULL, "<splash>");
#endif
  if (bbuf) {
    for (y = 0; y < SPECIAL_IMG_ROWS; y++) {
      for (x = 0; x < SPECIAL_IMG_COLS; x++) {
        int tile = SPECIAL_IMG_COLS * y + x;
        if (tile < SPECIAL_IMG_MAX) {
          ibuf = IMB_allocImBuf(SPECIAL_IMG_SIZE, SPECIAL_IMG_SIZE, 32, IB_rect);
          for (k = 0; k < SPECIAL_IMG_SIZE; k++) {
            memcpy(&ibuf->rect[k * SPECIAL_IMG_SIZE],
                   &bbuf->rect[(k + y * SPECIAL_IMG_SIZE) * SPECIAL_IMG_SIZE * SPECIAL_IMG_COLS +
                               x * SPECIAL_IMG_SIZE],
                   SPECIAL_IMG_SIZE * sizeof(int));
          }
          gSpecialFileImages[tile] = ibuf;
        }
      }
    }
    IMB_freeImBuf(bbuf);
  }
}

void filelist_free_icons(void)
{
  BLI_assert(G.background == false);

  for (int i = 0; i < SPECIAL_IMG_MAX; i++) {
    IMB_freeImBuf(gSpecialFileImages[i]);
    gSpecialFileImages[i] = NULL;
  }
}

static FileDirEntry *filelist_geticon_get_file(struct FileList *filelist, const int index)
{
  BLI_assert(G.background == false);

  return filelist_file(filelist, index);
}

ImBuf *filelist_getimage(struct FileList *filelist, const int index)
{
  FileDirEntry *file = filelist_geticon_get_file(filelist, index);

  return file->preview_icon_id ? BKE_icon_imbuf_get_buffer(file->preview_icon_id) : NULL;
}

ImBuf *filelist_file_getimage(const FileDirEntry *file)
{
  return file->preview_icon_id ? BKE_icon_imbuf_get_buffer(file->preview_icon_id) : NULL;
}

static ImBuf *filelist_geticon_image_ex(FileDirEntry *file)
{
  ImBuf *ibuf = NULL;

  if (file->typeflag & FILE_TYPE_DIR) {
    if (FILENAME_IS_PARENT(file->relpath)) {
      ibuf = gSpecialFileImages[SPECIAL_IMG_PARENT];
    }
    else {
      ibuf = gSpecialFileImages[SPECIAL_IMG_FOLDER];
    }
  }
  else {
    ibuf = gSpecialFileImages[SPECIAL_IMG_DOCUMENT];
  }

  return ibuf;
}

ImBuf *filelist_geticon_image(struct FileList *filelist, const int index)
{
  FileDirEntry *file = filelist_geticon_get_file(filelist, index);
  return filelist_geticon_image_ex(file);
}

static int filelist_geticon_ex(const FileDirEntry *file,
                               const char *root,
                               const bool is_main,
                               const bool ignore_libdir)
{
  const eFileSel_File_Types typeflag = file->typeflag;

  if ((typeflag & FILE_TYPE_DIR) &&
      !(ignore_libdir && (typeflag & (FILE_TYPE_BLENDERLIB | FILE_TYPE_BLENDER)))) {
    if (FILENAME_IS_PARENT(file->relpath)) {
      return is_main ? ICON_FILE_PARENT : ICON_NONE;
    }
    if (typeflag & FILE_TYPE_APPLICATIONBUNDLE) {
      return ICON_UGLYPACKAGE;
    }
    if (typeflag & FILE_TYPE_BLENDER) {
      return ICON_FILE_BLEND;
    }
    if (is_main) {
      /* Do not return icon for folders if icons are not 'main' draw type
       * (e.g. when used over previews). */
      return (file->attributes & FILE_ATTR_ANY_LINK) ? ICON_FOLDER_REDIRECT : ICON_FILE_FOLDER;
    }

    /* If this path is in System list or path cache then use that icon. */
    struct FSMenu *fsmenu = ED_fsmenu_get();
    FSMenuCategory categories[] = {
        FS_CATEGORY_SYSTEM,
        FS_CATEGORY_SYSTEM_BOOKMARKS,
        FS_CATEGORY_OTHER,
    };

    for (int i = 0; i < ARRAY_SIZE(categories); i++) {
      FSMenuEntry *tfsm = ED_fsmenu_get_category(fsmenu, categories[i]);
      char fullpath[FILE_MAX_LIBEXTRA];
      char *target = fullpath;
      if (file->redirection_path) {
        target = file->redirection_path;
      }
      else if (root) {
        BLI_join_dirfile(fullpath, sizeof(fullpath), root, file->relpath);
        BLI_path_slash_ensure(fullpath);
      }
      for (; tfsm; tfsm = tfsm->next) {
        if (STREQ(tfsm->path, target)) {
          /* Never want a little folder inside a large one. */
          return (tfsm->icon == ICON_FILE_FOLDER) ? ICON_NONE : tfsm->icon;
        }
      }
    }

    if (file->attributes & FILE_ATTR_OFFLINE) {
      return ICON_ERROR;
    }
    if (file->attributes & FILE_ATTR_TEMPORARY) {
      return ICON_FILE_CACHE;
    }
    if (file->attributes & FILE_ATTR_SYSTEM) {
      return ICON_SYSTEM;
    }
  }

  if (typeflag & FILE_TYPE_BLENDER) {
    return ICON_FILE_BLEND;
  }
  if (typeflag & FILE_TYPE_BLENDER_BACKUP) {
    return ICON_FILE_BACKUP;
  }
  if (typeflag & FILE_TYPE_IMAGE) {
    return ICON_FILE_IMAGE;
  }
  if (typeflag & FILE_TYPE_MOVIE) {
    return ICON_FILE_MOVIE;
  }
  if (typeflag & FILE_TYPE_PYSCRIPT) {
    return ICON_FILE_SCRIPT;
  }
  if (typeflag & FILE_TYPE_SOUND) {
    return ICON_FILE_SOUND;
  }
  if (typeflag & FILE_TYPE_FTFONT) {
    return ICON_FILE_FONT;
  }
  if (typeflag & FILE_TYPE_BTX) {
    return ICON_FILE_BLANK;
  }
  if (typeflag & FILE_TYPE_COLLADA) {
    return ICON_FILE_3D;
  }
  if (typeflag & FILE_TYPE_ALEMBIC) {
    return ICON_FILE_3D;
  }
  if (typeflag & FILE_TYPE_USD) {
    return ICON_FILE_3D;
  }
  if (typeflag & FILE_TYPE_VOLUME) {
    return ICON_FILE_VOLUME;
  }
  if (typeflag & FILE_TYPE_OBJECT_IO) {
    return ICON_FILE_3D;
  }
  if (typeflag & FILE_TYPE_TEXT) {
    return ICON_FILE_TEXT;
  }
  if (typeflag & FILE_TYPE_ARCHIVE) {
    return ICON_FILE_ARCHIVE;
  }
  if (typeflag & FILE_TYPE_BLENDERLIB) {
    const int ret = UI_icon_from_idcode(file->blentype);
    if (ret != ICON_NONE) {
      return ret;
    }
  }
  return is_main ? ICON_FILE_BLANK : ICON_NONE;
}

int filelist_geticon(struct FileList *filelist, const int index, const bool is_main)
{
  FileDirEntry *file = filelist_geticon_get_file(filelist, index);

  return filelist_geticon_ex(file, filelist->filelist.root, is_main, false);
}

int ED_file_icon(const FileDirEntry *file)
{
  return file->preview_icon_id ? file->preview_icon_id :
                                 filelist_geticon_ex(file, NULL, false, false);
}

/* ********** Main ********** */

static void parent_dir_until_exists_or_default_root(char *dir)
{
  if (!BLI_path_parent_dir_until_exists(dir)) {
#ifdef WIN32
    BLI_windows_get_default_root_dir(dir);
#else
    strcpy(dir, "/");
#endif
  }
}

static bool filelist_checkdir_dir(struct FileList *UNUSED(filelist),
                                  char *r_dir,
                                  const bool do_change)
{
  if (do_change) {
    parent_dir_until_exists_or_default_root(r_dir);
    return true;
  }
  return BLI_is_dir(r_dir);
}

static bool filelist_checkdir_lib(struct FileList *UNUSED(filelist),
                                  char *r_dir,
                                  const bool do_change)
{
  char tdir[FILE_MAX_LIBEXTRA];
  char *name;

  const bool is_valid = (BLI_is_dir(r_dir) ||
                         (BLO_library_path_explode(r_dir, tdir, NULL, &name) &&
                          BLI_is_file(tdir) && !name));

  if (do_change && !is_valid) {
    /* if not a valid library, we need it to be a valid directory! */
    parent_dir_until_exists_or_default_root(r_dir);
    return true;
  }
  return is_valid;
}

static bool filelist_checkdir_main(struct FileList *filelist, char *r_dir, const bool do_change)
{
  /* TODO */
  return filelist_checkdir_lib(filelist, r_dir, do_change);
}

static bool filelist_checkdir_main_assets(struct FileList *UNUSED(filelist),
                                          char *UNUSED(r_dir),
                                          const bool UNUSED(do_change))
{
  /* Main is always valid. */
  return true;
}

static void filelist_entry_clear(FileDirEntry *entry)
{
  if (entry->name && ((entry->flags & FILE_ENTRY_NAME_FREE) != 0)) {
    MEM_freeN(entry->name);
  }
  if (entry->description) {
    MEM_freeN(entry->description);
  }
  if (entry->relpath) {
    MEM_freeN(entry->relpath);
  }
  if (entry->redirection_path) {
    MEM_freeN(entry->redirection_path);
  }
  if (entry->preview_icon_id) {
    BKE_icon_delete(entry->preview_icon_id);
    entry->preview_icon_id = 0;
  }
  /* For now, consider FileDirEntryRevision::poin as not owned here,
   * so no need to do anything about it */

  if (!BLI_listbase_is_empty(&entry->variants)) {
    FileDirEntryVariant *var;

    for (var = entry->variants.first; var; var = var->next) {
      if (var->name) {
        MEM_freeN(var->name);
      }
      if (var->description) {
        MEM_freeN(var->description);
      }

      if (!BLI_listbase_is_empty(&var->revisions)) {
        FileDirEntryRevision *rev;

        for (rev = var->revisions.first; rev; rev = rev->next) {
          if (rev->comment) {
            MEM_freeN(rev->comment);
          }
        }

        BLI_freelistN(&var->revisions);
      }
    }

    /* TODO: tags! */

    BLI_freelistN(&entry->variants);
  }
  else if (entry->entry) {
    MEM_freeN(entry->entry);
  }
}

static void filelist_entry_free(FileDirEntry *entry)
{
  filelist_entry_clear(entry);
  MEM_freeN(entry);
}

static void filelist_direntryarr_free(FileDirEntryArr *array)
{
#if 0
  FileDirEntry *entry, *entry_next;

  for (entry = array->entries.first; entry; entry = entry_next) {
    entry_next = entry->next;
    filelist_entry_free(entry);
  }
  BLI_listbase_clear(&array->entries);
#else
  BLI_assert(BLI_listbase_is_empty(&array->entries));
#endif
  array->nbr_entries = FILEDIR_NBR_ENTRIES_UNSET;
  array->nbr_entries_filtered = FILEDIR_NBR_ENTRIES_UNSET;
  array->entry_idx_start = -1;
  array->entry_idx_end = -1;
}

static void filelist_intern_entry_free(FileListInternEntry *entry)
{
  if (entry->relpath) {
    MEM_freeN(entry->relpath);
  }
  if (entry->redirection_path) {
    MEM_freeN(entry->redirection_path);
  }
  if (entry->name && entry->free_name) {
    MEM_freeN(entry->name);
  }
  /* If we own the asset-data (it was generated from external file data), free it. */
  if (entry->imported_asset_data) {
    BKE_asset_metadata_free(&entry->imported_asset_data);
  }
  MEM_freeN(entry);
}

static void filelist_intern_free(FileListIntern *filelist_intern)
{
  FileListInternEntry *entry, *entry_next;

  for (entry = filelist_intern->entries.first; entry; entry = entry_next) {
    entry_next = entry->next;
    filelist_intern_entry_free(entry);
  }
  BLI_listbase_clear(&filelist_intern->entries);

  MEM_SAFE_FREE(filelist_intern->filtered);
}

static void filelist_cache_preview_runf(TaskPool *__restrict pool, void *taskdata)
{
  FileListEntryCache *cache = BLI_task_pool_user_data(pool);
  FileListEntryPreviewTaskData *preview_taskdata = taskdata;
  FileListEntryPreview *preview = preview_taskdata->preview;

  ThumbSource source = 0;
  bool done = false;

  //  printf("%s: Start (%d)...\n", __func__, threadid);

  if (preview->in_memory_preview) {
    if (BKE_previewimg_is_finished(preview->in_memory_preview, ICON_SIZE_PREVIEW)) {
      ImBuf *imbuf = BKE_previewimg_to_imbuf(preview->in_memory_preview, ICON_SIZE_PREVIEW);
      preview->icon_id = BKE_icon_imbuf_create(imbuf);
      done = true;
    }
  }
  else {
    //  printf("%s: %d - %s - %p\n", __func__, preview->index, preview->path, preview->img);
    BLI_assert(preview->flags &
               (FILE_TYPE_IMAGE | FILE_TYPE_MOVIE | FILE_TYPE_FTFONT | FILE_TYPE_BLENDER |
                FILE_TYPE_BLENDER_BACKUP | FILE_TYPE_BLENDERLIB));

    if (preview->flags & FILE_TYPE_IMAGE) {
      source = THB_SOURCE_IMAGE;
    }
    else if (preview->flags &
             (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP | FILE_TYPE_BLENDERLIB)) {
      source = THB_SOURCE_BLEND;
    }
    else if (preview->flags & FILE_TYPE_MOVIE) {
      source = THB_SOURCE_MOVIE;
    }
    else if (preview->flags & FILE_TYPE_FTFONT) {
      source = THB_SOURCE_FONT;
    }

    IMB_thumb_path_lock(preview->path);
    /* Always generate biggest preview size for now, it's simpler and avoids having to re-generate
     * in case user switch to a bigger preview size. */
    ImBuf *imbuf = IMB_thumb_manage(preview->path, THB_LARGE, source);
    IMB_thumb_path_unlock(preview->path);
    if (imbuf) {
      preview->icon_id = BKE_icon_imbuf_create(imbuf);
    }

    done = true;
  }

  if (done) {
    /* That way task freeing function won't free th preview, since it does not own it anymore. */
    atomic_cas_ptr((void **)&preview_taskdata->preview, preview, NULL);
    BLI_thread_queue_push(cache->previews_done, preview);
  }

  //  printf("%s: End (%d)...\n", __func__, threadid);
}

static void filelist_cache_preview_freef(TaskPool *__restrict UNUSED(pool), void *taskdata)
{
  FileListEntryPreviewTaskData *preview_taskdata = taskdata;
  FileListEntryPreview *preview = preview_taskdata->preview;

  /* preview_taskdata->preview is atomically set to NULL once preview has been processed and sent
   * to previews_done queue. */
  if (preview != NULL) {
    if (preview->icon_id) {
      BKE_icon_delete(preview->icon_id);
    }
    MEM_freeN(preview);
  }
  MEM_freeN(preview_taskdata);
}

static void filelist_cache_preview_ensure_running(FileListEntryCache *cache)
{
  if (!cache->previews_pool) {
    cache->previews_pool = BLI_task_pool_create_background(cache, TASK_PRIORITY_LOW);
    cache->previews_done = BLI_thread_queue_init();

    IMB_thumb_locks_acquire();
  }
}

static void filelist_cache_previews_clear(FileListEntryCache *cache)
{
  if (cache->previews_pool) {
    BLI_task_pool_cancel(cache->previews_pool);

    FileListEntryPreview *preview;
    while ((preview = BLI_thread_queue_pop_timeout(cache->previews_done, 0))) {
      // printf("%s: DONE %d - %s - %p\n", __func__, preview->index, preview->path,
      // preview->img);
      if (preview->icon_id) {
        BKE_icon_delete(preview->icon_id);
      }
      MEM_freeN(preview);
    }
  }
}

static void filelist_cache_previews_free(FileListEntryCache *cache)
{
  if (cache->previews_pool) {
    BLI_thread_queue_nowait(cache->previews_done);

    filelist_cache_previews_clear(cache);

    BLI_thread_queue_free(cache->previews_done);
    BLI_task_pool_free(cache->previews_pool);
    cache->previews_pool = NULL;
    cache->previews_done = NULL;

    IMB_thumb_locks_release();
  }

  cache->flags &= ~FLC_PREVIEWS_ACTIVE;
}

static void filelist_cache_previews_push(FileList *filelist, FileDirEntry *entry, const int index)
{
  FileListEntryCache *cache = &filelist->filelist_cache;

  BLI_assert(cache->flags & FLC_PREVIEWS_ACTIVE);

  if (!entry->preview_icon_id && !(entry->flags & FILE_ENTRY_INVALID_PREVIEW) &&
      (entry->typeflag & (FILE_TYPE_IMAGE | FILE_TYPE_MOVIE | FILE_TYPE_FTFONT |
                          FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP | FILE_TYPE_BLENDERLIB))) {
    FileListEntryPreview *preview = MEM_mallocN(sizeof(*preview), __func__);
    FileListInternEntry *intern_entry = filelist->filelist_intern.filtered[index];

    if (entry->redirection_path) {
      BLI_strncpy(preview->path, entry->redirection_path, FILE_MAXDIR);
    }
    else {
      BLI_join_dirfile(
          preview->path, sizeof(preview->path), filelist->filelist.root, entry->relpath);
    }

    preview->index = index;
    preview->flags = entry->typeflag;
    preview->in_memory_preview = intern_entry->local_data.preview_image;
    preview->icon_id = 0;
    //      printf("%s: %d - %s - %p\n", __func__, preview->index, preview->path, preview->img);

    filelist_cache_preview_ensure_running(cache);

    FileListEntryPreviewTaskData *preview_taskdata = MEM_mallocN(sizeof(*preview_taskdata),
                                                                 __func__);
    preview_taskdata->preview = preview;
    BLI_task_pool_push(cache->previews_pool,
                       filelist_cache_preview_runf,
                       preview_taskdata,
                       true,
                       filelist_cache_preview_freef);
  }
}

static void filelist_cache_init(FileListEntryCache *cache, size_t cache_size)
{
  BLI_listbase_clear(&cache->cached_entries);

  cache->block_cursor = cache->block_start_index = cache->block_center_index =
      cache->block_end_index = 0;
  cache->block_entries = MEM_mallocN(sizeof(*cache->block_entries) * cache_size, __func__);

  cache->misc_entries = BLI_ghash_ptr_new_ex(__func__, cache_size);
  cache->misc_entries_indices = MEM_mallocN(sizeof(*cache->misc_entries_indices) * cache_size,
                                            __func__);
  copy_vn_i(cache->misc_entries_indices, cache_size, -1);
  cache->misc_cursor = 0;

  /* XXX This assumes uint is 32 bits and uuid is 128 bits (char[16]), be careful! */
  cache->uuids = BLI_ghash_new_ex(
      BLI_ghashutil_uinthash_v4_p, BLI_ghashutil_uinthash_v4_cmp, __func__, cache_size * 2);

  cache->size = cache_size;
  cache->flags = FLC_IS_INIT;

  /* We cannot translate from non-main thread, so init translated strings once from here. */
  IMB_thumb_ensure_translations();
}

static void filelist_cache_free(FileListEntryCache *cache)
{
  FileDirEntry *entry, *entry_next;

  if (!(cache->flags & FLC_IS_INIT)) {
    return;
  }

  filelist_cache_previews_free(cache);

  MEM_freeN(cache->block_entries);

  BLI_ghash_free(cache->misc_entries, NULL, NULL);
  MEM_freeN(cache->misc_entries_indices);

  BLI_ghash_free(cache->uuids, NULL, NULL);

  for (entry = cache->cached_entries.first; entry; entry = entry_next) {
    entry_next = entry->next;
    filelist_entry_free(entry);
  }
  BLI_listbase_clear(&cache->cached_entries);
}

static void filelist_cache_clear(FileListEntryCache *cache, size_t new_size)
{
  FileDirEntry *entry, *entry_next;

  if (!(cache->flags & FLC_IS_INIT)) {
    return;
  }

  filelist_cache_previews_clear(cache);

  cache->block_cursor = cache->block_start_index = cache->block_center_index =
      cache->block_end_index = 0;
  if (new_size != cache->size) {
    cache->block_entries = MEM_reallocN(cache->block_entries,
                                        sizeof(*cache->block_entries) * new_size);
  }

  BLI_ghash_clear_ex(cache->misc_entries, NULL, NULL, new_size);
  if (new_size != cache->size) {
    cache->misc_entries_indices = MEM_reallocN(cache->misc_entries_indices,
                                               sizeof(*cache->misc_entries_indices) * new_size);
  }
  copy_vn_i(cache->misc_entries_indices, new_size, -1);

  BLI_ghash_clear_ex(cache->uuids, NULL, NULL, new_size * 2);

  cache->size = new_size;

  for (entry = cache->cached_entries.first; entry; entry = entry_next) {
    entry_next = entry->next;
    filelist_entry_free(entry);
  }
  BLI_listbase_clear(&cache->cached_entries);
}

FileList *filelist_new(short type)
{
  FileList *p = MEM_callocN(sizeof(*p), __func__);

  filelist_cache_init(&p->filelist_cache, FILELIST_ENTRYCACHESIZE_DEFAULT);

  p->selection_state = BLI_ghash_new(
      BLI_ghashutil_uinthash_v4_p, BLI_ghashutil_uinthash_v4_cmp, __func__);
  p->filelist.nbr_entries = FILEDIR_NBR_ENTRIES_UNSET;
  filelist_settype(p, type);

  return p;
}

void filelist_settype(FileList *filelist, short type)
{
  if (filelist->type == type) {
    return;
  }

  filelist->type = type;
  filelist->tags = 0;
  switch (filelist->type) {
    case FILE_MAIN:
      filelist->check_dir_fn = filelist_checkdir_main;
      filelist->read_job_fn = filelist_readjob_main;
      filelist->filter_fn = is_filtered_main;
      break;
    case FILE_LOADLIB:
      filelist->check_dir_fn = filelist_checkdir_lib;
      filelist->read_job_fn = filelist_readjob_lib;
      filelist->filter_fn = is_filtered_lib;
      break;
    case FILE_MAIN_ASSET:
      filelist->check_dir_fn = filelist_checkdir_main_assets;
      filelist->read_job_fn = filelist_readjob_main_assets;
      filelist->filter_fn = is_filtered_main_assets;
      filelist->tags |= FILELIST_TAGS_USES_MAIN_DATA | FILELIST_TAGS_NO_THREADS;
      break;
    default:
      filelist->check_dir_fn = filelist_checkdir_dir;
      filelist->read_job_fn = filelist_readjob_dir;
      filelist->filter_fn = is_filtered_file;
      break;
  }

  filelist->flags |= FL_FORCE_RESET;
}

void filelist_clear_ex(struct FileList *filelist, const bool do_cache, const bool do_selection)
{
  if (!filelist) {
    return;
  }

  filelist_filter_clear(filelist);

  if (do_cache) {
    filelist_cache_clear(&filelist->filelist_cache, filelist->filelist_cache.size);
  }

  filelist_intern_free(&filelist->filelist_intern);

  filelist_direntryarr_free(&filelist->filelist);

  if (do_selection && filelist->selection_state) {
    BLI_ghash_clear(filelist->selection_state, MEM_freeN, NULL);
  }
}

void filelist_clear(struct FileList *filelist)
{
  filelist_clear_ex(filelist, true, true);
}

void filelist_free(struct FileList *filelist)
{
  if (!filelist) {
    printf("Attempting to delete empty filelist.\n");
    return;
  }

  /* No need to clear cache & selection_state, we free them anyway. */
  filelist_clear_ex(filelist, false, false);
  filelist_cache_free(&filelist->filelist_cache);

  if (filelist->selection_state) {
    BLI_ghash_free(filelist->selection_state, MEM_freeN, NULL);
    filelist->selection_state = NULL;
  }

  MEM_SAFE_FREE(filelist->asset_library);

  memset(&filelist->filter_data, 0, sizeof(filelist->filter_data));

  filelist->flags &= ~(FL_NEED_SORTING | FL_NEED_FILTERING);
}

void filelist_freelib(struct FileList *filelist)
{
  if (filelist->libfiledata) {
    BLO_blendhandle_close(filelist->libfiledata);
  }
  filelist->libfiledata = NULL;
}

BlendHandle *filelist_lib(struct FileList *filelist)
{
  return filelist->libfiledata;
}

static const char *fileentry_uiname(const char *root,
                                    const char *relpath,
                                    const eFileSel_File_Types typeflag,
                                    char *buff)
{
  char *name = NULL;

  if (typeflag & FILE_TYPE_BLENDERLIB) {
    char abspath[FILE_MAX_LIBEXTRA];
    char *group;

    BLI_join_dirfile(abspath, sizeof(abspath), root, relpath);
    BLO_library_path_explode(abspath, buff, &group, &name);
    if (!name) {
      name = group;
    }
  }
  /* Depending on platforms, 'my_file.blend/..' might be viewed as dir or not... */
  if (!name) {
    if (typeflag & FILE_TYPE_DIR) {
      name = (char *)relpath;
    }
    else {
      name = (char *)BLI_path_basename(relpath);
    }
  }
  BLI_assert(name);

  return name;
}

const char *filelist_dir(struct FileList *filelist)
{
  return filelist->filelist.root;
}

bool filelist_is_dir(struct FileList *filelist, const char *path)
{
  return filelist->check_dir_fn(filelist, (char *)path, false);
}

/**
 * May modify in place given r_dir, which is expected to be FILE_MAX_LIBEXTRA length.
 */
void filelist_setdir(struct FileList *filelist, char *r_dir)
{
  const bool allow_invalid = filelist->asset_library != NULL;
  BLI_assert(strlen(r_dir) < FILE_MAX_LIBEXTRA);

  BLI_path_normalize_dir(BKE_main_blendfile_path_from_global(), r_dir);
  const bool is_valid_path = filelist->check_dir_fn(filelist, r_dir, !allow_invalid);
  BLI_assert(is_valid_path || allow_invalid);
  UNUSED_VARS_NDEBUG(is_valid_path);

  if (!STREQ(filelist->filelist.root, r_dir)) {
    BLI_strncpy(filelist->filelist.root, r_dir, sizeof(filelist->filelist.root));
    filelist->flags |= FL_FORCE_RESET;
  }
}

void filelist_setrecursion(struct FileList *filelist, const int recursion_level)
{
  if (filelist->max_recursion != recursion_level) {
    filelist->max_recursion = recursion_level;
    filelist->flags |= FL_FORCE_RESET;
  }
}

bool filelist_needs_force_reset(FileList *filelist)
{
  return (filelist->flags & FL_FORCE_RESET) != 0;
}

void filelist_tag_force_reset(FileList *filelist)
{
  filelist->flags |= FL_FORCE_RESET;
}

bool filelist_is_ready(struct FileList *filelist)
{
  return (filelist->flags & FL_IS_READY) != 0;
}

bool filelist_pending(struct FileList *filelist)
{
  return (filelist->flags & FL_IS_PENDING) != 0;
}

bool filelist_needs_reset_on_main_changes(const FileList *filelist)
{
  return (filelist->tags & FILELIST_TAGS_USES_MAIN_DATA) != 0;
}

/**
 * Limited version of full update done by space_file's file_refresh(),
 * to be used by operators and such.
 * Ensures given filelist is ready to be used (i.e. it is filtered and sorted),
 * unless it is tagged for a full refresh.
 */
int filelist_files_ensure(FileList *filelist)
{
  if (!filelist_needs_force_reset(filelist) || !filelist_needs_reading(filelist)) {
    filelist_sort(filelist);
    filelist_filter(filelist);
  }

  return filelist->filelist.nbr_entries_filtered;
}

static FileDirEntry *filelist_file_create_entry(FileList *filelist, const int index)
{
  FileListInternEntry *entry = filelist->filelist_intern.filtered[index];
  FileListEntryCache *cache = &filelist->filelist_cache;
  FileDirEntry *ret;
  FileDirEntryRevision *rev;

  ret = MEM_callocN(sizeof(*ret), __func__);
  rev = MEM_callocN(sizeof(*rev), __func__);

  rev->size = (uint64_t)entry->st.st_size;

  rev->time = (int64_t)entry->st.st_mtime;

  ret->entry = rev;
  ret->relpath = BLI_strdup(entry->relpath);
  if (entry->free_name) {
    ret->name = BLI_strdup(entry->name);
    ret->flags |= FILE_ENTRY_NAME_FREE;
  }
  else {
    ret->name = entry->name;
  }
  ret->description = BLI_strdupcat(filelist->filelist.root, entry->relpath);
  memcpy(ret->uuid, entry->uuid, sizeof(ret->uuid));
  ret->blentype = entry->blentype;
  ret->typeflag = entry->typeflag;
  ret->attributes = entry->attributes;
  if (entry->redirection_path) {
    ret->redirection_path = BLI_strdup(entry->redirection_path);
  }
  ret->id = entry->local_data.id;
  ret->asset_data = entry->imported_asset_data ? entry->imported_asset_data : NULL;
  if (ret->id && (ret->asset_data == NULL)) {
    ret->asset_data = ret->id->asset_data;
  }
  /* For some file types the preview is already available. */
  if (entry->local_data.preview_image &&
      BKE_previewimg_is_finished(entry->local_data.preview_image, ICON_SIZE_PREVIEW)) {
    ImBuf *ibuf = BKE_previewimg_to_imbuf(entry->local_data.preview_image, ICON_SIZE_PREVIEW);
    ret->preview_icon_id = BKE_icon_imbuf_create(ibuf);
  }
  BLI_addtail(&cache->cached_entries, ret);
  return ret;
}

static void filelist_file_release_entry(FileList *filelist, FileDirEntry *entry)
{
  BLI_remlink(&filelist->filelist_cache.cached_entries, entry);
  filelist_entry_free(entry);
}

FileDirEntry *filelist_file_ex(struct FileList *filelist, const int index, const bool use_request)
{
  FileDirEntry *ret = NULL, *old;
  FileListEntryCache *cache = &filelist->filelist_cache;
  const size_t cache_size = cache->size;
  int old_index;

  if ((index < 0) || (index >= filelist->filelist.nbr_entries_filtered)) {
    return ret;
  }

  if (index >= cache->block_start_index && index < cache->block_end_index) {
    const int idx = (index - cache->block_start_index + cache->block_cursor) % cache_size;
    return cache->block_entries[idx];
  }

  if ((ret = BLI_ghash_lookup(cache->misc_entries, POINTER_FROM_INT(index)))) {
    return ret;
  }

  if (!use_request) {
    return NULL;
  }

  //  printf("requesting file %d (not yet cached)\n", index);

  /* Else, we have to add new entry to 'misc' cache - and possibly make room for it first! */
  ret = filelist_file_create_entry(filelist, index);
  old_index = cache->misc_entries_indices[cache->misc_cursor];
  if ((old = BLI_ghash_popkey(cache->misc_entries, POINTER_FROM_INT(old_index), NULL))) {
    BLI_ghash_remove(cache->uuids, old->uuid, NULL, NULL);
    filelist_file_release_entry(filelist, old);
  }
  BLI_ghash_insert(cache->misc_entries, POINTER_FROM_INT(index), ret);
  BLI_ghash_insert(cache->uuids, ret->uuid, ret);

  cache->misc_entries_indices[cache->misc_cursor] = index;
  cache->misc_cursor = (cache->misc_cursor + 1) % cache_size;

#if 0 /* Actually no, only block cached entries should have preview imho. */
  if (cache->previews_pool) {
    filelist_cache_previews_push(filelist, ret, index);
  }
#endif

  return ret;
}

FileDirEntry *filelist_file(struct FileList *filelist, int index)
{
  return filelist_file_ex(filelist, index, true);
}

int filelist_file_findpath(struct FileList *filelist, const char *filename)
{
  int fidx = -1;

  if (filelist->filelist.nbr_entries_filtered == FILEDIR_NBR_ENTRIES_UNSET) {
    return fidx;
  }

  /* XXX TODO Cache could probably use a ghash on paths too? Not really urgent though.
   *          This is only used to find again renamed entry,
   *          annoying but looks hairy to get rid of it currently. */

  for (fidx = 0; fidx < filelist->filelist.nbr_entries_filtered; fidx++) {
    FileListInternEntry *entry = filelist->filelist_intern.filtered[fidx];
    if (STREQ(entry->relpath, filename)) {
      return fidx;
    }
  }

  return -1;
}

/**
 * Get the ID a file represents (if any). For #FILE_MAIN, #FILE_MAIN_ASSET.
 */
ID *filelist_file_get_id(const FileDirEntry *file)
{
  return file->id;
}

FileDirEntry *filelist_entry_find_uuid(struct FileList *filelist, const int uuid[4])
{
  if (filelist->filelist.nbr_entries_filtered == FILEDIR_NBR_ENTRIES_UNSET) {
    return NULL;
  }

  if (filelist->filelist_cache.uuids) {
    FileDirEntry *entry = BLI_ghash_lookup(filelist->filelist_cache.uuids, uuid);
    if (entry) {
      return entry;
    }
  }

  {
    int fidx;

    for (fidx = 0; fidx < filelist->filelist.nbr_entries_filtered; fidx++) {
      FileListInternEntry *entry = filelist->filelist_intern.filtered[fidx];
      if (memcmp(entry->uuid, uuid, sizeof(entry->uuid)) == 0) {
        return filelist_file(filelist, fidx);
      }
    }
  }

  return NULL;
}

void filelist_file_cache_slidingwindow_set(FileList *filelist, size_t window_size)
{
  /* Always keep it power of 2, in [256, 8192] range for now,
   * cache being app. twice bigger than requested window. */
  size_t size = 256;
  window_size *= 2;

  while (size < window_size && size < 8192) {
    size *= 2;
  }

  if (size != filelist->filelist_cache.size) {
    filelist_cache_clear(&filelist->filelist_cache, size);
  }
}

/* Helpers, low-level, they assume cursor + size <= cache_size */
static bool filelist_file_cache_block_create(FileList *filelist,
                                             const int start_index,
                                             const int size,
                                             int cursor)
{
  FileListEntryCache *cache = &filelist->filelist_cache;

  {
    int i, idx;

    for (i = 0, idx = start_index; i < size; i++, idx++, cursor++) {
      FileDirEntry *entry;

      /* That entry might have already been requested and stored in misc cache... */
      if ((entry = BLI_ghash_popkey(cache->misc_entries, POINTER_FROM_INT(idx), NULL)) == NULL) {
        entry = filelist_file_create_entry(filelist, idx);
        BLI_ghash_insert(cache->uuids, entry->uuid, entry);
      }
      cache->block_entries[cursor] = entry;
    }
    return true;
  }

  return false;
}

static void filelist_file_cache_block_release(struct FileList *filelist,
                                              const int size,
                                              int cursor)
{
  FileListEntryCache *cache = &filelist->filelist_cache;

  {
    int i;

    for (i = 0; i < size; i++, cursor++) {
      FileDirEntry *entry = cache->block_entries[cursor];
#if 0
      printf("%s: release cacheidx %d (%%p %%s)\n",
             __func__,
             cursor /*, cache->block_entries[cursor], cache->block_entries[cursor]->relpath*/);
#endif
      BLI_ghash_remove(cache->uuids, entry->uuid, NULL, NULL);
      filelist_file_release_entry(filelist, entry);
#ifndef NDEBUG
      cache->block_entries[cursor] = NULL;
#endif
    }
  }
}

/* Load in cache all entries "around" given index (as much as block cache may hold). */
bool filelist_file_cache_block(struct FileList *filelist, const int index)
{
  FileListEntryCache *cache = &filelist->filelist_cache;
  const size_t cache_size = cache->size;

  const int nbr_entries = filelist->filelist.nbr_entries_filtered;
  int start_index = max_ii(0, index - (cache_size / 2));
  int end_index = min_ii(nbr_entries, index + (cache_size / 2));
  int i;
  const bool full_refresh = (filelist->flags & FL_IS_READY) == 0;

  if ((index < 0) || (index >= nbr_entries)) {
    //      printf("Wrong index %d ([%d:%d])", index, 0, nbr_entries);
    return false;
  }

  /* Maximize cached range! */
  if ((end_index - start_index) < cache_size) {
    if (start_index == 0) {
      end_index = min_ii(nbr_entries, start_index + cache_size);
    }
    else if (end_index == nbr_entries) {
      start_index = max_ii(0, end_index - cache_size);
    }
  }

  BLI_assert((end_index - start_index) <= cache_size);

  //  printf("%s: [%d:%d] around index %d (current cache: [%d:%d])\n", __func__,
  //         start_index, end_index, index, cache->block_start_index, cache->block_end_index);

  /* If we have something to (re)cache... */
  if (full_refresh || (start_index != cache->block_start_index) ||
      (end_index != cache->block_end_index)) {
    if (full_refresh || (start_index >= cache->block_end_index) ||
        (end_index <= cache->block_start_index)) {
      int size1 = cache->block_end_index - cache->block_start_index;
      int size2 = 0;
      int idx1 = cache->block_cursor, idx2 = 0;

      //          printf("Full Recaching!\n");

      if (cache->flags & FLC_PREVIEWS_ACTIVE) {
        filelist_cache_previews_clear(cache);
      }

      if (idx1 + size1 > cache_size) {
        size2 = idx1 + size1 - cache_size;
        size1 -= size2;
        filelist_file_cache_block_release(filelist, size2, idx2);
      }
      filelist_file_cache_block_release(filelist, size1, idx1);

      cache->block_start_index = cache->block_end_index = cache->block_cursor = 0;

      /* New cached block does not overlap existing one, simple. */
      if (!filelist_file_cache_block_create(filelist, start_index, end_index - start_index, 0)) {
        return false;
      }

      cache->block_start_index = start_index;
      cache->block_end_index = end_index;
    }
    else {
      //          printf("Partial Recaching!\n");

      /* At this point, we know we keep part of currently cached entries, so update previews
       * if needed, and remove everything from working queue - we'll add all newly needed
       * entries at the end. */
      if (cache->flags & FLC_PREVIEWS_ACTIVE) {
        filelist_cache_previews_update(filelist);
        filelist_cache_previews_clear(cache);
      }

      //          printf("\tpreview cleaned up...\n");

      if (start_index > cache->block_start_index) {
        int size1 = start_index - cache->block_start_index;
        int size2 = 0;
        int idx1 = cache->block_cursor, idx2 = 0;

        //              printf("\tcache releasing: [%d:%d] (%d, %d)\n",
        //                     cache->block_start_index, cache->block_start_index + size1,
        //                     cache->block_cursor, size1);

        if (idx1 + size1 > cache_size) {
          size2 = idx1 + size1 - cache_size;
          size1 -= size2;
          filelist_file_cache_block_release(filelist, size2, idx2);
        }
        filelist_file_cache_block_release(filelist, size1, idx1);

        cache->block_cursor = (idx1 + size1 + size2) % cache_size;
        cache->block_start_index = start_index;
      }
      if (end_index < cache->block_end_index) {
        int size1 = cache->block_end_index - end_index;
        int size2 = 0;
        int idx1, idx2 = 0;

#if 0
        printf("\tcache releasing: [%d:%d] (%d)\n",
               cache->block_end_index - size1,
               cache->block_end_index,
               cache->block_cursor);
#endif

        idx1 = (cache->block_cursor + end_index - cache->block_start_index) % cache_size;
        if (idx1 + size1 > cache_size) {
          size2 = idx1 + size1 - cache_size;
          size1 -= size2;
          filelist_file_cache_block_release(filelist, size2, idx2);
        }
        filelist_file_cache_block_release(filelist, size1, idx1);

        cache->block_end_index = end_index;
      }

      //          printf("\tcache cleaned up...\n");

      if (start_index < cache->block_start_index) {
        /* Add (request) needed entries before already cached ones. */
        /* Note: We need some index black magic to wrap around (cycle)
         * inside our cache_size array... */
        int size1 = cache->block_start_index - start_index;
        int size2 = 0;
        int idx1, idx2;

        if (size1 > cache->block_cursor) {
          size2 = size1;
          size1 -= cache->block_cursor;
          size2 -= size1;
          idx2 = 0;
          idx1 = cache_size - size1;
        }
        else {
          idx1 = cache->block_cursor - size1;
        }

        if (size2) {
          if (!filelist_file_cache_block_create(filelist, start_index + size1, size2, idx2)) {
            return false;
          }
        }
        if (!filelist_file_cache_block_create(filelist, start_index, size1, idx1)) {
          return false;
        }

        cache->block_cursor = idx1;
        cache->block_start_index = start_index;
      }
      //          printf("\tstart-extended...\n");
      if (end_index > cache->block_end_index) {
        /* Add (request) needed entries after already cached ones. */
        /* Note: We need some index black magic to wrap around (cycle)
         * inside our cache_size array... */
        int size1 = end_index - cache->block_end_index;
        int size2 = 0;
        int idx1, idx2;

        idx1 = (cache->block_cursor + end_index - cache->block_start_index - size1) % cache_size;
        if ((idx1 + size1) > cache_size) {
          size2 = size1;
          size1 = cache_size - idx1;
          size2 -= size1;
          idx2 = 0;
        }

        if (size2) {
          if (!filelist_file_cache_block_create(filelist, end_index - size2, size2, idx2)) {
            return false;
          }
        }
        if (!filelist_file_cache_block_create(filelist, end_index - size1 - size2, size1, idx1)) {
          return false;
        }

        cache->block_end_index = end_index;
      }

      //          printf("\tend-extended...\n");
    }
  }
  else if ((cache->block_center_index != index) && (cache->flags & FLC_PREVIEWS_ACTIVE)) {
    /* We try to always preview visible entries first, so 'restart' preview background task. */
    filelist_cache_previews_update(filelist);
    filelist_cache_previews_clear(cache);
  }

  //  printf("Re-queueing previews...\n");

  /* Note we try to preview first images around given index - i.e. assumed visible ones. */
  if (cache->flags & FLC_PREVIEWS_ACTIVE) {
    for (i = 0; ((index + i) < end_index) || ((index - i) >= start_index); i++) {
      if ((index - i) >= start_index) {
        const int idx = (cache->block_cursor + (index - start_index) - i) % cache_size;
        filelist_cache_previews_push(filelist, cache->block_entries[idx], index - i);
      }
      if ((index + i) < end_index) {
        const int idx = (cache->block_cursor + (index - start_index) + i) % cache_size;
        filelist_cache_previews_push(filelist, cache->block_entries[idx], index + i);
      }
    }
  }

  cache->block_center_index = index;

  //  printf("%s Finished!\n", __func__);

  return true;
}

void filelist_cache_previews_set(FileList *filelist, const bool use_previews)
{
  FileListEntryCache *cache = &filelist->filelist_cache;

  if (use_previews == ((cache->flags & FLC_PREVIEWS_ACTIVE) != 0)) {
    return;
  }
  /* Do not start preview work while listing, gives nasty flickering! */
  if (use_previews && (filelist->flags & FL_IS_READY)) {
    cache->flags |= FLC_PREVIEWS_ACTIVE;

    BLI_assert((cache->previews_pool == NULL) && (cache->previews_done == NULL));

    //      printf("%s: Init Previews...\n", __func__);

    /* No need to populate preview queue here, filelist_file_cache_block() handles this. */
  }
  else {
    //      printf("%s: Clear Previews...\n", __func__);

    filelist_cache_previews_free(cache);
  }
}

bool filelist_cache_previews_update(FileList *filelist)
{
  FileListEntryCache *cache = &filelist->filelist_cache;
  TaskPool *pool = cache->previews_pool;
  bool changed = false;

  if (!pool) {
    return changed;
  }

  //  printf("%s: Update Previews...\n", __func__);

  while (!BLI_thread_queue_is_empty(cache->previews_done)) {
    FileListEntryPreview *preview = BLI_thread_queue_pop(cache->previews_done);
    FileDirEntry *entry;

    /* Paranoid (should never happen currently
     * since we consume this queue from a single thread), but... */
    if (!preview) {
      continue;
    }
    /* entry might have been removed from cache in the mean time,
     * we do not want to cache it again here. */
    entry = filelist_file_ex(filelist, preview->index, false);

    //      printf("%s: %d - %s - %p\n", __func__, preview->index, preview->path, preview->img);

    if (preview->icon_id) {
      /* Due to asynchronous process, a preview for a given image may be generated several times,
       * i.e. entry->image may already be set at this point. */
      if (entry && !entry->preview_icon_id) {
        /* Move ownership over icon. */
        entry->preview_icon_id = preview->icon_id;
        preview->icon_id = 0;
        changed = true;
      }
      else {
        BKE_icon_delete(preview->icon_id);
      }
    }
    else if (entry) {
      /* We want to avoid re-processing this entry continuously!
       * Note that, since entries only live in cache,
       * preview will be retried quite often anyway. */
      entry->flags |= FILE_ENTRY_INVALID_PREVIEW;
    }

    MEM_freeN(preview);
  }

  return changed;
}

bool filelist_cache_previews_running(FileList *filelist)
{
  FileListEntryCache *cache = &filelist->filelist_cache;

  return (cache->previews_pool != NULL);
}

/* would recognize .blend as well */
static bool file_is_blend_backup(const char *str)
{
  const size_t a = strlen(str);
  size_t b = 7;
  bool retval = 0;

  if (a == 0 || b >= a) {
    /* pass */
  }
  else {
    const char *loc;

    if (a > b + 1) {
      b++;
    }

    /* allow .blend1 .blend2 .blend32 */
    loc = BLI_strcasestr(str + a - b, ".blend");

    if (loc) {
      retval = 1;
    }
  }

  return retval;
}

/* TODO: Maybe we should move this to BLI?
 * On the other hand, it's using defines from space-file area, so not sure... */
int ED_path_extension_type(const char *path)
{
  if (BLO_has_bfile_extension(path)) {
    return FILE_TYPE_BLENDER;
  }
  if (file_is_blend_backup(path)) {
    return FILE_TYPE_BLENDER_BACKUP;
  }
  if (BLI_path_extension_check(path, ".app")) {
    return FILE_TYPE_APPLICATIONBUNDLE;
  }
  if (BLI_path_extension_check(path, ".py")) {
    return FILE_TYPE_PYSCRIPT;
  }
  if (BLI_path_extension_check_n(path,
                                 ".txt",
                                 ".glsl",
                                 ".osl",
                                 ".data",
                                 ".pov",
                                 ".ini",
                                 ".mcr",
                                 ".inc",
                                 ".fountain",
                                 NULL)) {
    return FILE_TYPE_TEXT;
  }
  if (BLI_path_extension_check_n(path, ".ttf", ".ttc", ".pfb", ".otf", ".otc", NULL)) {
    return FILE_TYPE_FTFONT;
  }
  if (BLI_path_extension_check(path, ".btx")) {
    return FILE_TYPE_BTX;
  }
  if (BLI_path_extension_check(path, ".dae")) {
    return FILE_TYPE_COLLADA;
  }
  if (BLI_path_extension_check(path, ".abc")) {
    return FILE_TYPE_ALEMBIC;
  }
  if (BLI_path_extension_check_n(path, ".usd", ".usda", ".usdc", NULL)) {
    return FILE_TYPE_USD;
  }
  if (BLI_path_extension_check(path, ".vdb")) {
    return FILE_TYPE_VOLUME;
  }
  if (BLI_path_extension_check(path, ".zip")) {
    return FILE_TYPE_ARCHIVE;
  }
  if (BLI_path_extension_check_n(path, ".obj", ".3ds", ".fbx", ".glb", ".gltf", NULL)) {
    return FILE_TYPE_OBJECT_IO;
  }
  if (BLI_path_extension_check_array(path, imb_ext_image)) {
    return FILE_TYPE_IMAGE;
  }
  if (BLI_path_extension_check(path, ".ogg")) {
    if (IMB_isanim(path)) {
      return FILE_TYPE_MOVIE;
    }
    return FILE_TYPE_SOUND;
  }
  if (BLI_path_extension_check_array(path, imb_ext_movie)) {
    return FILE_TYPE_MOVIE;
  }
  if (BLI_path_extension_check_array(path, imb_ext_audio)) {
    return FILE_TYPE_SOUND;
  }
  return 0;
}

int ED_file_extension_icon(const char *path)
{
  const int type = ED_path_extension_type(path);

  switch (type) {
    case FILE_TYPE_BLENDER:
      return ICON_FILE_BLEND;
    case FILE_TYPE_BLENDER_BACKUP:
      return ICON_FILE_BACKUP;
    case FILE_TYPE_IMAGE:
      return ICON_FILE_IMAGE;
    case FILE_TYPE_MOVIE:
      return ICON_FILE_MOVIE;
    case FILE_TYPE_PYSCRIPT:
      return ICON_FILE_SCRIPT;
    case FILE_TYPE_SOUND:
      return ICON_FILE_SOUND;
    case FILE_TYPE_FTFONT:
      return ICON_FILE_FONT;
    case FILE_TYPE_BTX:
      return ICON_FILE_BLANK;
    case FILE_TYPE_COLLADA:
    case FILE_TYPE_ALEMBIC:
    case FILE_TYPE_OBJECT_IO:
      return ICON_FILE_3D;
    case FILE_TYPE_TEXT:
      return ICON_FILE_TEXT;
    case FILE_TYPE_ARCHIVE:
      return ICON_FILE_ARCHIVE;
    case FILE_TYPE_VOLUME:
      return ICON_FILE_VOLUME;
    default:
      return ICON_FILE_BLANK;
  }
}

int filelist_needs_reading(struct FileList *filelist)
{
  return (filelist->filelist.nbr_entries == FILEDIR_NBR_ENTRIES_UNSET);
}

uint filelist_entry_select_set(const FileList *filelist,
                               const FileDirEntry *entry,
                               FileSelType select,
                               uint flag,
                               FileCheckType check)
{
  /* Default NULL pointer if not found is fine here! */
  void **es_p = BLI_ghash_lookup_p(filelist->selection_state, entry->uuid);
  uint entry_flag = es_p ? POINTER_AS_UINT(*es_p) : 0;
  const uint org_entry_flag = entry_flag;

  BLI_assert(entry);
  BLI_assert(ELEM(check, CHECK_DIRS, CHECK_FILES, CHECK_ALL));

  if (((check == CHECK_ALL)) || ((check == CHECK_DIRS) && (entry->typeflag & FILE_TYPE_DIR)) ||
      ((check == CHECK_FILES) && !(entry->typeflag & FILE_TYPE_DIR))) {
    switch (select) {
      case FILE_SEL_REMOVE:
        entry_flag &= ~flag;
        break;
      case FILE_SEL_ADD:
        entry_flag |= flag;
        break;
      case FILE_SEL_TOGGLE:
        entry_flag ^= flag;
        break;
    }
  }

  if (entry_flag != org_entry_flag) {
    if (es_p) {
      if (entry_flag) {
        *es_p = POINTER_FROM_UINT(entry_flag);
      }
      else {
        BLI_ghash_remove(filelist->selection_state, entry->uuid, MEM_freeN, NULL);
      }
    }
    else if (entry_flag) {
      void *key = MEM_mallocN(sizeof(entry->uuid), __func__);
      memcpy(key, entry->uuid, sizeof(entry->uuid));
      BLI_ghash_insert(filelist->selection_state, key, POINTER_FROM_UINT(entry_flag));
    }
  }

  return entry_flag;
}

void filelist_entry_select_index_set(
    FileList *filelist, const int index, FileSelType select, uint flag, FileCheckType check)
{
  FileDirEntry *entry = filelist_file(filelist, index);

  if (entry) {
    filelist_entry_select_set(filelist, entry, select, flag, check);
  }
}

void filelist_entries_select_index_range_set(
    FileList *filelist, FileSelection *sel, FileSelType select, uint flag, FileCheckType check)
{
  /* select all valid files between first and last indicated */
  if ((sel->first >= 0) && (sel->first < filelist->filelist.nbr_entries_filtered) &&
      (sel->last >= 0) && (sel->last < filelist->filelist.nbr_entries_filtered)) {
    int current_file;
    for (current_file = sel->first; current_file <= sel->last; current_file++) {
      filelist_entry_select_index_set(filelist, current_file, select, flag, check);
    }
  }
}

uint filelist_entry_select_get(FileList *filelist, FileDirEntry *entry, FileCheckType check)
{
  BLI_assert(entry);
  BLI_assert(ELEM(check, CHECK_DIRS, CHECK_FILES, CHECK_ALL));

  if (((check == CHECK_ALL)) || ((check == CHECK_DIRS) && (entry->typeflag & FILE_TYPE_DIR)) ||
      ((check == CHECK_FILES) && !(entry->typeflag & FILE_TYPE_DIR))) {
    /* Default NULL pointer if not found is fine here! */
    return POINTER_AS_UINT(BLI_ghash_lookup(filelist->selection_state, entry->uuid));
  }

  return 0;
}

uint filelist_entry_select_index_get(FileList *filelist, const int index, FileCheckType check)
{
  FileDirEntry *entry = filelist_file(filelist, index);

  if (entry) {
    return filelist_entry_select_get(filelist, entry, check);
  }

  return 0;
}

bool filelist_entry_is_selected(FileList *filelist, const int index)
{
  BLI_assert(index >= 0 && index < filelist->filelist.nbr_entries_filtered);
  FileListInternEntry *intern_entry = filelist->filelist_intern.filtered[index];

  /* BLI_ghash_lookup returns NULL if not found, which gets mapped to 0, which gets mapped to
   * "not selected". */
  const uint selection_state = POINTER_AS_UINT(
      BLI_ghash_lookup(filelist->selection_state, intern_entry->uuid));

  return selection_state != 0;
}

/**
 * Set selection of the '..' parent entry, but only if it's actually visible.
 */
void filelist_entry_parent_select_set(FileList *filelist,
                                      FileSelType select,
                                      uint flag,
                                      FileCheckType check)
{
  if ((filelist->filter_data.flags & FLF_HIDE_PARENT) == 0) {
    filelist_entry_select_index_set(filelist, 0, select, flag, check);
  }
}

/* WARNING! dir must be FILE_MAX_LIBEXTRA long! */
bool filelist_islibrary(struct FileList *filelist, char *dir, char **r_group)
{
  return BLO_library_path_explode(filelist->filelist.root, dir, r_group, NULL);
}

static int groupname_to_code(const char *group)
{
  char buf[BLO_GROUP_MAX];
  char *lslash;

  BLI_assert(group);

  BLI_strncpy(buf, group, sizeof(buf));
  lslash = (char *)BLI_path_slash_rfind(buf);
  if (lslash) {
    lslash[0] = '\0';
  }

  return buf[0] ? BKE_idtype_idcode_from_name(buf) : 0;
}

static uint64_t groupname_to_filter_id(const char *group)
{
  int id_code = groupname_to_code(group);

  return BKE_idtype_idcode_to_idfilter(id_code);
}

/**
 * From here, we are in 'Job Context',
 * i.e. have to be careful about sharing stuff between background working thread.
 * and main one (used by UI among other things).
 */
typedef struct TodoDir {
  int level;
  char *dir;
} TodoDir;

static int filelist_readjob_list_dir(const char *root,
                                     ListBase *entries,
                                     const char *filter_glob,
                                     const bool do_lib,
                                     const char *main_name,
                                     const bool skip_currpar)
{
  struct direntry *files;
  int nbr_files, nbr_entries = 0;
  /* Full path of the item. */
  char full_path[FILE_MAX];

  nbr_files = BLI_filelist_dir_contents(root, &files);
  if (files) {
    int i = nbr_files;
    while (i--) {
      FileListInternEntry *entry;

      if (skip_currpar && FILENAME_IS_CURRPAR(files[i].relname)) {
        continue;
      }

      entry = MEM_callocN(sizeof(*entry), __func__);
      entry->relpath = MEM_dupallocN(files[i].relname);
      entry->st = files[i].s;

      BLI_join_dirfile(full_path, FILE_MAX, root, entry->relpath);
      char *target = full_path;

      /* Set initial file type and attributes. */
      entry->attributes = BLI_file_attributes(full_path);
      if (S_ISDIR(files[i].s.st_mode)
#ifdef __APPLE__
          && !(ED_path_extension_type(full_path) & FILE_TYPE_APPLICATIONBUNDLE)
#endif
      ) {
        entry->typeflag = FILE_TYPE_DIR;
      }

      /* Is this a file that points to another file? */
      if (entry->attributes & FILE_ATTR_ALIAS) {
        entry->redirection_path = MEM_callocN(FILE_MAXDIR, __func__);
        if (BLI_file_alias_target(full_path, entry->redirection_path)) {
          if (BLI_is_dir(entry->redirection_path)) {
            entry->typeflag = FILE_TYPE_DIR;
            BLI_path_slash_ensure(entry->redirection_path);
          }
          else {
            entry->typeflag = ED_path_extension_type(entry->redirection_path);
          }
          target = entry->redirection_path;
#ifdef WIN32
          /* On Windows don't show ".lnk" extension for valid shortcuts. */
          BLI_path_extension_replace(entry->relpath, FILE_MAXDIR, "");
#endif
        }
        else {
          MEM_freeN(entry->redirection_path);
          entry->redirection_path = NULL;
          entry->attributes |= FILE_ATTR_HIDDEN;
        }
      }

      if (!(entry->typeflag & FILE_TYPE_DIR)) {
        if (do_lib && BLO_has_bfile_extension(target)) {
          /* If we are considering .blend files as libs, promote them to directory status. */
          entry->typeflag = FILE_TYPE_BLENDER;
          /* prevent current file being used as acceptable dir */
          if (BLI_path_cmp(main_name, target) != 0) {
            entry->typeflag |= FILE_TYPE_DIR;
          }
        }
        else {
          entry->typeflag = ED_path_extension_type(target);
          if (filter_glob[0] && BLI_path_extension_check_glob(target, filter_glob)) {
            entry->typeflag |= FILE_TYPE_OPERATOR;
          }
        }
      }

#ifndef WIN32
      /* Set linux-style dot files hidden too. */
      if (is_hidden_dot_filename(entry->relpath, entry)) {
        entry->attributes |= FILE_ATTR_HIDDEN;
      }
#endif

      BLI_addtail(entries, entry);
      nbr_entries++;
    }
    BLI_filelist_free(files, nbr_files);
  }
  return nbr_entries;
}

static int filelist_readjob_list_lib(const char *root, ListBase *entries, const bool skip_currpar)
{
  FileListInternEntry *entry;
  LinkNode *ln, *names = NULL, *datablock_infos = NULL;
  int i, nitems, idcode = 0, nbr_entries = 0;
  char dir[FILE_MAX_LIBEXTRA], *group;
  bool ok;

  struct BlendHandle *libfiledata = NULL;

  /* name test */
  ok = BLO_library_path_explode(root, dir, &group, NULL);
  if (!ok) {
    return nbr_entries;
  }

  /* there we go */
  libfiledata = BLO_blendhandle_from_file(dir, NULL);
  if (libfiledata == NULL) {
    return nbr_entries;
  }

  /* memory for strings is passed into filelist[i].entry->relpath
   * and freed in filelist_entry_free. */
  if (group) {
    idcode = groupname_to_code(group);
    datablock_infos = BLO_blendhandle_get_datablock_info(libfiledata, idcode, &nitems);
  }
  else {
    names = BLO_blendhandle_get_linkable_groups(libfiledata);
    nitems = BLI_linklist_count(names);
  }

  BLO_blendhandle_close(libfiledata);

  if (!skip_currpar) {
    entry = MEM_callocN(sizeof(*entry), __func__);
    entry->relpath = BLI_strdup(FILENAME_PARENT);
    entry->typeflag |= (FILE_TYPE_BLENDERLIB | FILE_TYPE_DIR);
    BLI_addtail(entries, entry);
    nbr_entries++;
  }

  for (i = 0, ln = (datablock_infos ? datablock_infos : names); i < nitems; i++, ln = ln->next) {
    struct BLODataBlockInfo *info = datablock_infos ? ln->link : NULL;
    const char *blockname = info ? info->name : ln->link;

    entry = MEM_callocN(sizeof(*entry), __func__);
    entry->relpath = BLI_strdup(blockname);
    entry->typeflag |= FILE_TYPE_BLENDERLIB;
    if (info && info->asset_data) {
      entry->typeflag |= FILE_TYPE_ASSET;
      /* Moves ownership! */
      entry->imported_asset_data = info->asset_data;
    }
    if (!(group && idcode)) {
      entry->typeflag |= FILE_TYPE_DIR;
      entry->blentype = groupname_to_code(blockname);
    }
    else {
      entry->blentype = idcode;
    }
    BLI_addtail(entries, entry);
    nbr_entries++;
  }

  BLI_linklist_freeN(datablock_infos ? datablock_infos : names);

  return nbr_entries;
}

#if 0
/* Kept for reference here, in case we want to add back that feature later.
 * We do not need it currently. */
/* Code ***NOT*** updated for job stuff! */
static void filelist_readjob_main_recursive(Main *bmain, FileList *filelist)
{
  ID *id;
  FileDirEntry *files, *firstlib = NULL;
  ListBase *lb;
  int a, fake, idcode, ok, totlib, totbl;

  // filelist->type = FILE_MAIN; /* XXX TODO: add modes to filebrowser */

  BLI_assert(filelist->filelist.entries == NULL);

  if (filelist->filelist.root[0] == '/') {
    filelist->filelist.root[0] = '\0';
  }

  if (filelist->filelist.root[0]) {
    idcode = groupname_to_code(filelist->filelist.root);
    if (idcode == 0) {
      filelist->filelist.root[0] = '\0';
    }
  }

  if (filelist->dir[0] == 0) {
    /* make directories */
#  ifdef WITH_FREESTYLE
    filelist->filelist.nbr_entries = 27;
#  else
    filelist->filelist.nbr_entries = 26;
#  endif
    filelist_resize(filelist, filelist->filelist.nbr_entries);

    for (a = 0; a < filelist->filelist.nbr_entries; a++) {
      filelist->filelist.entries[a].typeflag |= FILE_TYPE_DIR;
    }

    filelist->filelist.entries[0].entry->relpath = BLI_strdup(FILENAME_PARENT);
    filelist->filelist.entries[1].entry->relpath = BLI_strdup("Scene");
    filelist->filelist.entries[2].entry->relpath = BLI_strdup("Object");
    filelist->filelist.entries[3].entry->relpath = BLI_strdup("Mesh");
    filelist->filelist.entries[4].entry->relpath = BLI_strdup("Curve");
    filelist->filelist.entries[5].entry->relpath = BLI_strdup("Metaball");
    filelist->filelist.entries[6].entry->relpath = BLI_strdup("Material");
    filelist->filelist.entries[7].entry->relpath = BLI_strdup("Texture");
    filelist->filelist.entries[8].entry->relpath = BLI_strdup("Image");
    filelist->filelist.entries[9].entry->relpath = BLI_strdup("Ika");
    filelist->filelist.entries[10].entry->relpath = BLI_strdup("Wave");
    filelist->filelist.entries[11].entry->relpath = BLI_strdup("Lattice");
    filelist->filelist.entries[12].entry->relpath = BLI_strdup("Light");
    filelist->filelist.entries[13].entry->relpath = BLI_strdup("Camera");
    filelist->filelist.entries[14].entry->relpath = BLI_strdup("Ipo");
    filelist->filelist.entries[15].entry->relpath = BLI_strdup("World");
    filelist->filelist.entries[16].entry->relpath = BLI_strdup("Screen");
    filelist->filelist.entries[17].entry->relpath = BLI_strdup("VFont");
    filelist->filelist.entries[18].entry->relpath = BLI_strdup("Text");
    filelist->filelist.entries[19].entry->relpath = BLI_strdup("Armature");
    filelist->filelist.entries[20].entry->relpath = BLI_strdup("Action");
    filelist->filelist.entries[21].entry->relpath = BLI_strdup("NodeTree");
    filelist->filelist.entries[22].entry->relpath = BLI_strdup("Speaker");
    filelist->filelist.entries[23].entry->relpath = BLI_strdup("Hair");
    filelist->filelist.entries[24].entry->relpath = BLI_strdup("Point Cloud");
    filelist->filelist.entries[25].entry->relpath = BLI_strdup("Volume");
#  ifdef WITH_FREESTYLE
    filelist->filelist.entries[26].entry->relpath = BLI_strdup("FreestyleLineStyle");
#  endif
  }
  else {
    /* make files */
    idcode = groupname_to_code(filelist->filelist.root);

    lb = which_libbase(bmain, idcode);
    if (lb == NULL) {
      return;
    }

    filelist->filelist.nbr_entries = 0;
    for (id = lb->first; id; id = id->next) {
      if (!(filelist->filter_data.flags & FLF_HIDE_DOT) || id->name[2] != '.') {
        filelist->filelist.nbr_entries++;
      }
    }

    /* XXX TODO: if databrowse F4 or append/link
     * filelist->flags & FLF_HIDE_PARENT has to be set */
    if (!(filelist->filter_data.flags & FLF_HIDE_PARENT)) {
      filelist->filelist.nbr_entries++;
    }

    if (filelist->filelist.nbr_entries > 0) {
      filelist_resize(filelist, filelist->filelist.nbr_entries);
    }

    files = filelist->filelist.entries;

    if (!(filelist->filter_data.flags & FLF_HIDE_PARENT)) {
      files->entry->relpath = BLI_strdup(FILENAME_PARENT);
      files->typeflag |= FILE_TYPE_DIR;

      files++;
    }

    totlib = totbl = 0;
    for (id = lb->first; id; id = id->next) {
      ok = 1;
      if (ok) {
        if (!(filelist->filter_data.flags & FLF_HIDE_DOT) || id->name[2] != '.') {
          if (id->lib == NULL) {
            files->entry->relpath = BLI_strdup(id->name + 2);
          }
          else {
            char relname[FILE_MAX + (MAX_ID_NAME - 2) + 3];
            BLI_snprintf(relname, sizeof(relname), "%s | %s", id->lib->filepath, id->name + 2);
            files->entry->relpath = BLI_strdup(relname);
          }
//                  files->type |= S_IFREG;
#  if 0 /* XXX TODO show the selection status of the objects */
          if (!filelist->has_func) { /* F4 DATA BROWSE */
            if (idcode == ID_OB) {
              if ( ((Object *)id)->flag & SELECT) {
                files->entry->selflag |= FILE_SEL_SELECTED;
              }
            }
            else if (idcode == ID_SCE) {
              if ( ((Scene *)id)->r.scemode & R_BG_RENDER) {
                files->entry->selflag |= FILE_SEL_SELECTED;
              }
            }
          }
#  endif
          //                  files->entry->nr = totbl + 1;
          files->entry->poin = id;
          fake = id->flag & LIB_FAKEUSER;
          if (idcode == ID_MA || idcode == ID_TE || idcode == ID_LA || idcode == ID_WO ||
              idcode == ID_IM) {
            files->typeflag |= FILE_TYPE_IMAGE;
          }
#  if 0
          if (id->lib && fake) {
            BLI_snprintf(files->extra, sizeof(files->entry->extra), "LF %d", id->us);
          }
          else if (id->lib) {
            BLI_snprintf(files->extra, sizeof(files->entry->extra), "L    %d", id->us);
          }
          else if (fake) {
            BLI_snprintf(files->extra, sizeof(files->entry->extra), "F    %d", id->us);
          }
          else {
            BLI_snprintf(files->extra, sizeof(files->entry->extra), "      %d", id->us);
          }
#  endif

          if (id->lib) {
            if (totlib == 0) {
              firstlib = files;
            }
            totlib++;
          }

          files++;
        }
        totbl++;
      }
    }

    /* only qsort of library blocks */
    if (totlib > 1) {
      qsort(firstlib, totlib, sizeof(*files), compare_name);
    }
  }
}
#endif

static void filelist_readjob_do(const bool do_lib,
                                FileList *filelist,
                                const char *main_name,
                                const short *stop,
                                short *do_update,
                                float *progress,
                                ThreadMutex *lock)
{
  ListBase entries = {0};
  BLI_Stack *todo_dirs;
  TodoDir *td_dir;
  char dir[FILE_MAX_LIBEXTRA];
  char filter_glob[FILE_MAXFILE];
  const char *root = filelist->filelist.root;
  const int max_recursion = filelist->max_recursion;
  int nbr_done_dirs = 0, nbr_todo_dirs = 1;

  //  BLI_assert(filelist->filtered == NULL);
  BLI_assert(BLI_listbase_is_empty(&filelist->filelist.entries) &&
             (filelist->filelist.nbr_entries == FILEDIR_NBR_ENTRIES_UNSET));

  /* A valid, but empty directory from now. */
  filelist->filelist.nbr_entries = 0;

  todo_dirs = BLI_stack_new(sizeof(*td_dir), __func__);
  td_dir = BLI_stack_push_r(todo_dirs);
  td_dir->level = 1;

  BLI_strncpy(dir, filelist->filelist.root, sizeof(dir));
  BLI_strncpy(filter_glob, filelist->filter_data.filter_glob, sizeof(filter_glob));

  BLI_path_normalize_dir(main_name, dir);
  td_dir->dir = BLI_strdup(dir);

  while (!BLI_stack_is_empty(todo_dirs) && !(*stop)) {
    FileListInternEntry *entry;
    int nbr_entries = 0;
    bool is_lib = do_lib;

    char *subdir;
    char rel_subdir[FILE_MAX_LIBEXTRA];
    int recursion_level;
    bool skip_currpar;

    td_dir = BLI_stack_peek(todo_dirs);
    subdir = td_dir->dir;
    recursion_level = td_dir->level;
    skip_currpar = (recursion_level > 1);

    BLI_stack_discard(todo_dirs);

    /* ARRRG! We have to be very careful *not to use* common BLI_path_util helpers over
     * entry->relpath itself (nor any path containing it), since it may actually be a datablock
     * name inside .blend file, which can have slashes and backslashes! See T46827.
     * Note that in the end, this means we 'cache' valid relative subdir once here,
     * this is actually better. */
    BLI_strncpy(rel_subdir, subdir, sizeof(rel_subdir));
    BLI_path_normalize_dir(root, rel_subdir);
    BLI_path_rel(rel_subdir, root);

    if (do_lib) {
      nbr_entries = filelist_readjob_list_lib(subdir, &entries, skip_currpar);
    }
    if (!nbr_entries) {
      is_lib = false;
      nbr_entries = filelist_readjob_list_dir(
          subdir, &entries, filter_glob, do_lib, main_name, skip_currpar);
    }

    for (entry = entries.first; entry; entry = entry->next) {
      BLI_join_dirfile(dir, sizeof(dir), rel_subdir, entry->relpath);

      /* Generate our entry uuid. Abusing uuid as an uint32, shall be more than enough here,
       * things would crash way before we overflow that counter!
       * Using an atomic operation to avoid having to lock thread...
       * Note that we do not really need this here currently,
       * since there is a single listing thread, but better
       * remain consistent about threading! */
      *((uint32_t *)entry->uuid) = atomic_add_and_fetch_uint32(
          (uint32_t *)filelist->filelist_intern.curr_uuid, 1);

      /* Only thing we change in direntry here, so we need to free it first. */
      MEM_freeN(entry->relpath);
      entry->relpath = BLI_strdup(dir + 2); /* + 2 to remove '//'
                                             * added by BLI_path_rel to rel_subdir. */
      entry->name = BLI_strdup(fileentry_uiname(root, entry->relpath, entry->typeflag, dir));
      entry->free_name = true;

      /* Here we decide whether current filedirentry is to be listed too, or not. */
      if (max_recursion && (is_lib || (recursion_level <= max_recursion))) {
        if (((entry->typeflag & FILE_TYPE_DIR) == 0) || FILENAME_IS_CURRPAR(entry->relpath)) {
          /* Skip... */
        }
        else if (!is_lib && (recursion_level >= max_recursion) &&
                 ((entry->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) == 0)) {
          /* Do not recurse in real directories in this case, only in .blend libs. */
        }
        else {
          /* We have a directory we want to list, add it to todo list! */
          BLI_join_dirfile(dir, sizeof(dir), root, entry->relpath);
          BLI_path_normalize_dir(main_name, dir);
          td_dir = BLI_stack_push_r(todo_dirs);
          td_dir->level = recursion_level + 1;
          td_dir->dir = BLI_strdup(dir);
          nbr_todo_dirs++;
        }
      }
    }

    if (nbr_entries) {
      BLI_mutex_lock(lock);

      *do_update = true;

      BLI_movelisttolist(&filelist->filelist.entries, &entries);
      filelist->filelist.nbr_entries += nbr_entries;

      BLI_mutex_unlock(lock);
    }

    nbr_done_dirs++;
    *progress = (float)nbr_done_dirs / (float)nbr_todo_dirs;
    MEM_freeN(subdir);
  }

  /* If we were interrupted by stop, stack may not be empty and we need to free
   * pending dir paths. */
  while (!BLI_stack_is_empty(todo_dirs)) {
    td_dir = BLI_stack_peek(todo_dirs);
    MEM_freeN(td_dir->dir);
    BLI_stack_discard(todo_dirs);
  }
  BLI_stack_free(todo_dirs);
}

static void filelist_readjob_dir(Main *UNUSED(current_main),
                                 FileList *filelist,
                                 const char *main_name,
                                 short *stop,
                                 short *do_update,
                                 float *progress,
                                 ThreadMutex *lock)
{
  filelist_readjob_do(false, filelist, main_name, stop, do_update, progress, lock);
}

static void filelist_readjob_lib(Main *UNUSED(current_main),
                                 FileList *filelist,
                                 const char *main_name,
                                 short *stop,
                                 short *do_update,
                                 float *progress,
                                 ThreadMutex *lock)
{
  filelist_readjob_do(true, filelist, main_name, stop, do_update, progress, lock);
}

static void filelist_readjob_main(Main *current_main,
                                  FileList *filelist,
                                  const char *main_name,
                                  short *stop,
                                  short *do_update,
                                  float *progress,
                                  ThreadMutex *lock)
{
  /* TODO! */
  filelist_readjob_dir(current_main, filelist, main_name, stop, do_update, progress, lock);
}

/**
 * \warning Acts on main, so NOT thread-safe!
 */
static void filelist_readjob_main_assets(Main *current_main,
                                         FileList *filelist,
                                         const char *UNUSED(main_name),
                                         short *UNUSED(stop),
                                         short *do_update,
                                         float *UNUSED(progress),
                                         ThreadMutex *UNUSED(lock))
{
  BLI_assert(BLI_listbase_is_empty(&filelist->filelist.entries) &&
             (filelist->filelist.nbr_entries == FILEDIR_NBR_ENTRIES_UNSET));

  /* A valid, but empty directory from now. */
  filelist->filelist.nbr_entries = 0;

  FileListInternEntry *entry;
  ListBase tmp_entries = {0};
  ID *id_iter;
  int nbr_entries = 0;

  FOREACH_MAIN_ID_BEGIN (current_main, id_iter) {
    if (!id_iter->asset_data) {
      continue;
    }

    const char *id_code_name = BKE_idtype_idcode_to_name(GS(id_iter->name));

    entry = MEM_callocN(sizeof(*entry), __func__);
    entry->relpath = BLI_strdup(id_code_name);
    entry->name = id_iter->name + 2;
    entry->free_name = false;
    entry->typeflag |= FILE_TYPE_BLENDERLIB | FILE_TYPE_ASSET;
    entry->blentype = GS(id_iter->name);
    *((uint32_t *)entry->uuid) = atomic_add_and_fetch_uint32(
        (uint32_t *)filelist->filelist_intern.curr_uuid, 1);
    entry->local_data.preview_image = BKE_asset_metadata_preview_get_from_id(id_iter->asset_data,
                                                                             id_iter);
    entry->local_data.id = id_iter;
    nbr_entries++;
    BLI_addtail(&tmp_entries, entry);
  }
  FOREACH_MAIN_ID_END;

  if (nbr_entries) {
    *do_update = true;

    BLI_movelisttolist(&filelist->filelist.entries, &tmp_entries);
    filelist->filelist.nbr_entries += nbr_entries;
    filelist->filelist.nbr_entries_filtered = filelist->filelist.entry_idx_start =
        filelist->filelist.entry_idx_end = -1;
  }
}

typedef struct FileListReadJob {
  ThreadMutex lock;
  char main_name[FILE_MAX];
  Main *current_main;
  struct FileList *filelist;
  /** XXX We may use a simpler struct here... just a linked list and root path? */
  struct FileList *tmp_filelist;
} FileListReadJob;

static void filelist_readjob_startjob(void *flrjv, short *stop, short *do_update, float *progress)
{
  FileListReadJob *flrj = flrjv;

  //  printf("START filelist reading (%d files, main thread: %d)\n",
  //         flrj->filelist->filelist.nbr_entries, BLI_thread_is_main());

  BLI_mutex_lock(&flrj->lock);

  BLI_assert((flrj->tmp_filelist == NULL) && flrj->filelist);

  flrj->tmp_filelist = MEM_dupallocN(flrj->filelist);

  BLI_listbase_clear(&flrj->tmp_filelist->filelist.entries);
  flrj->tmp_filelist->filelist.nbr_entries = FILEDIR_NBR_ENTRIES_UNSET;

  flrj->tmp_filelist->filelist_intern.filtered = NULL;
  BLI_listbase_clear(&flrj->tmp_filelist->filelist_intern.entries);
  memset(flrj->tmp_filelist->filelist_intern.curr_uuid,
         0,
         sizeof(flrj->tmp_filelist->filelist_intern.curr_uuid));

  flrj->tmp_filelist->libfiledata = NULL;
  memset(&flrj->tmp_filelist->filelist_cache, 0, sizeof(flrj->tmp_filelist->filelist_cache));
  flrj->tmp_filelist->selection_state = NULL;
  flrj->tmp_filelist->asset_library = NULL;

  BLI_mutex_unlock(&flrj->lock);

  flrj->tmp_filelist->read_job_fn(flrj->current_main,
                                  flrj->tmp_filelist,
                                  flrj->main_name,
                                  stop,
                                  do_update,
                                  progress,
                                  &flrj->lock);
}

static void filelist_readjob_update(void *flrjv)
{
  FileListReadJob *flrj = flrjv;
  FileListIntern *fl_intern = &flrj->filelist->filelist_intern;
  ListBase new_entries = {NULL};
  int nbr_entries, new_nbr_entries = 0;

  BLI_movelisttolist(&new_entries, &fl_intern->entries);
  nbr_entries = flrj->filelist->filelist.nbr_entries;

  BLI_mutex_lock(&flrj->lock);

  if (flrj->tmp_filelist->filelist.nbr_entries > 0) {
    /* We just move everything out of 'thread context' into final list. */
    new_nbr_entries = flrj->tmp_filelist->filelist.nbr_entries;
    BLI_movelisttolist(&new_entries, &flrj->tmp_filelist->filelist.entries);
    flrj->tmp_filelist->filelist.nbr_entries = 0;
  }

  BLI_mutex_unlock(&flrj->lock);

  if (new_nbr_entries) {
    /* Do not clear selection cache, we can assume already 'selected' uuids are still valid! */
    filelist_clear_ex(flrj->filelist, true, false);

    flrj->filelist->flags |= (FL_NEED_SORTING | FL_NEED_FILTERING);
  }

  /* if no new_nbr_entries, this is NOP */
  BLI_movelisttolist(&fl_intern->entries, &new_entries);
  flrj->filelist->filelist.nbr_entries = MAX2(nbr_entries, 0) + new_nbr_entries;
}

static void filelist_readjob_endjob(void *flrjv)
{
  FileListReadJob *flrj = flrjv;

  /* In case there would be some dangling update... */
  filelist_readjob_update(flrjv);

  flrj->filelist->flags &= ~FL_IS_PENDING;
  flrj->filelist->flags |= FL_IS_READY;
}

static void filelist_readjob_free(void *flrjv)
{
  FileListReadJob *flrj = flrjv;

  //  printf("END filelist reading (%d files)\n", flrj->filelist->filelist.nbr_entries);

  if (flrj->tmp_filelist) {
    /* tmp_filelist shall never ever be filtered! */
    BLI_assert(flrj->tmp_filelist->filelist.nbr_entries == 0);
    BLI_assert(BLI_listbase_is_empty(&flrj->tmp_filelist->filelist.entries));

    filelist_freelib(flrj->tmp_filelist);
    filelist_free(flrj->tmp_filelist);
    MEM_freeN(flrj->tmp_filelist);
  }

  BLI_mutex_end(&flrj->lock);

  MEM_freeN(flrj);
}

void filelist_readjob_start(FileList *filelist, const bContext *C)
{
  Main *bmain = CTX_data_main(C);
  wmJob *wm_job;
  FileListReadJob *flrj;

  if (!filelist_is_dir(filelist, filelist->filelist.root)) {
    return;
  }

  /* prepare job data */
  flrj = MEM_callocN(sizeof(*flrj), __func__);
  flrj->filelist = filelist;
  flrj->current_main = bmain;
  BLI_strncpy(flrj->main_name, BKE_main_blendfile_path(bmain), sizeof(flrj->main_name));

  filelist->flags &= ~(FL_FORCE_RESET | FL_IS_READY);
  filelist->flags |= FL_IS_PENDING;

  /* Init even for single threaded execution. Called functions use it. */
  BLI_mutex_init(&flrj->lock);

  if (filelist->tags & FILELIST_TAGS_NO_THREADS) {
    short dummy_stop = false;
    short dummy_do_update = false;
    float dummy_progress = 0.0f;

    /* Single threaded execution. Just directly call the callbacks. */
    filelist_readjob_startjob(flrj, &dummy_stop, &dummy_do_update, &dummy_progress);
    filelist_readjob_endjob(flrj);
    filelist_readjob_free(flrj);

    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_FILE_LIST | NA_JOB_FINISHED, NULL);
    return;
  }

  /* setup job */
  wm_job = WM_jobs_get(CTX_wm_manager(C),
                       CTX_wm_window(C),
                       CTX_data_scene(C),
                       "Listing Dirs...",
                       WM_JOB_PROGRESS,
                       WM_JOB_TYPE_FILESEL_READDIR);
  WM_jobs_customdata_set(wm_job, flrj, filelist_readjob_free);
  WM_jobs_timer(wm_job,
                0.01,
                NC_SPACE | ND_SPACE_FILE_LIST,
                NC_SPACE | ND_SPACE_FILE_LIST | NA_JOB_FINISHED);
  WM_jobs_callbacks(
      wm_job, filelist_readjob_startjob, NULL, filelist_readjob_update, filelist_readjob_endjob);

  /* start the job */
  WM_jobs_start(CTX_wm_manager(C), wm_job);
}

void filelist_readjob_stop(wmWindowManager *wm, Scene *owner_scene)
{
  WM_jobs_kill_type(wm, owner_scene, WM_JOB_TYPE_FILESEL_READDIR);
}

int filelist_readjob_running(wmWindowManager *wm, Scene *owner_scene)
{
  return WM_jobs_test(wm, owner_scene, WM_JOB_TYPE_FILESEL_READDIR);
}
