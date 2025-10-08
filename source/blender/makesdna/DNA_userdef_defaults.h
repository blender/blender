/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_asset_types.h"

/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name bUserAssetLibrary Struct
 * \{ */

#define _DNA_DEFAULT_bUserAssetLibrary \
  { \
    .import_method = ASSET_IMPORT_PACK, \
    .flag = ASSET_LIBRARY_RELATIVE_PATH, \
  }

/** \} */

/* -------------------------------------------------------------------- */
/** \name bUserExtensionRepo Struct
 * \{ */

#define _DNA_DEFAULT_bUserExtensionRepo \
  { \
    .name = {'\0'}, \
    .module = {'\0'}, \
    .custom_dirpath = {'\0'}, \
    .remote_url = {'\0'}, \
    .flag = 0, \
  }

/** \} */

/* -------------------------------------------------------------------- */
/** \name bUserExtensionRepo Struct
 * \{ */

#define _DNA_DEFAULT_bUserAssetShelfSettings \
  { \
    .shelf_idname = {'\0'}, \
    .enabled_catalog_paths = {NULL, NULL}, \
  }

/** \} */

/* clang-format on */
