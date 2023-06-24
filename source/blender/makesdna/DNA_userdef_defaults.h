/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_asset_types.h"

/* Struct members on own line. */
/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name bUserAssetLibrary Struct
 * \{ */

#define _DNA_DEFAULT_bUserAssetLibrary \
  { \
    .import_method = ASSET_IMPORT_APPEND_REUSE, \
    .flag = ASSET_LIBRARY_RELATIVE_PATH, \
  }

/** \} */

/* clang-format off */

/** \} */
