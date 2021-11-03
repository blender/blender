/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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
