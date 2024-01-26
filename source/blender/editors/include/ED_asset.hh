/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * The public API for assets is defined in dedicated headers. This is a utility file that just
 * includes all of these.
 */

#pragma once

/* Barely anything here. Just general editor level functions. Actual asset level code is in
 * dedicated headers. */

#include "../asset/ED_asset_catalog.hh"
#include "../asset/ED_asset_handle.hh"
#include "../asset/ED_asset_library.hh"
#include "../asset/ED_asset_list.hh"
#include "../asset/ED_asset_mark_clear.hh"
#include "../asset/ED_asset_temp_id_consumer.hh"
#include "../asset/ED_asset_type.hh"

#include "../asset/ED_asset_catalog.hh"
#include "../asset/ED_asset_filter.hh"
#include "../asset/ED_asset_import.hh"
#include "../asset/ED_asset_list.hh"

namespace blender::ed::asset {

void operatortypes_asset();

}
