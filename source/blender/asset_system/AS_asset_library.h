/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

struct AssetWeakReference;
struct IDRemapper;

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declaration, defined in intern/asset_library.hh */
typedef struct AssetLibrary AssetLibrary;

/**
 * Force clearing of all asset library data. After calling this, new asset libraries can be loaded
 * just as usual using #AS_asset_library_load(), no init or other setup is needed.
 *
 * Does not need to be called on exit, this is handled internally.
 */
void AS_asset_libraries_exit(void);

/**
 * Return the #AssetLibrary rooted at the given directory path.
 *
 * Will return the same pointer for repeated calls, until another blend file is loaded.
 *
 * To get the in-memory-only "current file" asset library, pass an empty path.
 */
struct AssetLibrary *AS_asset_library_load(const char *name, const char *library_dirpath);

/** Look up the asset's catalog and copy its simple name into #asset_data. */
void AS_asset_library_refresh_catalog_simplename(struct AssetLibrary *asset_library,
                                                 struct AssetMetaData *asset_data);

/** Return whether any loaded AssetLibrary has unsaved changes to its catalogs. */
bool AS_asset_library_has_any_unsaved_catalogs(void);

/** An asset library can include local IDs (IDs in the current file). Their pointers need to be
 * remapped on change (or assets removed as IDs gets removed). */
void AS_asset_library_remap_ids(const struct IDRemapper *mappings);

/**
 * Attempt to resolve a full path to an asset based on the currently available (not necessary
 * loaded) asset libraries, and split it into it's directory, ID group and ID name components. The
 * path is not guaranteed to exist on disk. On failure to resolve the reference, return arguments
 * will point to null.
 *
 * \note Only works for asset libraries on disk and the "Current File" one (others can't be
 *       resolved).
 *
 * \param r_path_buffer: Buffer to hold the result in on success. Will be the full path with null
 *                       terminators instead of slashes separating the directory, group and name
 *                       components. Must be at least #FILE_MAX_LIBEXTRA long.
 * \param r_dir: Returns the .blend file path with native slashes on success. Optional (passing
 *               null is allowed). For the "Current File" library this will be empty.
 * \param r_group: Returns the ID group such as "Object", "Material" or "Brush". Optional (passing
 *                 null is allowed).
 * \param r_name: Returns the ID name on success. Optional (passing null is allowed).
 */
void AS_asset_full_path_explode_from_weak_ref(const struct AssetWeakReference *asset_reference,
                                              char r_path_buffer[1090 /* FILE_MAX_LIBEXTRA */],
                                              char **r_dir,
                                              char **r_group,
                                              char **r_name);

#ifdef __cplusplus
}
#endif
