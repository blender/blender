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
 */

/** \file
 * \ingroup bke
 */

#pragma once

struct Main;

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declaration, defined in intern/asset_library.hh */
typedef struct AssetLibrary AssetLibrary;

/**
 * Return the #AssetLibrary rooted at the given directory path.
 *
 * Will return the same pointer for repeated calls, until another blend file is loaded.
 *
 * To get the in-memory-only "current file" asset library, pass an empty path.
 */
struct AssetLibrary *BKE_asset_library_load(const char *library_path);

/**
 * Try to find an appropriate location for an asset library root from a file or directory path.
 * Does not check if \a input_path exists.
 *
 * The design is made to find an appropriate asset library path from a .blend file path, but
 * technically works with any file or directory as \a input_path.
 * Design is:
 * * If \a input_path lies within a known asset library path (i.e. an asset library registered in
 *   the Preferences), return the asset library path.
 * * Otherwise, if \a input_path has a parent path, return the parent path (e.g. to use the
 *   directory a .blend file is in as asset library root).
 * * If \a input_path is empty or doesn't have a parent path (e.g. because a .blend wasn't saved
 *   yet), there is no suitable path. The caller has to decide how to handle this case.
 *
 * \param r_library_path: The returned asset library path with a trailing slash, or an empty string
 *                        if no suitable path is found. Assumed to be a buffer of at least
 *                        #FILE_MAXDIR bytes.
 *
 * \return True if the function could find a valid, that is, a non-empty path to return in \a
 *         r_library_path.
 */
bool BKE_asset_library_find_suitable_root_path_from_path(
    const char *input_path, char r_library_path[768 /* FILE_MAXDIR */]);
/**
 * Uses the current location on disk of the file represented by \a bmain as input to
 * #BKE_asset_library_find_suitable_root_path_from_path(). Refer to it for a design
 * description.
 *
 * \return True if the function could find a valid, that is, a non-empty path to return in \a
 *         r_library_path. If \a bmain wasn't saved into a file yet, the return value will be
 *         false.
 */
bool BKE_asset_library_find_suitable_root_path_from_main(
    const struct Main *bmain, char r_library_path[768 /* FILE_MAXDIR */]);

/** Look up the asset's catalog and copy its simple name into #asset_data. */
void BKE_asset_library_refresh_catalog_simplename(struct AssetLibrary *asset_library,
                                                  struct AssetMetaData *asset_data);

/** Return whether any loaded AssetLibrary has unsaved changes to its catalogs. */
bool BKE_asset_library_has_any_unsaved_catalogs(void);

#ifdef __cplusplus
}
#endif
