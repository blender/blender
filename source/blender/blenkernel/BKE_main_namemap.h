/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * API to ensure name uniqueness.
 *
 * Main database contains the UniqueName_Map which is a cache that tracks names, base
 * names and their suffixes currently in use. So that whenever a new name has to be
 * assigned or validated, it can quickly ensure uniqueness and adjust the name in case
 * of collisions.
 *
 * \section Function Names
 *
 * - `BKE_main_namemap_` Should be used for functions in this file.
 */

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct Main;
struct UniqueName_Map;

struct UniqueName_Map *BKE_main_namemap_create(void) ATTR_WARN_UNUSED_RESULT;
void BKE_main_namemap_destroy(struct UniqueName_Map **r_name_map) ATTR_NONNULL();

/**
 * Destroy all name_maps in given bmain:
 * - In bmain itself for local IDs.
 * - In the split bmains in the list is any (for linked IDs in some cases, e.g. if called during
 *   readfile code).
 * - In all of the libraries IDs (for linked IDs).
 */
void BKE_main_namemap_clear(struct Main *bmain) ATTR_NONNULL();

/**
 * Ensures the given name is unique within the given ID type.
 *
 * In case of name collisions, the name will be adjusted to be unique.
 *
 * \param do_unique_in_bmain: if `true`, ensure that the final name is unique in the whole Main
 * (for the given ID type), not only in the set of IDs from the same library.
 *
 * \return true if the name had to be adjusted for uniqueness.
 */
bool BKE_main_namemap_get_name(struct Main *bmain,
                               struct ID *id,
                               char *name,
                               const bool do_unique_in_bmain) ATTR_NONNULL();

/**
 * Remove a given name from usage.
 *
 * Call this whenever deleting or renaming an object.
 */
void BKE_main_namemap_remove_name(struct Main *bmain, struct ID *id, const char *name)
    ATTR_NONNULL();

/**
 * Check that all ID names in given `bmain` are unique (per ID type and library), and that existing
 * name maps are consistent with existing relevant IDs.
 *
 * This is typically called within an assert, or in tests.
 */
bool BKE_main_namemap_validate(struct Main *bmain) ATTR_NONNULL();

/**
 * Same as #BKE_main_namemap_validate, but also fixes any issue by re-generating all name maps,
 * and ensuring again all ID names are unique.
 *
 * This is typically only used in `do_versions` code to fix broken files.
 */
bool BKE_main_namemap_validate_and_fix(struct Main *bmain) ATTR_NONNULL();

#ifdef __cplusplus
}
#endif
