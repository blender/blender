/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/* Struct members on own line. */
/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name Asset Struct
 * \{ */

#define _DNA_DEFAULT_AssetMetaData \
  { \
    0 \
  }

#define _DNA_DEFAULT_AssetLibraryReference \
  { \
    .type = ASSET_LIBRARY_LOCAL, \
    /* Not needed really (should be ignored for anything but #ASSET_LIBRARY_CUSTOM), but helps debugging. */ \
    .custom_library_index = -1, \
  }

/** \} */

/* -------------------------------------------------------------------- */
/** \name CustomAssetLibraryDefinition Struct
 * \{ */

#define _DNA_DEFAULT_CustomAssetLibraryDefinition \
  { \
    .import_method = ASSET_IMPORT_APPEND_REUSE, \
    .flag = ASSET_LIBRARY_RELATIVE_PATH, \
  }

/** \} */

/* clang-format on */
