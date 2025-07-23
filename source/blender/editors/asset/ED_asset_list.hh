/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "BLI_function_ref.hh"

struct AssetLibraryReference;
struct bContext;
struct ID;
struct ImBuf;
struct wmNotifier;
struct wmRegionListenerParams;
namespace blender::asset_system {
class AssetLibrary;
class AssetRepresentation;
}  // namespace blender::asset_system

namespace blender::ed::asset::list {

void asset_reading_region_listen_fn(const wmRegionListenerParams *params);

/**
 * Get the asset library being read into an asset-list and identified using \a library_reference.
 *
 * \note The asset library may be allocated and loaded asynchronously, so it's not available right
 *       after fetching, and this function will return null. The asset list code sends
 *       `NC_ASSET | ND_ASSET_LIST_READING` notifiers until loading is done, they can be used
 *       to continuously call this function to retrieve the asset library once available.
 */
asset_system::AssetLibrary *library_get_once_available(
    const AssetLibraryReference &library_reference);

/** Can return false to stop iterating. */
using AssetListIterFn = FunctionRef<bool(asset_system::AssetRepresentation &)>;
void iterate(const AssetLibraryReference &library_reference, AssetListIterFn fn);

/**
 * Invoke asset list reading, potentially in a parallel job. Won't wait until the job is done,
 * and may return earlier.
 *
 * \see: #storage_fetch_blocking for a blocking version.
 * \warning: Asset list reading involves an #AS_asset_library_load() call which may reload asset
 *           library data like catalogs (invalidating pointers). Refer to its warning for details.
 */
void storage_fetch(const AssetLibraryReference *library_reference, const bContext *C);
/**
 * Invoke asset list reading, guaranteed to execute on the same thread.
 *
 * \see #storage_fetch for an asynchronous version.
 */
void storage_fetch_blocking(const AssetLibraryReference &library_reference, const bContext &C);
bool is_loaded(const AssetLibraryReference *library_reference);
/**
 * Clears this asset library and the "All" asset library for reload in both the static asset list
 * storage, as well as for all open asset browsers. Call this whenever the content of the given
 * asset library changed in a way that a reload is necessary.
 */
void clear(const AssetLibraryReference *library_reference, const bContext *C);
/**
 * Clears the all asset library for reload in both the static asset list storage, as well as for
 * all open asset browsers. Call this whenever any asset library content changed in a way that a
 * reload is necessary.
 */
void clear_all_library(const bContext *C);
/**
 * Returns if the given asset library in global asset list storage.
 */
bool has_list_storage_for_library(const AssetLibraryReference *library_reference);
/**
 * Returns if any asset browser is visible showing the given asset library. Asset browsers are not
 * really handled by this API, but for convenience of managing clearing it's handled here together
 * with #has_list_storage_for_library().
 */
bool has_asset_browser_storage_for_library(const AssetLibraryReference *library_reference,
                                           const bContext *C);
/**
 * Tag all asset lists in the storage that show main data as needing an update (re-fetch).
 *
 * This only tags the data. If the asset list is visible on screen, the space is still responsible
 * for ensuring the necessary redraw. It can use #listen() to check if the asset-list
 * needs a redraw for a given notifier.
 */
void storage_tag_main_data_dirty();
/**
 * Remapping of ID pointers within the asset lists. Typically called when an ID is deleted to clear
 * all references to it (\a id_new is null then).
 */
void storage_id_remap(ID *id_old, ID *id_new);
/**
 * Can't wait for static deallocation to run. There's nested data allocated with our guarded
 * allocator, it will complain about unfreed memory on exit.
 */
void storage_exit();

/**
 * \return True if the region needs a UI redraw.
 */
bool listen(const wmNotifier *notifier);
/**
 * \return The number of assets stored in the asset list for \a library_reference, or -1 if there
 *         is no list fetched for it.
 */
int size(const AssetLibraryReference *library_reference);

}  // namespace blender::ed::asset::list
