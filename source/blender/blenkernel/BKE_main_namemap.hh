/* SPDX-FileCopyrightText: 2023 Blender Authors
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
#include "BLI_string_ref.hh"

struct ID;
struct Library;
struct Main;
struct UniqueName_Map;

/** If given pointer is not null, destroy the namemap and set the pointer to null. */
void BKE_main_namemap_destroy(UniqueName_Map **r_name_map) ATTR_NONNULL();

/**
 * Destroy all name_maps in given bmain:
 * - In bmain itself for local IDs.
 * - In the split bmains in the list is any (for linked IDs in some cases, e.g. if called during
 *   readfile code).
 * - In all of the libraries IDs (for linked IDs).
 */
void BKE_main_namemap_clear(Main &bmain);

/**
 * Check if the given name is already in use in the whole Main data-base (local and all linked
 * data).
 *
 * \return true if the name is already in use.
 */
bool BKE_main_global_namemap_contain_name(Main &bmain, short id_type, blender::StringRef name);
/**
 * Same as #BKE_main_global_namemap_contain_name, but only search in the local or related library
 * namemap.
 */
bool BKE_main_namemap_contain_name(Main &bmain,
                                   Library *lib,
                                   short id_type,
                                   blender::StringRef name);

/**
 * Ensures the given name is unique within the given ID type, in the whole Main data-base (local
 * and all linked data).
 *
 * In case of name collisions, the name will be adjusted to be unique.
 *
 * \return true if the name had to be adjusted for uniqueness.
 */
bool BKE_main_global_namemap_get_unique_name(Main &bmain, ID &id, char *r_name);
/**
 * Same as #BKE_main_global_namemap_get_name, but only make the name unique in the local or related
 * library namemap.
 */
bool BKE_main_namemap_get_unique_name(Main &bmain, ID &id, char *r_name);

/**
 * Remove a given name from usage.
 *
 * Call this whenever deleting or renaming an object.
 */
void BKE_main_namemap_remove_id(Main &bmain, ID &id);

/**
 * Check that all ID names in given `bmain` are unique (per ID type and library), and that existing
 * name maps are consistent with existing relevant IDs.
 *
 * This is typically called within an assert, or in tests.
 */
bool BKE_main_namemap_validate(Main &bmain);

/**
 * Same as #BKE_main_namemap_validate, but also fixes any issue by re-generating all name maps,
 * and ensuring again all ID names are unique.
 *
 * This is typically only used in `do_versions` code to fix broken files.
 */
bool BKE_main_namemap_validate_and_fix(Main &bmain);
