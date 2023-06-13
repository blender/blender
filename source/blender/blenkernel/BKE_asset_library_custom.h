/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * API to manage a list of #CustomAssetLibraryDefinition items.
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CustomAssetLibraryDefinition;
struct ListBase;

struct CustomAssetLibraryDefinition *BKE_asset_library_custom_add(
    struct ListBase *custom_libraries,
    const char *name CPP_ARG_DEFAULT(nullptr),
    const char *path CPP_ARG_DEFAULT(nullptr)) ATTR_NONNULL(1);
/**
 * Unlink and free a library preference member.
 * \note Free's \a library itself.
 */
void BKE_asset_library_custom_remove(struct ListBase *custom_libraries,
                                     struct CustomAssetLibraryDefinition *library) ATTR_NONNULL();

void BKE_asset_library_custom_name_set(struct ListBase *custom_libraries,
                                       struct CustomAssetLibraryDefinition *library,
                                       const char *name) ATTR_NONNULL();

/**
 * Set the library path, ensuring it is pointing to a directory.
 * Single blend files can only act as "Current File" library; libraries on disk
 * should always be directories. Blindly sets the path without additional checks. The asset system
 * can ignore libraries that it can't resolve to a valid location. If the path does not exist,
 * that's fine; it can created as directory if necessary later.
 */
void BKE_asset_library_custom_path_set(struct CustomAssetLibraryDefinition *library,
                                       const char *path) ATTR_NONNULL();

struct CustomAssetLibraryDefinition *BKE_asset_library_custom_find_from_index(
    const struct ListBase *custom_libraries, int index) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
struct CustomAssetLibraryDefinition *BKE_asset_library_custom_find_from_name(
    const struct ListBase *custom_libraries, const char *name)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

/**
 * Return the #CustomAssetLibraryDefinition that contains the given file/directory path. The given
 * path can be the library's top-level directory, or any path inside that directory.
 *
 * When more than one asset libraries match, the first matching one is returned (no smartness when
 * there nested asset libraries).
 *
 * Return NULL when no such asset library is found. */
struct CustomAssetLibraryDefinition *BKE_asset_library_custom_containing_path(
    const struct ListBase *custom_libraries, const char *path)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

int BKE_asset_library_custom_get_index(
    const struct ListBase /*#CustomAssetLibraryDefinition*/ *custom_libraries,
    const struct CustomAssetLibraryDefinition *library) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
#ifdef __cplusplus
}
#endif
