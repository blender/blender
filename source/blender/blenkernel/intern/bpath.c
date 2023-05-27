/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

/* TODO:
 * currently there are some cases we don't support.
 * - passing output paths to the visitor?, like render out.
 * - passing sequence strips with many images.
 * - passing directory paths - visitors don't know which path is a dir or a file.
 */

#include <sys/stat.h>

#include <string.h>

/* path/file handling stuff */
#ifndef WIN32
#  include <dirent.h>
#  include <unistd.h>
#else
#  include "BLI_winstuff.h"
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_fluid_types.h"
#include "DNA_freestyle_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_node_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcache_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_texture_types.h"
#include "DNA_vfont_types.h"
#include "DNA_volume_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DEG_depsgraph.h"

#include "BKE_idtype.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_vfont.h"

#include "BKE_bpath.h" /* own include */

#include "CLG_log.h"

#include "SEQ_iterator.h"

#ifndef _MSC_VER
#  include "BLI_strict_flags.h"
#endif

static CLG_LogRef LOG = {"bke.bpath"};

/* -------------------------------------------------------------------- */
/** \name Generic File Path Traversal API
 * \{ */

void BKE_bpath_foreach_path_id(BPathForeachPathData *bpath_data, ID *id)
{
  const eBPathForeachFlag flag = bpath_data->flag;
  const char *absbase = (flag & BKE_BPATH_FOREACH_PATH_ABSOLUTE) ?
                            ID_BLEND_PATH(bpath_data->bmain, id) :
                            NULL;
  bpath_data->absolute_base_path = absbase;
  bpath_data->owner_id = id;
  bpath_data->is_path_modified = false;

  if ((flag & BKE_BPATH_FOREACH_PATH_SKIP_LINKED) && ID_IS_LINKED(id)) {
    return;
  }

  if (id->library_weak_reference != NULL && (flag & BKE_BPATH_TRAVERSE_SKIP_WEAK_REFERENCES) == 0)
  {
    BKE_bpath_foreach_path_fixed_process(bpath_data, id->library_weak_reference->library_filepath);
  }

  bNodeTree *embedded_node_tree = ntreeFromID(id);
  if (embedded_node_tree != NULL) {
    BKE_bpath_foreach_path_id(bpath_data, &embedded_node_tree->id);
  }

  const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);

  BLI_assert(id_type != NULL);
  if (id_type == NULL || id_type->foreach_path == NULL) {
    return;
  }

  id_type->foreach_path(id, bpath_data);

  if (bpath_data->is_path_modified) {
    DEG_id_tag_update(id, ID_RECALC_SOURCE | ID_RECALC_COPY_ON_WRITE);
  }
}

void BKE_bpath_foreach_path_main(BPathForeachPathData *bpath_data)
{
  ID *id;
  FOREACH_MAIN_ID_BEGIN (bpath_data->bmain, id) {
    BKE_bpath_foreach_path_id(bpath_data, id);
  }
  FOREACH_MAIN_ID_END;
}

bool BKE_bpath_foreach_path_fixed_process(BPathForeachPathData *bpath_data, char *path)
{
  const char *absolute_base_path = bpath_data->absolute_base_path;

  char path_src_buf[FILE_MAX];
  const char *path_src;
  char path_dst[FILE_MAX];

  if (absolute_base_path) {
    STRNCPY(path_src_buf, path);
    BLI_path_abs(path_src_buf, absolute_base_path);
    path_src = path_src_buf;
  }
  else {
    path_src = path;
  }

  /* so functions can check old value */
  STRNCPY(path_dst, path);

  if (bpath_data->callback_function(bpath_data, path_dst, path_src)) {
    BLI_strncpy(path, path_dst, FILE_MAX);
    bpath_data->is_path_modified = true;
    return true;
  }

  return false;
}

bool BKE_bpath_foreach_path_dirfile_fixed_process(BPathForeachPathData *bpath_data,
                                                  char *path_dir,
                                                  char *path_file)
{
  const char *absolute_base_path = bpath_data->absolute_base_path;

  char path_src[FILE_MAX];
  char path_dst[FILE_MAX];

  BLI_path_join(path_src, sizeof(path_src), path_dir, path_file);

  /* So that functions can access the old value. */
  STRNCPY(path_dst, path_src);

  if (absolute_base_path) {
    BLI_path_abs(path_src, absolute_base_path);
  }

  if (bpath_data->callback_function(bpath_data, path_dst, (const char *)path_src)) {
    BLI_path_split_dir_file(path_dst, path_dir, FILE_MAXDIR, path_file, FILE_MAXFILE);
    bpath_data->is_path_modified = true;
    return true;
  }

  return false;
}

bool BKE_bpath_foreach_path_allocated_process(BPathForeachPathData *bpath_data, char **path)
{
  const char *absolute_base_path = bpath_data->absolute_base_path;

  char path_src_buf[FILE_MAX];
  const char *path_src;
  char path_dst[FILE_MAX];

  if (absolute_base_path) {
    STRNCPY(path_src_buf, *path);
    BLI_path_abs(path_src_buf, absolute_base_path);
    path_src = path_src_buf;
  }
  else {
    path_src = *path;
  }

  if (bpath_data->callback_function(bpath_data, path_dst, path_src)) {
    MEM_freeN(*path);
    (*path) = BLI_strdup(path_dst);
    bpath_data->is_path_modified = true;
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Check Missing Files
 * \{ */

static bool check_missing_files_foreach_path_cb(BPathForeachPathData *bpath_data,
                                                char *UNUSED(path_dst),
                                                const char *path_src)
{
  ReportList *reports = (ReportList *)bpath_data->user_data;

  if (!BLI_exists(path_src)) {
    BKE_reportf(reports, RPT_WARNING, "Path '%s' not found", path_src);
  }

  return false;
}

void BKE_bpath_missing_files_check(Main *bmain, ReportList *reports)
{
  BKE_bpath_foreach_path_main(&(BPathForeachPathData){
      .bmain = bmain,
      .callback_function = check_missing_files_foreach_path_cb,
      .flag = BKE_BPATH_FOREACH_PATH_ABSOLUTE | BKE_BPATH_FOREACH_PATH_SKIP_PACKED |
              BKE_BPATH_FOREACH_PATH_RESOLVE_TOKEN | BKE_BPATH_TRAVERSE_SKIP_WEAK_REFERENCES,
      .user_data = reports});

  if (BLI_listbase_is_empty(&reports->list)) {
    BKE_reportf(reports, RPT_INFO, "No missing files");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Missing Files
 * \{ */

#define MAX_DIR_RECURSE 16
#define FILESIZE_INVALID_DIRECTORY -1

/**
 * Find the given filename recursively in the given search directory and its sub-directories.
 *
 * \note Use the biggest matching file found, so that thumbnails don't get used by mistake.
 *
 * \param search_directory: Directory to search in.
 * \param filename_src: Search for this filename.
 * \param r_filepath_new: The path of the new found file will be copied here, caller must
 *                        initialize as empty string.
 * \param r_filesize: Size of the file, `FILESIZE_INVALID_DIRECTORY` if search directory could not
 *                    be opened.
 * \param r_recurse_depth: Current recursion depth.
 *
 * \return true if found, false otherwise.
 */
static bool missing_files_find__recursive(const char *search_directory,
                                          const char *filename_src,
                                          char r_filepath_new[FILE_MAX],
                                          int64_t *r_filesize,
                                          int *r_recurse_depth)
{
  /* TODO: Move this function to BLI_path_utils? The 'biggest size' behavior is quite specific
   * though... */
  DIR *dir;
  BLI_stat_t status;
  char path[FILE_MAX];
  int64_t size;
  bool found = false;

  dir = opendir(search_directory);

  if (dir == NULL) {
    return found;
  }

  if (*r_filesize == FILESIZE_INVALID_DIRECTORY) {
    *r_filesize = 0; /* The directory opened fine. */
  }

  for (struct dirent *de = readdir(dir); de != NULL; de = readdir(dir)) {
    if (FILENAME_IS_CURRPAR(de->d_name)) {
      continue;
    }

    BLI_path_join(path, sizeof(path), search_directory, de->d_name);

    if (BLI_stat(path, &status) == -1) {
      CLOG_WARN(&LOG, "Cannot get file status (`stat()`) of '%s'", path);
      continue;
    }

    if (S_ISREG(status.st_mode)) {                                  /* It is a file. */
      if (BLI_path_ncmp(filename_src, de->d_name, FILE_MAX) == 0) { /* Names match. */
        size = status.st_size;
        if ((size > 0) && (size > *r_filesize)) { /* Find the biggest matching file. */
          *r_filesize = size;
          BLI_strncpy(r_filepath_new, path, FILE_MAX);
          found = true;
        }
      }
    }
    else if (S_ISDIR(status.st_mode)) { /* It is a sub-directory. */
      if (*r_recurse_depth <= MAX_DIR_RECURSE) {
        (*r_recurse_depth)++;
        found |= missing_files_find__recursive(
            path, filename_src, r_filepath_new, r_filesize, r_recurse_depth);
        (*r_recurse_depth)--;
      }
    }
  }

  closedir(dir);
  return found;
}

typedef struct BPathFind_Data {
  const char *basedir;
  const char *searchdir;
  ReportList *reports;
  bool find_all; /* Also search for files which current path is still valid. */
} BPathFind_Data;

static bool missing_files_find_foreach_path_cb(BPathForeachPathData *bpath_data,
                                               char *path_dst,
                                               const char *path_src)
{
  BPathFind_Data *data = (BPathFind_Data *)bpath_data->user_data;
  char filepath_new[FILE_MAX];

  int64_t filesize = FILESIZE_INVALID_DIRECTORY;
  int recurse_depth = 0;
  bool is_found;

  if (!data->find_all && BLI_exists(path_src)) {
    return false;
  }

  filepath_new[0] = '\0';

  is_found = missing_files_find__recursive(
      data->searchdir, BLI_path_basename(path_src), filepath_new, &filesize, &recurse_depth);

  if (filesize == FILESIZE_INVALID_DIRECTORY) {
    BKE_reportf(data->reports,
                RPT_WARNING,
                "Could not open the directory '%s'",
                BLI_path_basename(data->searchdir));
    return false;
  }
  if (is_found == false) {
    BKE_reportf(data->reports,
                RPT_WARNING,
                "Could not find '%s' in '%s'",
                BLI_path_basename(path_src),
                data->searchdir);
    return false;
  }

  bool was_relative = BLI_path_is_rel(path_dst);

  BLI_strncpy(path_dst, filepath_new, FILE_MAX);

  /* Keep the path relative if the previous one was relative. */
  if (was_relative) {
    BLI_path_rel(path_dst, data->basedir);
  }

  return true;
}

void BKE_bpath_missing_files_find(Main *bmain,
                                  const char *searchpath,
                                  ReportList *reports,
                                  const bool find_all)
{
  struct BPathFind_Data data = {NULL};
  const int flag = BKE_BPATH_FOREACH_PATH_ABSOLUTE | BKE_BPATH_FOREACH_PATH_RELOAD_EDITED |
                   BKE_BPATH_FOREACH_PATH_RESOLVE_TOKEN | BKE_BPATH_TRAVERSE_SKIP_WEAK_REFERENCES;

  data.basedir = BKE_main_blendfile_path(bmain);
  data.reports = reports;
  data.searchdir = searchpath;
  data.find_all = find_all;

  BKE_bpath_foreach_path_main(
      &(BPathForeachPathData){.bmain = bmain,
                              .callback_function = missing_files_find_foreach_path_cb,
                              .flag = flag,
                              .user_data = &data});
}

#undef MAX_DIR_RECURSE
#undef FILESIZE_INVALID_DIRECTORY

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rebase Relative Paths
 * \{ */

typedef struct BPathRebase_Data {
  const char *basedir_src;
  const char *basedir_dst;
  ReportList *reports;

  int count_tot;
  int count_changed;
  int count_failed;
} BPathRebase_Data;

static bool relative_rebase_foreach_path_cb(BPathForeachPathData *bpath_data,
                                            char *path_dst,
                                            const char *path_src)
{
  BPathRebase_Data *data = (BPathRebase_Data *)bpath_data->user_data;

  data->count_tot++;

  if (!BLI_path_is_rel(path_src)) {
    /* Absolute, leave this as-is. */
    return false;
  }

  char filepath[(FILE_MAXDIR * 2) + FILE_MAXFILE];
  BLI_strncpy(filepath, path_src, FILE_MAX);
  if (!BLI_path_abs(filepath, data->basedir_src)) {
    BKE_reportf(data->reports, RPT_WARNING, "Path '%s' cannot be made absolute", path_src);
    data->count_failed++;
    return false;
  }

  BLI_path_normalize(filepath);

  /* This may fail, if so it's fine to leave absolute since the path is still valid. */
  BLI_path_rel(filepath, data->basedir_dst);

  BLI_strncpy(path_dst, filepath, FILE_MAX);
  data->count_changed++;
  return true;
}

void BKE_bpath_relative_rebase(Main *bmain,
                               const char *basedir_src,
                               const char *basedir_dst,
                               ReportList *reports)
{
  BPathRebase_Data data = {NULL};
  const int flag = (BKE_BPATH_FOREACH_PATH_SKIP_LINKED | BKE_BPATH_FOREACH_PATH_SKIP_MULTIFILE);

  BLI_assert(basedir_src[0] != '\0');
  BLI_assert(basedir_dst[0] != '\0');

  data.basedir_src = basedir_src;
  data.basedir_dst = basedir_dst;
  data.reports = reports;

  BKE_bpath_foreach_path_main(
      &(BPathForeachPathData){.bmain = bmain,
                              .callback_function = relative_rebase_foreach_path_cb,
                              .flag = flag,
                              .user_data = &data});

  BKE_reportf(reports,
              data.count_failed ? RPT_WARNING : RPT_INFO,
              "Total files %d | Changed %d | Failed %d",
              data.count_tot,
              data.count_changed,
              data.count_failed);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Paths Relative Or Absolute
 * \{ */

typedef struct BPathRemap_Data {
  const char *basedir;
  ReportList *reports;

  int count_tot;
  int count_changed;
  int count_failed;
} BPathRemap_Data;

static bool relative_convert_foreach_path_cb(BPathForeachPathData *bpath_data,
                                             char *path_dst,
                                             const char *path_src)
{
  BPathRemap_Data *data = (BPathRemap_Data *)bpath_data->user_data;

  data->count_tot++;

  if (BLI_path_is_rel(path_src)) {
    return false; /* Already relative. */
  }

  BLI_strncpy(path_dst, path_src, FILE_MAX);
  BLI_path_rel(path_dst, data->basedir);
  if (BLI_path_is_rel(path_dst)) {
    data->count_changed++;
  }
  else {
    const char *type_name = BKE_idtype_get_info_from_id(bpath_data->owner_id)->name;
    const char *id_name = bpath_data->owner_id->name + 2;
    BKE_reportf(data->reports,
                RPT_WARNING,
                "Path '%s' cannot be made relative for %s '%s'",
                path_src,
                type_name,
                id_name);
    data->count_failed++;
  }
  return true;
}

static bool absolute_convert_foreach_path_cb(BPathForeachPathData *bpath_data,
                                             char *path_dst,
                                             const char *path_src)
{
  BPathRemap_Data *data = (BPathRemap_Data *)bpath_data->user_data;

  data->count_tot++;

  if (!BLI_path_is_rel(path_src)) {
    return false; /* Already absolute. */
  }

  BLI_strncpy(path_dst, path_src, FILE_MAX);
  BLI_path_abs(path_dst, data->basedir);
  if (BLI_path_is_rel(path_dst) == false) {
    data->count_changed++;
  }
  else {
    const char *type_name = BKE_idtype_get_info_from_id(bpath_data->owner_id)->name;
    const char *id_name = bpath_data->owner_id->name + 2;
    BKE_reportf(data->reports,
                RPT_WARNING,
                "Path '%s' cannot be made absolute for %s '%s'",
                path_src,
                type_name,
                id_name);
    data->count_failed++;
  }
  return true;
}

static void bpath_absolute_relative_convert(Main *bmain,
                                            const char *basedir,
                                            ReportList *reports,
                                            BPathForeachPathFunctionCallback callback_function)
{
  BPathRemap_Data data = {NULL};
  const int flag = BKE_BPATH_FOREACH_PATH_SKIP_LINKED;

  BLI_assert(basedir[0] != '\0');
  if (basedir[0] == '\0') {
    CLOG_ERROR(&LOG, "basedir='', this is a bug");
    return;
  }

  data.basedir = basedir;
  data.reports = reports;

  BKE_bpath_foreach_path_main(&(BPathForeachPathData){
      .bmain = bmain, .callback_function = callback_function, .flag = flag, .user_data = &data});

  BKE_reportf(reports,
              data.count_failed ? RPT_WARNING : RPT_INFO,
              "Total files %d | Changed %d | Failed %d",
              data.count_tot,
              data.count_changed,
              data.count_failed);
}

void BKE_bpath_relative_convert(Main *bmain, const char *basedir, ReportList *reports)
{
  bpath_absolute_relative_convert(bmain, basedir, reports, relative_convert_foreach_path_cb);
}

void BKE_bpath_absolute_convert(Main *bmain, const char *basedir, ReportList *reports)
{
  bpath_absolute_relative_convert(bmain, basedir, reports, absolute_convert_foreach_path_cb);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Backup/Restore/Free paths list functions.
 *
 * \{ */

struct PathStore {
  struct PathStore *next, *prev;
};

static bool bpath_list_append(BPathForeachPathData *bpath_data,
                              char *UNUSED(path_dst),
                              const char *path_src)
{
  ListBase *path_list = bpath_data->user_data;
  size_t path_size = strlen(path_src) + 1;

  /* NOTE: the PathStore and its string are allocated together in a single alloc. */
  struct PathStore *path_store = MEM_mallocN(sizeof(struct PathStore) + path_size, __func__);
  char *filepath = (char *)(path_store + 1);

  memcpy(filepath, path_src, path_size);
  BLI_addtail(path_list, path_store);
  return false;
}

static bool bpath_list_restore(BPathForeachPathData *bpath_data,
                               char *path_dst,
                               const char *path_src)
{
  ListBase *path_list = bpath_data->user_data;

  /* `ls->first` should never be NULL, because the number of paths should not change.
   * If this happens, there is a bug in caller code. */
  BLI_assert(!BLI_listbase_is_empty(path_list));

  struct PathStore *path_store = path_list->first;
  const char *filepath = (char *)(path_store + 1);
  bool is_path_changed = false;

  if (!STREQ(path_src, filepath)) {
    BLI_strncpy(path_dst, filepath, FILE_MAX);
    is_path_changed = true;
  }

  BLI_freelinkN(path_list, path_store);
  return is_path_changed;
}

void *BKE_bpath_list_backup(Main *bmain, const eBPathForeachFlag flag)
{
  ListBase *path_list = MEM_callocN(sizeof(ListBase), __func__);

  BKE_bpath_foreach_path_main(&(BPathForeachPathData){.bmain = bmain,
                                                      .callback_function = bpath_list_append,
                                                      .flag = flag,
                                                      .user_data = path_list});

  return path_list;
}

void BKE_bpath_list_restore(Main *bmain, const eBPathForeachFlag flag, void *path_list_handle)
{
  ListBase *path_list = path_list_handle;

  BKE_bpath_foreach_path_main(&(BPathForeachPathData){.bmain = bmain,
                                                      .callback_function = bpath_list_restore,
                                                      .flag = flag,
                                                      .user_data = path_list});
}

void BKE_bpath_list_free(void *path_list_handle)
{
  ListBase *path_list = path_list_handle;
  /* The whole list should have been consumed by #BKE_bpath_list_restore, see also comment in
   * #bpath_list_restore. */
  BLI_assert(BLI_listbase_is_empty(path_list));

  BLI_freelistN(path_list);
  MEM_freeN(path_list);
}

/** \} */
