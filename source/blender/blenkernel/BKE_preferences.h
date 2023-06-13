/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"

struct UserDef;

/** Name of the asset library added by default. Needs translation with `DATA_()` still. */
#define BKE_PREFS_ASSET_LIBRARY_DEFAULT_NAME N_("User Library")

void BKE_preferences_custom_asset_library_default_add(struct UserDef *userdef) ATTR_NONNULL();

#ifdef __cplusplus
}
#endif
