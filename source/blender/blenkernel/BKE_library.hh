/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * API to manage `Library` data-blocks.
 */

#include "DNA_ID.h"

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"

#include "BKE_main.hh"

struct FileData;
struct ListBase;
struct Main;
struct UniqueName_Map;

namespace blender::bke::library {

struct LibraryRuntime {
  /** Used for efficient calculations of unique names. */
  UniqueName_Map *name_map = nullptr;

  /**
   * Filedata (i.e. opened blendfile) source of this library data.
   *
   * \note: This is not always matching the library's blendfile path. E.g. for archive packed
   * libraries, this will be the filedata of the packing blendfile, not of the reference/source
   * library.
   */
  FileData *filedata = nullptr;
  /**
   * Whether this library is owning its filedata pointer (and therefore should take care of
   * releasing it as part of the readfile process).
   */
  bool is_filedata_owner = false;

  /**
   * Run-time only, absolute file-path (set on read).
   * This is only for convenience, `filepath` is the real path
   * used on file read but in some cases its useful to access the absolute one.
   *
   * Use #BKE_library_filepath_set() rather than setting `filepath`
   * directly and it will be kept in sync - campbell
   */
  char filepath_abs[1024] = "";

  /** Set for indirectly linked libraries, used in the outliner and while reading. */
  Library *parent = nullptr;

  /**
   * Helper listing all archived libraries 'versions' of this library.
   * Should only contain something if this library is a regular 'real' blendfile library.
   */
  blender::Vector<Library *> archived_libraries = {};

  /** #eLibrary_Tag. */
  ushort tag = 0;

  /** Temp data needed by read/write code, and lib-override recursive re-synchronized. */
  int temp_index = 0;

  /** See BLENDER_FILE_VERSION, BLENDER_FILE_SUBVERSION, needed for do_versions. */
  short versionfile = 0;
  short subversionfile = 0;

  /* Colorspace information. */
  MainColorspace colorspace;
};

/**
 * Search for given absolute filepath in all libraries in given #ListBase.
 */
Library *search_filepath_abs(ListBase *libraries, blender::StringRef filepath_abs);

/**
 * Pack given linked ID, and all the related hierarchy.
 *
 * Will set final embedded ID into each ID::newid pointers.
 */
void pack_linked_id_hierarchy(Main &bmain, ID &root_id);

/**
 * Cleanup references to removed/deleted archive libraries in their archive parent.
 */
void main_cleanup_parent_archives(Main &bmain);

/**
 * Ensure that there is a valid archive library in given `bmain`, for the given `id`,
 * `reference_library` and `id_deep_hash` parameters.
 *
 * \note Typically, both the `reference_library` and `id_deep_hash` are the same as the `id`
 * library and deephash, but in some cases they may still differ (see e.g.
 * #PartialWriteContext::ensure_library).
 *
 * \return the archive library. `is_new` is set to `true` if a new archive library had to be
 * created, false if an existing one could be re-used.
 */
Library *ensure_archive_library(
    Main &bmain, ID &id, Library &reference_library, const IDHash &id_deep_hash, bool &is_new);

};  // namespace blender::bke::library

/** #LibraryRuntime.tag */
enum eLibrary_Tag {
  /** Automatic recursive re-synchronize was needed when linking/loading data from that library. */
  LIBRARY_TAG_RESYNC_REQUIRED = 1 << 0,
  /**
   * Data-blocks from this library are editable in the UI despite being linked.
   * Used for asset that can be temporarily or permanently edited.
   * Currently all data-blocks from this library will be edited. In the future this
   * may need to become per data-block to handle cases where a library is both used
   * for editable assets and linked into the blend file for other reasons.
   */
  LIBRARY_ASSET_EDITABLE = 1 << 1,
  /** The blend file of this library is writable for asset editing. */
  LIBRARY_ASSET_FILE_WRITABLE = 1 << 2,
  /**
   * The blend file of this library has the #G_FILE_ASSET_EDIT_FILE flag set (refer to it for more
   * info).
   */
  LIBRARY_IS_ASSET_EDIT_FILE = 1 << 3,
};

void BKE_library_filepath_set(Main *bmain, Library *lib, const char *filepath);

/**
 * Rebuild the hierarchy of libraries, after e.g. deleting or relocating one, often some indirectly
 * linked libraries lose their 'parent' pointer, making them wrongly directly used ones.
 */
void BKE_library_main_rebuild_hierarchy(Main *bmain);
