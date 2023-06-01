/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * The public API for assets is defined in dedicated headers. This is a utility file that just
 * includes all of these.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Barely anything here. Just general editor level functions. Actual asset level code is in
 * dedicated headers. */

void ED_operatortypes_asset(void);

#ifdef __cplusplus
}
#endif

#include "../asset/ED_asset_catalog.h"
#include "../asset/ED_asset_filter.h"
#include "../asset/ED_asset_handle.h"
#include "../asset/ED_asset_import.h"
#include "../asset/ED_asset_library.h"
#include "../asset/ED_asset_list.h"
#include "../asset/ED_asset_mark_clear.h"
#include "../asset/ED_asset_temp_id_consumer.h"
#include "../asset/ED_asset_type.h"

/* C++ only headers. */
#ifdef __cplusplus
#  include "../asset/ED_asset_catalog.hh"
#  include "../asset/ED_asset_list.hh"
#endif
