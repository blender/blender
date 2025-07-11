/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "AS_asset_catalog.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "../filelist.hh"
#include "filelist_intern.hh"

struct FileSortData {
  bool inverted;
};

static int compare_apply_inverted(int val, const FileSortData *sort_data)
{
  return sort_data->inverted ? -val : val;
}

/**
 * If all relevant characteristics match (e.g. the file type when sorting by file types), this
 * should be used as tiebreaker. It makes sure there's a well defined sorting even in such cases.
 *
 * Multiple files with the same name can appear with recursive file loading and/or when displaying
 * IDs of different types, so these cases need to be handled.
 *
 * 1) Sort files by name using natural sorting.
 * 2) If not possible (file names match) and both represent local IDs, sort by ID-type.
 * 3) If not possible and only one is a local ID, place files representing local IDs first.
 *
 * TODO: (not actually implemented, but should be):
 * 4) If no file represents a local ID, sort by file path, so that files higher up the file system
 *    hierarchy are placed first.
 */
static int compare_tiebreaker(const FileListInternEntry *entry1, const FileListInternEntry *entry2)
{
  /* Case 1. */
  {
    const int order = BLI_strcasecmp_natural(entry1->name, entry2->name);
    if (order) {
      return order;
    }
  }

  /* Case 2. */
  if (entry1->local_data.id && entry2->local_data.id) {
    if (entry1->blentype < entry2->blentype) {
      return -1;
    }
    if (entry1->blentype > entry2->blentype) {
      return 1;
    }
  }
  /* Case 3. */
  {
    if (entry1->local_data.id && !entry2->local_data.id) {
      return -1;
    }
    if (!entry1->local_data.id && entry2->local_data.id) {
      return 1;
    }
  }

  return 0;
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
       * then libraries (.blend files), then categories in libraries. */
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
  const FileListInternEntry *entry1 = static_cast<const FileListInternEntry *>(a1);
  const FileListInternEntry *entry2 = static_cast<const FileListInternEntry *>(a2);
  const FileSortData *sort_data = static_cast<const FileSortData *>(user_data);

  int ret;
  if ((ret = compare_direntry_generic(entry1, entry2))) {
    return ret;
  }

  return compare_apply_inverted(compare_tiebreaker(entry1, entry2), sort_data);
}

static int compare_date(void *user_data, const void *a1, const void *a2)
{
  const FileListInternEntry *entry1 = static_cast<const FileListInternEntry *>(a1);
  const FileListInternEntry *entry2 = static_cast<const FileListInternEntry *>(a2);
  const FileSortData *sort_data = static_cast<const FileSortData *>(user_data);
  int64_t time1, time2;

  int ret;
  if ((ret = compare_direntry_generic(entry1, entry2))) {
    return ret;
  }

  time1 = int64_t(entry1->st.st_mtime);
  time2 = int64_t(entry2->st.st_mtime);
  if (time1 < time2) {
    return compare_apply_inverted(1, sort_data);
  }
  if (time1 > time2) {
    return compare_apply_inverted(-1, sort_data);
  }

  return compare_apply_inverted(compare_tiebreaker(entry1, entry2), sort_data);
}

static int compare_size(void *user_data, const void *a1, const void *a2)
{
  const FileListInternEntry *entry1 = static_cast<const FileListInternEntry *>(a1);
  const FileListInternEntry *entry2 = static_cast<const FileListInternEntry *>(a2);
  const FileSortData *sort_data = static_cast<const FileSortData *>(user_data);
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

  return compare_apply_inverted(compare_tiebreaker(entry1, entry2), sort_data);
}

static int compare_extension(void *user_data, const void *a1, const void *a2)
{
  const FileListInternEntry *entry1 = static_cast<const FileListInternEntry *>(a1);
  const FileListInternEntry *entry2 = static_cast<const FileListInternEntry *>(a2);
  const FileSortData *sort_data = static_cast<const FileSortData *>(user_data);
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

  return compare_apply_inverted(compare_tiebreaker(entry1, entry2), sort_data);
}

static int compare_asset_catalog(void *user_data, const void *a1, const void *a2)
{
  using namespace blender;

  const FileListInternEntry *entry1 = static_cast<const FileListInternEntry *>(a1);
  const FileListInternEntry *entry2 = static_cast<const FileListInternEntry *>(a2);
  const FileSortData *sort_data = static_cast<const FileSortData *>(user_data);
  const asset_system::AssetRepresentation *asset1 = entry1->get_asset();
  const asset_system::AssetRepresentation *asset2 = entry2->get_asset();

  /* Order non-assets. */
  if (asset1 && !asset2) {
    return 1;
  }
  if (!asset1 && asset2) {
    return -1;
  }
  if (!asset1 && !asset2) {
    if (int order = compare_direntry_generic(entry1, entry2); order) {
      return compare_apply_inverted(order, sort_data);
    }

    return compare_apply_inverted(compare_tiebreaker(entry1, entry2), sort_data);
  }

  const asset_system::AssetLibrary &asset_library1 = asset1->owner_asset_library();
  const asset_system::AssetLibrary &asset_library2 = asset2->owner_asset_library();

  const asset_system::AssetCatalog *catalog1 = asset_library1.catalog_service().find_catalog(
      asset1->get_metadata().catalog_id);
  const asset_system::AssetCatalog *catalog2 = asset_library2.catalog_service().find_catalog(
      asset2->get_metadata().catalog_id);

  /* Order by catalog. Always keep assets without catalog last. */
  int order = 0;

  if (catalog1 && !catalog2) {
    order = 1;
  }
  else if (!catalog1 && catalog2) {
    order = -1;
  }
  else if (catalog1 && catalog2) {
    order = BLI_strcasecmp_natural(catalog1->path.c_str(), catalog2->path.c_str());
  }

  if (!order) {
    /* Order by name. */
    order = compare_tiebreaker(entry1, entry2);
    if (!order) {
      /* Order by library name. */
      order = BLI_strcasecmp_natural(asset_library1.name().c_str(), asset_library2.name().c_str());
    }
  }

  return compare_apply_inverted(order, sort_data);
}

void filelist_sort(FileList *filelist)
{
  if (filelist->flags & FL_NEED_SORTING) {
    int (*sort_cb)(void *, const void *, const void *) = nullptr;

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
      case FILE_SORT_ASSET_CATALOG:
        sort_cb = compare_asset_catalog;
        break;
      case FILE_SORT_DEFAULT:
      default:
        BLI_assert(0);
        break;
    }

    FileSortData sort_data{};
    sort_data.inverted = (filelist->flags & FL_SORT_INVERT) != 0;
    BLI_listbase_sort_r(&filelist->filelist_intern.entries, sort_cb, &sort_data);

    filelist_tag_needs_filtering(filelist);
    filelist->flags &= ~FL_NEED_SORTING;
  }
}

void filelist_setsorting(FileList *filelist, const short sort, bool invert_sort)
{
  const bool was_invert_sort = filelist->flags & FL_SORT_INVERT;

  if ((filelist->sort != sort) || (was_invert_sort != invert_sort)) {
    filelist->sort = sort;
    filelist->flags |= FL_NEED_SORTING;
    filelist->flags = invert_sort ? (filelist->flags | FL_SORT_INVERT) :
                                    (filelist->flags & ~FL_SORT_INVERT);
  }
}
